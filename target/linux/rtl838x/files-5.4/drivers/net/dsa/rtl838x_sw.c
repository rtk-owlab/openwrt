// SPDX-License-Identifier: GPL-2.0-only

#include <linux/etherdevice.h>
#include <linux/if_bridge.h>
#include <linux/iopoll.h>
#include <linux/mdio.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/of_platform.h>
#include <linux/phylink.h>
#include <linux/phy_fixed.h>
#include <net/dsa.h>

#include <asm/mach-rtl838x/mach-rtl838x.h>
#include "rtl838x.h"

#define RTL8380_VERSION_A 'A'
#define RTL8380_VERSION_B 'B'

DEFINE_MUTEX(smi_lock);

extern void rtl8380_sds_rst(int mac);

struct rtl838x_vlan_info {
	u32 untagged_ports;
	u32 tagged_ports;
	u32 vlan_conf;
};

#define MIB_DESC(_size, _offset, _name) {.size = _size, .offset = _offset, .name = _name}
struct rtl838x_mib_desc {
	unsigned int size;
	unsigned int offset;
	const char *name;
};

static const struct rtl838x_mib_desc rtl838x_mib[] = {
	MIB_DESC(2, 0xf8, "ifInOctets"),
	MIB_DESC(2, 0xf0, "ifOutOctets"),
	MIB_DESC(1, 0xec, "dot1dTpPortInDiscards"),
	MIB_DESC(1, 0xe8, "ifInUcastPkts"),
	MIB_DESC(1, 0xe4, "ifInMulticastPkts"),
	MIB_DESC(1, 0xe0, "ifInBroadcastPkts"),
	MIB_DESC(1, 0xdc, "ifOutUcastPkts"),
	MIB_DESC(1, 0xd8, "ifOutMulticastPkts"),
	MIB_DESC(1, 0xd4, "ifOutBroadcastPkts"),
	MIB_DESC(1, 0xd0, "ifOutDiscards"),
	MIB_DESC(1, 0xcc, ".3SingleCollisionFrames"),
	MIB_DESC(1, 0xc8, ".3MultipleCollisionFrames"),
	MIB_DESC(1, 0xc4, ".3DeferredTransmissions"),
	MIB_DESC(1, 0xc0, ".3LateCollisions"),
	MIB_DESC(1, 0xbc, ".3ExcessiveCollisions"),
	MIB_DESC(1, 0xb8, ".3SymbolErrors"),
	MIB_DESC(1, 0xb4, ".3ControlInUnknownOpcodes"),
	MIB_DESC(1, 0xb0, ".3InPauseFrames"),
	MIB_DESC(1, 0xac, ".3OutPauseFrames"),
	MIB_DESC(1, 0xa8, "DropEvents"),
	MIB_DESC(1, 0xa4, "tx_BroadcastPkts"),
	MIB_DESC(1, 0xa0, "tx_MulticastPkts"),
	MIB_DESC(1, 0x9c, "CRCAlignErrors"),
	MIB_DESC(1, 0x98, "tx_UndersizePkts"),
	MIB_DESC(1, 0x94, "rx_UndersizePkts"),
	MIB_DESC(1, 0x90, "rx_UndersizedropPkts"),
	MIB_DESC(1, 0x8c, "tx_OversizePkts"),
	MIB_DESC(1, 0x88, "rx_OversizePkts"),
	MIB_DESC(1, 0x84, "Fragments"),
	MIB_DESC(1, 0x80, "Jabbers"),
	MIB_DESC(1, 0x7c, "Collisions"),
	MIB_DESC(1, 0x78, "tx_Pkts64Octets"),
	MIB_DESC(1, 0x74, "rx_Pkts64Octets"),
	MIB_DESC(1, 0x70, "tx_Pkts65to127Octets"),
	MIB_DESC(1, 0x6c, "rx_Pkts65to127Octets"),
	MIB_DESC(1, 0x68, "tx_Pkts128to255Octets"),
	MIB_DESC(1, 0x64, "rx_Pkts128to255Octets"),
	MIB_DESC(1, 0x60, "tx_Pkts256to511Octets"),
	MIB_DESC(1, 0x5c, "rx_Pkts256to511Octets"),
	MIB_DESC(1, 0x58, "tx_Pkts512to1023Octets"),
	MIB_DESC(1, 0x54, "rx_Pkts512to1023Octets"),
	MIB_DESC(1, 0x50, "tx_Pkts1024to1518Octets"),
	MIB_DESC(1, 0x4c, "rx_StatsPkts1024to1518Octets"),
	MIB_DESC(1, 0x48, "tx_Pkts1519tomaxOctets"),
	MIB_DESC(1, 0x44, "rx_Pkts1519tomaxOctets"),
	MIB_DESC(1, 0x40, "rxMacDiscards")
};

static irqreturn_t rtl838x_switch_irq(int irq, void *dev_id)
{
	struct dsa_switch *ds = dev_id;
//	struct rtl838x_switch_priv *priv = ds->priv;
	u32 status = sw_r32(RTL838X_ISR_GLB_SRC);
	u32 ports = sw_r32(RTL838X_ISR_PORT_LINK_STS_CHG);
	u32 link;
	int i;

	/* Clear status */
	sw_w32(ports, RTL838X_ISR_PORT_LINK_STS_CHG);
	pr_info("Link change: status: %x, ports %x\n", status, ports);

	for (i = 0; i < 28; i++) {
		if (ports & (1 << i)) {
			link = sw_r32(RTL838X_MAC_LINK_STS);
			if (link & (1 << i))
				dsa_port_phylink_mac_change(ds, i, true);
			else
				dsa_port_phylink_mac_change(ds, i, false);
		}
	}
	return IRQ_HANDLED;
}

int rtl8380_sds_power(int mac, int val)
{
	u32 mode = (val == 1)? 0x4: 0x9;
	u32 offset = (mac == 24)? 5: 0;

	if ( (mac != 24) && (mac != 26) ) {
		printk("rtl8380_sds_power: not a fibre port: %d\n", mac);
		return -1;
	}

	sw_w32_mask(0x1f << offset, mode << offset, RTL838X_SDS_MODE_SEL);

	rtl8380_sds_rst(mac);

	return 0;
}

static int rtl838x_smi_wait_op(int timeout)
{
	do {
		timeout--;
		udelay(10);
	} while ((sw_r32(RTL838X_SMI_ACCESS_PHY_CTRL_1) & 0x1) && (timeout >= 0));
	if (timeout <= 0) {
		return -1;
	}
	return 0;
}


/* 
 * Write to a register in a page of the PHY
 */
