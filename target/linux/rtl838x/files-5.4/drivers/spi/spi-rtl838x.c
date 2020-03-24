/*
 * SPI controller driver for Realtek RTL838x series
 * 
 * - Supports Single I/O only
 * - Supports SPI mode 0 only?
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>

#define DRIVER_NAME			"spi-rtl838x"

#define RTL838X_SPIF_REGISTER_BASE		0xB8001200
/* SFCR - SPI Flash Configuration Register */
#define RTL838X_SPIF_CONFIG_REG			0x0
#define   RTL838X_SFCR_SPI_CLK_DIV		  29
#define     RTL838X_SFCR_SPI_CLK_DIV_MASK	  GENMASK(31, 29)

/* SFCR2 - SPI Flash Configuration Register 2 */
#define RTL838X_SPIF_CONFIG_REG2		0x4
#define     RTL838X_SFCR2_SFCMD		  	  24
#define     RTL838X_SFCR2_SFSIZE_MASK		  GENMASK(23, 21)
#define   RTL838X_SFCR2_RDOPT			  BIT(20)
#define   RTL838X_SFCR2_HOLD_TILL_SFDR2		  BIT(10)

/* SFCSR - SPI Flash Controll & Status Register */
#define RTL838X_SPIF_CONTROL_STAT_REG		0x8
#define   RTL838X_SFCSR_SPI_CSB0		  BIT(31)
#define   RTL838X_SFCSR_SPI_CSB1		  BIT(30)
#define   RTL838X_SFCSR_SPI_LEN			  28
#define     RTL838X_SFCSR_SPI_LEN_MASK		  GENMASK(29, 28)
#define   RTL838X_SFCSR_SPI_RDY			  BIT(27) /* SPI ready flag */
#define     RTL838X_SFCSR_IO_WIDTH_MASK		  GENMASK(26, 25)
#define   RTL838X_SFCSR_CHIP_SEL		  BIT(24)

/* SFDR - SPI Flash Data Register */
#define RTL838X_SPIF_DATA_REG			0xc

/* SFDR2 - SPI Flash Data Register 2 */
#define RTL838X_SPIF_DATA_REG2			0x10

#define RTL838X_CSNUM				2
#define RTL838X_MAX_TRANSFER_SIZE		256
#define REALTEK_SPI_WAIT_MAX_LOOP		2000 /* usec */

#define RTL838X_DRAM_FREQ			200000000 /* 200MHz */

/*
 * Data length in read/write
 */
enum {
	SPI_RW_LEN1 = 0,
	SPI_RW_LEN2,
	SPI_RW_LEN3,
	SPI_RW_LEN4,
};

/*
 * I/O width in writing? when reading? LEN=0?
 */
enum {
	SPI_IO_WIDTH1 = 1,	/* Single IO */
	SPI_IO_WIDTH2,		/* Dual IO */
	SPI_IO_WIDTH4,		/* Quad IO */
};

struct rtl838x_spi {
	struct spi_master	*master;
	void __iomem		*base;
	unsigned int 		dram_freq;
	unsigned int		speed;
//	struct clk		*clk;
};

static inline struct rtl838x_spi *spidev_to_rtl838x_spi(struct spi_device *spi)
{
	return spi_controller_get_devdata(spi->master);
}

static inline u32 rtl838x_reg_read(struct rtl838x_spi *rs, u32 reg)
{
	return __raw_readl(RTL838X_SPIF_REGISTER_BASE + reg);
//	return ioread32(rs->base + reg);
}

static inline void rtl838x_reg_write(struct rtl838x_spi *rs, u32 reg, u32 val)
{
	__raw_writel(val, RTL838X_SPIF_REGISTER_BASE + reg);
//	iowrite32(val, rs->base + reg);
}

