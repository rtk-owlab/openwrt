//  SPDX-License-Identifier: GPL-2.0-only

#ifndef _RTL838X_ETH_H
#define _RTL838X_ETH_H

#define RTL838X_SW_BASE ((volatile void __iomem *)0xBB000000)

#define sw_r32(reg)	__raw_readl(reg)
#define sw_w32(val, reg)	__raw_writel(val, reg)
#define sw_w32_mask(clear, set, reg)	\
	sw_w32((sw_r32(reg) & ~(clear)) | (set), reg)
/*
 * Register definition
 */

#define RTL838X_CPU_PORT			28
#define RTL839X_CPU_PORT			52

#define RTL838X_MAC_PORT_CTRL			(RTL838X_SW_BASE + 0xd560)
#define RTL839X_MAC_PORT_CTRL			(RTL838X_SW_BASE + 0x8004)
#define RTL838X_DMA_IF_INTR_STS			(RTL838X_SW_BASE + 0x9f54)
#define RTL839X_DMA_IF_INTR_STS			(RTL838X_SW_BASE + 0x7868)
#define RTL838X_DMA_IF_INTR_MSK			(RTL838X_SW_BASE + 0x9f50)
#define RTL839X_DMA_IF_INTR_MSK			(RTL838X_SW_BASE + 0x7864)
#define RTL838X_DMA_IF_CTRL			(RTL838X_SW_BASE + 0x9f58)
#define RTL839X_DMA_IF_CTRL			(RTL838X_SW_BASE + 0x786c)
#define RTL838X_RST_GLB_CTRL_0			(RTL838X_SW_BASE + 0x003c)
#define RTL838X_MAC_FORCE_MODE_CTRL		(RTL838X_SW_BASE + 0xa104)
#define RTL839X_MAC_FORCE_MODE_CTRL		(RTL838X_SW_BASE + 0x02bc)

/* MAC address settings */
#define RTL838X_MAC				(RTL838X_SW_BASE + 0xa9ec)
#define RTL839X_MAC				(RTL838X_SW_BASE + 0x02b4)
#define RTL838X_MAC_ALE				(RTL838X_SW_BASE + 0x6b04)
#define RTL838X_MAC2				(RTL838X_SW_BASE + 0xa320)

#define RTL838X_DMA_RX_BASE			(RTL838X_SW_BASE + 0x9f00)
#define RTL839X_DMA_RX_BASE			(RTL838X_SW_BASE + 0x780c)
#define RTL838X_DMA_TX_BASE			(RTL838X_SW_BASE + 0x9f40)
#define RTL839X_DMA_TX_BASE			(RTL838X_SW_BASE + 0x784c)
#define RTL838X_DMA_IF_RX_RING_SIZE		(RTL838X_SW_BASE + 0xB7E4)
#define RTL839X_DMA_IF_RX_RING_SIZE		(RTL838X_SW_BASE + 0x6038)
#define RTL838X_DMA_IF_RX_RING_CNTR		(RTL838X_SW_BASE + 0xB7E8)
#define RTL839X_DMA_IF_RX_RING_CNTR		(RTL838X_SW_BASE + 0x603c)
#define RTL838X_DMA_IF_RX_CUR			(RTL838X_SW_BASE + 0x9F20)
#define RTL839X_DMA_IF_RX_CUR			(RTL838X_SW_BASE + 0x782c)

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
#define RTL839X_TBL_ACCESS_L2_CTRL		(RTL838X_SW_BASE + 0x1180)
#define RTL839X_TBL_ACCESS_L2_DATA(idx)		(RTL838X_SW_BASE + 0x1184 + ((idx) << 2))
/* MAC handling */
#define RTL838X_MAC_LINK_STS			(RTL838X_SW_BASE + 0xa188)
#define RTL839X_MAC_LINK_STS			(RTL838X_SW_BASE + 0x0390)
#define RTL838X_MAC_LINK_SPD_STS		(RTL838X_SW_BASE + 0xa190)
#define RTL839X_MAC_LINK_SPD_STS		(RTL838X_SW_BASE + 0x03a0)
#define RTL838X_MAC_LINK_DUP_STS		(RTL838X_SW_BASE + 0xa19c)
#define RTL839X_MAC_LINK_DUP_STS		(RTL838X_SW_BASE + 0x03b0)
// TODO: RTL8390_MAC_LINK_MEDIA_STS_ADDR ???
#define RTL838X_MAC_TX_PAUSE_STS		(RTL838X_SW_BASE + 0xa1a0)
#define RTL839X_MAC_TX_PAUSE_STS		(RTL838X_SW_BASE + 0x03b8)
#define RTL838X_MAC_RX_PAUSE_STS		(RTL838X_SW_BASE + 0xa1a4)
#define RTL839X_MAC_RX_PAUSE_STS		(RTL838X_SW_BASE + 0x03c0)
#define RTL838X_EEE_TX_TIMER_GIGA_CTRL		(RTL838X_SW_BASE + 0xaa04)
#define RTL838X_EEE_TX_TIMER_GELITE_CTRL	(RTL838X_SW_BASE + 0xaa08)
#define RTL839X_MAC_GLB_CTRL			(RTL838X_SW_BASE + 0x02a8)
#define RTL839X_SCHED_LB_TICK_TKN_CTRL		(RTL838X_SW_BASE + 0x60f8)

