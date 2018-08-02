/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Author: Sung-Hyun Na <sunghyun.na@samsung.com>
 * Author: Minho Lee <minho55.lee@samsung.com>
 *
 * Chip Abstraction Layer for USB PHY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __EXCITE__
#include <linux/delay.h>
#include <linux/io.h>
#endif

#include <linux/platform_device.h>

#include "phy-samsung-usb-cal.h"
#include "phy-samsung-usb3-cal.h"

enum exynos_usbcon_cr {
	USBCON_CR_ADDR = 0,
	USBCON_CR_DATA = 1,
	USBCON_CR_READ = 18,
	USBCON_CR_WRITE = 19,
};

static u16 samsung_exynos_cal_cr_access(struct exynos_usbphy_info *usbphy_info,
		enum exynos_usbcon_cr cr_bit, u16 data)
{
	void __iomem *regs_base = usbphy_info->regs_base;
	u32 phyreg0;
	u32 phyreg1;
	u32 loop;
	u32 loop_cnt;

	/* Clear CR port register */
	phyreg0 = readl(regs_base + EXYNOS_USBCON_PHYREG0);
	phyreg0 &= ~(0xfffff);
	writel(phyreg0, regs_base + EXYNOS_USBCON_PHYREG0);

	/* Set Data for cr port */
	phyreg0 &= ~PHYREG0_CR_DATA_MASK;
	phyreg0 |= PHYREG0_CR_DATA_IN(data);
	writel(phyreg0, regs_base + EXYNOS_USBCON_PHYREG0);

	if (cr_bit == USBCON_CR_ADDR)
		loop = 1;
	else
		loop = 2;

	for (loop_cnt = 0; loop_cnt < loop; loop_cnt++) {
		u32 trigger_bit = 0;
		u32 handshake_cnt = 2;
		/* Trigger cr port */
		if (cr_bit == USBCON_CR_ADDR)
			trigger_bit = PHYREG0_CR_CR_CAP_ADDR;
		else {
			if (loop_cnt == 0)
				trigger_bit = PHYREG0_CR_CR_CAP_DATA;
			else {
				if (cr_bit == USBCON_CR_READ)
					trigger_bit = PHYREG0_CR_READ;
				else
					trigger_bit = PHYREG0_CR_WRITE;
			}
		}
		/* Handshake Procedure */
		do {
			u32 usec = 100;

			if (handshake_cnt == 2)
				phyreg0 |= trigger_bit;
			else
				phyreg0 &= ~trigger_bit;
			writel(phyreg0, regs_base + EXYNOS_USBCON_PHYREG0);

			/* Handshake */
			do {
				phyreg1 = readl(regs_base +
						EXYNOS_USBCON_PHYREG1);
				if ((handshake_cnt == 2)
						&& (phyreg1 & PHYREG1_CR_ACK))
					break;
				else if ((handshake_cnt == 1)
						&& !(phyreg1 & PHYREG1_CR_ACK))
					break;

				udelay(1);
			} while (usec-- > 0);

			if (!usec)
				pr_err("CRPORT handshake timeout1 (0x%08x)\n",
						phyreg0);

			udelay(5);
			handshake_cnt--;
		} while (handshake_cnt != 0);
		udelay(50);
	}
	return (u16) ((phyreg1 & PHYREG1_CR_DATA_OUT_MASK) >> 1);
}

void samsung_exynos_cal_cr_write(struct exynos_usbphy_info *usbphy_info,
		u16 addr, u16 data)
{
	samsung_exynos_cal_cr_access(usbphy_info, USBCON_CR_ADDR, addr);
	samsung_exynos_cal_cr_access(usbphy_info, USBCON_CR_WRITE, data);
}

u16 samsung_exynos_cal_cr_read(struct exynos_usbphy_info *usbphy_info, u16 addr)
{
	samsung_exynos_cal_cr_access(usbphy_info, USBCON_CR_ADDR, addr);
	return samsung_exynos_cal_cr_access(usbphy_info, USBCON_CR_READ, 0);
}

