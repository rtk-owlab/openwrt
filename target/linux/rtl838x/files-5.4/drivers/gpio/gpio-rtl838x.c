// SPDX-License-Identifier: GPL-2.0-only
#include <linux/gpio/driver.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <asm/mach-rtl838x/mach-rtl838x.h>

/* RTL8231 registers for LED control */
#define RTL8231_LED_FUNC0			0x0000
#define RTL8231_GPIO_PIN_SEL(gpio)		((0x0002) + ((gpio) >> 4))
#define RTL8231_GPIO_DIR(gpio)			((0x0005) + ((gpio) >> 4))
#define RTL8231_GPIO_DATA(gpio)			((0x001C) + ((gpio) >> 4))

struct rtl838x_gpios {
	struct gpio_chip gc;
	u32 id;
	struct device *dev;
	int irq;
	int bus_id;
	int num_leds;
	int min_led;
	int leds_per_port;
	u32 led_mode;
};

/*
	TODO: TRY TYPE for RST on Zyxel
	RTL838X_GPIO_PABC_TYPE
*/

u32 rtl838x_rtl8231_read(u8 bus_id, u32 reg)
{
	u32 t = 0;
	reg &= 0x1f;
	bus_id &= 0x1f;
	/* Calculate read register address */
	t = (bus_id << 2) | (reg << 7);
	
	/* Set execution bit: cleared when operation completed */
	t |= 1;
	rtl838x_w32(t, RTL838X_EXT_GPIO_INDRT_ACCESS);
	do {	/* TODO: Return 0x80000000 if timeout */
		t = rtl838x_r32(RTL838X_EXT_GPIO_INDRT_ACCESS);
	} while (t & 1);
	pr_debug("rtl838x_rtl8231_read: %x, %x, %x\n", bus_id, reg, (t & 0xffff0000) >> 16);
	return (t & 0xffff0000) >> 16;
}

int rtl838x_rtl8231_write(u8 bus_id, u32 reg, u32 data)
{
	u32 t = 0;
	data &= 0xffff;
	reg &= 0x1f;
	bus_id &= 0x1f;
	
	pr_debug("rtl838x_rtl8231_write: %x, %x, %x\n", bus_id, reg, data);
	t = (bus_id << 2) | (reg << 7) | (data << 16);
	/* Set write bit */
	t |= 2;

	/* Set execution bit: cleared when operation completed */
	t |= 1;
	rtl838x_w32(t, RTL838X_EXT_GPIO_INDRT_ACCESS);
	do {	/* TODO: Return -1 if timeout */
		t = rtl838x_r32(RTL838X_EXT_GPIO_INDRT_ACCESS);
	} while (t & 1);
	
	return 0;
}

static int rtl8231_pin_dir(u8 bus_id, u32 gpio, u32 dir)
{
	/* dir 1: input
	 * dir 0: output
	 */
	
	 u32  v;
	 
	 if( gpio > 31 ) {
		 return -1;
		 pr_err("rtl8231_pin_dir: GPIO >= 32 not implemented!");
	 }
	 
	/* Select GPIO function for pin */
	v = rtl838x_rtl8231_read(bus_id, RTL8231_GPIO_PIN_SEL(gpio));
	if (v & 0x80000000) {
		pr_err("Error reading RTL8231\n");
		return -1;
	}
	rtl838x_rtl8231_write(bus_id, RTL8231_GPIO_PIN_SEL(gpio), v | (1 << (gpio % 16)));

	
	v = rtl838x_rtl8231_read(bus_id, RTL8231_GPIO_DIR(gpio));
	if (v & 0x80000000) {
		pr_err("Error reading RTL8231\n");
		return -1;
	}
	rtl838x_rtl8231_write(bus_id, RTL8231_GPIO_DIR(gpio),
				(v & ~(1 << (gpio % 16))) | (dir << (gpio % 16)));
	return 0;
}

static int rtl8231_pin_dir_get(u8 bus_id, u32 gpio, u32 *dir)
{
	/* dir 1: input
	 * dir 0: output
	 */
	
	 u32  v;
	 
	 if( gpio > 31 ) {
		 return -1;
		 pr_err("rtl8231_pin_dir_get: GPIO >= 32 not implemented!");
	 }
	 
	v = rtl838x_rtl8231_read(bus_id, RTL8231_GPIO_DIR(gpio));
	if (v & (1 << (gpio %16)))
		*dir = 1;
	else
		*dir = 0;
	return 0;
}