int rtl838x_write_phy(u32 port, u32 page, u32 reg, u32 val)
{
	u32 v;
	u32 park_page;
	val &= 0xffff;

	if (port > 31 || page > 4095 || reg > 31)
		return -ENOTSUPP;

	mutex_lock(&smi_lock);
	if(rtl838x_smi_wait_op(10000))
		goto timeout;

	sw_w32(1 << port, RTL838X_SMI_ACCESS_PHY_CTRL_0);
	mdelay(10);

	sw_w32_mask(0xffff0000, val << 16, RTL838X_SMI_ACCESS_PHY_CTRL_2);

	park_page = sw_r32(RTL838X_SMI_ACCESS_PHY_CTRL_1) & ((0x1f << 15) | 0x2);
	v = reg << 20 | page << 3 | 0x4;
	sw_w32(v | park_page, RTL838X_SMI_ACCESS_PHY_CTRL_1);
	sw_w32_mask(0, 1, RTL838X_SMI_ACCESS_PHY_CTRL_1);

	if(rtl838x_smi_wait_op(10000))
		goto timeout;

	mutex_unlock(&smi_lock);
	return 0;

timeout:
	mutex_unlock(&smi_lock);
	return -ETIMEDOUT;
}

/* 
 * Reads a register in a page from the PHY
 */
int rtl838x_read_phy(u32 port, u32 page, u32 reg, u32 *val)
{
	u32 v;
	u32 park_page;
	
	if (port > 31 || page > 4095 || reg > 31)
		return -ENOTSUPP;

	mutex_lock(&smi_lock);

	if(rtl838x_smi_wait_op(10000))
		goto timeout;
		

	sw_w32_mask(0xffff0000, port << 16, RTL838X_SMI_ACCESS_PHY_CTRL_2);
	
	park_page = sw_r32(RTL838X_SMI_ACCESS_PHY_CTRL_1) & ((0x1f << 15) | 0x2);
	v = reg << 20 | page << 3;
	sw_w32(v | park_page, RTL838X_SMI_ACCESS_PHY_CTRL_1);
	sw_w32_mask(0, 1, RTL838X_SMI_ACCESS_PHY_CTRL_1);

	if(rtl838x_smi_wait_op(10000))
		goto timeout;

	*val = sw_r32(RTL838X_SMI_ACCESS_PHY_CTRL_2) & 0xffff;
//	printk("PHY-read: port %d reg: %x res: %x\n", port, reg, *val);

	mutex_unlock(&smi_lock);
	return 0;

timeout:
	mutex_unlock(&smi_lock);
	return -ETIMEDOUT;
}

/*
 * Write to an mmd register of the PHY
 */
int rtl838x_write_mmd_phy(u32 port, u32 addr, u32 reg, u32 val)
{
	u32 v;
	val &= 0xffff;

	printk("MMD write: port %d, dev %d, reg %d, val %x\n", port, addr, reg, val);
	mutex_lock(&smi_lock);

	if(rtl838x_smi_wait_op(10000))
		goto timeout;

	sw_w32(1 << port, RTL838X_SMI_ACCESS_PHY_CTRL_0);
	mdelay(10);

	sw_w32_mask(0xffff0000, val << 16, RTL838X_SMI_ACCESS_PHY_CTRL_2);

	sw_w32_mask(0x1f << 16, addr << 16, RTL838X_SMI_ACCESS_PHY_CTRL_3);
	sw_w32_mask(0xffff, reg, RTL838X_SMI_ACCESS_PHY_CTRL_3);
	/* mmd-access | write | cmd-start */
	v = 1 << 1| 1 << 2 | 1;
	sw_w32(v, RTL838X_SMI_ACCESS_PHY_CTRL_1);

	if(rtl838x_smi_wait_op(10000))
		goto timeout;

	mutex_unlock(&smi_lock);
	return 0;

timeout:
	mutex_unlock(&smi_lock);
	return -ETIMEDOUT;
}

/*
 * Read to an mmd register of the PHY
 */
int rtl838x_read_mmd_phy(u32 port, u32 addr, u32 reg, u32 *val)
{
	u32 v;

	mutex_lock(&smi_lock);

	if(rtl838x_smi_wait_op(10000))
		goto timeout;

	sw_w32(1 << port, RTL838X_SMI_ACCESS_PHY_CTRL_0);
	mdelay(10);

	sw_w32_mask(0xffff0000, port << 16, RTL838X_SMI_ACCESS_PHY_CTRL_2);

	v = addr << 16 | reg;
	sw_w32(v, RTL838X_SMI_ACCESS_PHY_CTRL_3);

	/* mmd-access | read | cmd-start */
	v = 1 << 1| 0 << 2 | 1;
	sw_w32(v, RTL838X_SMI_ACCESS_PHY_CTRL_1);

	if(rtl838x_smi_wait_op(10000))
		goto timeout;

	*val = sw_r32(RTL838X_SMI_ACCESS_PHY_CTRL_2) & 0xffff;
/*	printk("PHY-read mmd: port %d, addr %d, reg: %x res: %x\n",
	port, addr, reg, *val); */

	mutex_unlock(&smi_lock);
	return 0;

timeout:
	mutex_unlock(&smi_lock);
	return -ETIMEDOUT;
}

static void rtl8380_get_version(struct rtl838x_switch_priv *priv)
{
	u32 rw_save, info_save;
	u32 info;

	priv->id = sw_r32(RTL838X_MODEL_NAME_INFO) >> 16;
	
	switch (priv->id) {
	case 0x8380:
	printk("Found RTL8380M\n");
		break;
	case 0x8382:
		printk("Found RTL8382M\n");
		break;
	default:
		pr_err("Unknown chip id (%04x)\n", priv->id);
		priv->id = 0;
	}

	rw_save = sw_r32(RTL838X_INT_RW_CTRL);
	sw_w32(rw_save | 0x3, RTL838X_INT_RW_CTRL);

	info_save = sw_r32(RTL838X_CHIP_INFO);
	sw_w32(info_save | 0xA0000000, RTL838X_CHIP_INFO);

	info = sw_r32(RTL838X_CHIP_INFO);
	sw_w32(info_save, RTL838X_CHIP_INFO);
	sw_w32(rw_save, RTL838X_INT_RW_CTRL);

	if ((info & 0xFFFF) == 0x6275) {
		if(((info >> 16) & 0x1F) == 0x1) {
			priv->version = RTL8380_VERSION_A; 
		} else if(((info >> 16) & 0x1F) == 0x2) {
			priv->version = RTL8380_VERSION_B;
		} else {
			priv->version = RTL8380_VERSION_B;
		}
	} else {
		priv->version = '-';
	}
}


int dsa_phy_read(struct dsa_switch *ds, int phy_addr, int phy_reg)
{
	u32 val;
	u32 offset = 0;
	struct rtl838x_switch_priv *priv = ds->priv;

	if (phy_addr >= 24 && phy_addr <= 27 
	     && priv->ports[24].phy == PHY_RTL838X_SDS) {
		if (phy_addr == 26)
			offset = 0x100;
		val = sw_r32(MAPLE_SDS4_FIB_REG0r + offset + (phy_reg << 2)) & 0xffff;
		printk("PHY-read from SDS: port %d reg: %x res: %x\n", phy_addr, phy_reg, val);
		return val;
	}

	rtl838x_read_phy(phy_addr, 0, phy_reg, &val);
//	printk("PHY-read: port %d reg: %x res: %x\n", phy_addr, phy_reg, val);
	return val;
}