void samsung_exynos_cal_usb3phy_set_cr_port(
		struct exynos_usbphy_info *usbphy_info)
{
	if (usbphy_info->ss_tune) {
		u16 reg;

		/* Enable override los_bias, los_level and
		 * tx_vboost_lvl, Set los_bias to 0x5 and
		 * los_level to 0x9 */
		samsung_exynos_cal_cr_write(usbphy_info, 0x15, 0xA409);
		/* Set TX_VBOOST_LEVLE to default Value (0x4) */
		reg = usbphy_info->ss_tune->tx_boost_level << 13;
		samsung_exynos_cal_cr_write(usbphy_info, 0x12, reg);

		/* to set the charge pump proportional current */
		if (usbphy_info->ss_tune->set_crport_mpll_charge_pump)
			samsung_exynos_cal_cr_write(usbphy_info, 0x30, 0xC0);
		reg = usbphy_info->ss_tune->tx_deemphasis_3p5db << 7;
		reg |= 0x407f;
		samsung_exynos_cal_cr_write(usbphy_info, 0x1002, reg);
	}
	/* Set RXDET_MEAS_TIME[11:4] each reference clock */
	samsung_exynos_cal_cr_write(usbphy_info, 0x1010, 0x80);
}

void samsung_exynos_cal_usb3phy_tune_fix_rxeq(
		struct exynos_usbphy_info *usbphy_info)
{
	u16 reg;
	struct exynos_usbphy_ss_tune *tune = usbphy_info->ss_tune;

	reg = samsung_exynos_cal_cr_read(usbphy_info, 0x1006);
	reg &= ~(1 << 6);
	samsung_exynos_cal_cr_write(usbphy_info, 0x1006, reg);

	udelay(10);
	reg |= (1 << 7);
	samsung_exynos_cal_cr_write(usbphy_info, 0x1006, reg);

	udelay(10);

	reg |= (tune->fix_rxeq_value << 8);
	samsung_exynos_cal_cr_write(usbphy_info, 0x1006, reg);

	udelay(10);
	reg |= (1 << 11);
	samsung_exynos_cal_cr_write(usbphy_info, 0x1006, reg);
}

