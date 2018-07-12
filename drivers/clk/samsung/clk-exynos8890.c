/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file contains clocks of Exynos8890.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <dt-bindings/clock/exynos8890.h>
#include "../../soc/samsung/pwrcal/S5E8890/S5E8890-vclk.h"
#include "composite.h"

#if defined(CONFIG_ECT)
#include <soc/samsung/ect_parser.h>
#endif

enum exynos8890_clks {
	none,

	oscclk = 1,

	/* number for mfc driver starts from 10 */
	mfc_hpm = 10, mfc_mfc, mfc_ppmu,

	/* number for mscl driver starts from 50 */
	mscl_mscl0 = 50, mscl_jpeg, mscl_mscl1, mscl_g2d, mscl_ppmu, mscl_bts,

	/* number for imem driver starts from 100 */
	gate_apm = 100, gate_sss, gate_gic400, gate_rtic, gate_mc, gate_intmem, gate_alv, gate_ppmu,

	/* number for peris driver starts from 150 */
	peris_sfr = 150, peris_hpm, peris_mct, wdt_mngs, wdt_apollo, sysreg_peris, monocnt_apbif, rtc_apbif, top_rtc, otp_con_top, peris_chipid, peris_tmu, peris_sysreg, peris_monocnt,

	/* number for peric0 driver starts from 200 */
	gate_hsi2c0 = 200, gate_hsi2c1, gate_hsi2c4, gate_hsi2c5, gate_hsi2c9, gate_hsi2c10, gate_hsi2c11, puart0, suart0, gate_adcif, gate_pwm, gate_sclk_pwm,

	/* number for peric1 driver starts from 250 */
	gate_hsi2c2 = 250, gate_hsi2c3, gate_hsi2c6, gate_hsi2c7, gate_hsi2c8, gate_hsi2c12, gate_hsi2c13, gate_hsi2c14,
	gate_uart1, gate_uart2, gate_uart3, gate_uart4, gate_uart5, suart1, suart2, suart3, suart4, suart5,
	gate_spi0, gate_spi1, gate_spi2, gate_spi3, gate_spi4, gate_spi5, gate_spi6, gate_spi7,
	sclk_peric1_spi0, sclk_peric1_spi1, sclk_peric1_spi2, sclk_peric1_spi3, sclk_peric1_spi4, sclk_peric1_spi5, sclk_peric1_spi6, sclk_peric1_spi7,
	gate_gpio_nfc, gate_gpio_touch, gate_gpio_fp, gate_gpio_ese, promise_int, promise_disp, ap2cp_mif_pll_out, gate_i2s1, gate_pcm1, gate_spdif,

	/* number for isp0 driver starts from 400 */
	gate_fimc_isp0 = 400, gate_fimc_tpu, isp0, isp0_tpu, isp0_trex, isp0_ppmu, isp0_bts, pxmxdx_isp0_pxl,

	/* number for isp1 driver starts from 450 */
	gate_fimc_isp1 = 450, isp1, isp1_ppmu, isp1_bts,

	/* number for isp sensor driver starts from 500 */
	isp_sensor0 = 500, isp_sensor1, isp_sensor2, isp_sensor3,

	/* number for cam0 driver starts from 550 */
	gate_csis0 = 550, gate_csis1, gate_fimc_bns, fimc_3aa0, fimc_3aa1, cam0_hpm, pxmxdx_csis0, pxmxdx_csis1, pxmxdx_csis2, pxmxdx_csis3,
	pxmxdx_3aa0, pxmxdx_3aa1, pxmxdx_trex, hs0_csis0_rx_byte, hs1_csis0_rx_byte, hs2_csis0_rx_byte, hs3_csis0_rx_byte, hs0_csis1_rx_byte, hs1_csis1_rx_byte, cam0_ppmu, cam0_bts,

	/* number for cam1 driver starts from 600 */
	gate_isp_cpu = 600, gate_csis2, gate_csis3, gate_fimc_vra, gate_mc_scaler, gate_i2c0_isp, gate_i2c1_isp, gate_i2c2_isp, gate_i2c3_isp, gate_wdt_isp,
	gate_mcuctl_isp, gate_uart_isp, gate_pdma_isp, gate_pwm_isp, gate_spi0_isp, gate_spi1_isp, isp_spi0, isp_spi1, isp_uart, gate_sclk_pwm_isp,
	gate_sclk_uart_isp, cam1_arm, cam1_vra, cam1_trex, cam1_bus, cam1_peri, cam1_csis2, cam1_csis3, cam1_scl, cam1_phy0_csis2, cam1_phy1_csis2,
	cam1_phy2_csis2, cam1_phy3_csis2, cam1_phy0_csis3, cam1_ppmu, cam1_bts,

	/* number for audio driver starts from 650 */
	gate_mi2s = 650, gate_pcm, gate_slimbus, gate_sclk_mi2s, d1_sclk_i2s, gate_sclk_pcm, d1_sclk_pcm, gate_sclk_slimbus, sclk_slimbus, sclk_cp_i2s,
	sclk_asrc, aud_pll, aud_cp, aud_lpass, aud_dma, aud_ppmu, aud_bts,

	/* number for fsys0 driver starts from 700 */
	gate_usbdrd30 = 700, gate_usbhost20, usbdrd30 = 703, sclk_fsys0_mmc0, ufsunipro20, phy24m, ufsunipro_cfg, gate_udrd30_phyclock, gate_udrd30_pipe, gate_ufs_tx0,
	gate_ufs_rx0, usbhost20_phyclock, usbhost20phy_ref = 715, fsys_200 = 719, fsys0_etr_usb, gate_mmc, gate_pdma0, gate_pdmas, gate_ufs_linkemedded,

	/* number for fsys1 driver starts from 750 */
	gate_ufs20_sdcard = 750, fsys1_hpm, fsys1_sclk_mmc2, ufsunipro20_sdcard, pcie_phy, sclk_ufsunipro_sdcard, ufs_link_sdcard_tx0, ufs_link_sdcard_rx0,
	pcie_wifi0_tx0, pcie_wifi0_rx0, pcie_wifi1_tx0, pcie_wifi1_rx0, wifi0_dig_refclk, wifi1_dig_refclk, gate_mmc2 =767, gate_sromc, gate_pciewifi0,
	gate_pciewifi1, fsys1_ppmu, fsys1_bts,

	/* number for g3d driver starts from 800 */
	gate_g3d = 800, gate_g3d_iram, g3d_bts, g3d_ppmu,

	/* number for disp0 driver starts from 850 */
	gate_decon0 = 850, gate_dsim0, gate_dsim1, gate_dsim2, gate_hdmi, gate_dp, gate_hpm_apbif_disp0, decon0_eclk0, decon0_vclk0, decon0_vclk1,
	decon0_eclk0_local, decon0_vclk0_local, decon0_vclk1_local, hdmi_audio, disp_pll,
	mipidphy0_rxclkesc0, mipidphy0_bitclkdiv8, mipidphy1_rxclkesc0, mipidphy1_bitclkdiv8, mipidphy2_rxclkesc0, mipidphy2_bitclkdiv8,
	dpphy_ch0_txd, dpphy_ch1_txd, dpphy_ch2_txd, dpphy_ch3_txd, dptx_phy_i_ref_clk_24m,
	mipi_dphy_m1s0, mipi_dphy_m4s0, mipi_dphy_m4s4, phyclk_hdmiphy_tmds_20b, phyclk_hdmiphy_pixel, mipidphy0_bitclkdiv2_user,
	mipidphy1_bitclkdiv2_user, mipidphy2_bitclkdiv2_user,

