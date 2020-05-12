//  SPDX-License-Identifier: GPL-2.0-only

#ifndef _RTL838X_H
#define _RTL838X_H

#include <net/dsa.h>

#define RTL838X_SW_BASE ((volatile void *)0xBB000000)

#define sw_r32(reg)	__raw_readl(reg)
#define sw_w32(val, reg)	__raw_writel(val, reg)
#define sw_w32_mask(clear, set, reg)	\
	sw_w32((sw_r32(reg) & ~(clear)) | (set), reg)
/*
 * Register definition
 */
 
#define CPU_PORT				28
#define RTL838X_MAC_PORT_CTRL(port)		(RTL838X_SW_BASE + 0xd560 + (((port) << 7)))
#define RTL838X_DMA_IF_INTR_STS			(RTL838X_SW_BASE + 0x9f54)
#define RTL838X_DMA_IF_INTR_MSK			(RTL838X_SW_BASE + 0x9f50)
#define RTL838X_DMA_IF_CTRL			(RTL838X_SW_BASE + 0x9f58)
#define RTL838X_RST_GLB_CTRL_0			(RTL838X_SW_BASE + 0x3c)
#define RTL838X_MAC_FORCE_MODE_CTRL(port)	(RTL838X_SW_BASE + 0xa104 + (((port) << 2)))
#define RTL838X_MAC				(RTL838X_SW_BASE + 0xa9ec)
#define RTL838X_MAC_ALE				(RTL838X_SW_BASE + 0x6b04)
#define RTL838X_MAC2				(RTL838X_SW_BASE + 0xa320)
#define RTL838X_DMA_RX_BASE(idx)		(RTL838X_SW_BASE + 0x9f00 + (((idx) << 2)))
#define RTL838X_DMA_TX_BASE(idx)		(RTL838X_SW_BASE + 0x9f40 + (((idx) << 2)))
#define RTL838X_DMA_IF_RX_RING_SIZE(idx)	(RTL838X_SW_BASE + 0xB7E4 + (((idx >> 3) << 2)))
#define RTL838X_DMA_IF_RX_RING_CNTR(idx)	(RTL838X_SW_BASE + 0xB7E8 + (((idx >> 3) << 2)))
#define RTL838X_DMA_IF_RX_CUR(idx)		(RTL838X_SW_BASE + 0x9F20 + (((idx) << 2)))
#define RTL838X_DMY_REG31			(RTL838X_SW_BASE + 0x3b28)
#define RTL838X_SDS_MODE_SEL			(RTL838X_SW_BASE + 0x28)
#define RTL838X_SDS_CFG_REG			(RTL838X_SW_BASE + 0x34)
#define RTL838X_INT_MODE_CTRL			(RTL838X_SW_BASE + 0x5c)
#define RTL838X_CHIP_INFO			(RTL838X_SW_BASE + 0xd8)
#define RTL838X_SDS4_REG28			(RTL838X_SW_BASE + 0xef80)
#define RTL838X_SDS4_DUMMY0			(RTL838X_SW_BASE + 0xef8c)
#define RTL838X_SDS5_EXT_REG6			(RTL838X_SW_BASE + 0xf18c)
#define RTL838X_PORT_ISO_CTRL(port)		(RTL838X_SW_BASE + 0x4100 + ((port) << 2))
#define RTL838X_STAT_PORT_STD_MIB(port)		(RTL838X_SW_BASE + 0x1200 + (((port) << 8)))
#define RTL838X_STAT_RST			(RTL838X_SW_BASE + 0x3100)
#define RTL838X_STAT_CTRL			(RTL838X_SW_BASE + 0x3108)

/* Registers of the internal Serdes of the 8380 */
#define MAPLE_SDS4_REG0r			RTL838X_SDS4_REG28
#define MAPLE_SDS5_REG0r			(RTL838X_SDS4_REG28 + 0x100)
#define MAPLE_SDS4_REG3r			RTL838X_SDS4_DUMMY0
#define MAPLE_SDS5_REG3r			(RTL838X_SDS4_REG28 + 0x100)
#define MAPLE_SDS4_FIB_REG0r			(RTL838X_SDS4_REG28 + 0x880)
#define MAPLE_SDS5_FIB_REG0r			(RTL838X_SDS4_REG28 + 0x980)