void samsung_exynos_cal_usb3phy_enable(struct exynos_usbphy_info *usbphy_info)
{
	void __iomem *regs_base = usbphy_info->regs_base;
	u32 version = usbphy_info->version;
	enum exynos_usbphy_refclk refclkfreq = usbphy_info->refclk;
	struct exynos_usbphy_ss_tune *tune = usbphy_info->ss_tune;
	u32 phyutmi;
	u32 phyclkrst;
	u32 phyparam0;
	u32 phyreg0;
	u32 phypipe;
	u32 linkport;

	/* Set force q-channel */
	if ((version & 0xf) >= 0x01) {
		u32 phy_resume;

		/* WA for Q-channel: disable all q-act from usb */
		phy_resume = readl(regs_base + EXYNOS_USBCON_PHYRESUME);
		phy_resume |= PHYRESUME_DIS_ID0_QACT;
		phy_resume |= PHYRESUME_DIS_VBUSVALID_QACT;
		phy_resume |= PHYRESUME_DIS_BVALID_QACT;
		phy_resume |= PHYRESUME_DIS_LINKGATE_QACT;
		phy_resume &= ~PHYRESUME_FORCE_QACT;
		udelay(500);
		writel(phy_resume, regs_base + EXYNOS_USBCON_PHYRESUME);

		udelay(500);

		phy_resume = readl(regs_base + EXYNOS_USBCON_PHYRESUME);
		phy_resume |= PHYRESUME_FORCE_QACT;
		udelay(500);
		writel(phy_resume, regs_base + EXYNOS_USBCON_PHYRESUME);
	}

	/* set phy clock & control HS phy */
	phyclkrst = readl(regs_base + EXYNOS_USBCON_PHYCLKRST);

	/* assert port_reset */
	phyclkrst |= PHYCLKRST_PORTRESET;

	/* Select Reference clock source path */
	phyclkrst &= ~PHYCLKRST_REFCLKSEL_MASK;
	phyclkrst |= PHYCLKRST_REFCLKSEL(usbphy_info->refsel);

	/* Select ref clk */
	phyclkrst &= ~PHYCLKRST_FSEL_MASK;
	phyclkrst |= PHYCLKRST_FSEL(refclkfreq & 0x3f);

	/* Enable Common Block in suspend/sleep mode */
	phyclkrst &= ~PHYCLKRST_COMMONONN;

	/* Additional control for 3.0 PHY */
	if ((EXYNOS_USBCON_VER_01_0_0 <= version)
			&& (version <= EXYNOS_USBCON_VER_01_MAX)) {
		if (version < EXYNOS_USBCON_VER_01_0_1)
			phyclkrst |= PHYCLKRST_COMMONONN;

		/* Disable Common block control by link */
		phyclkrst &= ~PHYCLKRST_EN_UTMISUSPEND;

		/* Digital Supply Mode : normal operating mode */
		phyclkrst |= PHYCLKRST_RETENABLEN;

		/* ref. clock enable for ss function */
		phyclkrst |= PHYCLKRST_REF_SSP_EN;

		phyparam0 = readl(regs_base + EXYNOS_USBCON_PHYPARAM0);
		if (usbphy_info->refsel == USBPHY_REFSEL_DIFF_PAD)
			phyparam0 |= PHYPARAM0_REF_USE_PAD;
		else
			phyparam0 &= ~PHYPARAM0_REF_USE_PAD;
		writel(phyparam0, regs_base + EXYNOS_USBCON_PHYPARAM0);

		if (version >= EXYNOS_USBCON_VER_01_0_1)
			phyreg0 = readl(regs_base + EXYNOS_USBCON_PHYREG0);

		switch (refclkfreq) {
		case USBPHY_REFCLK_DIFF_100MHZ:
			phyclkrst &= ~PHYCLKRST_MPLL_MULTIPLIER_MASK;
			phyclkrst |= PHYCLKRST_MPLL_MULTIPLIER(0x00);
			phyclkrst &= ~PHYCLKRST_REF_CLKDIV2;
			if (version == EXYNOS_USBCON_VER_01_0_1) {
				phyreg0 &= ~PHYREG0_SSC_REFCLKSEL_MASK;
				phyreg0 |= PHYREG0_SSC_REFCLKSEL(0x00);
			} else {
				phyclkrst &= ~PHYCLKRST_SSC_REFCLKSEL_MASK;
				phyclkrst |= PHYCLKRST_SSC_REFCLKSEL(0x00);
			}
			break;
		case USBPHY_REFCLK_DIFF_26MHZ:
			phyclkrst &= ~PHYCLKRST_MPLL_MULTIPLIER_MASK;
			phyclkrst |= PHYCLKRST_MPLL_MULTIPLIER(0x60);
			phyclkrst &= ~PHYCLKRST_REF_CLKDIV2;
			if (version == EXYNOS_USBCON_VER_01_0_1) {
				phyreg0 &= ~PHYREG0_SSC_REFCLKSEL_MASK;
				phyreg0 |= PHYREG0_SSC_REFCLKSEL(0x108);
			} else {
				phyclkrst &= ~PHYCLKRST_SSC_REFCLKSEL_MASK;
				phyclkrst |= PHYCLKRST_SSC_REFCLKSEL(0x108);
			}
			break;
		case USBPHY_REFCLK_DIFF_24MHZ:
		case USBPHY_REFCLK_EXT_24MHZ:
			phyclkrst &= ~PHYCLKRST_MPLL_MULTIPLIER_MASK;
			phyclkrst |= PHYCLKRST_MPLL_MULTIPLIER(0x00);
			phyclkrst &= ~PHYCLKRST_REF_CLKDIV2;
			if (version != EXYNOS_USBCON_VER_01_0_1) {
				phyclkrst &= ~PHYCLKRST_SSC_REFCLKSEL_MASK;
				phyclkrst |= PHYCLKRST_SSC_REFCLKSEL(0x88);
			} else {
				phyreg0 &= ~PHYREG0_SSC_REFCLKSEL_MASK;
				phyreg0 |= PHYREG0_SSC_REFCLKSEL(0x88);
			}
			break;
		default:
			break;
		}
		/* SSC Enable */
		if (tune->enable_ssc)
			phyclkrst |= PHYCLKRST_SSC_EN;
		else
			phyclkrst &= ~PHYCLKRST_SSC_EN;

		if (version != EXYNOS_USBCON_VER_01_0_1) {
			phyclkrst &= ~PHYCLKRST_SSC_RANGE_MASK;
			phyclkrst |= PHYCLKRST_SSC_RANGE(tune->ssc_range);
		} else {
			phyreg0 &= ~PHYREG0_SSC_RANGE_MASK;
			phyreg0 |= PHYREG0_SSC_RAMGE(tune->ssc_range);
		}

		/* Select UTMI CLOCK 0 : PHY CLOCK, 1 : FREE CLOCK */
		phypipe = readl(regs_base + EXYNOS_USBCON_PHYPIPE);
		phypipe |= PHY_CLOCK_SEL;
		writel(phypipe, regs_base + EXYNOS_USBCON_PHYPIPE);

		if (version >= EXYNOS_USBCON_VER_01_0_1)
			writel(phyreg0, regs_base + EXYNOS_USBCON_PHYREG0);
	} else if ((EXYNOS_USBCON_VER_02_0_0 <= version)
			&& (version <= EXYNOS_USBCON_VER_02_MAX)) {
		u32 hsphyplltune = readl(
				regs_base + EXYNOS_USBCON_HSPHYPLLTUNE);

		if ((version & 0xf0) >= 0x10) {
			/* Disable Common block control by link */
			phyclkrst |= PHYCLKRST_EN_UTMISUSPEND;
			phyclkrst |= PHYCLKRST_COMMONONN;
		} else {
			phyclkrst |= PHYCLKRST_EN_UTMISUSPEND;
			phyclkrst |= PHYCLKRST_COMMONONN;
		}

		/* Change PHY PLL Tune value */
		if (refclkfreq == USBPHY_REFCLK_EXT_24MHZ)
			hsphyplltune |= HSPHYPLLTUNE_PLL_B_TUNE;
		else
			hsphyplltune &= ~HSPHYPLLTUNE_PLL_B_TUNE;
		hsphyplltune |= HSPHYPLLTUNE_PLL_P_TUNE(0xe);
		writel(hsphyplltune, regs_base + EXYNOS_USBCON_HSPHYPLLTUNE);
	}
	writel(phyclkrst, regs_base + EXYNOS_USBCON_PHYCLKRST);
	udelay(10);

	phyclkrst &= ~PHYCLKRST_PORTRESET;
	writel(phyclkrst, regs_base + EXYNOS_USBCON_PHYCLKRST);

	if ((EXYNOS_USBCON_VER_01_0_0 <= version)
			&& (version <= EXYNOS_USBCON_VER_01_MAX)) {
		u32 phytest;

		phytest = readl(regs_base + EXYNOS_USBCON_PHYTEST);
		phytest &= ~PHYTEST_POWERDOWN_HSP;
		phytest &= ~PHYTEST_POWERDOWN_SSP;
		writel(phytest, regs_base + EXYNOS_USBCON_PHYTEST);
	} else if (version >= EXYNOS_USBCON_VER_02_0_0
			&& version <= EXYNOS_USBCON_VER_02_MAX) {
		u32 hsphyctrl;

		hsphyctrl = readl(regs_base + EXYNOS_USBCON_HSPHYCTRL);
		hsphyctrl &= ~HSPHYCTRL_SIDDQ;
		hsphyctrl &= ~HSPHYCTRL_PHYSWRST;
		hsphyctrl &= ~HSPHYCTRL_PHYSWRSTALL;
		writel(hsphyctrl, regs_base + EXYNOS_USBCON_HSPHYCTRL);
	}
	udelay(500);


	/* Set VBUSVALID signal if VBUS pad is not used */
	if (usbphy_info->not_used_vbus_pad) {
		u32 linksystem;

		linksystem = readl(regs_base + EXYNOS_USBCON_LINKSYSTEM);
		linksystem |= LINKSYSTEM_FORCE_BVALID;
		linksystem |= LINKSYSTEM_FORCE_VBUSVALID;
		writel(linksystem, regs_base + EXYNOS_USBCON_LINKSYSTEM);
	}

	/* release force_sleep & force_suspend */
	phyutmi = readl(regs_base + EXYNOS_USBCON_PHYUTMI);
	phyutmi &= ~PHYUTMI_FORCESLEEP;
	phyutmi &= ~PHYUTMI_FORCESUSPEND;

	/* DP/DM Pull Down Control */
	phyutmi &= ~PHYUTMI_DMPULLDOWN;
	phyutmi &= ~PHYUTMI_DPPULLDOWN;

	/* Set DP-Pull up if VBUS pad is not used */
	if (usbphy_info->not_used_vbus_pad) {
		phyutmi |= PHYUTMI_VBUSVLDEXTSEL;
		phyutmi |= PHYUTMI_VBUSVLDEXT;
	}

	/* disable OTG block and VBUS valid comparator */
	phyutmi &= ~PHYUTMI_DRVVBUS;
	phyutmi |= PHYUTMI_OTGDISABLE;
	writel(phyutmi, regs_base + EXYNOS_USBCON_PHYUTMI);

	/* OVC io usage */
	linkport = readl(regs_base + EXYNOS_USBCON_LINKPORT);
	if (usbphy_info->use_io_for_ovc) {
		linkport &= ~LINKPORT_HOST_PORT_OVCR_U3_SEL;
		linkport &= ~LINKPORT_HOST_PORT_OVCR_U2_SEL;
	} else {
		linkport |= LINKPORT_HOST_PORT_OVCR_U3_SEL;
		linkport |= LINKPORT_HOST_PORT_OVCR_U2_SEL;
	}
	writel(linkport, regs_base + EXYNOS_USBCON_LINKPORT);

	if ((EXYNOS_USBCON_VER_02_0_0 <= version)
				&& (version <= EXYNOS_USBCON_VER_02_MAX)) {

		u32 hsphyctrl;

		hsphyctrl = readl(regs_base + EXYNOS_USBCON_HSPHYCTRL);
		hsphyctrl |= HSPHYCTRL_PHYSWRST;
		writel(hsphyctrl, regs_base + EXYNOS_USBCON_HSPHYCTRL);
		udelay(20);
		hsphyctrl = readl(regs_base + EXYNOS_USBCON_HSPHYCTRL);
		hsphyctrl &= ~HSPHYCTRL_PHYSWRST;
		writel(hsphyctrl, regs_base + EXYNOS_USBCON_HSPHYCTRL);
	}
}