	/* number for disp1 driver starts from 900 */
	gate_decon1 = 900, gate_hpmdisp1, decon1_eclk0, decon1_eclk1, decon1_eclk0_local, decon1_eclk1_local,
	disp1_phyclk_mipidphy0_bitclkdiv2_user, disp1_phyclk_mipidphy1_bitclkdiv2_user,
	disp1_phyclk_mipidphy2_bitclkdiv2_user, disp1_phyclk_disp1_hdmiphy_pixel_clko_user, disp1_ppmu, disp1_bts,

	/* number for ccore driver starts from 950 */
	ccore_i2c = 950,

	/* number for clkout port starts from 1000 */
	oscclk_nfc = 1000, oscclk_aud,

	/* clk id for sysmmu: 1100 ~ 1149
	 * NOTE: clock IDs of sysmmus are defined in
	 * include/dt-bindings/clock/exynos8890.h
	 */
	sysmmu_last = 1149,

	/* number of dfs driver starts from 2000 */
	dfs_mif = 2000, dfs_mif_sw, dfs_int, dfs_cam, dfs_disp,

	nr_clks,
};

static struct samsung_fixed_rate exynos8890_fixed_rate_ext_clks[] __initdata = {
	FRATE(oscclk, "fin_pll", NULL, CLK_IS_ROOT, 0),
};

static struct of_device_id ext_clk_match[] __initdata = {
	{ .compatible = "samsung,exynos8890-oscclk", .data = (void *)0, },
};

static struct init_vclk exynos8890_clkout_vclks[] __initdata = {
	VCLK(oscclk_nfc, pxmxdx_oscclk_nfc, "pxmxdx_oscclk_nfc", 0, 0, NULL),
	VCLK(oscclk_aud, pxmxdx_oscclk_aud, "pxmxdx_oscclk_aud", 0, 0, NULL),
};

static struct init_vclk exynos8890_ccore_vclks[] __initdata = {
	VCLK(ccore_i2c, gate_ccore_i2c, "gate_ccore_i2c", 0, 0, NULL),
};

static struct init_vclk exynos8890_mfc_vclks[] __initdata = {
	/* MFC */
	VCLK(mfc_hpm, gate_mfc_hpm, "mfc_hpm", 0, 0, NULL),
	VCLK(mfc_mfc, gate_mfc_mfc, "mfc_mfc", 0, 0, NULL),
	VCLK(CLK_VCLK_SYSMMU_MFC, gate_mfc_sysmmu, "mfc_sysmmu", 0, 0, NULL),
	VCLK(mfc_ppmu, gate_mfc_ppmu, "mfc_ppmu", 0, 0, NULL),
};

static struct init_vclk exynos8890_mscl_vclks[] __initdata = {
	/* MSCL */
	VCLK(mscl_mscl0, gate_mscl_mscl0, "gate_mscl_mscl0", 0, 0, NULL),
	VCLK(mscl_jpeg, gate_mscl_jpeg, "gate_mscl_jpeg", 0, 0, NULL),
	VCLK(mscl_mscl1, gate_mscl_mscl1, "gate_mscl_mscl1", 0, 0, NULL),
	VCLK(mscl_g2d, gate_mscl_g2d, "gate_mscl_g2d", 0, 0, NULL),
	VCLK(CLK_VCLK_SYSMMU_MSCL, gate_mscl_sysmmu, "gate_mscl_sysmmu", 0, 0, NULL),
	VCLK(mscl_ppmu, gate_mscl_ppmu, "gate_mscl_ppmu", 0, 0, NULL),
	VCLK(mscl_bts, gate_mscl_bts, "gate_mscl_bts", 0, 0, NULL),
};

static struct init_vclk exynos8890_imem_vclks[] __initdata = {
	/* IMEM */
	VCLK(gate_apm, gate_imem_apm, "gate_imem_apm", 0, 0, NULL),
	VCLK(gate_sss, gate_imem_sss, "gate_imem_sss", 0, 0, NULL),
	VCLK(gate_gic400, gate_imem_gic400, "gate_imem_gic400", 0, 0, NULL),
	VCLK(gate_rtic, gate_imem_rtic, "gate_imem_rtic", 0, 0, NULL),
	VCLK(gate_mc, gate_imem_mc, "gate_imem_mc", 0, 0, NULL),
	VCLK(gate_intmem, gate_imem_intmem, "gate_imem_intmem", 0, 0, NULL),
	VCLK(gate_alv, gate_imem_intmem_alv, "gate_imem_intmem_alv", 0, 0, NULL),
	VCLK(gate_ppmu, gate_imem_ppmu, "gate_imem_ppmu", 0, 0, NULL),
};

static struct init_vclk exynos8890_peris_vclks[] __initdata = {
	/* PERIS */
	VCLK(peris_sfr, gate_peris_sfr_apbif_hdmi_cec, "gate_peris_sfr_apbif_hdmi_cec", 0, 0, NULL),
	VCLK(peris_hpm, gate_peris_hpm, "gate_peris_hpm", 0, 0, NULL),
	VCLK(peris_mct, gate_peris_mct, "gate_peris_mct", 0, 0, NULL),
	VCLK(wdt_mngs, gate_peris_wdt_mngs, "gate_peris_wdt_mngs", 0, 0, NULL),
	VCLK(wdt_apollo, gate_peris_wdt_apollo, "gate_peris_wdt_apollo", 0, 0, NULL),
	VCLK(rtc_apbif, gate_peris_rtc_apbif, "gate_peris_rtc_apbif", 0, 0, NULL),
	VCLK(top_rtc, gate_peris_top_rtc, "gate_peris_top_rtc", 0, 0, NULL),
	VCLK(otp_con_top, gate_peris_otp_con_top, "gate_peris_otp_con_top", 0, 0, NULL),
	VCLK(peris_chipid, gate_peris_chipid, "gate_peris_chipid", 0, 0, NULL),
	VCLK(peris_tmu, gate_peris_tmu, "gate_peris_tmu", 0, 0, NULL),
	VCLK(peris_sysreg, gate_peris_sysreg_peris, "gate_peris_sysreg_peris", 0, 0, NULL),
	VCLK(peris_monocnt, gate_peris_monocnt_apbif, "gate_peris_monocnt_apbif", 0, 0, NULL),
};