/* MAC link state bits */
#define FORCE_EN				(1 << 0)
#define FORCE_LINK_EN				(1 << 1)
#define NWAY_EN					(1 << 2)
#define DUPLX_MODE				(1 << 3)
#define TX_PAUSE_EN				(1 << 6)
#define RX_PAUSE_EN				(1 << 7)

inline volatile void __iomem *rtl838x_mac_port_ctrl(int p)
{
	return RTL838X_MAC_PORT_CTRL + (p << 7);
}

inline volatile void __iomem *rtl839x_mac_port_ctrl(int p)
{
	return RTL839X_MAC_PORT_CTRL + (p << 7);
}

static inline volatile void __iomem *rtl838x_mac_force_mode_ctrl(int p)
{
	return RTL838X_MAC_FORCE_MODE_CTRL + (p << 2);
}

static inline volatile void __iomem *rtl839x_mac_force_mode_ctrl(int p)
{
	return RTL839X_MAC_FORCE_MODE_CTRL + (p << 2);
}

inline volatile void __iomem *rtl838x_dma_rx_base(int i)
{
	return RTL838X_DMA_RX_BASE + (i << 2);
}

inline volatile void __iomem *rtl839x_dma_rx_base(int i)
{
	return RTL839X_DMA_RX_BASE + (i << 2);
}

inline volatile void __iomem *rtl838x_dma_tx_base(int i)
{
	return RTL838X_DMA_TX_BASE + (i << 2);
}

inline volatile void __iomem *rtl839x_dma_tx_base(int i)
{
	return RTL839X_DMA_TX_BASE + (i << 2);
}

inline volatile void __iomem *rtl838x_dma_if_rx_ring_size(int i)
{
	return RTL838X_DMA_IF_RX_RING_SIZE + ((i >> 3) << 2);
}

inline volatile void __iomem *rtl839x_dma_if_rx_ring_size(int i)
{
	return RTL839X_DMA_IF_RX_RING_SIZE + ((i >> 3) << 2);
}

inline volatile void __iomem *rtl838x_dma_if_rx_ring_cntr(int i)
{
	return RTL838X_DMA_IF_RX_RING_CNTR + ((i >> 3) << 2);
}

inline volatile void __iomem *rtl839x_dma_if_rx_ring_cntr(int i)
{
	return RTL839X_DMA_IF_RX_RING_CNTR + ((i >> 3) << 2);
}


