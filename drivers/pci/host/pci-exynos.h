/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PCIE_EXYNOS_H
#define __PCIE_EXYNOS_H

#define MAX_TIMEOUT		2000
#define ID_MASK			0xffff
#define TPUT_THRESHOLD		150
#define MAX_RC_NUM		2

#if defined(CONFIG_SOC_EXYNOS8890)
#define PCI_DEVICE_ID_EXYNOS	0xa544
#define GPIO_DEBUG_SFR		0x15601068
#else
#define PCI_DEVICE_ID_EXYNOS	0xecec
#define GPIO_DEBUG_SFR		0x0
#endif

#define to_exynos_pcie(x)	container_of(x, struct exynos_pcie, pp)

#define PCIE_BUS_PRIV_DATA(pdev) \
	((struct pcie_port *)pdev->bus->sysdata)

struct exynos_pcie_clks {
	struct clk	*pcie_clks[10];
	struct clk	*phy_clks[3];
};

enum exynos_pcie_state {
	STATE_LINK_DOWN = 0,
	STATE_LINK_UP_TRY,
	STATE_LINK_DOWN_TRY,
	STATE_LINK_UP,
};

struct exynos_pcie {
	void __iomem		*elbi_base;
	void __iomem		*phy_base;
	void __iomem		*block_base;
	void __iomem		*rc_dbi_base;
	void __iomem		*phy_pcs_base;
	struct regmap		*pmureg;
	int			perst_gpio;
	int			ch_num;
	int			pcie_clk_num;
	int			phy_clk_num;
	enum exynos_pcie_state	state;
	int			probe_ok;
	int			l1ss_enable;
	int			linkdown_cnt;
	int			idle_ip_index;
	bool			use_msi;
	bool			pcie_changed;
	struct workqueue_struct	*pcie_wq;
	struct exynos_pcie_clks	clks;
	struct pcie_port	pp;
	struct pci_dev		*pci_dev;
	struct pci_saved_state	*pci_saved_configs;
	struct notifier_block	lpa_nb;
	struct notifier_block	ss_dma_mon_nb;
	struct delayed_work	work;
	struct exynos_pcie_register_event *event_reg;
#ifdef CONFIG_PCI_EXYNOS_TEST
	int			wlan_gpio;
	int			bt_gpio;
#endif
#ifdef CONFIG_PM_DEVFREQ
	unsigned int		int_min_lock;
#endif
	int			l1ss_ctrl_id_state;
	struct workqueue_struct *pcie_wq_l1ss;
	struct delayed_work     work_l1ss;
	int			boot_cnt;
	int			work_l1ss_cnt;
};

/* PCIe ELBI registers */
#define PCIE_IRQ_PULSE			0x000
#define IRQ_INTA_ASSERT			(0x1 << 0)
#define IRQ_INTB_ASSERT			(0x1 << 2)
#define IRQ_INTC_ASSERT			(0x1 << 4)
#define IRQ_INTD_ASSERT			(0x1 << 6)
#define IRQ_RADM_PM_TO_ACK		(0x1 << 18)
#define IRQ_L1_EXIT			(0x1 << 24)
#define PCIE_IRQ_LEVEL			0x004
#define IRQ_MSI_CTRL			(0x1 << 1)
#define PCIE_IRQ_SPECIAL		0x008
#define PCIE_IRQ_EN_PULSE		0x00c
#define PCIE_IRQ_EN_LEVEL		0x010
#define IRQ_MSI_ENABLE			(0x1 << 1)
#define IRQ_LINK_DOWN			(0x1 << 30)
#define IRQ_LINKDOWN_ENABLE		(0x1 << 30)
#define PCIE_IRQ_EN_SPECIAL		0x014
#define PCIE_SW_WAKE			0x018
#define PCIE_IRQ_LEVEL_FOR_READ		0x020
#define L1_2_IDLE_STATE			(0x1 << 23)
#define PCIE_APP_LTSSM_ENABLE		0x02c
#define PCIE_L1_BUG_FIX_ENABLE		0x038
#define PCIE_APP_REQ_EXIT_L1		0x040
#define PCIE_CXPL_DEBUG_INFO_H		0x070
#define PCIE_ELBI_RDLH_LINKUP		0x074
#define PCIE_ELBI_LTSSM_DISABLE		0x0
#define PCIE_ELBI_LTSSM_ENABLE		0x1
#define PCIE_PM_DSTATE			0x88
#define PCIE_D0_UNINIT_STATE		0x4
#define PCIE_APP_REQ_EXIT_L1_MODE	0xF4
#define APP_REQ_EXIT_L1_MODE		0x1
#define L1_REQ_NAK_CONTROL		(0x3 << 4)
#define L1_REQ_NAK_CONTROL_MASTER	(0x1 << 4)
#define PCIE_HISTORY_REG(x)		(0x138 + ((x) * 0x4))
#define LTSSM_STATE(x)			(((x) >> 16) & 0x3f)
#define PM_DSTATE(x)			(((x) >> 8) & 0x7)
#define L1SUB_STATE(x)			(((x) >> 0) & 0x7)
#define PCIE_LINKDOWN_RST_CTRL_SEL	0x1B8
#define PCIE_LINKDOWN_RST_MANUAL	(0x1 << 1)
#define PCIE_LINKDOWN_RST_FSM		(0x1 << 0)
#define PCIE_SOFT_AUXCLK_SEL_CTRL	0x1C4
#define CORE_CLK_GATING			(0x1 << 0)
#define PCIE_SOFT_CORE_RESET		0x1D0
#define PCIE_STATE_HISTORY_CHECK	0x274
#define HISTORY_BUFFER_ENABLE		(0x1 << 0)
#define HISTORY_BUFFER_CLEAR		(0x1 << 1)
#define PCIE_QCH_SEL			0x2C8
#define CLOCK_GATING_IN_L12		0x1
#define CLOCK_NOT_GATING		0x3
#define CLOCK_GATING_MASK		0x3
#define PCIE_DMA_MONITOR1		0x2CC
#define PCIE_DMA_MONITOR2		0x2D0
#define PCIE_DMA_MONITOR3		0x2D4
#define FSYS1_MON_SEL_MASK		0xf
#define PCIE_MON_SEL_MASK		0xff

/* PCIe PMU registers */
#define PCIE_PHY_CONTROL		0x071C
#define PCIE_PHY_CONTROL_MASK		0x1

#endif
