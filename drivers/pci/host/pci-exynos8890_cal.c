/*
 * PCIe phy driver for Samsung EXYNOS8890
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Kyoungil Kim <ki0351.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#include <linux/exynos_otp.h>

static struct exynos_pcie g_pcie[MAX_RC_NUM];
#endif

void exynos_pcie_phy_otp_config(void *phy_base_regs, int ch_num)
{
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
	u8 utype;
	u8 uindex_count;
	struct tune_bits *data;
	u32 index;
	u32 value;
	u16 magic_code;
	u32 i;

	struct pcie_port *pp = &g_pcie[ch_num].pp;

	if (ch_num == 0)
		magic_code = 0x5030;
	else if (ch_num == 1)
		magic_code = 0x5031;
	else
		return;

	if (otp_tune_bits_parsed(magic_code, &utype, &uindex_count, &data) == 0) {
		dev_err(pp->dev, "%s: [OTP] uindex_count %d", __func__, uindex_count);
		for (i = 0; i < uindex_count; i++){
			index = data[i].index;
			value = data[i].value;

			dev_err(pp->dev, "%s: [OTP][Return Value] index = 0x%x, value = 0x%x\n", __func__, index, value);
			dev_err(pp->dev, "%s: [OTP][Before Reg Value] offset 0x%x = 0x%x\n", __func__, index * 4, readl(phy_base_regs + (index * 4)));
			if(readl(phy_base_regs + (index * 4)) != value)
				writel(value, phy_base_regs + (index * 4));
			else
				return;
			dev_err(pp->dev, "%s: [OTP][After Reg Value] offset 0x%x = 0x%x\n", __func__, index * 4, readl(phy_base_regs + (index * 4)));
               }
       }
#else
	return;
#endif
}

void exynos_pcie_phy_config(void *phy_base_regs, void *phy_pcs_base_regs, void *sysreg_base_regs, void *elbi_bsae_regs, int ch_num)
{
	/* 26MHz gen1 */
	u32 cmn_config_val[26] = {0x01, 0x0F, 0xA6, 0x31, 0x90, 0x62, 0x20, 0x00, 0x00, 0xA7, 0x0A,
				  0x37, 0x20, 0x08, 0xEF, 0xFC, 0x96, 0x14, 0x00, 0x10, 0x60, 0x01,
				  0x00, 0x00, 0x04, 0x10};
	u32 trsv_config_val[41] = {0x31, 0xF4, 0xF4, 0x80, 0x25, 0x40, 0xD8, 0x03, 0x35, 0x55, 0x4C,
				   0xC3, 0x10, 0x54, 0x70, 0xC5, 0x00, 0x2F, 0x38, 0xA4, 0x00, 0x3B,
				   0x30, 0x9A, 0x64, 0x00, 0x1F, 0x83, 0x1B, 0x01, 0xE0, 0x00, 0x00,
				   0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1D, 0x00};
	int i;

	writel(readl(sysreg_base_regs) & ~(0x1 << 1), sysreg_base_regs);
	writel((((readl(sysreg_base_regs + 0xC) & ~(0xf << 4)) & ~(0xf << 2)) | (0x3 << 2)) & ~(0x1 << 1), sysreg_base_regs + 0xC);

	/* pcs_g_rst */
	writel(0x1, elbi_bsae_regs + 0x288);
	udelay(10);
	writel(0x0, elbi_bsae_regs + 0x288);
	udelay(10);
	writel(0x1, elbi_bsae_regs + 0x288);
	udelay(10);

	/* PHY Common block Setting */
	for (i = 0; i < 26; i++)
		writel(cmn_config_val[i], phy_base_regs + (i * 4));

	/* PHY Tranceiver/Receiver block Setting */
	for (i = 0; i < 41; i++)
		writel(trsv_config_val[i], phy_base_regs + ((0x30 + i) * 4));

#if IS_ENABLED(CONFIG_EXYNOS_OTP)
	/* PHY OTP Tuning bit configuration Setting */
	exynos_pcie_phy_otp_config(phy_base_regs, ch_num);
#endif

	/* tx amplitude control */
	writel(0x14, phy_base_regs + (0x5C * 4));

	/* tx latency */
	writel(0x70, phy_pcs_base_regs + 0xF8);

	/* PRGM_TIMEOUT_L1SS_VAL Setting */
	writel(readl(phy_pcs_base_regs + 0xC) | (0x1 << 4), phy_pcs_base_regs + 0xC);

	/* PCIE_MAC CMN_RST */
	writel(0x1, elbi_bsae_regs + 0x290);
	udelay(10);
	writel(0x0, elbi_bsae_regs + 0x290);
	udelay(10);
	writel(0x1, elbi_bsae_regs + 0x290);
	udelay(10);

	/* PCIE_PHY PCS&PMA(CMN)_RST */
	writel(0x1, elbi_bsae_regs + 0x28C);
	udelay(10);
	writel(0x0, elbi_bsae_regs + 0x28C);
	udelay(10);
	writel(0x1, elbi_bsae_regs + 0x28C);
	udelay(10);
}