void samsung_exynos_cal_usb3phy_late_enable(
		struct exynos_usbphy_info *usbphy_info)
{
	u32 version = usbphy_info->version;
	struct exynos_usbphy_ss_tune *tune = usbphy_info->ss_tune;

	if (EXYNOS_USBCON_VER_01_0_0 <= version
			&& version <= EXYNOS_USBCON_VER_01_MAX) {
		samsung_exynos_cal_usb3phy_set_cr_port(usbphy_info);

		if (tune->enable_fixed_rxeq_mode)
			samsung_exynos_cal_usb3phy_tune_fix_rxeq(usbphy_info);
	}
}

void samsung_exynos_cal_usb3phy_disable(struct exynos_usbphy_info *usbphy_info)
{
	void __iomem *regs_base = usbphy_info->regs_base;
	u32 version = usbphy_info->version;
	u32 phyutmi;
	u32 phyclkrst;

	phyclkrst = readl(regs_base + EXYNOS_USBCON_PHYCLKRST);
	phyclkrst |= PHYCLKRST_EN_UTMISUSPEND;
	phyclkrst |= PHYCLKRST_COMMONONN;
	phyclkrst |= PHYCLKRST_RETENABLEN;
	phyclkrst &= ~PHYCLKRST_REF_SSP_EN;
	phyclkrst &= ~PHYCLKRST_SSC_EN;
	/* Select Reference clock source path */
	phyclkrst &= ~PHYCLKRST_REFCLKSEL_MASK;
	phyclkrst |= PHYCLKRST_REFCLKSEL(usbphy_info->refsel);

	/* Select ref clk */
	phyclkrst &= ~PHYCLKRST_FSEL_MASK;
	phyclkrst |= PHYCLKRST_FSEL(usbphy_info->refclk & 0x3f);
	writel(phyclkrst, regs_base + EXYNOS_USBCON_PHYCLKRST);

	phyutmi = readl(regs_base + EXYNOS_USBCON_PHYUTMI);
	phyutmi &= ~PHYUTMI_IDPULLUP;
	phyutmi &= ~PHYUTMI_DRVVBUS;
	phyutmi |= PHYUTMI_FORCESUSPEND;
	phyutmi |= PHYUTMI_FORCESLEEP;
	if (usbphy_info->not_used_vbus_pad) {
		phyutmi &= ~PHYUTMI_VBUSVLDEXTSEL;
		phyutmi &= ~PHYUTMI_VBUSVLDEXT;
	}
	writel(phyutmi, regs_base + EXYNOS_USBCON_PHYUTMI);

	if ((EXYNOS_USBCON_VER_01_0_0 <= version)
			&& (version <= EXYNOS_USBCON_VER_01_MAX)) {
		u32 phytest;

		phytest = readl(regs_base + EXYNOS_USBCON_PHYTEST);
		phytest |= PHYTEST_POWERDOWN_HSP;
		phytest |= PHYTEST_POWERDOWN_SSP;
		writel(phytest, regs_base + EXYNOS_USBCON_PHYTEST);
	} else if (version >= EXYNOS_USBCON_VER_02_0_0
			&& version <= EXYNOS_USBCON_VER_02_MAX) {
		u32 hsphyctrl;

		hsphyctrl = readl(regs_base + EXYNOS_USBCON_HSPHYCTRL);
		hsphyctrl |= HSPHYCTRL_SIDDQ;
		writel(hsphyctrl, regs_base + EXYNOS_USBCON_HSPHYCTRL);
	}

	/* Clear VBUSVALID signal if VBUS pad is not used */
	if (usbphy_info->not_used_vbus_pad) {
		u32 linksystem;

		linksystem = readl(regs_base + EXYNOS_USBCON_LINKSYSTEM);
		linksystem &= ~LINKSYSTEM_FORCE_BVALID;
		linksystem &= ~LINKSYSTEM_FORCE_VBUSVALID;
		writel(linksystem, regs_base + EXYNOS_USBCON_LINKSYSTEM);
	}

	/* Set force q-channel */
	if ((version & 0xf) >= 0x01) {
		u32 phy_resume;

		phy_resume = readl(regs_base + EXYNOS_USBCON_PHYRESUME);
		phy_resume &= ~PHYRESUME_FORCE_QACT;
		phy_resume |= PHYRESUME_DIS_ID0_QACT;
		phy_resume |= PHYRESUME_DIS_VBUSVALID_QACT;
		phy_resume |= PHYRESUME_DIS_BVALID_QACT;
		phy_resume |= PHYRESUME_DIS_LINKGATE_QACT;
		writel(phy_resume, regs_base + EXYNOS_USBCON_PHYRESUME);
	}
}