int dsa_phy_write(struct dsa_switch *ds, int phy_addr, int phy_reg, u16 val)
{
	u32 offset = 0;
	struct rtl838x_switch_priv *priv = ds->priv;
	
	if (phy_addr >= 24 && phy_addr <= 27 
	     && priv->ports[24].phy == PHY_RTL838X_SDS) {
//		printk("PHY_write to SDS, port %d\n", phy_addr);
		if (phy_addr == 26)
			offset = 0x100;
		sw_w32(val, MAPLE_SDS4_FIB_REG0r + offset + (phy_reg << 2));
		return 0;
	}
	return rtl838x_write_phy(phy_addr, 0, phy_reg, val);
}

static int rtl838x_mdio_read(struct mii_bus *bus, int addr, int regnum)
{
	int ret;
	
	struct rtl838x_switch_priv *priv = bus->priv;
//	printk("switch rtl838x_mdio_read %d %d\n", addr, regnum);
	ret = dsa_phy_read(priv->ds, addr, regnum);
//	printk("switch rtl838x_mdio_read %d\n", ret);
	return ret;
}

static int rtl838x_mdio_write(struct mii_bus *bus, int addr, int regnum,
				 u16 val)
{
	struct rtl838x_switch_priv *priv = bus->priv;
	return dsa_phy_write(priv->ds, addr, regnum, val);
}


static int rtl8380_enable_phy_polling(struct rtl838x_switch_priv *priv)
{
	int i;
	u32 v = 0;

	msleep(1000);
	/* Enable all ports with a PHY, including the SFP-ports */
	for (i = 0; i < 28; i++) {
		if (priv->ports[i].phy) v |= 1 << i;
	}
	printk("rtl8300_enable_phy_polling: %8x\n", v);
	
	sw_w32(v, RTL838X_SMI_POLL_CTRL);
	/* PHY update complete */
	sw_w32_mask(0, 0x8000, RTL838X_SMI_GLB_CTRL);
	return 0;
}

void print_matrix(void)
{
	unsigned volatile int *ptr = RTL838X_SW_BASE + 0x4100;
	printk("> %8x %8x %8x %8x %8x %8x %8x %8x\n", 
	       ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7]);
	printk("> %8x %8x %8x %8x %8x %8x %8x %8x\n", 
	       ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14], ptr[15]);
	printk("> %8x %8x %8x %8x %8x %8x %8x %8x\n", 
	       ptr[16], ptr[17], ptr[18], ptr[19], ptr[20], ptr[21], ptr[22], ptr[23]);
	printk("> %8x %8x %8x %8x %8x\n", ptr[24], ptr[25], ptr[26], ptr[27], ptr[28]);
}

static int rtl838x_setup(struct dsa_switch *ds)
{
	int i;
	u32 port_bitmap = 0;
	struct rtl838x_switch_priv *priv = ds->priv;
	
	/* Disable MAC polling the PHY so that we can start configuration */
	sw_w32(0x00000000, RTL838X_SMI_POLL_CTRL);

	for (i = 0; i < ds->num_ports; i++)
		priv->ports[i].enable = false;
	priv->ports[CPU_PORT].enable = true;
	
	/* Isolate ports from each other: traffic only CPU <-> port */
	for (i = 0; i < CPU_PORT; i++) {
		if (priv->ports[i].phy) {
			sw_w32(1 << CPU_PORT, RTL838X_PORT_ISO_CTRL(i));
			sw_w32_mask(0, 1 << i, RTL838X_PORT_ISO_CTRL(i));
			port_bitmap |= 1 << i;
		}
	}
//	printk("Port-isolation bitmap: %8x\n", port_bitmap);
	sw_w32(port_bitmap, RTL838X_PORT_ISO_CTRL(CPU_PORT));
	
	print_matrix();
	
	/* Enable statistics module */
	sw_w32_mask(0, 3, RTL838X_STAT_CTRL);
	/* Reset statistics counters */
	sw_w32_mask(0, 1, RTL838X_STAT_RST);
	
	/* Enable MAC Polling PHY again */
	rtl8380_enable_phy_polling(priv);
	printk("Please wait until PHY is settled\n");
	msleep(1000);

	return 0;
}

static void rtl838x_get_strings(struct dsa_switch *ds, 
				int port, u32 stringset, u8 *data)
{
	int i;

	if (stringset != ETH_SS_STATS)
		return;

	for (i = 0; i < ARRAY_SIZE(rtl838x_mib); i++)
		strncpy(data + i * ETH_GSTRING_LEN, rtl838x_mib[i].name,
			ETH_GSTRING_LEN);
}

static void rtl838x_get_ethtool_stats(struct dsa_switch *ds, int port,
				      uint64_t *data)
{
	const struct rtl838x_mib_desc *mib;
	int i;
	u64 high;

	for (i = 0; i < ARRAY_SIZE(rtl838x_mib); i++) {
		mib = &rtl838x_mib[i];

		data[i] = sw_r32(RTL838X_STAT_PORT_STD_MIB(port) + 252 - mib->offset);
		if (mib->size == 2) {
			high = sw_r32(RTL838X_STAT_PORT_STD_MIB(port) + 252 - mib->offset - 4);
			data[i] |= high << 32;
		}
	}
}

static int rtl838x_get_sset_count(struct dsa_switch *ds, int port, int sset)
{
	if (sset != ETH_SS_STATS)
		return 0;

	return ARRAY_SIZE(rtl838x_mib);
}

static enum dsa_tag_protocol 
rtl838x_get_tag_protocol(struct dsa_switch *ds, int port)
{
	/* The switch does not tag the frames, instead internally the header
	 * structure for each packet is tagged accordingly.
	 */
	return DSA_TAG_PROTO_TRAILER;
}

static int rtl838x_get_l2aging(void)
{
	int t = sw_r32(RTL838X_L2_CTRL_1) & 0x7fffff;
	u32 val;

	t = t * 128 / 625; /* Aging time in seconds. 0: L2 aging disabled */
	pr_info("L2 AGING time: %d sec\n", t);
	pr_info("Dynamic aging for ports: %x\n", sw_r32(RTL838X_L2_PORT_AGING_OUT));
	return t;
}

/*
 * Set Switch L2 Aging time, t is time in seconds
 * t = 0: aging is disabled
 */
static void rtl838x_set_l2aging(int t)
{
	/* Convert time in seconds (max 0x2000000) to internal time value */
	if (t > 0x2000000)
		t = 0x7fffff;
	else
		t = (t * 625 + 127) / 128;

	sw_w32(t, RTL838X_L2_CTRL_1);
}