static struct init_vclk exynos8890_peric0_vclks[] __initdata = {
	/* PERIC0 */
	VCLK(gate_hsi2c0, gate_peric0_hsi2c0, "gate_peric0_hsi2c0", 0, 0, NULL),
	VCLK(gate_hsi2c1, gate_peric0_hsi2c1, "gate_peric0_hsi2c1", 0, 0, NULL),
	VCLK(gate_hsi2c4, gate_peric0_hsi2c4, "gate_peric0_hsi2c4", 0, 0, NULL),
	VCLK(gate_hsi2c5, gate_peric0_hsi2c5, "gate_peric0_hsi2c5", 0, 0, NULL),
	VCLK(gate_hsi2c9, gate_peric0_hsi2c9, "gate_peric0_hsi2c9", 0, 0, NULL),
	VCLK(gate_hsi2c10, gate_peric0_hsi2c10, "gate_peric0_hsi2c10", 0, 0, NULL),
	VCLK(gate_hsi2c11, gate_peric0_hsi2c11, "gate_peric0_hsi2c11", 0, 0, NULL),
	VCLK(puart0, gate_peric0_uart0, "gate_peric0_uart0", 0, 0, "console-pclk0"),
	VCLK(suart0, sclk_uart0, "sclk_uart0", 0, 0, "console-sclk0"),
	VCLK(gate_adcif, gate_peric0_adcif, "gate_peric0_adcif", 0, 0, NULL),
	VCLK(gate_pwm, gate_peric0_pwm, "gate_peric0_pwm", 0, 0, NULL),
	VCLK(gate_sclk_pwm, gate_peric0_sclk_pwm, "gate_peric0_sclk_pwm", 0, 0, NULL),
};

static struct init_vclk exynos8890_peric1_vclks[] __initdata = {
	/* PERIC1 HSI2C */
	VCLK(gate_hsi2c2, gate_peric1_hsi2c2, "gate_hsi2c2", 0, 0, NULL),
	VCLK(gate_hsi2c3, gate_peric1_hsi2c3, "gate_hsi2c3", 0, 0, NULL),
	VCLK(gate_hsi2c6, gate_peric1_hsi2c6, "gate_hsi2c6", 0, 0, NULL),
	VCLK(gate_hsi2c7, gate_peric1_hsi2c7, "gate_hsi2c7", 0, 0, "pclk_hsi2c7"),
	VCLK(gate_hsi2c8, gate_peric1_hsi2c8, "gate_hsi2c8", 0, 0, NULL),
	VCLK(gate_hsi2c12, gate_peric1_hsi2c12, "gate_hsi2c12", 0, 0, NULL),
	VCLK(gate_hsi2c13, gate_peric1_hsi2c13, "gate_hsi2c13", 0, 0, NULL),
	VCLK(gate_hsi2c14, gate_peric1_hsi2c14, "gate_hsi2c14", 0, 0, NULL),
	/* PERIC1 UART0~5 */
	VCLK(gate_uart1, gate_peric1_uart1, "gate_uart1", 0, 0, "console-pclk1"),
	VCLK(gate_uart2, gate_peric1_uart2, "gate_uart2", 0, 0, "console-pclk2"),
	VCLK(gate_uart3, gate_peric1_uart3, "gate_uart3", 0, 0, "console-pclk3"),
	VCLK(gate_uart4, gate_peric1_uart4, "gate_uart4", 0, 0, "console-pclk4"),
	VCLK(gate_uart5, gate_peric1_uart5, "gate_uart5", 0, 0, "console-pclk5"),
	VCLK(suart1, sclk_uart1, "sclk_uart1", 0, 0, "console-sclk1"),
	VCLK(suart2, sclk_uart2, "sclk_uart2", 0, 0, "console-sclk2"),
	VCLK(suart3, sclk_uart3, "sclk_uart3", 0, 0, "console-sclk3"),
	VCLK(suart4, sclk_uart4, "sclk_uart4", 0, 0, "console-sclk4"),
	VCLK(suart5, sclk_uart5, "sclk_uart5", 0, 0, "console-sclk5"),
	/* PERIC1 SPI0~7 */
	VCLK(gate_spi0, gate_peric1_spi0, "gate_spi0", 0, 0, NULL),
	VCLK(gate_spi1, gate_peric1_spi1, "gate_spi1", 0, 0, NULL),
	VCLK(gate_spi2, gate_peric1_spi2, "gate_spi2", 0, 0, NULL),
	VCLK(gate_spi3, gate_peric1_spi3, "gate_spi3", 0, 0, "ese-spi-pclk"),
	VCLK(gate_spi4, gate_peric1_spi4, "gate_spi4", 0, 0, "fp-spi-pclk"),
	VCLK(gate_spi5, gate_peric1_spi5, "gate_spi5", 0, 0, NULL),
	VCLK(gate_spi6, gate_peric1_spi6, "gate_spi6", 0, 0, NULL),
	VCLK(gate_spi7, gate_peric1_spi7, "gate_spi7", 0, 0, NULL),
	VCLK(sclk_peric1_spi0, sclk_spi0, "sclk_spi0", 0, 0, NULL),
	VCLK(sclk_peric1_spi1, sclk_spi1, "sclk_spi1", 0, 0, NULL),
	VCLK(sclk_peric1_spi2, sclk_spi2, "sclk_spi2", 0, 0, NULL),
	VCLK(sclk_peric1_spi3, sclk_spi3, "sclk_spi3", 0, 0, "ese-spi-sclk"),
	VCLK(sclk_peric1_spi4, sclk_spi4, "sclk_spi4", 0, 0, "fp-spi-sclk"),
	VCLK(sclk_peric1_spi5, sclk_spi5, "sclk_spi5", 0, 0, NULL),
	VCLK(sclk_peric1_spi6, sclk_spi6, "sclk_spi6", 0, 0, NULL),
	VCLK(sclk_peric1_spi7, sclk_spi7, "sclk_spi7", 0, 0, NULL),
	/* PERIC1 GPIO */
	VCLK(gate_gpio_nfc, gate_peric1_gpio_nfc, "gate_gpio_nfc", 0, 0, NULL),
	VCLK(gate_gpio_touch, gate_peric1_gpio_touch, "gate_gpio_touch", 0, 0, NULL),
	VCLK(gate_gpio_fp, gate_peric1_gpio_fp, "gate_gpio_fp", 0, 0, NULL),
	VCLK(gate_gpio_ese, gate_peric1_gpio_ese, "gate_gpio_ese", 0, 0, NULL),
	/* PERIC1 promise */
	VCLK(promise_int, sclk_promise_int, "sclk_promise_int", 0, 0, NULL),
	VCLK(promise_disp, sclk_promise_disp, "sclk_promise_disp", 0, 0, NULL),
	VCLK(ap2cp_mif_pll_out, sclk_ap2cp_mif_pll_out, "sclk_ap2cp_mif_pll_out", 0, 0, NULL),
	VCLK(gate_i2s1, gate_peric1_i2s1, "gate_peric1_i2s1", 0, 0, NULL),
	VCLK(gate_pcm1, gate_peric1_pcm1, "gate_peric1_pcm1", 0, 0, NULL),
	VCLK(gate_spdif, gate_peric1_spdif, "gate_peric1_spdif", 0, 0, NULL),
};