static void rtl838x_dump_spi_regs(struct rtl838x_spi *rs)
{
	u32 reg;

	reg = rtl838x_reg_read(rs, RTL838X_SPIF_CONFIG_REG);
	printk(KERN_INFO "# rtl838x regs: SFCR -> %x, ", reg);

	reg = rtl838x_reg_read(rs, RTL838X_SPIF_CONFIG_REG2);
	printk(KERN_INFO "SFCR2 -> %x, ", reg);

	reg = rtl838x_reg_read(rs, RTL838X_SPIF_CONTROL_STAT_REG);
	printk(KERN_INFO "SFCSR -> %x, ", reg);
#if 0
	reg = rtl838x_reg_read(rs, RTL838X_SPIF_DATA_REG);
	printk(KERN_INFO "SFDR -> %x, ", reg);

	reg = rtl838x_reg_read(rs, RTL838X_SPIF_DATA_REG2);
	printk(KERN_INFO "SFDR2 -> %x\n", reg);
#endif
}
static inline int rtl838x_spi_wait_till_ready(struct rtl838x_spi *rs)
{
	int i;

	for (i = 0; i < REALTEK_SPI_WAIT_MAX_LOOP; i++) {
		u32 status;

		status = rtl838x_reg_read(rs, RTL838X_SPIF_CONTROL_STAT_REG);
		if ((status & RTL838X_SFCSR_SPI_RDY) != 0) {
			printk(KERN_INFO "exit waiting loop in rtl838x_spi_wait_till_ready\n");
			return 0;
		}
		cpu_relax();
		udelay(1);
	}

	return -ETIMEDOUT;
}

static void rtl838x_spi_set_cs(struct spi_device *spi, int enable)
{
	struct rtl838x_spi *rs = spidev_to_rtl838x_spi(spi);
	int cs = spi->chip_select;

	dev_info(&spi->dev, "## rtl838x_spi_set_cs\n");
	/* needed for MMU constraints */
	rtl838x_reg_write(rs, RTL838X_SPIF_CONTROL_STAT_REG
		, RTL838X_SFCSR_SPI_CSB0 | RTL838X_SFCSR_SPI_CSB1);
	rtl838x_spi_wait_till_ready(rs);
	rtl838x_reg_write(rs, RTL838X_SPIF_CONTROL_STAT_REG, 0);
	rtl838x_spi_wait_till_ready(rs);
	rtl838x_reg_write(rs, RTL838X_SPIF_CONTROL_STAT_REG
		, RTL838X_SFCSR_SPI_CSB0 | RTL838X_SFCSR_SPI_CSB1);
	rtl838x_spi_wait_till_ready(rs);

	if (!enable)
		return;

	rtl838x_reg_write(rs, RTL838X_SPIF_CONTROL_STAT_REG, (cs == 0)
		? RTL838X_SFCSR_SPI_CSB1				/* set CSB1 to HIGH if cs=0 */
		: RTL838X_SFCSR_SPI_CSB0 | RTL838X_SFCSR_CHIP_SEL);	/* set CSB0 to HIGH if cs=1 */
	rtl838x_spi_wait_till_ready(rs);
	dev_info(&spi->dev, "## exit rtl838x_spi_set_cs\n");
}

static int rtl838x_spi_prepare(struct spi_device *spi, unsigned int speed)
{
	struct rtl838x_spi *rs = spidev_to_rtl838x_spi(spi);
	u32 rate;
	u32 reg;

	dev_info(&spi->dev, "## rtl838x_spi_prepare\n");
	dev_dbg(&spi->dev, "speed: %u\n", speed);

	rate = DIV_ROUND_UP(rs->dram_freq, speed);
	dev_dbg(&spi->dev, "rate-1: %u\n", rate);

	if (rate > 16)
		return -EINVAL;

	if (rate < 2)
		rate = 2;

	reg = rtl838x_reg_read(rs, RTL838X_SPIF_CONFIG_REG);
	reg &= ~(RTL838X_SFCR_SPI_CLK_DIV_MASK);
	reg |= ((rate - 2) / 2) << RTL838X_SFCR_SPI_CLK_DIV;
	rs->speed = speed;

	rtl838x_reg_write(rs, RTL838X_SPIF_CONFIG_REG, reg);

	dev_info(&spi->dev, "## exit rtl838x_spi_prepare\n");
	return 0;
}