static int rtl8231_pin_set(u8 bus_id, u32 gpio, u32 data)
{
	u32 v = rtl838x_rtl8231_read(bus_id, RTL8231_GPIO_DATA(gpio));
	if (v & 0x80000000) {
		pr_err("Error reading RTL8231\n");
		return -1;
	}
	rtl838x_rtl8231_write(bus_id, RTL8231_GPIO_DATA(gpio), 
			      (v & ~(1 << (gpio % 16))) | (data << (gpio % 16)));
	return 0;
}

static int rtl8231_pin_get(u8 bus_id, u32 gpio, u16 *state)
{
	u32 v = rtl838x_rtl8231_read(bus_id, RTL8231_GPIO_DATA(gpio));
	if (v & 0x80000000) {
		pr_err("Error reading RTL8231\n");
		return -1;
	}
	
	*state = v & 0xffff;
	return 0;
}


static int rtl838x_direction_input(struct gpio_chip *gc, unsigned offset)
{
	struct rtl838x_gpios *gpios = gpiochip_get_data(gc);

	pr_debug("rtl838x_direction_input: %d\n", offset);

	if (offset < 32)
		rtl838x_w32_mask(1 << offset, 0, RTL838X_GPIO_PABC_DIR);

	/* Internal LED driver does not support input */
	if (offset >=32 && offset <64) 
		return -ENOTSUPP;

	if (offset >= 64 && offset < 100 && gpios->bus_id >= 0)
		return rtl8231_pin_dir(gpios->bus_id, offset - 64, 1);

	return -ENOTSUPP;
}

static int rtl838x_direction_output(struct gpio_chip *gc, unsigned offset,
				 int value)
{
	struct rtl838x_gpios *gpios = gpiochip_get_data(gc);
	pr_debug("rtl838x_direction_output: %d\n", offset);

	if (offset < 32)
		rtl838x_w32_mask(0, 1 << offset, RTL838X_GPIO_PABC_DIR);

	/* LED for PWR and SYS driver is direction output by default */
	if (offset >=32 && offset <64) 
		return 0;

	if (offset >= 64 && offset < 100 && gpios->bus_id >= 0)
		return rtl8231_pin_dir(gpios->bus_id, offset - 64, 0);
	return 0;
}

static int rtl838x_get_direction(struct gpio_chip *gc, unsigned offset)
{
	u32 v = 0;
	struct rtl838x_gpios *gpios = gpiochip_get_data(gc);

	pr_debug("rtl838x_get_direction: %d\n", offset);
	if (offset < 32) {
		v = rtl838x_r32(RTL838X_GPIO_PABC_DIR);
		if (v & (1 << offset))
			return 0;
		return 1;
	}

	/* LED driver for PWR and SYS is direction output by default */
	if (offset >=32 && offset <64) 
		return 0;

	if (offset >= 64 && offset < 100 && gpios->bus_id >= 0) {
		rtl8231_pin_dir_get(gpios->bus_id, offset - 64, &v);
		return v;
	}

	return 0;
}

static int rtl838x_gpio_get(struct gpio_chip *gc, unsigned offset)
{
	u32 v;
	u16 state = 0;
	int bit;
	struct rtl838x_gpios *gpios = gpiochip_get_data(gc);

	pr_debug("rtl838x_get: %d\n", offset);

	/* Internal GPIO of the RTL8380 */
	if (offset < 32) {
		v = rtl838x_r32(RTL838X_GPIO_PABC_DATA);
		if (v & (1 << offset))
			return 1;
		return 0;
	}

	/* LED driver for PWR and SYS */
	if (offset >=32 && offset <64) {
		v = rtl838x_r32(RTL838X_LED_GLB_CTRL);
		if (v & (1 << (offset-32)))
			return 1;
		return 0;
	}

	/* Indirect access GPIO with RTL8231 */
	if (offset >= 64 && offset < 100 && gpios->bus_id >= 0) {
		rtl8231_pin_get(gpios->bus_id, offset - 64, &state);
		if (state & (1 << (offset % 16)))
			return 1;
		return 0;
	}

	bit = (offset - 100) % 32;
	if (offset >= 100 && offset < 132) {
		if (rtl838x_r32(RTL838X_LED1_SW_P_EN_CTRL) & (1 << bit))
			return 1;
		return 0;
	}
	if (offset >= 132 && offset < 164) {
		if (rtl838x_r32(RTL838X_LED1_SW_P_EN_CTRL) & (1 << bit))
			return 1;
		return 0;
	}
	if (offset >= 164 && offset < 196) {
		if (rtl838x_r32(RTL838X_LED1_SW_P_EN_CTRL) & (1 << bit))
			return 1;
		return 0;
	}
	return 0;
}