static struct init_vclk exynos8890_isp0_vclks[] __initdata = {
	/* ISP0 */
	VCLK(gate_fimc_isp0, gate_isp0_fimc_isp0, "gate_fimc_isp0", 0, 0, NULL),
	VCLK(gate_fimc_tpu, gate_isp0_fimc_tpu, "gate_isp0_fimc_tpu", 0, 0, NULL),
	VCLK(isp0, pxmxdx_isp0_isp0, "clk_isp0", 0, 0, NULL),
	VCLK(isp0_tpu, pxmxdx_isp0_tpu, "clk_isp0_tpu", 0, 0, NULL),
	VCLK(isp0_trex, pxmxdx_isp0_trex, "clk_isp0_trex", 0, 0, NULL),
	VCLK(CLK_VCLK_SYSMMU_ISP0, gate_isp0_sysmmu, "gate_isp0_sysmmu", 0, 0, NULL),
	VCLK(isp0_ppmu, gate_isp0_ppmu, "gate_isp0_ppmu", 0, 0, NULL),
	VCLK(isp0_bts, gate_isp0_bts, "gate_isp0_bts", 0, 0, NULL),
	VCLK(pxmxdx_isp0_pxl, pxmxdx_isp0_pxl_asbs, "pxmxdx_isp0_pxl_asbs", 0, 0, NULL),
};

static struct init_vclk exynos8890_isp1_vclks[] __initdata = {
	/* ISP1 */
	VCLK(gate_fimc_isp1, gate_isp1_fimc_isp1, "gate_isp1_fimc_isp1", 0, 0, NULL),
	VCLK(isp1, pxmxdx_isp1_isp1, "clk_isp1", 0, 0, NULL),
	VCLK(isp1_ppmu, gate_isp1_ppmu, "gate_isp1_ppmu", 0, 0, NULL),
	VCLK(isp1_bts, gate_isp1_bts, "gate_isp1_bts", 0, 0, NULL),
};

static struct init_vclk exynos8890_isp_sensor_vclks[] __initdata = {
	/* ISP1 */
	VCLK(isp_sensor0, sclk_isp_sensor0, "sclk_isp_sensor0", 0, 0, NULL),
	VCLK(isp_sensor1, sclk_isp_sensor1, "sclk_isp_sensor1", 0, 0, NULL),
	VCLK(isp_sensor2, sclk_isp_sensor2, "sclk_isp_sensor2", 0, 0, NULL),
	VCLK(isp_sensor3, sclk_isp_sensor3, "sclk_isp_sensor3", 0, 0, NULL),
};

static struct init_vclk exynos8890_cam0_vclks[] __initdata = {
	/* CAM0 */
	VCLK(gate_csis0, gate_cam0_csis0, "gate_cam0_csis0", 0, 0, NULL),
	VCLK(gate_csis1, gate_cam0_csis1, "gate_cam0_csis1", 0, 0, NULL),
	VCLK(gate_fimc_bns, gate_cam0_fimc_bns, "gate_cam0_fimc_bns", 0, 0, NULL),
	VCLK(fimc_3aa0, gate_cam0_fimc_3aa0, "gate_cam0_fimc_3aa0", 0, 0, NULL),
	VCLK(fimc_3aa1, gate_cam0_fimc_3aa1, "gate_cam0_fimc_3aa1", 0, 0, NULL),
	VCLK(cam0_hpm, gate_cam0_hpm, "gate_cam0_hpm", 0, 0, NULL),
	VCLK(pxmxdx_csis0, pxmxdx_cam0_csis0, "gate_pxmxdx_cam0_csis0", 0, 0, NULL),
	VCLK(pxmxdx_csis1, pxmxdx_cam0_csis1, "gate_pxmxdx_cam0_csis1", 0, 0, NULL),
	VCLK(pxmxdx_csis2, pxmxdx_cam0_csis2, "gate_pxmxdx_cam0_csis2", 0, 0, NULL),
	VCLK(pxmxdx_csis3, pxmxdx_cam0_csis3, "gate_pxmxdx_cam0_csis3", 0, 0, NULL),
	VCLK(pxmxdx_3aa0, pxmxdx_cam0_3aa0, "gate_pxmxdx_cam0_3aa0", 0, 0, NULL),
	VCLK(pxmxdx_3aa1, pxmxdx_cam0_3aa1, "gate_pxmxdx_cam0_3aa1", 0, 0, NULL),
	VCLK(pxmxdx_trex, pxmxdx_cam0_trex, "gate_pxmxdx_cam0_trex", 0, 0, NULL),
	VCLK(hs0_csis0_rx_byte, umux_cam0_phyclk_rxbyteclkhs0_csis0_user, "umux_cam0_phyclk_rxbyteclkhs0_csis0_user", 0, 0, NULL),
	VCLK(hs1_csis0_rx_byte, umux_cam0_phyclk_rxbyteclkhs1_csis0_user, "umux_cam0_phyclk_rxbyteclkhs1_csis0_user", 0, 0, NULL),
	VCLK(hs2_csis0_rx_byte, umux_cam0_phyclk_rxbyteclkhs2_csis0_user, "umux_cam0_phyclk_rxbyteclkhs2_csis0_user", 0, 0, NULL),
	VCLK(hs3_csis0_rx_byte, umux_cam0_phyclk_rxbyteclkhs3_csis0_user, "umux_cam0_phyclk_rxbyteclkhs3_csis0_user", 0, 0, NULL),
	VCLK(hs0_csis1_rx_byte, umux_cam0_phyclk_rxbyteclkhs0_csis1_user, "umux_cam0_phyclk_rxbyteclkhs0_csis1_user", 0, 0, NULL),
	VCLK(hs1_csis1_rx_byte, umux_cam0_phyclk_rxbyteclkhs1_csis1_user, "umux_cam0_phyclk_rxbyteclkhs1_csis1_user", 0, 0, NULL),
	VCLK(CLK_VCLK_SYSMMU_CAM0, gate_cam0_sysmmu, "gate_cam0_sysmmu", 0, 0, NULL),
	VCLK(cam0_ppmu, gate_cam0_ppmu, "gate_cam0_ppmu", 0, 0, NULL),
	VCLK(cam0_bts, gate_cam0_bts, "gate_cam0_bts", 0, 0, NULL),
};

