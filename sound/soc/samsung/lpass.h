/* sound/soc/samsung/lpass.h
 *
 * ALSA SoC Audio Layer - Samsung Audio Subsystem driver
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd.
 *	Yeongman Seo <yman.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_SAMSUNG_LPASS_H
#define __SND_SOC_SAMSUNG_LPASS_H

#include <linux/pm_qos.h>

/* SFR */
#define LPASS_VERSION		(0x00)
#define LPASS_CA5_SW_RESET	(0x04)
#define LPASS_CORE_SW_RESET	(0x08)
#define LPASS_MIF_POWER		(0x10)
#define LPASS_CA5_BOOTADDR	(0x20)
#define LPASS_CA5_DBG		(0x30)
#define LPASS_SW_INTR_CA5	(0x40)
#define LPASS_INTR_CA5_STATUS	(0x44)
#define LPASS_INTR_CA5_MASK	(0x48)
#define LPASS_SW_INTR_CPU	(0x50)
#define LPASS_INTR_CPU_STATUS	(0x54)
#define LPASS_INTR_CPU_MASK	(0x58)

/* SW_RESET */
#define LPASS_SW_RESET_CA5	(1 << 0)
#define LPASS_SW_RESET_SB	(1 << 11)
#define LPASS_SW_RESET_UART	(1 << 10)
#ifndef CONFIG_SOC_EXYNOS8890
#define LPASS_SW_RESET_PCM	(1 << 9)
#define LPASS_SW_RESET_I2S	(1 << 8)
#else
#define LPASS_SW_RESET_PCM	(1 << 8)
#define LPASS_SW_RESET_I2S	(1 << 7)
#endif
#define LPASS_SW_RESET_TIMER	(1 << 2)
#define LPASS_SW_RESET_MEM	(1 << 1)
#define LPASS_SW_RESET_DMA	(1 << 0)

/* Interrupt mask */
#define LPASS_INTR_APM		(1 << 9)
#define LPASS_INTR_MIF		(1 << 8)
#define LPASS_INTR_TIMER	(1 << 7)
#define LPASS_INTR_DMA		(1 << 6)
#define LPASS_INTR_GPIO		(1 << 5)
#define LPASS_INTR_I2S		(1 << 4)
#define LPASS_INTR_PCM		(1 << 3)
#define LPASS_INTR_SB		(1 << 2)
#define LPASS_INTR_UART		(1 << 1)
#define LPASS_INTR_SFR		(1 << 0)

#define LPCLK_CTRLID_OFFLOAD	(1 << 0)
#define LPCLK_CTRLID_LEGACY	(1 << 1)
#define LPCLK_CTRLID_RECORD	(1 << 2)

struct lpass_info {
	spinlock_t		lock;
	bool			valid;
	bool			enabled;
	int			ver;
	struct platform_device	*pdev;
	void __iomem		*regs;
	void __iomem		*regs_s;
	void __iomem		*mem;
	int			mem_size;
	void __iomem		*sysmmu;
	struct iommu_domain	*domain;
	struct proc_dir_entry	*proc_file;
	struct clk		*clk_dmac;
	struct clk		*clk_sramc;
	struct clk		*clk_intr;
	struct clk		*clk_timer;
	struct regmap 		*pmureg;
	atomic_t		dma_use_cnt;
	atomic_t		use_cnt;
	atomic_t		stream_cnt;
	bool			display_on;
	bool			uhqa_on;
	bool			i2s_master_mode;
	struct pm_qos_request	aud_cluster1_qos;
	struct pm_qos_request	aud_cluster0_qos;
	struct pm_qos_request	aud_mif_qos;
	struct pm_qos_request	aud_int_qos;
	int			cpu_qos;
	int			kfc_qos;
	int			mif_qos;
	int			int_qos;
#ifdef CONFIG_SOC_EXYNOS8890
	void			*sram_fw_back;
#endif
};

extern void __iomem *lpass_get_regs(void);
extern void __iomem *lpass_get_regs_s(void);
extern void __iomem *lpass_get_mem(void);

extern struct clk *lpass_get_i2s_opclk(int clk_id);
extern void lpass_reg_dump(void);

extern void __iomem *lpass_cmu_save[];
extern int lpass_get_clk(struct device *dev, struct lpass_info *lpass);
extern void lpass_put_clk(struct lpass_info *lpass);
extern int lpass_set_clk_heirachy(struct device *dev);
extern void lpass_set_mux_pll(void);
extern void lpass_set_mux_osc(void);
extern void lpass_enable_pll(bool on);
extern void lpass_retention_pad_reg(void);
extern void lpass_release_pad_reg(void);
extern void lpass_reset_clk_default(void);
extern void lpass_init_clk_gate(void);
extern void lpass_disable_mif_status(bool on);
extern void lpass_mif_power_on(void);

extern void lpass_update_lpclock(u32 ctrlid, bool idle);
extern void lpass_update_lpclock_impl(struct device *dev, u32 ctrlid, bool idle);

int lpass_get_dram_usage_count(void);
void lpass_inc_dram_usage_count(void);
void lpass_dec_dram_usage_count(void);
extern void update_cp_available(bool);
extern bool lpass_i2s_master_mode(void);
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
extern void lpass_set_cpu_lock(int level);
#endif

#endif /* __SND_SOC_SAMSUNG_LPASS_H */