void samsung_exynos_cal_usb3phy_config_host_mode(
		struct exynos_usbphy_info *usbphy_info)
{
	void __iomem *regs_base = usbphy_info->regs_base;
	u32 phyutmi;

	phyutmi = readl(regs_base + EXYNOS_USBCON_PHYUTMI);
	phyutmi |= PHYUTMI_DMPULLDOWN;
	phyutmi |= PHYUTMI_DPPULLDOWN;
	phyutmi &= ~PHYUTMI_VBUSVLDEXTSEL;
	phyutmi &= ~PHYUTMI_VBUSVLDEXT;
	writel(phyutmi, regs_base + EXYNOS_USBCON_PHYUTMI);
}

void samsung_exynos_cal_usb3phy_enable_dp_pullup(
		struct exynos_usbphy_info *usbphy_info)
{
	void __iomem *regs_base = usbphy_info->regs_base;
	u32 phyutmi;

	phyutmi = readl(regs_base + EXYNOS_USBCON_PHYUTMI);
	phyutmi |= PHYUTMI_VBUSVLDEXT;
	writel(phyutmi, regs_base + EXYNOS_USBCON_PHYUTMI);
}

void samsung_exynos_cal_usb3phy_disable_dp_pullup(
		struct exynos_usbphy_info *usbphy_info)
{
	void __iomem *regs_base = usbphy_info->regs_base;
	u32 phyutmi;