void rtl838x_gpio_set (struct gpio_chip *gc, unsigned offset, int value)
{
	int bit;
	struct rtl838x_gpios *gpios = gpiochip_get_data(gc);
	pr_debug("rtl838x_set: %d, value: %d\n", offset, value);

	/* Internal GPIO of the RTL8380 */
	if (offset < 32) {
		if (value)
			rtl838x_w32_mask(0, 1 << offset, RTL838X_GPIO_PABC_DATA);
		else
			rtl838x_w32_mask(1 << offset, 0, RTL838X_GPIO_PABC_DATA);
	}

	/* LED driver for PWR and SYS */
	if (offset >=32 && offset < 64) {
		bit = offset - 32;
		if (value)
			rtl838x_w32_mask(0, 1 << bit, RTL838X_LED_GLB_CTRL);
		else 
			rtl838x_w32_mask(1 << bit, 0, RTL838X_LED_GLB_CTRL);
		return;
	}

	/* Indirect access GPIO with RTL8231 */
	if (offset >= 64 && offset < 100 && gpios->bus_id >= 0) {
		rtl8231_pin_set(gpios->bus_id, offset - 64, value);
		return;
	}

	bit = (offset - 100) % 32;
/*	printk("%d -> %x: %x %x %x, P_CRTL: %x\n", bit, rtl838x_r32(RTL838X_LED_P_EN_CTRL), rtl838x_r32(RTL838X_LED0_SW_P_EN_CTRL), rtl838x_r32(RTL838X_LED1_SW_P_EN_CTRL), rtl838x_r32(RTL838X_LED2_SW_P_EN_CTRL),
	       rtl838x_r32(RTL838X_LED_SW_P_CTRL(bit))); */
	/* First Port-LED */
	if (offset >= 100 && offset < 132
	   && offset >= (100 + gpios->min_led)
	   && offset < (100 + gpios->min_led + gpios->num_leds)) {
		if (value)
			rtl838x_w32_mask(7, 5, RTL838X_LED_SW_P_CTRL(bit));
		else
			rtl838x_w32_mask(7, 0, RTL838X_LED_SW_P_CTRL(bit));
	}
	if (offset >= 132 && offset < 164
	    && offset >= (132 + gpios->min_led)
	    && offset < (132 + gpios->min_led + gpios->num_leds)) {
		if (value)
			rtl838x_w32_mask(7 << 3, 5 << 3, RTL838X_LED_SW_P_CTRL(bit));
		else
			rtl838x_w32_mask(7 << 3, 0, RTL838X_LED_SW_P_CTRL(bit));
	}
	if (offset >= 164 && offset < 196
	    && offset >= (164 + gpios->min_led)
	    && offset < (164 + gpios->min_led + gpios->num_leds)) {
		if (value)
			rtl838x_w32_mask(7 << 6, 5 << 6, RTL838X_LED_SW_P_CTRL(bit));
		else
			rtl838x_w32_mask(7 << 6, 0, RTL838X_LED_SW_P_CTRL(bit));
	}
	__asm__ volatile ("sync");
}

	
/* Needed for the Zyxel */
int rtl8231_init(struct rtl838x_gpios *gpios)
{
	uint32_t v;
	u8 bus_id = gpios->bus_id;

	printk("rtl8231_init\n");
	
	/* Enable RTL8231 indirect access mode */
	rtl838x_w32_mask(0, 1, RTL838X_EXTRA_GPIO_CTRL);
	rtl838x_w32_mask(3, 1, RTL838X_DMY_REG5);

	/* Enable RTL8231 via GPIO_A1 line */
	rtl838x_w32_mask(0, 1 << RTL838X_GPIO_A1, RTL838X_GPIO_PABC_DIR);
	rtl838x_w32_mask(0, 1 << RTL838X_GPIO_A1, RTL838X_GPIO_PABC_DATA);
	mdelay(50); /* wait 50ms for reset */
	
	/*Select GPIO functionality for pins 0-15 and 16-32*/
	rtl838x_rtl8231_write(bus_id, RTL8231_GPIO_PIN_SEL(0), 0xffff);
	rtl838x_rtl8231_write(bus_id, RTL8231_GPIO_PIN_SEL(16), 0xffff);

	v = rtl838x_rtl8231_read(bus_id, RTL8231_LED_FUNC0);
	printk("RTL8231 led function now: %x\n", v);

	printk("rtl8231_init done\n");
	return 0;
}