static void rtl838x_fast_age(struct dsa_switch *ds, int port)
{
	rtl838x_set_l2aging(180);
	sw_w32_mask(0, 1 << port, RTL838X_L2_PORT_AGING_OUT);
}

static void rtl838x_port_stp_state_get(u32 msti, u32 *state)
{
	u32 cmd = 1 << 15 /* Execute cmd */
		| 1 << 14 /* Read */
		| 2 << 12 /* Table type 0b10 */
		| (msti & 0xfff);
	sw_w32(cmd, RTL838X_TBL_ACCESS_CTRL_0);
	do { }  while (sw_r32(RTL838X_TBL_ACCESS_CTRL_0) & (1 << 15));
	state[0] = sw_r32(RTL838X_TBL_ACCESS_DATA_0(0));
	state[1] = sw_r32(RTL838X_TBL_ACCESS_DATA_0(1));
/*	printk("STP port state %x %x\n", state[0], state[1]);*/
}

static void rtl838x_port_stp_state_set(struct dsa_switch *ds, int port,
				       u8 state)
{
	u32 cmd, msti = 0;
	u32 port_state[2];
	int index, bit;
	struct rtl838x_switch_priv *priv = ds->priv;
/*	printk("rtl838x_port_stp_state_set, port %d state %2x\n", port, state); */
	mutex_lock(&priv->reg_mutex);

	index = port >= 16? 0: 1;
	bit = (port << 1) % 32;

	rtl838x_port_stp_state_get(msti, &port_state[0]);

	port_state[index] &= ~(3 << bit);

	switch (state) {
	case BR_STATE_DISABLED:
		port_state[index] |= (0 << bit);
		break;
	case BR_STATE_BLOCKING:
		port_state[index] |= (1 << bit);
		break;
	case BR_STATE_LISTENING:
		break;
	case BR_STATE_LEARNING:
		port_state[index] |= (2 << bit);
		break;
	case BR_STATE_FORWARDING:
		port_state[index] |= (3 << bit);
	default:
		break;
	}

	cmd = 1 << 15 /* Execute cmd */
		| 0 << 14 /* Write */
		| 2 << 12 /* Table type 0b10 */
		| (msti & 0xfff);
	sw_w32(port_state[0], RTL838X_TBL_ACCESS_DATA_0(0));
	sw_w32(port_state[1], RTL838X_TBL_ACCESS_DATA_0(1));
	sw_w32(cmd, RTL838X_TBL_ACCESS_CTRL_0);
	do { }  while (sw_r32(RTL838X_TBL_ACCESS_CTRL_0) & (1 << 15));

	mutex_unlock(&priv->reg_mutex);
}

static void rtl838x_vlan_tables_read(u32 vlan, struct rtl838x_vlan_info *info)
{
	u32 cmd;

	cmd = 1 << 15 /* Execute cmd */
		| 1 << 14 /* Read */
		| 0 << 12 /* Table type 0b00 */
		| (vlan & 0xfff);
	sw_w32(cmd, RTL838X_TBL_ACCESS_CTRL_0);
	do { }  while (sw_r32(RTL838X_TBL_ACCESS_CTRL_0) & (1 << 15));
	info->tagged_ports = sw_r32(RTL838X_TBL_ACCESS_DATA_0(0));
	info->vlan_conf = sw_r32(RTL838X_TBL_ACCESS_DATA_0(1));
	
	cmd = 1 << 15 /* Execute cmd */
		| 1 << 14 /* Read */
		| 0 << 12 /* Table type 0b00 */
		| (vlan & 0xfff);
	sw_w32(cmd, RTL838X_TBL_ACCESS_CTRL_1);
	do { } while (sw_r32(RTL838X_TBL_ACCESS_CTRL_1) & (1 << 15));
	info->untagged_ports = sw_r32(RTL838X_TBL_ACCESS_DATA_0(0));
}

static void rtl838x_vlan_set_tagged(u32 vlan, u32 portmask, u32 conf)
{
	u32 cmd = 1 << 15 /* Execute cmd */
		| 0 << 14 /* Write */
		| 0 << 12 /* Table type 0b00 */
		| (vlan & 0xfff);
	sw_w32(portmask, RTL838X_TBL_ACCESS_DATA_0(0));
	sw_w32(conf, RTL838X_TBL_ACCESS_DATA_0(1));
	sw_w32(cmd, RTL838X_TBL_ACCESS_CTRL_0);
	do { }  while (sw_r32(RTL838X_TBL_ACCESS_CTRL_0) & (1 << 15));
}

static void rtl838x_vlan_set_untagged(u32 vlan, u32 portmask)
{
	u32 cmd = 1 << 15 /* Execute cmd */
		| 0 << 14 /* Write */
		| 0 << 12 /* Table type 0b00 */
		| (vlan & 0xfff);
	sw_w32(portmask & 0x1fffffff, RTL838X_TBL_ACCESS_DATA_0(0));
	sw_w32(cmd, RTL838X_TBL_ACCESS_CTRL_1);
	do { } while (sw_r32(RTL838X_TBL_ACCESS_CTRL_1) & (1 << 15));
}

void rtl838x_vlan_profile_dump(int index)
{
	u32 profile;

	if (index <0 || index > 7)
		return;

	profile = sw_r32(RTL838X_VLAN_PROFILE(index));
	
	printk("VLAN %d: L2 learning: %d, L2 Unknown MultiCast Field %x, \
		IPv4 Unknown MultiCast Field %x, IPv6 Unknown MultiCast Field: %x",
		index, profile & 1, (profile >> 1) & 0x1ff, (profile >> 10) & 0x1ff,
		(profile >> 19) & 0x1ff);
}

static int rtl838x_vlan_filtering(struct dsa_switch *ds, int port,
				  bool vlan_filtering)
{
	struct rtl838x_switch_priv *priv = ds->priv;
	printk("rtl838x_port_vlan_filtering port %d\n", port);
	mutex_lock(&priv->reg_mutex);

	if (vlan_filtering) {
		/* Enable ingress and egress filtering */
		if (port != CPU_PORT) {
			if (port < 16) {
				sw_w32_mask(0b10 << (port << 1),
					    0b01 << (port << 1),
					    RTL838X_VLAN_PORT_IGR_FLTR_0);
			} else {
				sw_w32_mask(0b10 << ( (port - 16) << 1),
					    0b01 << ( (port - 16) << 1),
					    RTL838X_VLAN_PORT_IGR_FLTR_1);
			}
		}
		sw_w32_mask(0, 1 << port, RTL838X_VLAN_PORT_EGR_FLTR);
	} else { 
		/* Disable ingress and egress filtering */
		if (port != CPU_PORT) {
			if (port < 16) {
				sw_w32_mask(0b11 << (port << 1),
					    0,
					    RTL838X_VLAN_PORT_IGR_FLTR_0);
			} else {
				sw_w32_mask(0b11 << ( (port - 16) << 1),
					    0,
					    RTL838X_VLAN_PORT_IGR_FLTR_1);
			}
		}
		sw_w32_mask(1 << port, 0, RTL838X_VLAN_PORT_EGR_FLTR);
	}

	/* We need to do something to the CPU-Port, too */
	mutex_unlock(&priv->reg_mutex);

	return 0;
}
	