static void rtl838x_spi_read(struct rtl838x_spi *rs, int rx_len, u8 *buf)
{
	u32 sfcsr;

	sfcsr = rtl838x_reg_read(rs, RTL838X_SPIF_CONTROL_STAT_REG);
	/* set 0 to LEN bits */
	sfcsr &= ~RTL838X_SFCSR_SPI_LEN_MASK;

	/* set 4-byte len (3) */
	rtl838x_spi_wait_till_ready(rs);
	rtl838x_reg_write(rs, RTL838X_SPIF_CONTROL_STAT_REG, sfcsr
			| (SPI_RW_LEN4 << RTL838X_SFCSR_SPI_LEN));
	/* read each 4-byte to buf*/
	while (rx_len>=4) {
		rtl838x_spi_wait_till_ready(rs);
		*((u32*) buf) = rtl838x_reg_read(rs, RTL838X_SPIF_DATA_REG);
		buf+=4;
		rx_len-=4;
	}

	/* set 1-byte len (0) */
	rtl838x_spi_wait_till_ready(rs);
	rtl838x_reg_write(rs, RTL838X_SPIF_CONTROL_STAT_REG, sfcsr);
	/* read each 1-byte to buf (rest 1-3 bytes) */
	while (rx_len > 0) {
		rtl838x_spi_wait_till_ready(rs);
		*(buf) = rtl838x_reg_read(rs, RTL838X_SPIF_DATA_REG)
			>> (sizeof(u8) * 3);
		buf++;
		rx_len--;
	}
}

static void rtl838x_spi_write(struct rtl838x_spi *rs, int tx_len, u8 *buf)
{
	u32 sfcsr;

	sfcsr = rtl838x_reg_read(rs, RTL838X_SPIF_CONTROL_STAT_REG);
	/* set 0 to LEN bits */
	sfcsr &= ~RTL838X_SFCSR_SPI_LEN_MASK;

	/* set 4-byte len (3) */
	rtl838x_spi_wait_till_ready(rs);
	rtl838x_reg_write(rs, RTL838X_SPIF_CONTROL_STAT_REG, sfcsr
			| (SPI_RW_LEN4 << RTL838X_SFCSR_SPI_LEN));
	/* write each 4-byte to DATA reg */
	while (tx_len >= 4) {
		rtl838x_spi_wait_till_ready(rs);
		rtl838x_reg_write(rs, RTL838X_SPIF_DATA_REG, *((u32*) buf));
		buf+=4;
		tx_len-=4;
	}

	/* set 1-byte len (0) */
	rtl838x_spi_wait_till_ready(rs);
	rtl838x_reg_write(rs, RTL838X_SPIF_CONTROL_STAT_REG, sfcsr);
	/* write each 1-byte to DATA reg (rest 1-3 bytes) */
	while (tx_len > 0) {
		rtl838x_spi_wait_till_ready(rs);
		rtl838x_reg_write(rs, RTL838X_SPIF_DATA_REG,
				((u32) *buf) << (sizeof(u8) * 3));
		buf++;
		tx_len--;
	}
}

static int rtl838x_spi_transfer_one_message(struct spi_controller *master,
					struct spi_message *m)
{
	struct rtl838x_spi *rs = spi_controller_get_devdata(master);
	struct spi_device *spi = m->spi;
	unsigned int speed = spi->max_speed_hz;
	struct spi_transfer *t = NULL;
	int status = 0;

	rtl838x_dump_spi_regs(rs);
	dev_info(&spi->dev, "## rtl838x_spi_transfer_one_message\n");
	status = rtl838x_spi_wait_till_ready(rs);
	if (status)
		goto msg_done;

	list_for_each_entry(t, &m->transfers, transfer_list)
		if (t->speed_hz < speed)
			speed = t->speed_hz;

	if (rtl838x_spi_prepare(spi, speed)) {
		status = -EIO;
		goto msg_done;
	}

	/* setup CS */
	rtl838x_spi_set_cs(spi, 1);

	m->actual_length = 0;
	list_for_each_entry(t, &m->transfers, transfer_list) {
		if ((t->rx_buf) && (t->tx_buf)) {
			status = -EIO;
			goto msg_done;
		} else if (t->rx_buf) {
			rtl838x_spi_read(rs, t->len, t->rx_buf);
		} else if (t->tx_buf) {
			rtl838x_spi_write(rs, t->len, t->tx_buf);
		}
		m->actual_length += t->len;
	}

	/* disable CS (set to HIGH) */
	rtl838x_spi_set_cs(spi, 0);

msg_done:
	m->status = status;
	spi_finalize_current_message(master);

	dev_info(&spi->dev, "## exit rtl838x_spi_transfer_one_message: %d\n", status);
	return 0;
}