	phyutmi = readl(regs_base + EXYNOS_USBCON_PHYUTMI);
	phyutmi &= ~PHYUTMI_VBUSVLDEXT;
	writel(phyutmi, regs_base + EXYNOS_USBCON_PHYUTMI);
}

void samsung_exynos_cal_usb3phy_tune_dev(struct exynos_usbphy_info *usbphy_info)
{
	void __iomem *regs_base = usbphy_info->regs_base;
	u32 linksystem;
	u32 phyparam0;
	u32 phyparam1;
	u32 phyparam2;
	u32 phypcsval;

	/* Set the LINK Version Control and Frame Adjust Value */
	linksystem = readl(regs_base + EXYNOS_USBCON_LINKSYSTEM);
	linksystem &= ~LINKSYSTEM_FLADJ_MASK;
	linksystem |= LINKSYSTEM_FLADJ(0x20);
	linksystem |= LINKSYSTEM_XHCI_VERSION_CONTROL;
	writel(linksystem, regs_base + EXYNOS_USBCON_LINKSYSTEM);

	/* Tuning the HS Block of phy */
	if (usbphy_info->hs_tune) {
		struct exynos_usbphy_hs_tune *tune = usbphy_info->hs_tune;

		/* Set tune value for 2.0(HS/FS) function */
		phyparam0 = readl(regs_base + EXYNOS_USBCON_PHYPARAM0);
		/* TX VREF TUNE */
		phyparam0 &= ~PHYPARAM0_TXVREFTUNE_MASK;
		phyparam0 |= PHYPARAM0_TXVREFTUNE(tune->tx_vref);
		/* TX RISE TUNE */
		phyparam0 &= ~PHYPARAM0_TXRISETUNE_MASK;
		phyparam0 |= PHYPARAM0_TXRISETUNE(tune->tx_rise);
		/* TX RES TUNE */
		phyparam0 &= ~PHYPARAM0_TXRESTUNE_MASK;
		phyparam0 |= PHYPARAM0_TXRESTUNE(tune->tx_res);
		/* TX PRE EMPHASIS PLUS */
		if (tune->tx_pre_emp_plus)
			phyparam0 |= PHYPARAM0_TXPREEMPPULSETUNE;
		else
			phyparam0 &= ~PHYPARAM0_TXPREEMPPULSETUNE;
		/* TX PRE EMPHASIS */
		phyparam0 &= ~PHYPARAM0_TXPREEMPAMPTUNE_MASK;
		phyparam0 |= PHYPARAM0_TXPREEMPAMPTUNE(tune->tx_pre_emp);
		/* TX HS XV TUNE */
		phyparam0 &= ~PHYPARAM0_TXHSXVTUNE_MASK;
		phyparam0 |= PHYPARAM0_TXHSXVTUNE(tune->tx_hsxv);
		/* TX FSLS TUNE */
		phyparam0 &= ~PHYPARAM0_TXFSLSTUNE_MASK;
		phyparam0 |= PHYPARAM0_TXFSLSTUNE(tune->tx_fsls);
		/* RX SQ TUNE */
		phyparam0 &= ~PHYPARAM0_SQRXTUNE_MASK;
		phyparam0 |= PHYPARAM0_SQRXTUNE(tune->rx_sqrx);
		/* OTG TUNE */
		phyparam0 &= ~PHYPARAM0_OTGTUNE_MASK;
		phyparam0 |= PHYPARAM0_OTGTUNE(tune->otg);
		/* COM DIS TUNE */
		phyparam0 &= ~PHYPARAM0_COMPDISTUNE_MASK;
		phyparam0 |= PHYPARAM0_COMPDISTUNE(tune->compdis);

		writel(phyparam0, regs_base + EXYNOS_USBCON_PHYPARAM0);
	}

	/* Tuning the SS Block of phy */
	if (usbphy_info->ss_tune) {
		struct exynos_usbphy_ss_tune *tune = usbphy_info->ss_tune;

		/* Set the PHY Signal Quality Tuning Value */
		phyparam1 = readl(regs_base + EXYNOS5_USBCON_PHYPARAM1);
		/* TX SWING FULL */
		phyparam1 &= ~PHYPARAM1_PCS_TXSWING_FULL_MASK;
		phyparam1 |= PHYPARAM1_PCS_TXSWING_FULL(tune->tx_swing_full);
		/* TX DE EMPHASIS 3.5 dB */
		phyparam1 &= ~PHYPARAM1_PCS_TXDEEMPH_3P5DB_MASK;
		phyparam1 |= PHYPARAM1_PCS_TXDEEMPH_3P5DB(
				tune->tx_deemphasis_3p5db);
		writel(phyparam1, regs_base + EXYNOS5_USBCON_PHYPARAM1);

		/* Set vboost value for eye diagram */
		phyparam2 = readl(regs_base + EXYNOS_USBCON_PHYPARAM2);
		/* TX VBOOST Value */
		phyparam2 &= ~PHYPARAM2_TX_VBOOST_LVL_MASK;
		phyparam2 |= PHYPARAM2_TX_VBOOST_LVL(tune->tx_boost_level);
		/* LOS BIAS */
		phyparam2 &= ~PHYPARAM2_LOS_BIAS_MASK;
		phyparam2 |= PHYPARAM2_LOS_BIAS(tune->los_bias);
		writel(phyparam2, regs_base + EXYNOS_USBCON_PHYPARAM2);

		/*
		 * Set pcs_rx_los_mask_val for 14nm PHY to mask the abnormal
		 * LFPS and glitches
		 */
		phypcsval = readl(regs_base + EXYNOS_USBCON_PHYPCSVAL);
		phypcsval &= ~PHYPCSVAL_PCS_RX_LOS_MASK_VAL_MASK;
		phypcsval |= PHYPCSVAL_PCS_RX_LOS_MASK_VAL(tune->los_mask_val);
		writel(phypcsval, regs_base + EXYNOS_USBCON_PHYPCSVAL);
	}
}