static int rtl838x_vlan_prepare(struct dsa_switch *ds, int port,
				const struct switchdev_obj_port_vlan *vlan)
{
	struct rtl838x_vlan_info info;
	struct rtl838x_switch_priv *priv = ds->priv;
	mutex_lock(&priv->reg_mutex);

	printk("rtl838x_vlan_prepare, port %d\n", port);
//	rtl838x_vlan_profile_dump(0);

	rtl838x_vlan_tables_read(1, &info);

	printk("Tagged ports %x, untag %x, prof %x, MC# %d, UC# %d, MSTI %x\n",
		info.tagged_ports, info.untagged_ports, info.vlan_conf & 7,
	       (info.vlan_conf & 8) >> 3, (info.vlan_conf & 16) >> 4,
	       (info.vlan_conf & 0x3e0) >> 5 );

	mutex_unlock(&priv->reg_mutex);
	return 0;
}

static void rtl838x_vlan_add(struct dsa_switch *ds, int port,
			    const struct switchdev_obj_port_vlan *vlan)
{
	struct rtl838x_vlan_info info;
	struct rtl838x_switch_priv *priv = ds->priv;
	int v;
	u32 portmask;

	printk("rtl838x_vlan_add, port %d, vid_end %d, vid_end %d, flags %x\n",
	       port, vlan->vid_begin, vlan->vid_end, vlan->flags);

	if (vlan->vid_begin > 4095 || vlan->vid_end > 4095) {
		dev_err(priv->dev, "VLAN out of range: %d - %d",
			vlan->vid_begin, vlan->vid_end);
		return;
	}

	mutex_lock(&priv->reg_mutex);

	if (vlan->flags & BRIDGE_VLAN_INFO_PVID) {
		for (v = vlan->vid_begin; v <= vlan->vid_end; v++) {
			/* Set both inner and outer PVID of the port */
			sw_w32((v << 16) | v, RTL838X_VLAN_PORT_PB_VLAN(port));
		}
	}

	if (vlan->flags & BRIDGE_VLAN_INFO_UNTAGGED) {
		for (v = vlan->vid_begin; v <= vlan->vid_end; v++) {
			/* Get untagged port memberships of this vlan */
			rtl838x_vlan_tables_read(v, &info);
			portmask = info.untagged_ports | (1 << port);
			printk("Untagged ports, VLAN %d: %x\n", v, portmask);
			rtl838x_vlan_set_untagged(v, portmask);
		}
	} else {
		for (v = vlan->vid_begin; v <= vlan->vid_end; v++) {
			/* Get tagged port memberships of this vlan */
			rtl838x_vlan_tables_read(v, &info);
			portmask = info.tagged_ports | (1 << port);
			printk("Tagged ports, VLAN %d: %x\n", v, portmask);
			rtl838x_vlan_set_tagged(v, portmask, info.vlan_conf);
		}
	}
	mutex_unlock(&priv->reg_mutex);
}

static int rtl838x_vlan_del(struct dsa_switch *ds, int port,
			    const struct switchdev_obj_port_vlan *vlan)
{
	struct rtl838x_vlan_info info;
	struct rtl838x_switch_priv *priv = ds->priv;
	int v;
	u32 portmask;

	printk("rtl838x_vlan_del, port %d, vid_end %d, vid_end %d, flags %x\n",
	       port, vlan->vid_begin, vlan->vid_end, vlan->flags);

	if (vlan->vid_begin > 4095 || vlan->vid_end > 4095) {
		dev_err(priv->dev, "VLAN out of range: %d - %d",
			vlan->vid_begin, vlan->vid_end);
		return -ENOTSUPP;
	}

	mutex_lock(&priv->reg_mutex);

	for (v = vlan->vid_begin; v <= vlan->vid_end; v++) {
		/* Reset both inner and out PVID of the port */
		sw_w32(0, RTL838X_VLAN_PORT_PB_VLAN(port));
	
		if (vlan->flags & BRIDGE_VLAN_INFO_UNTAGGED) {
			/* Get untagged port memberships of this vlan */
			rtl838x_vlan_tables_read(v, &info);
			portmask = info.untagged_ports & (~(1 << port));
			printk("Untagged ports, VLAN %d: %x\n", v, portmask);
			rtl838x_vlan_set_untagged(v, portmask);
		}
	
		/* Get tagged port memberships of this vlan */
		rtl838x_vlan_tables_read(v, &info);
		portmask = info.tagged_ports & (~(1 << port));
		printk("Tagged ports, VLAN %d: %x\n", v, portmask);
		rtl838x_vlan_set_tagged(v, portmask, info.vlan_conf);
	}
	mutex_unlock(&priv->reg_mutex);
	
	return 0;
}

static int rtl838x_port_fdb_add(struct dsa_switch *ds, int port,
				const unsigned char *addr, u16 vid)
{
	return -EOPNOTSUPP;
}

static void rtl838x_port_bridge_leave (struct dsa_switch *ds, int port,
				      struct net_device *bridge)
{
	struct rtl838x_switch_priv *priv = ds->priv;
	u32 port_bitmap = 1 << CPU_PORT;
	int i;

	printk("port_bridge_leave %x: %d", (u32)priv, port);
	mutex_lock(&priv->reg_mutex);
	for (i = 0; i < ds->num_ports; i++) {
		/* Remove this port from the port matrix of the other ports
		 * in the same bridge. If the port is disabled, port matrix
		 * is kept and not being setup until the port becomes enabled.
		 * And the other port's port matrix cannot be broken when the
		 * other port is still a VLAN-aware port. */
		if (dsa_is_user_port(ds, i) && i != port) {
			if (dsa_to_port(ds, i)->bridge_dev != bridge)
				continue;
			if (priv->ports[i].enable) {
				sw_w32_mask(1 << port, 0, RTL838X_PORT_ISO_CTRL(i));
			}
			priv->ports[i].pm |= 1 << port;

			port_bitmap &= ~(1 << i);
		}
	}

	/* Add all other ports to this port matrix. */
	if (priv->ports[port].enable) {
		sw_w32_mask(0, port_bitmap, RTL838X_PORT_ISO_CTRL(port));
	}
	priv->ports[port].pm &= ~port_bitmap;
	mutex_unlock(&priv->reg_mutex);

	print_matrix();
	return;
}

static int rtl838x_port_bridge_join(struct dsa_switch *ds, int port,
				      struct net_device *bridge)
{
	struct rtl838x_switch_priv *priv = ds->priv;
	u32 port_bitmap = 1 << CPU_PORT;
	int i;