/* VLAN registers */
#define RTL838X_VLAN_PROFILE(idx)		(RTL838X_SW_BASE + 0x3A88 + ((idx) << 2))
#define RTL838X_VLAN_PORT_EGR_FLTR		(RTL838X_SW_BASE + 0x3A84)
#define RTL838X_VLAN_PORT_PB_VLAN(port)		(RTL838X_SW_BASE + 0x3C00 + ((port) << 2))
#define RTL838X_VLAN_PORT_IGR_FLTR_0		(RTL838X_SW_BASE + 0x3A7C)
#define RTL838X_VLAN_PORT_IGR_FLTR_1		(RTL838X_SW_BASE + 0x3A7C + 4)
#define RTL838X_TBL_ACCESS_CTRL_0		(RTL838X_SW_BASE + 0x6914)
#define RTL838X_TBL_ACCESS_DATA_0(idx)		(RTL838X_SW_BASE + 0x6918 + ((idx) << 2))
#define RTL838X_TBL_ACCESS_CTRL_1		(RTL838X_SW_BASE + 0xA4C8)
#define RTL838X_TBL_ACCESS_DATA_1(idx)		(RTL838X_SW_BASE + 0xA4CC + ((idx) << 2))

/* MAC handling */
#define RTL838X_MAC_LINK_STS			(RTL838X_SW_BASE + 0xa188)
#define RTL838X_MAC_LINK_SPD_STS(port)		(RTL838X_SW_BASE + 0xa190 + (((port >> 4) << 2)))
#define RTL838X_MAC_LINK_DUP_STS		(RTL838X_SW_BASE + 0xa19c)
#define RTL838X_MAC_TX_PAUSE_STS		(RTL838X_SW_BASE + 0xa1a0)
#define RTL838X_MAC_RX_PAUSE_STS		(RTL838X_SW_BASE + 0xa1a4)
#define RTL838X_EEE_TX_TIMER_GIGA_CTRL		(RTL838X_SW_BASE + 0xaa04)
#define RTL838X_EEE_TX_TIMER_GELITE_CTRL	(RTL838X_SW_BASE + 0xaa08)

/* MAC link state bits */
#define FORCE_EN				(1 << 0)
#define FORCE_LINK_EN				(1 << 1)
#define NWAY_EN					(1 << 2)
#define DUPLX_MODE				(1 << 3)
#define TX_PAUSE_EN				(1 << 6)
#define RX_PAUSE_EN				(1 << 7)

/* EEE */
#define RTL838X_MAC_EEE_ABLTY			(RTL838X_SW_BASE + 0xa1a8)
#define RTL838X_EEE_PORT_TX_EN			(RTL838X_SW_BASE + 0x014c)
#define RTL838X_EEE_PORT_RX_EN			(RTL838X_SW_BASE + 0x0150)
#define RTL8380_EEE_CLK_STOP_CTRL		(RTL838X_SW_BASE + 0x0148)

enum phy_type {
	PHY_NONE = 0,
	PHY_RTL838X_SDS = 1,
	PHY_RTL8218B_INT = 2,
	PHY_RTL8218B_EXT = 3,
	PHY_RTL8214FC = 4
};

struct rtl838x_port {
	bool enable;
	u32 pm;
	u16 pvid;
	bool eee_enabled;
	enum phy_type phy;
};

struct rtl838x_switch_priv {
	/* Switch operation */
	struct dsa_switch *ds;
	struct device *dev;
	u32 id;
	char version;
	struct rtl838x_port ports[32]; /* TODO: correct size! */
	struct mutex reg_mutex;
	int link_state_irq;
	struct mii_bus *mii_bus;
};

#endif /* _RTL838X_H */