void samsung_exynos_cal_usb3phy_tune_host(
		struct exynos_usbphy_info *usbphy_info)
{
	void __iomem *regs_base = usbphy_info->regs_base;
	u32 linksystem;
	u32 phyparam0;
	u32 phyparam1;
	u32 phyparam2;
	u32 phypcsval;

	/* Set the LINK Version Control and Frame Adjust Value */
	linksystem = readl(regs_base + EXYNOS_USBCON_LINKSYSTEM);
	linksystem &= ~LINKSYSTEM_FLADJ_MASK;
	linksystem |= LINKSYSTEM_FLADJ(0x20);
	linksystem |= LINKSYSTEM_XHCI_VERSION_CONTROL;
	writel(linksystem, regs_base + EXYNOS_USBCON_LINKSYSTEM);

	/* Tuning the HS Block of phy */
	if (usbphy_info->hs_tune) {
		struct exynos_usbphy_hs_tune *tune = usbphy_info->hs_tune;

		/* Set tune value for 2.0(HS/FS) function */
		phyparam0 = readl(regs_base + EXYNOS_USBCON_PHYPARAM0);
		/* TX VREF TUNE */
		phyparam0 &= ~PHYPARAM0_TXVREFTUNE_MASK;
		phyparam0 |= PHYPARAM0_TXVREFTUNE(tune->tx_vref);
		/* TX RISE TUNE */
		phyparam0 &= ~PHYPARAM0_TXRISETUNE_MASK;
		phyparam0 |= PHYPARAM0_TXRISETUNE(tune->tx_rise);
		/* TX RES TUNE */
		phyparam0 &= ~PHYPARAM0_TXRESTUNE_MASK;
		phyparam0 |= PHYPARAM0_TXRESTUNE(tune->tx_res);
		/* TX PRE EMPHASIS PLUS */
		if (tune->tx_pre_emp_plus)
			phyparam0 |= PHYPARAM0_TXPREEMPPULSETUNE;
		else
			phyparam0 &= ~PHYPARAM0_TXPREEMPPULSETUNE;
		/* TX PRE EMPHASIS */
		phyparam0 &= ~PHYPARAM0_TXPREEMPAMPTUNE_MASK;
		phyparam0 |= PHYPARAM0_TXPREEMPAMPTUNE(tune->tx_pre_emp);
		/* TX HS XV TUNE */
		phyparam0 &= ~PHYPARAM0_TXHSXVTUNE_MASK;
		phyparam0 |= PHYPARAM0_TXHSXVTUNE(tune->tx_hsxv);
		/* TX FSLS TUNE */
		phyparam0 &= ~PHYPARAM0_TXFSLSTUNE_MASK;
		phyparam0 |= PHYPARAM0_TXFSLSTUNE(tune->tx_fsls);
		/* RX SQ TUNE */
		phyparam0 &= ~PHYPARAM0_SQRXTUNE_MASK;
		phyparam0 |= PHYPARAM0_SQRXTUNE(tune->rx_sqrx);
		/* OTG TUNE */
		phyparam0 &= ~PHYPARAM0_OTGTUNE_MASK;
		phyparam0 |= PHYPARAM0_OTGTUNE(tune->otg);
		/* COM DIS TUNE */
		phyparam0 &= ~PHYPARAM0_COMPDISTUNE_MASK;
		phyparam0 |= PHYPARAM0_COMPDISTUNE(tune->compdis);

		writel(phyparam0, regs_base + EXYNOS_USBCON_PHYPARAM0);
	}