	printk("port_bridge_join %x: %d %x", (u32)priv, port, port_bitmap);
	mutex_lock(&priv->reg_mutex);
	for (i = 0; i < ds->num_ports; i++) {
		/* Add this port to the port matrix of the other ports in the
		 * same bridge. If the port is disabled, port matrix is kept
		 * and not being setup until the port becomes enabled.
		 */
		if (dsa_is_user_port(ds, i) && i != port) {
			if (dsa_to_port(ds, i)->bridge_dev != bridge)
				continue;
			if (priv->ports[i].enable) {
				sw_w32_mask(0, 1 << port, RTL838X_PORT_ISO_CTRL(i));
			}
			priv->ports[i].pm |= 1 << port;

			port_bitmap |= 1 << i;
//			printk("pm: %x, port_bitmap: %x\n",  priv->ports[i].pm, port_bitmap);
		}
	}

	/* Add all other ports to this port matrix. */
	if (priv->ports[port].enable) {
		sw_w32_mask(0, 1 << port, RTL838X_PORT_ISO_CTRL(CPU_PORT));
		sw_w32_mask(0, port_bitmap, RTL838X_PORT_ISO_CTRL(port));
	}
	priv->ports[port].pm |= port_bitmap;
	mutex_unlock(&priv->reg_mutex);

//	print_matrix();
	return 0;
}

static int rtl838x_port_fdb_del(struct dsa_switch *ds, int port,
			   const unsigned char *addr, u16 vid)
{
	
	return 0;
}

static int rtl838x_port_fdb_dump(struct dsa_switch *ds, int port,
				 dsa_fdb_dump_cb_t *cb, void *data)
{
	
	return 0;
}
	

static int rtl838x_port_enable(struct dsa_switch *ds, int port,
				struct phy_device *phydev)
{
	struct rtl838x_switch_priv *priv = ds->priv;
	printk("rtl838x_port_enable: %x %d", (u32) priv, port);
	priv->ports[port].enable = true;

	if (dsa_is_cpu_port(ds, port))
		return 0;

	/* add port to switch mask of CPU_PORT */
	sw_w32_mask(0, 1 << port, RTL838X_PORT_ISO_CTRL(CPU_PORT));

	/* add all other ports in the same bridge to switch mask of port */
	sw_w32_mask(0, priv->ports[port].pm, RTL838X_PORT_ISO_CTRL(port));

	/* enable PHY polling */
	sw_w32_mask(0, 1 << port, RTL838X_SMI_POLL_CTRL);

	return 0;
}

static void rtl838x_port_disable(struct dsa_switch *ds, int port)
{
	struct rtl838x_switch_priv *priv = ds->priv;
	printk("rtl838x_port_disable %x: %d", (u32)priv, port);

	/* you can only disable user ports */
	if (!dsa_is_user_port(ds, port))
		return;

	/* remove port from switch mask of CPU_PORT */
	sw_w32_mask(1 << port, 0, RTL838X_PORT_ISO_CTRL(CPU_PORT));

	/* remove all other ports in the same bridge from switch mask of port */
	sw_w32_mask(priv->ports[port].pm, 0, RTL838X_PORT_ISO_CTRL(port));

	priv->ports[port].enable = false;

	/* disable PHY polling */
	sw_w32_mask(1 << port, 0, RTL838X_SMI_POLL_CTRL);

	return;
}

static int rtl838x_get_mac_eee(struct dsa_switch *ds, int port,
			       struct ethtool_eee *e)
{
	struct rtl838x_switch_priv *priv = ds->priv;
	printk("rtl838x_get_mac_eee, port %d", port);
	e->supported = SUPPORTED_100baseT_Full | SUPPORTED_1000baseT_Full;
	if ( sw_r32(RTL838X_MAC_FORCE_MODE_CTRL(port)) & (1 << 9)) {
		e->advertised |= ADVERTISED_100baseT_Full;
	}

	if ( sw_r32(RTL838X_MAC_FORCE_MODE_CTRL(port)) & (1 << 10)) {
		e->advertised |= ADVERTISED_1000baseT_Full;
	}
	e->eee_enabled = priv->ports[port].eee_enabled;
	printk("enabled: %d, active %x\n", e->eee_enabled, e->advertised);

	if (sw_r32(RTL838X_MAC_EEE_ABLTY) & (1 << port)) {
		e->lp_advertised = ADVERTISED_100baseT_Full;
		e->lp_advertised |= ADVERTISED_1000baseT_Full;
	}

	e->eee_active = !! (e->advertised & e->lp_advertised);
	printk("active: %d, lp %x\n", e->eee_active, e->lp_advertised);

	return 0;
}

static int rtl838x_set_mac_eee(struct dsa_switch *ds, int port,
			       struct ethtool_eee *e)
{
	struct rtl838x_switch_priv *priv = ds->priv;
	printk("rtl838x_set_mac_eee, port %d", port);
	if (e->eee_enabled) {
		printk("Globally enabling EEE\n");
		sw_w32_mask(0x4, 0, RTL838X_SMI_GLB_CTRL);
	}
	if (e->eee_enabled) {
		printk("Enabling EEE for MAC %d\n", port);
		sw_w32_mask(0, 3 << 9, RTL838X_MAC_FORCE_MODE_CTRL(port));
		sw_w32_mask(0, 1 << port, RTL838X_EEE_PORT_TX_EN);
		sw_w32_mask(0, 1 << port, RTL838X_EEE_PORT_RX_EN);
		priv->ports[port].eee_enabled = true;
		e->eee_enabled = true;
	} else {
		printk("Disabling EEE for MAC %d\n", port);
		sw_w32_mask(3 << 9, 0, RTL838X_MAC_FORCE_MODE_CTRL(port));
		sw_w32_mask(1 << port, 0, RTL838X_EEE_PORT_TX_EN);
		sw_w32_mask(1 << port, 0, RTL838X_EEE_PORT_RX_EN);
		priv->ports[port].eee_enabled = false;
		e->eee_enabled = false;
	}
	return 0;
}