void rtl8380_led_test(u32 mask)
{
	int i;
	u32 mode_sel = rtl838x_r32(RTL838X_LED_MODE_SEL);
	u32 led_gbl = rtl838x_r32(RTL838X_LED_GLB_CTRL);
	u32 led_p_en = rtl838x_r32(RTL838X_LED_P_EN_CTRL);

	/* 2 Leds for ports 0-23 and 24-27, 3 would be 0x7 */
	rtl838x_w32_mask(0x3f, 0x3 | (0x3 << 3), RTL838X_LED_GLB_CTRL);
	/* Enable all leds */
	rtl838x_w32(0xFFFFFFF, RTL838X_LED_P_EN_CTRL);

	/* Enable software control of all leds */
	rtl838x_w32(0xFFFFFFF, RTL838X_LED_SW_CTRL);
	rtl838x_w32(0xFFFFFFF, RTL838X_LED0_SW_P_EN_CTRL);
	rtl838x_w32(0xFFFFFFF, RTL838X_LED1_SW_P_EN_CTRL);
	rtl838x_w32(0x0000000, RTL838X_LED2_SW_P_EN_CTRL);

	for (i = 0; i < 28; i++) {
		if (mask & (1 <<i) )
			rtl838x_w32(5 | (5 << 3) | (5 << 6),
			       RTL838X_LED_SW_P_CTRL(i));
	}
	msleep(3000);

	rtl838x_w32(led_p_en, RTL838X_LED_P_EN_CTRL);
	/* Disable software control of all leds */
	rtl838x_w32(0x0000000, RTL838X_LED_SW_CTRL);
	rtl838x_w32(0x0000000, RTL838X_LED0_SW_P_EN_CTRL);
	rtl838x_w32(0x0000000, RTL838X_LED1_SW_P_EN_CTRL);
	rtl838x_w32(0x0000000, RTL838X_LED2_SW_P_EN_CTRL);

	rtl838x_w32(led_gbl, RTL838X_LED_GLB_CTRL);
	rtl838x_w32(mode_sel, RTL838X_LED_MODE_SEL);
}

void take_port_leds(struct rtl838x_gpios *gpios)
{
	int leds_per_port = gpios->leds_per_port;
	int mode = gpios->led_mode;

	pr_info("take_port_leds, %d, %x\n", leds_per_port, mode);
	pr_debug("Bootloader settings: %x %x %x\n",
		rtl838x_r32(RTL838X_LED0_SW_P_EN_CTRL),
		rtl838x_r32(RTL838X_LED1_SW_P_EN_CTRL),
		rtl838x_r32(RTL838X_LED2_SW_P_EN_CTRL)
	);

	pr_debug("led glb: %x, sel %x\n",
	       rtl838x_r32(RTL838X_LED_GLB_CTRL), rtl838x_r32(RTL838X_LED_MODE_SEL));
	pr_debug("RTL838X_LED_P_EN_CTRL: %x", rtl838x_r32(RTL838X_LED_P_EN_CTRL));
	pr_debug("RTL838X_LED_MODE_CTRL: %x", rtl838x_r32(RTL838X_LED_MODE_CTRL));

	rtl838x_w32_mask(3, 0, RTL838X_LED_MODE_SEL);
	rtl838x_w32(mode, RTL838X_LED_MODE_CTRL);

	/* Enable software control of all leds */
	rtl838x_w32(0xFFFFFFF, RTL838X_LED_SW_CTRL);
	rtl838x_w32(0xFFFFFFF, RTL838X_LED_P_EN_CTRL);

	rtl838x_w32(0x0000000, RTL838X_LED0_SW_P_EN_CTRL);
	rtl838x_w32(0x0000000, RTL838X_LED1_SW_P_EN_CTRL);
	rtl838x_w32(0x0000000, RTL838X_LED2_SW_P_EN_CTRL);

	rtl838x_w32_mask(0x3f, 0, RTL838X_LED_GLB_CTRL);
	switch (leds_per_port) {
	case 3:
		rtl838x_w32_mask(0, 0x7 | (0x7 << 3), RTL838X_LED_GLB_CTRL);
		rtl838x_w32(0xFFFFFFF, RTL838X_LED2_SW_P_EN_CTRL);
		/* FALLTHRU */
	case 2:
		rtl838x_w32_mask(0, 0x3 | (0x3 << 3), RTL838X_LED_GLB_CTRL);
		rtl838x_w32(0xFFFFFFF, RTL838X_LED1_SW_P_EN_CTRL);
		/* FALLTHRU */
	case 1:
		rtl838x_w32_mask(0, 0x1 | (0x1 << 3), RTL838X_LED_GLB_CTRL);
		rtl838x_w32(0xFFFFFFF, RTL838X_LED0_SW_P_EN_CTRL);
		break;
	default:
		pr_err("No LEDS configured for software control\n");
	}
}