static int rtl838x_spi_setup(struct spi_device *spi)
{
	struct rtl838x_spi *rs = spidev_to_rtl838x_spi(spi);

	dev_info(&spi->dev, "## rtl838x_spi_setup\n");
	dev_dbg(&spi->dev, "setup: requested max_speed %u\n",
		spi->max_speed_hz);
	/* The RTL838x SPI controller seems to like the same as the RTL819x
	 * SPI controller; The RTL838x/RTL819x SPI controller allows as the
	 * divider value up to 7 (111b).
	 * SPI freq = DRAM freq / div
	 * 
	 * div values (from RTL819x):
	 *   0: 2
	 *   1: 4
	 *   2: 6
	 *   3: 8
	 *   4: 10
	 *   5: 12
	 *   6: 14
	 *   8: 16
	 */
	if ((spi->max_speed_hz == 0) ||
	    (spi->max_speed_hz > (rs->dram_freq / 2)))
	    	spi->max_speed_hz = (rs->dram_freq / 2);

	if (spi->max_speed_hz < (rs->dram_freq / 16)) {
		dev_err(&spi->dev, "setup: requested speed is too low %u Hz\n",
			spi->max_speed_hz);
		return -EINVAL;
	}

	dev_info(&spi->dev, "## exit rtl838x_spi_setup\n");
	return 0;
}

static size_t rtl838x_spi_max_transfer_size(struct spi_device *spi)
{
	return RTL838X_MAX_TRANSFER_SIZE;
}

static int rtl838x_spi_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct rtl838x_spi *rs;
	void __iomem *base;
	struct resource *r;
	int status = 0;
//	struct clk *clk;
	u32 sfcr2, tmp;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(base))
		return PTR_ERR(base);

	master = spi_alloc_master(&pdev->dev, sizeof(*rs));
	if (!master) {
		dev_err(&pdev->dev, "master allocation failed\n");
		return -ENOMEM;
	}

//	master->mode_bits = 
//	master->flags =
	master->max_transfer_size = rtl838x_spi_max_transfer_size;
	master->setup = rtl838x_spi_setup;
	master->transfer_one_message = rtl838x_spi_transfer_one_message;
	master->bits_per_word_mask = SPI_BPW_MASK(8);
	master->dev.of_node = pdev->dev.of_node;
	master->num_chipselect = RTL838X_CSNUM;

	dev_set_drvdata(&pdev->dev, master);

	/* TODO: auto-detection for DRAM freq? */
	rs = spi_controller_get_devdata(master);
	rs->base = base;
//	rs->clk = clk;
	rs->master = master;
	rs->dram_freq = RTL838X_DRAM_FREQ;
//	rs->dram_freq = clk_get_rate(rs->clk);
	dev_info(&pdev->dev, "dram_freq: %u\n", rs->dram_freq);

	rtl838x_dump_spi_regs(rs);
	/*
	 * set to serial I/O
	 * (U-Boot configure to dual I/O or quad I/O by default if that I/O is
	 * supported by the flash chip?)
	 * below is a part of spi_enter_sio, is it OK?
	 */
	/* CMDIO=0, ADDRIO=0, DUMMYCYCLE=0?, DATAIO=0 */
#if 0
	dev_info(&pdev->dev, "SFCR2 configuration in probe\n");
	tmp = rtl838x_reg_read(rs, RTL838X_SPIF_CONFIG_REG2);
	sfcr2 = tmp & RTL838X_SFCR2_SFSIZE_MASK;
	sfcr2 |= tmp & RTL838X_SFCR2_RDOPT;
	sfcr2 |= 0x03 << RTL838X_SFCR2_SFCMD;	/* set READ cmd instead of READ_FAST */

	rtl838x_spi_wait_till_ready(rs);
	rtl838x_reg_write(rs, RTL838X_SPIF_CONFIG_REG2, sfcr2);
	rtl838x_spi_wait_till_ready(rs);
	dev_info(&pdev->dev, "SFCR2 configuration done\n");
#endif
	return devm_spi_register_controller(&pdev->dev, master);
}

static int rtl838x_spi_remove(struct platform_device *pdev)
{
	/* TODO */
	return 0;
}

static const struct of_device_id rtl838x_spi_match[] = {
	{ .compatible = "realtek,rtl838x-spi" },
	{},
};
MODULE_DEVICE_TABLE(of, rtl838x_spi_match);

static struct platform_driver rtl838x_spi_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = rtl838x_spi_match,
	},
	.probe = rtl838x_spi_probe,
	.remove = rtl838x_spi_remove,
};

module_platform_driver(rtl838x_spi_driver);

MODULE_DESCRIPTION("Realtek RTL838x SPI driver");
MODULE_LICENSE("GPL v2");