static struct init_vclk exynos8890_cam1_vclks[] __initdata = {
	/* CAM1 */
	VCLK(gate_isp_cpu, gate_cam1_isp_cpu, "gate_cam1_isp_cpu", 0, 0, NULL),
	VCLK(gate_csis2, gate_cam1_csis2, "gate_cam1_csis2", 0, 0, NULL),
	VCLK(gate_csis3, gate_cam1_csis3, "gate_cam1_csis3", 0, 0, NULL),
	VCLK(gate_fimc_vra, gate_cam1_fimc_vra, "gate_cam1_fimc_vra", 0, 0, NULL),
	VCLK(gate_mc_scaler, gate_cam1_mc_scaler, "gate_cam1_mc_scaler", 0, 0, NULL),
	VCLK(gate_i2c0_isp, gate_cam1_i2c0_isp, "gate_cam1_i2c0_isp", 0, 0, NULL),
	VCLK(gate_i2c1_isp, gate_cam1_i2c1_isp, "gate_cam1_i2c1_isp", 0, 0, NULL),
	VCLK(gate_i2c2_isp, gate_cam1_i2c2_isp, "gate_cam1_i2c2_isp", 0, 0, NULL),
	VCLK(gate_i2c3_isp, gate_cam1_i2c3_isp, "gate_cam1_i2c3_isp", 0, 0, NULL),
	VCLK(gate_wdt_isp, gate_cam1_wdt_isp, "gate_cam1_wdt_isp", 0, 0, NULL),
	VCLK(gate_mcuctl_isp, gate_cam1_mcuctl_isp, "gate_cam1_mcuctl_isp", 0, 0, NULL),
	VCLK(gate_uart_isp, gate_cam1_uart_isp, "gate_cam1_uart_isp", 0, 0, NULL),
	VCLK(gate_pdma_isp, gate_cam1_pdma_isp, "gate_cam1_pdma_isp", 0, 0, NULL),
	VCLK(gate_pwm_isp, gate_cam1_pwm_isp, "gate_cam1_pwm_isp", 0, 0, NULL),
	VCLK(gate_spi0_isp, gate_cam1_spi0_isp, "gate_cam1_spi0_isp", 0, 0, NULL),
	VCLK(gate_spi1_isp, gate_cam1_spi1_isp, "gate_cam1_spi1_isp", 0, 0, NULL),
	/* rate clock source */
	VCLK(isp_spi0, sclk_isp_spi0, "sclk_isp_spi0", 0, 0, NULL),
	VCLK(isp_spi1, sclk_isp_spi1, "sclk_isp_spi1", 0, 0, NULL),
	VCLK(isp_uart, sclk_isp_uart, "sclk_isp_uart", 0, 0, NULL),
	VCLK(gate_sclk_pwm_isp, gate_cam1_sclk_pwm_isp, "gate_cam1_sclk_pwm_isp", 0, 0, NULL),
	VCLK(gate_sclk_uart_isp, gate_cam1_sclk_uart_isp, "gate_cam1_sclk_uart_isp", 0, 0, NULL),
	/* rate clock source */
	VCLK(cam1_arm, pxmxdx_cam1_arm, "pxmxdx_cam1_arm", 0, 0, NULL),
	VCLK(cam1_vra, pxmxdx_cam1_vra, "pxmxdx_cam1_vra", 0, 0, NULL),
	VCLK(cam1_trex, pxmxdx_cam1_trex, "pxmxdx_cam1_trex", 0, 0, NULL),
	VCLK(cam1_bus, pxmxdx_cam1_bus, "pxmxdx_cam1_bus", 0, 0, NULL),
	VCLK(cam1_peri, pxmxdx_cam1_peri, "pxmxdx_cam1_peri", 0, 0, NULL),
	VCLK(cam1_csis2, pxmxdx_cam1_csis2, "pxmxdx_cam1_csis2", 0, 0, NULL),
	VCLK(cam1_csis3, pxmxdx_cam1_csis3, "pxmxdx_cam1_csis3", 0, 0, NULL),
	VCLK(cam1_scl, pxmxdx_cam1_scl, "pxmxdx_cam1_scl", 0, 0, NULL),
	/* usermux */
	VCLK(cam1_phy0_csis2, umux_cam1_phyclk_rxbyteclkhs0_csis2_user, "phyclk_rxbyteclkhs0_csis2_user", 0, 0, NULL),
	VCLK(cam1_phy1_csis2, umux_cam1_phyclk_rxbyteclkhs1_csis2_user, "phyclk_rxbyteclkhs1_csis2_user", 0, 0, NULL),
	VCLK(cam1_phy2_csis2, umux_cam1_phyclk_rxbyteclkhs2_csis2_user, "phyclk_rxbyteclkhs2_csis2_user", 0, 0, NULL),
	VCLK(cam1_phy3_csis2, umux_cam1_phyclk_rxbyteclkhs3_csis2_user, "phyclk_rxbyteclkhs3_csis2_user", 0, 0, NULL),
	VCLK(cam1_phy0_csis3, umux_cam1_phyclk_rxbyteclkhs0_csis3_user, "phyclk_rxbyteclkhs0_csis3_user", 0, 0, NULL),
	VCLK(CLK_VCLK_SYSMMU_CAM1, gate_cam1_sysmmu, "gate_cam1_sysmmu", 0, 0, NULL),
	VCLK(cam1_ppmu, gate_cam1_ppmu, "gate_cam1_ppmu", 0, 0, NULL),
	VCLK(cam1_bts, gate_cam1_bts, "gate_cam1_bts", 0, 0, NULL),
};

static struct init_vclk exynos8890_audio_vclks[] __initdata = {
	/* AUDIO */
	VCLK(gate_mi2s, gate_aud_mi2s, "gate_aud_mi2s", 0, 0, NULL),
	VCLK(gate_pcm, gate_aud_pcm, "gate_aud_pcm", 0, 0, NULL),
	VCLK(gate_slimbus, gate_aud_slimbus, "gate_aud_slimbus", 0, 0, NULL),
	VCLK(gate_sclk_mi2s, gate_aud_sclk_mi2s, "gate_aud_sclk_mi2s", 0, 0, NULL),
	VCLK(d1_sclk_i2s, d1_sclk_i2s_local, "dout_sclk_i2s_local", 0, 0, NULL),
	VCLK(gate_sclk_pcm, gate_aud_sclk_pcm, "gate_aud_sclk_pcm", 0, 0, NULL),
	VCLK(d1_sclk_pcm, d1_sclk_pcm_local, "dout_sclk_pcm_local", 0, 0, NULL),
	VCLK(gate_sclk_slimbus, gate_aud_sclk_slimbus, "gate_aud_sclk_slimbus", 0, 0, NULL),
	VCLK(sclk_slimbus, d1_sclk_slimbus, "dout_sclk_slimbus", 0, 0, NULL),
	VCLK(sclk_cp_i2s, d1_sclk_cp_i2s, "dout_sclk_cp_i2s", 0, 0, NULL),
	VCLK(sclk_asrc, d1_sclk_asrc, "dout_sclk_asrc", 0, 0, NULL),
	VCLK(aud_pll, p1_aud_pll, "sclk_aud_pll", 0, 0, NULL),
	VCLK(aud_cp, pxmxdx_aud_cp, "gate_aud_cp", 0, 0, NULL),
	VCLK(aud_lpass, gate_aud_lpass, "gate_aud_lpass", 0, 0, NULL),
	VCLK(aud_dma, gate_aud_dma, "gate_aud_dma", 0, 0, NULL),
	VCLK(CLK_VCLK_SYSMMU_AUD, gate_aud_sysmmu, "gate_aud_sysmmu", 0, 0, NULL),
	VCLK(aud_ppmu, gate_aud_ppmu, "gate_aud_ppmu", 0, 0, NULL),
	VCLK(aud_bts, gate_aud_bts, "gate_aud_bts", 0, 0, NULL),
};