static const struct of_device_id rtl838x_gpio_of_match[] = {
	{ .compatible = "realtek,rtl838x-gpio" },
	{},
};

MODULE_DEVICE_TABLE(of, rtl838x_gpio_of_match);

static int rtl838x_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct rtl838x_gpios *gpios;
	int err;
	u8 indirect_bus_id;
	
	printk("Probing RTL838X GPIOs\n");
	if (!np) {
		dev_err(&pdev->dev, "No DT found\n");
		return -EINVAL;
	}

	gpios = devm_kzalloc(dev, sizeof(*gpios), GFP_KERNEL);
	if (!gpios)
		return -ENOMEM;

	gpios->id = rtl838x_r32(RTL838X_MODEL_NAME_INFO) >> 16;

	switch (gpios->id) {
	case 0x8332:
		pr_debug("Found RTL8332M GPIO\n");
		break;
	case 0x8380:
		pr_debug("Found RTL8380M GPIO\n");
		break;
	case 0x8382:
		pr_debug("Found RTL8382M GPIO\n");
		break;
	default:
		pr_err("Unknown GPIO chip id (%04x)\n", gpios->id);
		return -ENODEV;
	}

	gpios->dev = dev;
	gpios->gc.base = 0;
	/* 0-31: internal
	 * 32-63, LED control register
	 * 64-99: external RTL8231
	 * 100-131: PORT-LED 0
	 * 132-163: PORT-LED 1
	 * 164-195: PORT-LED 2
	 */
	gpios->gc.ngpio = 196;
	gpios->gc.label = "rtl838x";
	gpios->gc.parent = dev;
	gpios->gc.owner = THIS_MODULE;
	gpios->gc.can_sleep = true;
	gpios->bus_id = -1;
	gpios->irq = 31;

	gpios->gc.direction_input = rtl838x_direction_input;
	gpios->gc.direction_output = rtl838x_direction_output;
	gpios->gc.set = rtl838x_gpio_set;
	gpios->gc.get = rtl838x_gpio_get;
	gpios->gc.get_direction = rtl838x_get_direction;
	
	if(!of_property_read_u8(np, "indirect-access-bus-id", &indirect_bus_id)) {
		gpios->bus_id = indirect_bus_id;
		rtl8231_init(gpios);
	}
	if (of_property_read_bool(np, "take-port-leds")) {
		if(of_property_read_u32(np, "leds-per-port", &gpios->leds_per_port))
			gpios->leds_per_port = 2;
		if(of_property_read_u32(np, "led-mode", &gpios->led_mode))
			gpios->led_mode = (0x1ea << 15) | 0x1ea ;
		if(of_property_read_u32(np, "num-leds", &gpios->num_leds))
			gpios->num_leds = 32;
		if(of_property_read_u32(np, "min-led", &gpios->min_led))
			gpios->min_led = 0;
		take_port_leds(gpios);
	}

	err = devm_gpiochip_add_data(dev, &gpios->gc, gpios);
	return err;
}


static struct platform_driver rtl838x_gpio_driver = {
	.driver = {
		.name = "rtl838x-gpio",
		.of_match_table	= rtl838x_gpio_of_match,
	},
	.probe = rtl838x_gpio_probe,
};

module_platform_driver(rtl838x_gpio_driver);

MODULE_DESCRIPTION("Realtek RTL838X GPIO API support");
MODULE_LICENSE("GPL v2");