	/* Tuning the SS Block of phy */
	if (usbphy_info->ss_tune) {
		struct exynos_usbphy_ss_tune *tune = usbphy_info->ss_tune;

		/* Set the PHY Signal Quality Tuning Value */
		phyparam1 = readl(regs_base + EXYNOS5_USBCON_PHYPARAM1);
		/* TX SWING FULL */
		phyparam1 &= ~PHYPARAM1_PCS_TXSWING_FULL_MASK;
		phyparam1 |= PHYPARAM1_PCS_TXSWING_FULL(tune->tx_swing_full);
		/* TX DE EMPHASIS 3.5 dB */
		phyparam1 &= ~PHYPARAM1_PCS_TXDEEMPH_3P5DB_MASK;
		phyparam1 |= PHYPARAM1_PCS_TXDEEMPH_3P5DB(
				tune->tx_deemphasis_3p5db);
		writel(phyparam1, regs_base + EXYNOS5_USBCON_PHYPARAM1);

		/* Set vboost value for eye diagram */
		phyparam2 = readl(regs_base + EXYNOS_USBCON_PHYPARAM2);
		/* TX VBOOST Value */
		phyparam2 &= ~PHYPARAM2_TX_VBOOST_LVL_MASK;
		phyparam2 |= PHYPARAM2_TX_VBOOST_LVL(tune->tx_boost_level);
		/* LOS BIAS */
		phyparam2 &= ~PHYPARAM2_LOS_BIAS_MASK;
		phyparam2 |= PHYPARAM2_LOS_BIAS(tune->los_bias);
		writel(phyparam2, regs_base + EXYNOS_USBCON_PHYPARAM2);

		/*
		 * Set pcs_rx_los_mask_val for 14nm PHY to mask the abnormal
		 * LFPS and glitches
		 */
		phypcsval = readl(regs_base + EXYNOS_USBCON_PHYPCSVAL);
		phypcsval &= ~PHYPCSVAL_PCS_RX_LOS_MASK_VAL_MASK;
		phypcsval |= PHYPCSVAL_PCS_RX_LOS_MASK_VAL(tune->los_mask_val);
		writel(phypcsval, regs_base + EXYNOS_USBCON_PHYPCSVAL);
	}
}

void samsung_exynos_cal_usb3phy_write_register(
		struct exynos_usbphy_info *usbphy_info, u32 offset, u32 value)
{
	void __iomem *regs_base = usbphy_info->regs_base;

	writel(value, regs_base + offset);
}