static struct init_vclk exynos8890_fsys0_vclks[] __initdata = {
	/* FSYS0 */
	VCLK(gate_usbdrd30, gate_fsys0_usbdrd30, "gate_fsys0_usbdrd30", 0, 0, NULL),
	VCLK(gate_usbhost20, gate_fsys0_usbhost20, "gate_fsys0_usbhost20", 0, 0, NULL),
	VCLK(usbdrd30, sclk_usbdrd30, "sclk_usbdrd30", 0, 0, NULL),
	VCLK(sclk_fsys0_mmc0, sclk_mmc0, "sclk_mmc0", 0, 0, NULL),
	VCLK(ufsunipro20, sclk_ufsunipro20, "sclk_ufsunipro20", 0, 0, NULL),
	VCLK(phy24m, sclk_phy24m, "sclk_phy24m", 0, 0, NULL),
	VCLK(ufsunipro_cfg, sclk_ufsunipro_cfg, "sclk_ufsunipro_cfg", 0, 0, NULL),
	/* UMUX GATE related clock sources */
	VCLK(gate_udrd30_phyclock, umux_fsys0_phyclk_usbdrd30_udrd30_phyclock_user, "umux_fsys0_phyclk_usbdrd30_udrd30_phyclock_user", 0, 0, NULL),
	VCLK(gate_udrd30_pipe, umux_fsys0_phyclk_usbdrd30_udrd30_pipe_pclk_user, "umux_fsys0_phyclk_usbdrd30_udrd30_pipe_pclk_user", 0, 0, NULL),
	VCLK(gate_ufs_tx0, umux_fsys0_phyclk_ufs_tx0_symbol_user, "umux_fsys0_phyclk_ufs_tx0_symbol_user", 0, 0, NULL),
	VCLK(gate_ufs_rx0, umux_fsys0_phyclk_ufs_rx0_symbol_user, "umux_fsys0_phyclk_ufs_rx0_symbol_user", 0, 0, NULL),
	VCLK(usbhost20_phyclock, umux_fsys0_phyclk_usbhost20_phyclock_user, "umux_fsys0_phyclk_usbhost20_phyclock_user", 0, 0, NULL),
	VCLK(usbhost20phy_ref, umux_fsys0_phyclk_usbhost20phy_ref_clk, "umux_fsys0_phyclk_usbhost20phy_ref_clk", 0, 0, NULL),
	VCLK(fsys_200, pxmxdx_fsys0, "aclk_ufs", 0, 0, NULL),
	VCLK(fsys0_etr_usb, gate_fsys0_etr_usb, "gate_fsys0_etr_usb", 0, 0, "etr_clk"),
	VCLK(gate_mmc, gate_fsys0_mmc0, "gate_fsys0_mmc0", 0, 0, NULL),
	VCLK(gate_pdma0, gate_fsys0_pdma0, "gate_fsys0_pdma0", 0, 0, NULL),
	VCLK(gate_pdmas, gate_fsys0_pdmas, "gate_fsys0_pdmas", 0, 0, NULL),
	VCLK(gate_ufs_linkemedded, gate_fsys0_ufs_linkemedded, "gate_fsys0_ufs_linkemedded", 0, 0, NULL),
};

static struct init_vclk exynos8890_fsys1_vclks[] __initdata = {
	/* FSYS1 */
	VCLK(fsys1_hpm, gate_fsys1_hpm, "gate_fsys1_hpm", 0, 0, NULL),
	VCLK(fsys1_sclk_mmc2, sclk_mmc2, "sclk_mmc2", 0, 0, NULL),
	VCLK(ufsunipro20_sdcard, sclk_ufsunipro20_sdcard, "sclk_ufsunipro20_sdcard", 0, 0, NULL),
	VCLK(pcie_phy, sclk_pcie_phy, "sclk_pcie_phy", 0, 0, NULL),
	VCLK(sclk_ufsunipro_sdcard, sclk_ufsunipro_sdcard_cfg, "sclk_ufsunipro_sdcard_cfg", 0, 0, NULL),
	/* UMUX GATE related clock sources */
	VCLK(ufs_link_sdcard_tx0, umux_fsys1_phyclk_ufs_link_sdcard_tx0_symbol_user, "umux_fsys1_phyclk_ufs_link_sdcard_tx0_symbol_user", 0, 0, NULL),
	VCLK(ufs_link_sdcard_rx0, umux_fsys1_phyclk_ufs_link_sdcard_rx0_symbol_user, "umux_fsys1_phyclk_ufs_link_sdcard_rx0_symbol_user", 0, 0, NULL),
	VCLK(pcie_wifi0_tx0, umux_fsys1_phyclk_pcie_wifi0_tx0_user, "umux_fsys1_phyclk_pcie_wifi0_tx0_user", 0, 0, NULL),
	VCLK(pcie_wifi0_rx0, umux_fsys1_phyclk_pcie_wifi0_rx0_user, "umux_fsys1_phyclk_pcie_wifi0_rx0_user", 0, 0, NULL),
	VCLK(pcie_wifi1_tx0, umux_fsys1_phyclk_pcie_wifi1_tx0_user, "umux_fsys1_phyclk_pcie_wifi1_tx0_user", 0, 0, NULL),
	VCLK(pcie_wifi1_rx0, umux_fsys1_phyclk_pcie_wifi1_rx0_user, "umux_fsys1_phyclk_pcie_wifi1_rx0_user", 0, 0, NULL),
	VCLK(wifi0_dig_refclk, umux_fsys1_phyclk_pcie_wifi0_dig_refclk_user, "umux_fsys1_phyclk_pcie_wifi0_dig_refclk_user", 0, 0, NULL),
	VCLK(wifi1_dig_refclk, umux_fsys1_phyclk_pcie_wifi1_dig_refclk_user, "umux_fsys1_phyclk_pcie_wifi1_dig_refclk_user", 0, 0, NULL),
	VCLK(gate_mmc2, gate_fsys1_mmc2, "gate_fsys1_mmc2", 0, 0, NULL),
	VCLK(gate_ufs20_sdcard, gate_fsys1_ufs20_sdcard, "gate_fsys1_ufs20_sdcard", 0, 0, NULL),
	VCLK(gate_sromc, gate_fsys1_sromc, "gate_fsys1_sromc", 0, 0, NULL),
	VCLK(gate_pciewifi0, gate_fsys1_pciewifi0, "gate_fsys1_pciewifi0", 0, 0, NULL),
	VCLK(gate_pciewifi1, gate_fsys1_pciewifi1, "gate_fsys1_pciewifi1", 0, 0, NULL),
	VCLK(fsys1_ppmu, gate_fsys1_ppmu, "gate_fsys1_ppmu", 0, 0, NULL),
	VCLK(fsys1_bts, gate_fsys1_bts, "gate_fsys1_bts", 0, 0, NULL),
};

static struct init_vclk exynos8890_g3d_vclks[] __initdata = {
	/* G3D */
	VCLK(gate_g3d, gate_g3d_g3d, "gate_g3d_g3d", 0, 0, "vclk_g3d"),
	VCLK(gate_g3d_iram, gate_g3d_iram_path_test, "g3d_iram_path", 0, 0, NULL),
	VCLK(g3d_bts, gate_g3d_bts, "gate_g3d_bts", 0, 0, NULL),
	VCLK(g3d_ppmu, gate_g3d_ppmu, "gate_g3d_ppmu", 0, 0, NULL),
};