static void rtl838x_phylink_mac_config(struct dsa_switch *ds, int port,
				      unsigned int mode,
				      const struct phylink_link_state *state)
{
	// struct rtl838x_eth_priv *priv = ds->priv;
	u32 reg;

	printk("rtl838x_phylink_mac_config port %d, mode %x\n", port, mode);
	
	if (port == CPU_PORT) {
		/* Set Speed, duplex, flow control
		* FORCE_EN | LINK_EN | NWAY_EN | DUP_SEL 
		* | SPD_SEL = 0b10 | FORCE_FC_EN | PHY_MASTER_SLV_MANUAL_EN 
		* | MEDIA_SEL
		*/
		sw_w32(0x6192F, RTL838X_MAC_FORCE_MODE_CTRL(CPU_PORT));

		/* allow CRC errors on CPU-port */
		sw_w32_mask(0, 0x8, RTL838X_MAC_PORT_CTRL(CPU_PORT));
		return;
	}

	reg = sw_r32(RTL838X_MAC_FORCE_MODE_CTRL(port));
	if (mode == MLO_AN_PHY) {
		printk("PHY autonegotiates\n");
		reg |= 1 << 2;
		sw_w32(reg, RTL838X_MAC_FORCE_MODE_CTRL(port));
		return;
	}

	if (mode != MLO_AN_FIXED) {
		printk("Not fixed\n");
	}

	/* Clear id_mode_dis bit, and the existing port mode, let
	 * RGMII_MODE_EN bet set by mac_link_{up,down}
	 */
	reg &= ~(RX_PAUSE_EN | TX_PAUSE_EN);
	
	if (state->pause & MLO_PAUSE_TXRX_MASK) {
		if (state->pause & MLO_PAUSE_TX)
			reg |= TX_PAUSE_EN;
		reg |= RX_PAUSE_EN;
	}

	reg &= ~(3 << 4);
	switch (state->speed) {
	case SPEED_1000:
		reg |= 2 << 4;
		break;
	case SPEED_100:
		reg |= 1 << 4;
		break;
	}

	reg &= ~(DUPLEX_FULL | FORCE_LINK_EN);
	if (state->link)
		reg |= FORCE_LINK_EN;
	if (state->duplex == DUPLEX_FULL)
		reg |= DUPLX_MODE;
	
	// Disable AN
	reg &= ~(1 << 2);
	sw_w32(reg, RTL838X_MAC_FORCE_MODE_CTRL(port));
}

static void rtl838x_phylink_mac_link_down(struct dsa_switch *ds, int port,
				     unsigned int mode,
				     phy_interface_t interface)
{
	/* Stop TX/RX to port */
	sw_w32_mask(0x03, 0, RTL838X_MAC_PORT_CTRL(port));
}

static void rtl838x_phylink_mac_link_up(struct dsa_switch *ds, int port,
				   unsigned int mode,
				   phy_interface_t interface,
				   struct phy_device *phydev)
{
	/* Restart TX/RX to port */
	sw_w32_mask(0, 0x03, RTL838X_MAC_PORT_CTRL(port));
}

static void rtl838x_phylink_validate(struct dsa_switch *ds, int port,
				     unsigned long *supported,
				     struct phylink_link_state *state)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(mask) = { 0, };
	// struct rtl838x_eth_priv *priv = ds->priv;
	printk("In rtl838x_phylink_validate, port %d", port);

	if (!phy_interface_mode_is_rgmii(state->interface) &&
	    state->interface != PHY_INTERFACE_MODE_1000BASEX &&
	    state->interface != PHY_INTERFACE_MODE_MII &&
	    state->interface != PHY_INTERFACE_MODE_REVMII &&
	    state->interface != PHY_INTERFACE_MODE_GMII &&
	    state->interface != PHY_INTERFACE_MODE_QSGMII &&
	    state->interface != PHY_INTERFACE_MODE_INTERNAL &&
	    state->interface != PHY_INTERFACE_MODE_SGMII) {
		bitmap_zero(supported, __ETHTOOL_LINK_MODE_MASK_NBITS);
		dev_err(ds->dev,
			"Unsupported interface: %d for port %d\n",
			state->interface, port);
		return;
	}

	/* switch chip-id? if (priv->id == 0x8382) */

	/* Allow all the expected bits */
	phylink_set(mask, Autoneg);
	phylink_set_port_modes(mask);
	phylink_set(mask, Pause);
	phylink_set(mask, Asym_Pause);
	
	/* With the exclusion of MII and Reverse MII, we support Gigabit,
	 * including Half duplex
	 */
	if (state->interface != PHY_INTERFACE_MODE_MII &&
	    state->interface != PHY_INTERFACE_MODE_REVMII) {
		phylink_set(mask, 1000baseT_Full);
		phylink_set(mask, 1000baseT_Half);
	}

	phylink_set(mask, 10baseT_Half);
	phylink_set(mask, 10baseT_Full);
	phylink_set(mask, 100baseT_Half);
	phylink_set(mask, 100baseT_Full);

	bitmap_and(supported, supported, mask,
		   __ETHTOOL_LINK_MODE_MASK_NBITS);
	bitmap_and(state->advertising, state->advertising, mask,
		   __ETHTOOL_LINK_MODE_MASK_NBITS);
}

static int rtl838x_phylink_mac_link_state(struct dsa_switch *ds, int port,
					  struct phylink_link_state *state)
{
	u32 speed;

	if (port < 0 || port > CPU_PORT)
		return -EINVAL;
	
	state->link = 0;
	if (sw_r32(RTL838X_MAC_LINK_STS) & (1 << port))
		state->link = 1;
	state->duplex = 0;
	if (sw_r32(RTL838X_MAC_LINK_DUP_STS) & (1 << port))
		state->duplex = 1;

	speed = sw_r32(RTL838X_MAC_LINK_SPD_STS(port));
	speed >>= (port % 16) << 1;
	switch (speed & 0x3) {
	case 0:
		state->speed = SPEED_10;
		break;
	case 1:
		state->speed = SPEED_100;
		break;
	case 2:
		state->speed = SPEED_1000;
		break;
	case 3:
		if (port == 24 || port == 26) /* Internal serdes */
			state->speed = SPEED_2500;
		else
			state->speed = SPEED_100; /* Is in fact 500Mbit */
	}

	state->pause &= (MLO_PAUSE_RX | MLO_PAUSE_TX);
	if (sw_r32(RTL838X_MAC_RX_PAUSE_STS) & (1 << port))
		state->pause |= MLO_PAUSE_RX;
	if (sw_r32(RTL838X_MAC_TX_PAUSE_STS) & (1 << port))
		state->pause |= MLO_PAUSE_TX;
	return 1;
}