inline volatile void __iomem *rtl838x_dma_if_rx_cur(int i)
{
	return RTL838X_DMA_IF_RX_CUR + (i << 2);
}

inline volatile void __iomem *rtl839x_dma_if_rx_cur(int i)
{
	return RTL839X_DMA_IF_RX_CUR + (i << 2);
}

inline u32 rtl838x_get_mac_link_sts(int port)
{
	return (sw_r32(RTL838X_MAC_LINK_STS) & (1 << port));
}

inline u32 rtl839x_get_mac_link_sts(int p)
{
	return (sw_r32(RTL839X_MAC_LINK_STS + ((p >> 5) << 2)) & (1 << p));
}

inline u32 rtl838x_get_mac_link_dup_sts(int port)
{
	return (sw_r32(RTL838X_MAC_LINK_DUP_STS) & (1 << port));
}

inline u32 rtl839x_get_mac_link_dup_sts(int p)
{
	return (sw_r32(RTL839X_MAC_LINK_DUP_STS + ((p >> 5) << 2)) & (1 << p));
}

inline u32 rtl838x_get_mac_link_spd_sts(int port)
{
	volatile void __iomem *r = RTL838X_MAC_LINK_SPD_STS + ((port >> 4) << 2);
	u32 speed = sw_r32(r);
	speed >>= (port % 16) << 1;
	return (speed & 0x3);
}

inline u32 rtl839x_get_mac_link_spd_sts(int port)
{
	volatile void __iomem *r = RTL839X_MAC_LINK_SPD_STS + ((port >> 4) << 2);
	u32 speed = sw_r32(r);
	speed >>= (port % 16) << 1;
	return (speed & 0x3);
}

inline u32 rtl838x_get_mac_rx_pause_sts(int port)
{
	return (sw_r32(RTL838X_MAC_RX_PAUSE_STS) & (1 << port));
}

inline u32 rtl839x_get_mac_rx_pause_sts(int p)
{
	return (sw_r32(RTL839X_MAC_RX_PAUSE_STS + ((p >> 5) << 2)) & (1 << p));
}

inline u32 rtl838x_get_mac_tx_pause_sts(int port)
{
	return (sw_r32(RTL838X_MAC_TX_PAUSE_STS) & (1 << port));
}

inline u32 rtl839x_get_mac_tx_pause_sts(int p)
{
	return (sw_r32(RTL839X_MAC_TX_PAUSE_STS + ((p >> 5) << 2)) & (1 << p));
}


struct rtl838x_reg {
	volatile void __iomem *(*mac_port_ctrl)(int);
	volatile void __iomem *dma_if_intr_sts;
	volatile void __iomem *dma_if_intr_msk;
	volatile void __iomem *dma_if_ctrl;
	volatile void __iomem * (*mac_force_mode_ctrl)(int);
	volatile void __iomem * (*dma_rx_base)(int);
	volatile void __iomem * (*dma_tx_base)(int);
	volatile void __iomem * (*dma_if_rx_ring_size)(int);
	volatile void __iomem * (*dma_if_rx_ring_cntr)(int);
	volatile void __iomem * (*dma_if_rx_cur)(int);
	volatile void __iomem *rst_glb_ctrl;
	u32 (*get_mac_link_sts)(int);
	u32 (*get_mac_link_dup_sts)(int);
	u32 (*get_mac_link_spd_sts)(int);
	u32 (*get_mac_rx_pause_sts)(int);
	u32 (*get_mac_tx_pause_sts)(int);
	volatile void __iomem *mac;
};

int rtl838x_write_phy(u32 port, u32 page, u32 reg, u32 val);
int rtl838x_read_phy(u32 port, u32 page, u32 reg, u32 *val);
int rtl839x_write_phy(u32 port, u32 page, u32 reg, u32 val);
int rtl839x_read_phy(u32 port, u32 page, u32 reg, u32 *val);


#endif /* _RTL838X_ETH_H */