static struct init_vclk exynos8890_disp0_vclks[] __initdata = {
	/* DISP0 */
	VCLK(gate_decon0, gate_disp0_decon0, "gate_disp0_decon0", 0, 0, NULL),
	VCLK(gate_dsim0, gate_disp0_dsim0, "gate_disp0_dsim0", 0, 0, NULL),
	VCLK(gate_dsim1, gate_disp0_dsim1, "gate_disp0_dsim1", 0, 0, NULL),
	VCLK(gate_dsim2, gate_disp0_dsim2, "gate_disp0_dsim2", 0, 0, NULL),
	VCLK(gate_hdmi, gate_disp0_hdmi, "gate_disp0_hdmi", 0, 0, NULL),
	VCLK(gate_dp, gate_disp0_dp, "gate_disp0_dp", 0, 0, NULL),
	VCLK(gate_hpm_apbif_disp0, gate_disp0_hpm_apbif_disp0, "gate_disp0_hpm_apbif_disp0", 0, 0, NULL),
	/* special clock - sclk */
	VCLK(decon0_eclk0, sclk_decon0_eclk0, "sclk_decon0_eclk0", 0, 0, NULL),
	VCLK(decon0_vclk0, sclk_decon0_vclk0, "sclk_decon0_vclk0", 0, 0, NULL),
	VCLK(decon0_vclk1, sclk_decon0_vclk1, "sclk_decon0_vclk1", 0, 0, NULL),
	VCLK(decon0_eclk0_local, sclk_decon0_eclk0_local, "sclk_decon0_eclk0_local", 0, 0, NULL),
	VCLK(decon0_vclk0_local, sclk_decon0_vclk0_local, "sclk_decon0_vclk0_local", 0, 0, NULL),
	VCLK(decon0_vclk1_local, sclk_decon0_vclk1_local, "sclk_decon0_vclk1_local", 0, 0, NULL),
	VCLK(hdmi_audio, sclk_hdmi_audio, "sclk_hdmi_audio", 0, 0, NULL),
	/* PLL clock source */
	VCLK(disp_pll, p1_disp_pll, "p1_disp_pll", 0, 0, "disp_pll"),
	/* USERMUX related clock source */
	VCLK(mipidphy0_rxclkesc0, umux_disp0_phyclk_mipidphy0_rxclkesc0_user, "umux_disp0_phyclk_mipidphy0_rxclkesc0_user", 0, 0, NULL),
	VCLK(mipidphy0_bitclkdiv8, umux_disp0_phyclk_mipidphy0_bitclkdiv8_user, "umux_disp0_phyclk_mipidphy0_bitclkdiv8_user", 0, 0, NULL),
	VCLK(mipidphy1_rxclkesc0, umux_disp0_phyclk_mipidphy1_rxclkesc0_user, "umux_disp0_phyclk_mipidphy1_rxclkesc0_user", 0, 0, NULL),
	VCLK(mipidphy1_bitclkdiv8, umux_disp0_phyclk_mipidphy1_bitclkdiv8_user, "umux_disp0_phyclk_mipidphy1_bitclkdiv8_user", 0, 0, NULL),
	VCLK(mipidphy2_rxclkesc0, umux_disp0_phyclk_mipidphy2_rxclkesc0_user, "umux_disp0_phyclk_mipidphy2_rxclkesc0_user", 0, 0, NULL),
	VCLK(mipidphy2_bitclkdiv8, umux_disp0_phyclk_mipidphy2_bitclkdiv8_user, "umux_disp0_phyclk_mipidphy2_bitclkdiv8_user", 0, 0, NULL),
	VCLK(dpphy_ch0_txd, umux_disp0_phyclk_dpphy_ch0_txd_clk_user, "umux_disp0_phyclk_dpphy_ch0_txd_clk_user", 0, 0, NULL),
	VCLK(dpphy_ch1_txd, umux_disp0_phyclk_dpphy_ch1_txd_clk_user, "umux_disp0_phyclk_dpphy_ch1_txd_clk_user", 0, 0, NULL),
	VCLK(dpphy_ch2_txd, umux_disp0_phyclk_dpphy_ch2_txd_clk_user, "umux_disp0_phyclk_dpphy_ch2_txd_clk_user", 0, 0, NULL),
	VCLK(dpphy_ch3_txd, umux_disp0_phyclk_dpphy_ch3_txd_clk_user, "umux_disp0_phyclk_dpphy_ch3_txd_clk_user", 0, 0, NULL),
	VCLK(dptx_phy_i_ref_clk_24m, gate_disp0_oscclk_i_dptx_phy_i_ref_clk_24m, "gate_disp0_oscclk_i_dptx_phy_i_ref_clk_24m", 0, 0, NULL),
	VCLK(mipi_dphy_m1s0, gate_disp0_oscclk_i_mipi_dphy_m1s0_m_xi, "gate_disp0_oscclk_i_mipi_dphy_m1s0_m_xi", 0, 0, NULL),
	VCLK(mipi_dphy_m4s0, gate_disp0_oscclk_i_mipi_dphy_m4s0_m_xi, "gate_disp0_oscclk_i_mipi_dphy_m4s0_m_xi", 0, 0, NULL),
	VCLK(mipi_dphy_m4s4, gate_disp0_oscclk_i_mipi_dphy_m4s4_m_xi, "gate_disp0_oscclk_i_mipi_dphy_m4s4_m_xi", 0, 0, NULL),
	VCLK(phyclk_hdmiphy_tmds_20b, umux_disp0_phyclk_hdmiphy_tmds_clko_user, "umux_disp0_phyclk_hdmiphy_tmds_clko_user", 0, 0, NULL),
	VCLK(phyclk_hdmiphy_pixel, umux_disp0_phyclk_hdmiphy_pixel_clko_user, "umux_disp0_phyclk_hdmiphy_pixel_clko_user", 0, 0, NULL),
	VCLK(mipidphy0_bitclkdiv2_user, umux_disp0_phyclk_mipidphy0_bitclkdiv2_user, "umux_disp0_phyclk_mipidphy0_bitclkdiv2_user", 0, 0, NULL),
	VCLK(mipidphy1_bitclkdiv2_user, umux_disp0_phyclk_mipidphy1_bitclkdiv2_user, "umux_disp0_phyclk_mipidphy1_bitclkdiv2_user", 0, 0, NULL),
	VCLK(mipidphy2_bitclkdiv2_user, umux_disp0_phyclk_mipidphy2_bitclkdiv2_user, "umux_disp0_phyclk_mipidphy2_bitclkdiv2_user", 0, 0, NULL),
	VCLK(CLK_VCLK_SYSMMU_DISP0, gate_disp0_sysmmu, "gate_disp0_sysmmu", 0, 0, NULL),
};