static int rtl838x_mdio_probe(struct rtl838x_switch_priv *priv)
{
	struct device *dev = priv->dev;
	struct device_node *dn, *mii_np = dev->of_node;
	struct mii_bus *bus;
	int ret;
	u32 pn;

	printk("In rtl838x_mdio_probe\n");
	mii_np = of_find_compatible_node(NULL, NULL, "realtek,rtl838x-mdio");
	if (mii_np) {
		printk("Found compatible MDIO node!\n");
	} else { 
		dev_err(priv->dev, "no %s child node found", "mdio-bus");
		return -ENODEV;
	}
	
	priv->mii_bus = of_mdio_find_bus(mii_np);
	if (!priv->mii_bus) {
		printk("Deferring probe of mdio bus\n");
		return -EPROBE_DEFER;
	}
	if (!of_device_is_available(mii_np))
		ret = -ENODEV;

	bus = devm_mdiobus_alloc(priv->ds->dev);
	if (!bus)
		return -ENOMEM;

	bus->name = "rtl838x slave mii";
	bus->read = &rtl838x_mdio_read;
	bus->write = &rtl838x_mdio_write;
	snprintf(bus->id, MII_BUS_ID_SIZE, "%s-%d", bus->name, dev->id);
	bus->parent = dev;
	priv->ds->slave_mii_bus = bus;
	priv->ds->slave_mii_bus->priv = priv;

	ret = mdiobus_register(priv->ds->slave_mii_bus);
	if (ret && mii_np) {
		of_node_put(dn);
		return ret;
	}

	dn = mii_np;
	for_each_node_by_name(dn, "ethernet-phy") {
		if(of_property_read_u32(dn, "reg", &pn))
			continue;
//		printk("index %d", pn);
		// Check for the integrated SerDes of the RTL8380M first
		if (of_property_read_bool(dn, "phy-is-integrated")
			&& priv->id == 0x8380 && pn >= 24) {
			priv->ports[pn].phy = PHY_RTL838X_SDS;
		}
		if (of_property_read_bool(dn, "phy-is-integrated")
			&& !of_property_read_bool(dn, "sfp")) {
			priv->ports[pn].phy = PHY_RTL8218B_INT;
		}
		if (!of_property_read_bool(dn, "phy-is-integrated")
			&& of_property_read_bool(dn, "sfp")) {
			priv->ports[pn].phy = PHY_RTL8214FC;
		}
		if (!of_property_read_bool(dn, "phy-is-integrated")
			&& !of_property_read_bool(dn, "sfp")) {
			priv->ports[pn].phy = PHY_RTL8218B_EXT;
		}
	}

	/* Disable MAC polling the PHY so that we can start configuration */
	sw_w32(0x00000000, RTL838X_SMI_POLL_CTRL);

	/* Enable PHY control via SoC */
	sw_w32_mask(0, 1 << 15, RTL838X_SMI_GLB_CTRL);
	/* Power on fibre ports and reset them if necessary */
	if (priv->ports[24].phy == PHY_RTL838X_SDS) {
		printk("Powering on fibre ports & reset\n");
		rtl8380_sds_power(24, 1);
		rtl8380_sds_power(26, 1);
	}

	printk("In rtl838x_mdio_probe done\n");
	return 0;
/*
err_out_free_mdiobus:
	mdiobus_free(bus);
	return ret; */
}

static const struct dsa_switch_ops rtl838x_switch_ops = {
	.get_tag_protocol	= rtl838x_get_tag_protocol,
	.setup			= rtl838x_setup,
	.port_vlan_filtering	= rtl838x_vlan_filtering,
	.port_vlan_prepare	= rtl838x_vlan_prepare,
	.port_vlan_add		= rtl838x_vlan_add,
	.port_vlan_del		= rtl838x_vlan_del,
	.port_bridge_join	= rtl838x_port_bridge_join,
	.port_bridge_leave	= rtl838x_port_bridge_leave,
	.port_stp_state_set	= rtl838x_port_stp_state_set,
	.port_fast_age		= rtl838x_fast_age,
	.port_fdb_add		= rtl838x_port_fdb_add,
	.port_fdb_del		= rtl838x_port_fdb_del,
	.port_fdb_dump		= rtl838x_port_fdb_dump,
	.port_enable		= rtl838x_port_enable,
	.port_disable		= rtl838x_port_disable,
	.phy_read		= dsa_phy_read,
	.phy_write		= dsa_phy_write,
	.get_strings		= rtl838x_get_strings,
	.get_ethtool_stats	= rtl838x_get_ethtool_stats,
	.get_sset_count		= rtl838x_get_sset_count,
	.phylink_validate	= rtl838x_phylink_validate,
	.phylink_mac_link_state	= rtl838x_phylink_mac_link_state,
	.phylink_mac_config	= rtl838x_phylink_mac_config,
	.phylink_mac_link_down	= rtl838x_phylink_mac_link_down,
	.phylink_mac_link_up	= rtl838x_phylink_mac_link_up,
	.set_mac_eee		= rtl838x_set_mac_eee,
	.get_mac_eee		= rtl838x_get_mac_eee,

};

static int __init rtl838x_sw_probe(struct platform_device *pdev)
{
	int err = 0;
	struct rtl838x_switch_priv *priv;
	struct device *dev = &pdev->dev;
//	struct rtl838x_vlan_info info;

	pr_info("Probing RTL838X switch device\n");
	if (!pdev->dev.of_node) {
		dev_err(dev, "No DT found\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->ds = dsa_switch_alloc(dev, DSA_MAX_PORTS);

	if (!priv->ds)
		return -ENOMEM;
	priv->ds->dev = dev;
	priv->ds->priv = priv;
	priv->ds->ops = &rtl838x_switch_ops;
	priv->dev = dev;
	priv->ds->num_ports = 30;
	
	rtl8380_get_version(priv);
	printk("Chip version %c\n", priv->version);

	err = rtl838x_mdio_probe(priv);
	if (err) {
		return err;
	}
	err = dsa_register_switch(priv->ds);
	if (err) {
		dev_err(dev, "Error registering switch: %d\n", err);
		return err;
	}

	/* Enable link and media change interrupts... */
	sw_w32_mask(0, 3, RTL838X_ISR_GLB_SRC);
	/* ... for all ports */
	sw_w32(0xffffffff, RTL838X_ISR_PORT_LINK_STS_CHG);
	sw_w32(0xffffffff, RTL838X_IMR_PORT_LINK_STS_CHG);
	/* Enable interrupts for switch */
	sw_w32(0x1, RTL838X_IMR_GLB);
	priv->link_state_irq = 28;
	err = request_irq(priv->link_state_irq, rtl838x_switch_irq,
			  IRQF_SHARED, "rtl8838x-link-state", priv->ds);
	if (err) {
		dev_err(dev, "Error setting up switch interrupt.\n");
		/* Need to free allocated switch here */
	}

	rtl838x_get_l2aging();

	return err;
}


static int rtl838x_sw_remove(struct platform_device *pdev)
{
	printk("Removing platform driver for rtl838x-sw\n");
	return 0;
}

static const struct of_device_id rtl838x_switch_of_ids[] = {
	{ .compatible = "realtek,rtl838x-switch"},
	{ /* sentinel */ }
};


MODULE_DEVICE_TABLE(of, rtl838x_switch_of_ids);

static struct platform_driver rtl838x_switch_driver = {
	.probe = rtl838x_sw_probe,
	.remove = rtl838x_sw_remove,
	.driver = {
		.name = "rtl838x-switch",
		.pm = NULL,
		.of_match_table = rtl838x_switch_of_ids,
	},
};

module_platform_driver(rtl838x_switch_driver);

MODULE_AUTHOR("B. Koblitz");
MODULE_DESCRIPTION("RTL838X SoC Switch Driver");
MODULE_LICENSE("GPL");