static struct init_vclk exynos8890_disp1_vclks[] __initdata = {
	/* DISP1 */
	VCLK(gate_decon1, gate_disp1_decon1, "gate_disp1_decon1", 0, 0, NULL),
	VCLK(gate_hpmdisp1, gate_disp1_hpmdisp1, "gate_disp1_hpmdisp1", 0, 0, NULL),
	/* special clock - sclk */
	VCLK(decon1_eclk0, sclk_decon1_eclk0, "sclk_decon1_eclk0", 0, 0, NULL),
	VCLK(decon1_eclk1, sclk_decon1_eclk1, "sclk_decon1_eclk1", 0, 0, NULL),
	VCLK(decon1_eclk0_local, sclk_decon1_eclk0_local, "sclk_decon1_eclk0_local", 0, 0, NULL),
	VCLK(decon1_eclk1_local, sclk_decon1_eclk1_local, "sclk_decon1_eclk1_local", 0, 0, NULL),
	/* USERMUX related clock source */
	VCLK(disp1_phyclk_mipidphy0_bitclkdiv2_user, umux_disp1_phyclk_mipidphy0_bitclkdiv2_user, "umux_disp1_phyclk_mipidphy0_bitclkdiv2_user", 0, 0, NULL),
	VCLK(disp1_phyclk_mipidphy1_bitclkdiv2_user, umux_disp1_phyclk_mipidphy1_bitclkdiv2_user, "umux_disp1_phyclk_mipidphy1_bitclkdiv2_user", 0, 0, NULL),
	VCLK(disp1_phyclk_mipidphy2_bitclkdiv2_user, umux_disp1_phyclk_mipidphy2_bitclkdiv2_user, "umux_disp1_phyclk_mipidphy2_bitclkdiv2_user", 0, 0, NULL),
	VCLK(disp1_phyclk_disp1_hdmiphy_pixel_clko_user, umux_disp1_phyclk_disp1_hdmiphy_pixel_clko_user, "umux_disp1_phyclk_disp1_hdmiphy_pixel_clko_user", 0, 0, NULL),
	VCLK(CLK_VCLK_SYSMMU_DISP1, gate_disp1_sysmmu, "gate_disp1_sysmmu", 0, 0, NULL),
	VCLK(disp1_ppmu, gate_disp1_ppmu, "gate_disp1_ppmu", 0, 0, NULL),
	VCLK(disp1_bts, gate_disp1_bts, "gate_disp1_bts", 0, 0, NULL),
};

static struct init_vclk exynos8890_dfs_vclks[] __initdata = {
	/* DFS */
	VCLK(dfs_mif, dvfs_mif, "dvfs_mif", 0, VCLK_DFS, NULL),
	VCLK(dfs_mif_sw, dvfs_mif, "dvfs_mif_sw", 0, VCLK_DFS_SWITCH, NULL),
	VCLK(dfs_int, dvfs_int, "dvfs_int", 0, VCLK_DFS, NULL),
	VCLK(dfs_cam, dvfs_cam, "dvfs_cam", 0, VCLK_DFS, NULL),
	VCLK(dfs_disp, dvfs_disp, "dvfs_disp", 0, VCLK_DFS, NULL),
};

void __init exynos8890_clk_init(struct device_node *np)
{
	struct samsung_clk_provider *ctx;
	void __iomem *reg_base;
	int ret;

	BUILD_BUG_ON(mscl_jpeg != CLK_GATE_SMFC);

	if (np) {
		reg_base = of_iomap(np, 0);
		if (!reg_base)
			panic("%s: failed to map registers\n", __func__);
	} else {
		panic("%s: unable to determine soc\n", __func__);
	}

#if defined(CONFIG_ECT)
	ect_parse_binary_header();
#endif

	ret = cal_init();
	if (ret)
		pr_err("%s: unable to initialize power cal\n", __func__);

	ctx = samsung_clk_init(np, reg_base, nr_clks);
	if (!ctx)
		panic("%s: unable to allocate context.\n", __func__);

	samsung_register_of_fixed_ext(ctx, exynos8890_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos8890_fixed_rate_ext_clks),
			ext_clk_match);

	/* Regist clock local IP */
	samsung_register_vclk(ctx, exynos8890_audio_vclks, ARRAY_SIZE(exynos8890_audio_vclks));
	samsung_register_vclk(ctx, exynos8890_cam0_vclks, ARRAY_SIZE(exynos8890_cam0_vclks));
	samsung_register_vclk(ctx, exynos8890_cam1_vclks, ARRAY_SIZE(exynos8890_cam1_vclks));
	samsung_register_vclk(ctx, exynos8890_disp0_vclks, ARRAY_SIZE(exynos8890_disp0_vclks));
	samsung_register_vclk(ctx, exynos8890_disp1_vclks, ARRAY_SIZE(exynos8890_disp1_vclks));
	samsung_register_vclk(ctx, exynos8890_fsys0_vclks, ARRAY_SIZE(exynos8890_fsys0_vclks));
	samsung_register_vclk(ctx, exynos8890_fsys1_vclks, ARRAY_SIZE(exynos8890_fsys1_vclks));
	samsung_register_vclk(ctx, exynos8890_g3d_vclks, ARRAY_SIZE(exynos8890_g3d_vclks));
	samsung_register_vclk(ctx, exynos8890_imem_vclks, ARRAY_SIZE(exynos8890_imem_vclks));
	samsung_register_vclk(ctx, exynos8890_isp0_vclks, ARRAY_SIZE(exynos8890_isp0_vclks));
	samsung_register_vclk(ctx, exynos8890_isp1_vclks, ARRAY_SIZE(exynos8890_isp1_vclks));
	samsung_register_vclk(ctx, exynos8890_isp_sensor_vclks, ARRAY_SIZE(exynos8890_isp1_vclks));
	samsung_register_vclk(ctx, exynos8890_mfc_vclks, ARRAY_SIZE(exynos8890_mfc_vclks));
	samsung_register_vclk(ctx, exynos8890_mscl_vclks, ARRAY_SIZE(exynos8890_mscl_vclks));
	samsung_register_vclk(ctx, exynos8890_peric0_vclks, ARRAY_SIZE(exynos8890_peric0_vclks));
	samsung_register_vclk(ctx, exynos8890_peric1_vclks, ARRAY_SIZE(exynos8890_peric1_vclks));
	samsung_register_vclk(ctx, exynos8890_peris_vclks, ARRAY_SIZE(exynos8890_peris_vclks));
	samsung_register_vclk(ctx, exynos8890_ccore_vclks, ARRAY_SIZE(exynos8890_ccore_vclks));
	samsung_register_vclk(ctx, exynos8890_clkout_vclks, ARRAY_SIZE(exynos8890_clkout_vclks));
	samsung_register_vclk(ctx, exynos8890_dfs_vclks, ARRAY_SIZE(exynos8890_dfs_vclks));

	samsung_clk_of_add_provider(np, ctx);

	clk_register_fixed_factor(NULL, "pwm-clock", "gate_peric0_sclk_pwm",CLK_SET_RATE_PARENT, 1, 1);

	pr_info("EXYNOS8890: Clock setup completed\n");
}
CLK_OF_DECLARE(exynos8890_clks, "samsung,exynos8890-clock", exynos8890_clk_init);
