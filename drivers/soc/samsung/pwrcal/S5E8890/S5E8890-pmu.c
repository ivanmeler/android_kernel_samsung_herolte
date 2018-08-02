#include "../pwrcal.h"
#include "../pwrcal-env.h"
#include "../pwrcal-rae.h"
#include "../pwrcal-pmu.h"
#include "S5E8890-cmusfr.h"
#include "S5E8890-pmusfr.h"
#include "S5E8890-cmu.h"
#include "S5E8890-vclk-internal.h"

#define PAD_INITIATE_WAKEUP	(0x1 << 28)

#define ISP_FORCE_POWERDOWN

static void cam0_prev(int enable)
{

#ifdef ISP_FORCE_POWERDOWN
	if (enable == 0) {
		pwrcal_writel(CLK_ENABLE_ACLK_CAM0_CSIS0_414, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM0_CSIS0_207, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM0_CSIS1_168_CAM0, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM0_CSIS2_234_CAM0, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM0_CSIS3_132_CAM0, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM0_3AA0_414_CAM0, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM0_3AA0_207, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM0_3AA1_414_CAM0, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM0_3AA1_207, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM0_TREX_528_CAM0, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM0_TREX_264, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM0_TREX_132, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_SCLK_PROMISE_CAM0, 0xFFFFFFFF);

		pwrcal_writel(CLK_ENABLE_ACLK_CAM0_CSIS0_414_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM0_CSIS0_207_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM0_CSIS1_168_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM0_CSIS2_234_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM0_CSIS3_132_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM0_3AA0_414_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM0_3AA0_207_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM0_3AA1_414_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM0_3AA1_207_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM0_TREX_264_LOCAL, 0xFFFFFFFF);

		pwrcal_writel(LPI_MASK_CAM0_BUSMASTER, 0x1F);
	}
#endif

	if (pwrcal_getf(CAM1_STATUS, 0, 0xF) == 0xF) {
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM1_CSIS2_414_CAM1, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM1_CSIS2_414_LOCAL, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM1_CSIS3_132_CAM1, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM1_CSIS3_132_LOCAL, 0, 1);
	}
	if (pwrcal_getf(ISP0_STATUS, 0, 0xF) == 0xF) {
		pwrcal_setbit(CLK_ENABLE_ACLK_ISP0, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_ISP0_LOCAL, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_ISP0_TPU, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_ISP0_TPU_LOCAL, 0, 1);
	}
	if (pwrcal_getf(ISP1_STATUS, 0, 0xF) == 0xF) {
		pwrcal_setbit(CLK_ENABLE_ACLK_ISP1, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_ISP1_LOCAL, 0, 1);
	}

	if (enable == 0) {
		vclk_disable(VCLK(dvfs_cam));

		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS0_CSIS0_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS0_CSIS0_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS1_CSIS0_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS1_CSIS0_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS2_CSIS0_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS2_CSIS0_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS3_CSIS0_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS3_CSIS0_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS0_CSIS1_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS0_CSIS1_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS1_CSIS1_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS1_CSIS1_USER, 27, 1);
	}
}

static void cam1_prev(int enable)
{
#ifdef ISP_FORCE_POWERDOWN
	if (enable == 0) {
		pwrcal_writel(CLK_ENABLE_ACLK_CAM1_ARM_672_CAM1, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM1_ARM_168, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM1_TREX_VRA_528_CAM1, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM1_TREX_VRA_264, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM1_TREX_B_528_CAM1, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM1_BUS_264_CAM1, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM1_BUS_132, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM1_PERI_84, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM1_CSIS2_414_CAM1, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM1_CSIS3_132_CAM1, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM1_SCL_566_CAM1, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM1_SCL_283, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_SCLK_CAM1_ISP_SPI0_CAM1, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_SCLK_CAM1_ISP_SPI1_CAM1, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_SCLK_CAM1_ISP_UART_CAM1, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_SCLK_ISP_PERI_IS_B, 0xFFFFFFFF);

		pwrcal_writel(CLK_ENABLE_ACLK_CAM1_TREX_VRA_528_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM1_TREX_VRA_264_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM1_BUS_264_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_CAM1_PERI_84_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM1_CSIS2_414_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM1_CSIS3_132_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_CAM1_SCL_566_LOCAL, 0xFFFFFFFF);

		pwrcal_writel(LPI_MASK_CAM1_BUSMASTER, 0xF);
		pwrcal_setf(A7IS_OPTION, 15, 0x7, 0);
	}
#endif

	if (pwrcal_getf(CAM0_STATUS, 0, 0xF) == 0xF) {
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS0_414, 1, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS0_414_LOCAL, 1, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS2_234_CAM0, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS2_234_LOCAL, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS3_132_CAM0, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS3_132_LOCAL, 0, 1);
	}
	if (pwrcal_getf(ISP0_STATUS, 0, 0xF) == 0xF) {
		pwrcal_setbit(CLK_ENABLE_ACLK_ISP0, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_ISP0_LOCAL, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_ISP0_TPU, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_ISP0_TPU_LOCAL, 0, 1);
	}
	if (enable == 0) {
		vclk_disable(VCLK(dvfs_cam));

		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS0_CSIS2_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS0_CSIS2_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS1_CSIS2_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS1_CSIS2_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS2_CSIS2_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS2_CSIS2_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS3_CSIS2_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS3_CSIS2_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS0_CSIS3_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_RXBYTECLKHS0_CSIS3_USER, 27, 1);
	}
}

static void isp0_prev(int enable)
{
#ifdef ISP_FORCE_POWERDOWN
	if (enable == 0) {
		pwrcal_writel(CLK_ENABLE_ACLK_ISP0, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_ISP0, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_ISP0_TPU, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_ISP0_TPU, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_ISP0_TREX, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_TREX_264, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_HPM_APBIF_ISP0, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_TREX_132, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_SCLK_PROMISE_ISP0, 0xFFFFFFFF);

		pwrcal_writel(CLK_ENABLE_ACLK_ISP0_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_ISP0_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_ISP0_TPU_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_ISP0_TPU_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_ACLK_ISP0_TREX_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_TREX_132_LOCAL, 0xFFFFFFFF);

		pwrcal_writel(LPI_MASK_ISP0_BUSMASTER, 0x3);
	}
#endif

	if (pwrcal_getf(CAM0_STATUS, 0, 0xF) == 0xF) {
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS2_234_CAM0, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS2_234_LOCAL, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS3_132_CAM0, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS3_132_LOCAL, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_3AA0_414_CAM0, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_3AA0_414_LOCAL, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_3AA1_414_CAM0, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_3AA1_414_LOCAL, 0, 1);
	}
	if (pwrcal_getf(CAM1_STATUS, 0, 0xF) == 0xF) {
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM1_SCL_566_CAM1, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM1_SCL_566_LOCAL, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM1_TREX_VRA_528_CAM1, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM1_TREX_VRA_528_LOCAL, 0, 1);
	}
}

static void isp1_prev(int enable)
{
#ifdef ISP_FORCE_POWERDOWN
	if (enable == 0) {
		pwrcal_writel(CLK_ENABLE_ACLK_ISP1, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_ISP1_234, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_HPM_APBIF_ISP1, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_SCLK_PROMISE_ISP1, 0xFFFFFFFF);

		pwrcal_writel(CLK_ENABLE_ACLK_ISP1_LOCAL, 0xFFFFFFFF);
		pwrcal_writel(CLK_ENABLE_PCLK_ISP1_234_LOCAL, 0xFFFFFFFF);

		pwrcal_writel(LPI_MASK_ISP1_BUSMASTER, 0x1);
	}
#endif

	if (pwrcal_getf(CAM0_STATUS, 0, 0xF) == 0xF) {
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS2_234_CAM0, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS2_234_LOCAL, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS3_132_CAM0, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_CSIS3_132_LOCAL, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_3AA0_414_CAM0, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_3AA0_414_LOCAL, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_3AA1_414_CAM0, 0, 1);
		pwrcal_setbit(CLK_ENABLE_ACLK_CAM0_3AA1_414_LOCAL, 0, 1);
	}
}

static void disp0_prev(int enable)
{
	if (enable == 0) {
		pwrcal_setbit(QCH_CTRL_AXI_LH_ASYNC_MI_DISP0SFR, 0, 1);
		pwrcal_setbit(QCH_CTRL_CMU_DISP0, 0, 1);
		pwrcal_setbit(QCH_CTRL_PMU_DISP0, 0, 1);
		pwrcal_setbit(QCH_CTRL_SYSREG_DISP0, 0, 1);
		pwrcal_setbit(QCH_CTRL_DECON0, 0, 1);
		pwrcal_setbit(QCH_CTRL_VPP0_G0, 0, 1);
		pwrcal_setbit(QCH_CTRL_VPP0_G1, 0, 1);
		pwrcal_setbit(QCH_CTRL_DSIM0, 0, 1);
		pwrcal_setbit(QCH_CTRL_DSIM1, 0, 1);
		pwrcal_setbit(QCH_CTRL_DSIM2, 0, 1);
		pwrcal_setbit(QCH_CTRL_HDMI, 0, 1);
		pwrcal_setbit(QCH_CTRL_DP, 0, 1);
		pwrcal_setbit(QCH_CTRL_PPMU_DISP0_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_PPMU_DISP0_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_SMMU_DISP0_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_SMMU_DISP0_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_SFW_DISP0_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_SFW_DISP0_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_LH_ASYNC_SI_R_TOP_DISP, 0, 1);
		pwrcal_setbit(QCH_CTRL_LH_ASYNC_SI_TOP_DISP, 0, 1);

		pwrcal_setbit(CLK_CON_MUX_DISP_PLL,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_DISP_PLL,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_ACLK_DISP0_0_400_USER,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_ACLK_DISP0_0_400_USER,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_ACLK_DISP0_1_400_USER,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_ACLK_DISP0_1_400_USER,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_ECLK0_USER,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_ECLK0_USER,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK0_USER,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK0_USER,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK1_USER,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK1_USER,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_HDMI_AUDIO_USER,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_HDMI_AUDIO_USER,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_ACLK_DISP0_1_400_DISP0,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_ACLK_DISP0_1_400_DISP0,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_ECLK0_DISP0,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_ECLK0_DISP0,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK0_DISP0,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK0_DISP0,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK1_DISP0,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK1_DISP0,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_HDMI_AUDIO_DISP0,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_HDMI_AUDIO_DISP0,	27,	1);

		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY0_BITCLKDIV2_USER_DISP0, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY0_BITCLKDIV2_USER_DISP0, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY1_BITCLKDIV2_USER_DISP0, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY1_BITCLKDIV2_USER_DISP0, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY2_BITCLKDIV2_USER_DISP0, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY2_BITCLKDIV2_USER_DISP0, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_HDMIPHY_PIXEL_CLKO_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_HDMIPHY_PIXEL_CLKO_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_HDMIPHY_TMDS_CLKO_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_HDMIPHY_TMDS_CLKO_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY0_RXCLKESC0_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY0_RXCLKESC0_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY0_BITCLKDIV8_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY0_BITCLKDIV8_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY1_RXCLKESC0_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY1_RXCLKESC0_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY1_BITCLKDIV8_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY1_BITCLKDIV8_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY2_RXCLKESC0_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY2_RXCLKESC0_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY2_BITCLKDIV8_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY2_BITCLKDIV8_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_DPPHY_CH0_TXD_CLK_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_DPPHY_CH0_TXD_CLK_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_DPPHY_CH1_TXD_CLK_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_DPPHY_CH1_TXD_CLK_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_DPPHY_CH2_TXD_CLK_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_DPPHY_CH2_TXD_CLK_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_DPPHY_CH3_TXD_CLK_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_DPPHY_CH3_TXD_CLK_USER, 27, 1);
	}
}

static void disp1_prev(int enable)
{
	if (enable == 0) {
		pwrcal_setbit(QCH_CTRL_AXI_LH_ASYNC_MI_DISP1SFR, 0, 1);
		pwrcal_setbit(QCH_CTRL_CMU_DISP1, 0, 1);
		pwrcal_setbit(QCH_CTRL_PMU_DISP1, 0, 1);
		pwrcal_setbit(QCH_CTRL_SYSREG_DISP1, 0, 1);
		pwrcal_setbit(QCH_CTRL_VPP1_G2, 0, 1);
		pwrcal_setbit(QCH_CTRL_VPP1_G3, 0, 1);
		pwrcal_setbit(QCH_CTRL_DECON1_PCLK_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_DECON1_PCLK_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_PPMU_DISP1_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_PPMU_DISP1_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_SMMU_DISP1_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_SMMU_DISP1_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_SFW_DISP1_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_SFW_DISP1_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_AXI_LH_ASYNC_SI_DISP1_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_AXI_LH_ASYNC_SI_DISP1_1, 0, 1);

		pwrcal_setbit(CLK_CON_MUX_ACLK_DISP1_0_400_USER,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_ACLK_DISP1_0_400_USER,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_ACLK_DISP1_1_400_USER,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_ACLK_DISP1_1_400_USER,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK0_USER,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK0_USER,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK1_USER,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK1_USER,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP1_600_USER,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP1_600_USER,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_ACLK_DISP1_1_400_DISP1,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_ACLK_DISP1_1_400_DISP1,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK0_DISP1,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK0_DISP1,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK1_DISP1,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK1_DISP1,	27,	1);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DECON1_ECLK1,	12,	0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_DECON1_ECLK1,	27,	1);

		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY0_BITCLKDIV2_USER_DISP1, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY0_BITCLKDIV2_USER_DISP1, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY1_BITCLKDIV2_USER_DISP1, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY1_BITCLKDIV2_USER_DISP1, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY2_BITCLKDIV2_USER_DISP1, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_MIPIDPHY2_BITCLKDIV2_USER_DISP1, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_DISP1_HDMIPHY_PIXEL_CLKO_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_DISP1_HDMIPHY_PIXEL_CLKO_USER, 27, 1);
	}
}

static void aud_prev(int enable)
{
}

static void fsys0_prev(int enable)
{
	if (enable == 0) {
		pwrcal_writel(QSTATE_CTRL_USBDRD30,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_UFS_LINK_EMBEDDED,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_USBHOST20,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_USBHOST20_PHY,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_GPIO_FSYS0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_HPM_APBIF_FSYS0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PROMISE_FSYS0,	HWACG_QSTATE_CLOCK_ENABLE);

		pwrcal_writel(CG_CTRL_MAN_ACLK_FSYS0_200,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_PCLK_HPM_APBIF_FSYS0,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_SCLK_USBDRD30_SUSPEND_CLK,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_SCLK_MMC0,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_SCLK_UFSUNIPRO_EMBEDDED,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_SCLK_USBDRD30_REF_CLK,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_USBDRD30_UDRD30_PHYCLOCK,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_USBDRD30_UDRD30_PIPE_PCLK,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_UFS_TX0_SYMBOL,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_UFS_RX0_SYMBOL,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_USBHOST20_PHYCLOCK,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_USBHOST20_FREECLK,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_SCLK_PROMISE_FSYS0,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_SCLK_USBHOST20PHY_REF_CLK,	0xFFFFFFFF);
		pwrcal_writel(CG_CTRL_MAN_SCLK_UFSUNIPRO_EMBEDDED_CFG,	0xFFFFFFFF);

		pwrcal_setbit(QCH_CTRL_AXI_LH_ASYNC_MI_TOP_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_AXI_LH_ASYNC_MI_ETR_USB_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_ETR_USB_FSYS0_ACLK, 0, 1);
		pwrcal_setbit(QCH_CTRL_ETR_USB_FSYS0_PCLK, 0, 1);
		pwrcal_setbit(QCH_CTRL_CMU_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_PMU_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_SYSREG_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_USBDRD30, 0, 1);
		pwrcal_setbit(QCH_CTRL_MMC0, 0, 1);
		pwrcal_setbit(QCH_CTRL_UFS_LINK_EMBEDDED, 0, 1);
		pwrcal_setbit(QCH_CTRL_USBHOST20, 0, 1);
		pwrcal_setbit(QCH_CTRL_PDMA0, 0, 1);
		pwrcal_setbit(QCH_CTRL_PDMAS, 0, 1);
		pwrcal_setbit(QCH_CTRL_PPMU_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_ACEL_LH_ASYNC_SI_TOP_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_USBDRD30_PHYCTRL, 0, 1);
		pwrcal_setbit(QCH_CTRL_USBHOST20_PHYCTRL, 0, 1);

		pwrcal_setbit(CLK_CON_MUX_PHYCLK_USBDRD30_UDRD30_PHYCLOCK_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_USBDRD30_UDRD30_PHYCLOCK_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_USBDRD30_UDRD30_PIPE_PCLK_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_USBDRD30_UDRD30_PIPE_PCLK_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_UFS_TX0_SYMBOL_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_UFS_TX0_SYMBOL_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_UFS_RX0_SYMBOL_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_UFS_RX0_SYMBOL_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_USBHOST20_PHYCLOCK_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_USBHOST20_PHYCLOCK_USER, 27, 1);

		pwrcal_setbit(CLK_CON_MUX_ACLK_FSYS0_200_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_FSYS0_USBDRD30_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_FSYS0_MMC0_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_FSYS0_UFSUNIPRO_EMBEDDED_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_SCLK_FSYS0_UFSUNIPRO_EMBEDDED_CFG_USER, 12, 0);

		pwrcal_setf(RESET_SLEEP_FSYS0_SYS_PWR_REG, 0, 0x3, 0x3);
		pwrcal_setf(RESET_CMU_FSYS0_SYS_PWR_REG, 0, 0x3, 0x3);
	}
}

static void fsys1_prev(int enable)
{
	if (enable == 0) {
		pwrcal_writel(QSTATE_CTRL_SROMC_FSYS1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_GPIO_FSYS1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_HPM_APBIF_FSYS1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PROMISE_FSYS1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PCIE_RC_LINK_WIFI0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PCIE_RC_LINK_WIFI1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PCIE_PCS_WIFI0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PCIE_PCS_WIFI1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PCIE_PHY_FSYS1_WIFI0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PCIE_PHY_FSYS1_WIFI1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_UFS_LINK_SDCARD,	HWACG_QSTATE_CLOCK_ENABLE);

		pwrcal_setbit(QCH_CTRL_AXI_LH_ASYNC_MI_TOP_FSYS1, 0, 1);
		pwrcal_setbit(QCH_CTRL_CMU_FSYS1, 0, 1);
		pwrcal_setbit(QCH_CTRL_PMU_FSYS1, 0, 1);
		pwrcal_setbit(QCH_CTRL_SYSREG_FSYS1, 0, 1);
		pwrcal_setbit(QCH_CTRL_MMC2, 0, 1);
		pwrcal_setbit(QCH_CTRL_UFS_LINK_SDCARD, 0, 1);
		pwrcal_setbit(QCH_CTRL_PPMU_FSYS1, 0, 1);
		pwrcal_setbit(QCH_CTRL_ACEL_LH_ASYNC_SI_TOP_FSYS1, 0, 1);
		pwrcal_setbit(QCH_CTRL_PCIE_RC_LINK_WIFI0_SLV, 0, 1);
		pwrcal_setbit(QCH_CTRL_PCIE_RC_LINK_WIFI0_DBI, 0, 1);
		pwrcal_setbit(QCH_CTRL_PCIE_RC_LINK_WIFI1_SLV, 0, 1);
		pwrcal_setbit(QCH_CTRL_PCIE_RC_LINK_WIFI1_DBI, 0, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_UFS_LINK_SDCARD_TX0_SYMBOL_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_UFS_LINK_SDCARD_TX0_SYMBOL_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_UFS_LINK_SDCARD_RX0_SYMBOL_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_UFS_LINK_SDCARD_RX0_SYMBOL_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI0_TX0_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI0_TX0_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI0_RX0_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI0_RX0_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI1_TX0_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI1_TX0_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI1_RX0_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI1_RX0_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI0_DIG_REFCLK_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI0_DIG_REFCLK_USER, 27, 1);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI1_DIG_REFCLK_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI1_DIG_REFCLK_USER, 27, 1);

		pwrcal_setf(RESET_SLEEP_FSYS1_SYS_PWR_REG, 0, 0x3, 0x3);
		pwrcal_setf(RESET_CMU_FSYS1_SYS_PWR_REG, 0, 0x3, 0x3);
	}
}

static void mscl_prev(int enable)
{
	if (enable == 0) {
		pwrcal_setbit(QCH_CTRL_LH_ASYNC_MI_MSCLSFR, 0, 1);
		pwrcal_setbit(QCH_CTRL_CMU_MSCL, 0, 1);
		pwrcal_setbit(QCH_CTRL_PMU_MSCL, 0, 1);
		pwrcal_setbit(QCH_CTRL_SYSREG_MSCL, 0, 1);
		pwrcal_setbit(QCH_CTRL_MSCL_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_MSCL_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_JPEG, 0, 1);
		pwrcal_setbit(QCH_CTRL_G2D, 0, 1);
		pwrcal_setbit(QCH_CTRL_SMMU_MSCL_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_SMMU_MSCL_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_SMMU_JPEG, 0, 1);
		pwrcal_setbit(QCH_CTRL_SMMU_G2D, 0, 1);
		pwrcal_setbit(QCH_CTRL_PPMU_MSCL_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_PPMU_MSCL_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_SFW_MSCL_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_SFW_MSCL_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_LH_ASYNC_SI_MSCL_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_LH_ASYNC_SI_MSCL_1, 0, 1);
	}
}

static void mfc_prev(int enable)
{
	if (enable == 0) {
		pwrcal_setbit(QCH_CTRL_MFC, 0, 1);
		pwrcal_setbit(QCH_CTRL_LH_M_MFC, 0, 1);
		pwrcal_setbit(QCH_CTRL_CMU_MFC, 0, 1);
		pwrcal_setbit(QCH_CTRL_PMU_MFC, 0, 1);
		pwrcal_setbit(QCH_CTRL_SYSREG_MFC, 0, 1);
		pwrcal_setbit(QCH_CTRL_PPMU_MFC_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_PPMU_MFC_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_SFW_MFC_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_SFW_MFC_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_SMMU_MFC_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_SMMU_MFC_1, 0, 1);
		pwrcal_setbit(QCH_CTRL_LH_S_MFC_0, 0, 1);
		pwrcal_setbit(QCH_CTRL_LH_S_MFC_1, 0, 1);
	}
}


static void g3d_prev(int enable)
{
	if (enable == 0) {
		pwrcal_setbit(CLK_CON_MUX_G3D_PLL_USER, 12, 0);
		while (pwrcal_getbit(CLK_CON_MUX_G3D_PLL_USER, 26));
		pwrcal_setbit(G3D_PLL_CON0, 31, 0);
	} else {
		pwrcal_setbit(G3D_PLL_CON0, 31, 1);
	}
}



static void cam0_post(int enable)
{
	if (enable == 1) {
		vclk_enable(VCLK(dvfs_cam));

		pwrcal_writel(CAM0_DRCG_EN, 0x1F);

		pwrcal_gate_disable(CLK(CAM0_GATE_ACLK_CSIS0));
		pwrcal_gate_disable(CLK(CAM0_GATE_PCLK_CSIS0));
		pwrcal_gate_disable(CLK(CAM0_GATE_ACLK_CSIS1));
		pwrcal_gate_disable(CLK(CAM0_GATE_PCLK_CSIS1));
		pwrcal_gate_disable(CLK(CAM0_GATE_ACLK_PXL_ASBS_CSIS2_int));
		pwrcal_gate_disable(CLK(CAM0_GATE_ACLK_3AA0));
		pwrcal_gate_disable(CLK(CAM0_GATE_PCLK_3AA0));
		pwrcal_gate_disable(CLK(CAM0_GATE_ACLK_3AA1));
		pwrcal_gate_disable(CLK(CAM0_GATE_PCLK_3AA1));
		pwrcal_gate_disable(CLK(CAM0_GATE_PCLK_HPM_APBIF_CAM0));
		pwrcal_gate_disable(CLK(CAM0_GATE_SCLK_PROMISE_CAM0));
	}
}
static void mscl_post(int enable)
{
	if (enable == 1) {
		pwrcal_writel(MSCL_DRCG_EN, 0x3FF);

		pwrcal_writel(CG_CTRL_MAN_ACLK_MSCL0_528,	0);
		pwrcal_writel(CG_CTRL_MAN_ACLK_MSCL0_528_SECURE_SFW_MSCL_0,	0);
		pwrcal_writel(CG_CTRL_MAN_ACLK_MSCL1_528,	0);
		pwrcal_writel(CG_CTRL_MAN_ACLK_MSCL1_528_SECURE_SFW_MSCL_1,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_MSCL,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_MSCL_SECURE_SFW_MSCL_0,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_MSCL_SECURE_SFW_MSCL_1,	0);

		pwrcal_writel(QCH_CTRL_LH_ASYNC_MI_MSCLSFR,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_CMU_MSCL,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PMU_MSCL,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SYSREG_MSCL,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_MSCL_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_MSCL_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_JPEG,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_G2D,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SMMU_MSCL_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SMMU_MSCL_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SMMU_JPEG,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SMMU_G2D,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PPMU_MSCL_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PPMU_MSCL_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SFW_MSCL_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SFW_MSCL_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_LH_ASYNC_SI_MSCL_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_LH_ASYNC_SI_MSCL_1,	HWACG_QCH_ENABLE);
	}
}
static void g3d_post(int enable)
{
	if (enable == 1) {
		pwrcal_setbit(CLK_CON_MUX_G3D_PLL_USER, 12, 1);
		while (pwrcal_getbit(CLK_CON_MUX_G3D_PLL_USER, 26));

		pwrcal_writel(G3D_DRCG_EN, 0x3F);

		pwrcal_gate_disable(CLK(G3D_GATE_ACLK_PPMU_G3D0));
		pwrcal_gate_disable(CLK(G3D_GATE_ACLK_PPMU_G3D1));
		pwrcal_gate_disable(CLK(G3D_GATE_PCLK_PPMU_G3D0));
		pwrcal_gate_disable(CLK(G3D_GATE_PCLK_PPMU_G3D1));
		pwrcal_gate_disable(CLK(G3D_GATE_ACLK_G3D));
		pwrcal_gate_disable(CLK(G3D_GATE_ACLK_ASYNCAPBM_G3D));
		pwrcal_gate_disable(CLK(G3D_GATE_ACLK_XIU_G3D));
		pwrcal_gate_disable(CLK(G3D_GATE_ACLK_GRAY_DEC));
		pwrcal_gate_disable(CLK(G3D_GATE_ACLK_ASYNCAPBS_G3D));
		pwrcal_gate_disable(CLK(G3D_GATE_ACLK_ACEL_LH_ASYNC_SI_G3D0));
		pwrcal_gate_disable(CLK(G3D_GATE_ACLK_ACEL_LH_ASYNC_SI_G3D1));
		pwrcal_gate_disable(CLK(G3D_GATE_PCLK_SYSREG_G3D));
		pwrcal_gate_disable(CLK(G3D_GATE_ACLK_AXI_DS_G3D));
		pwrcal_gate_disable(CLK(G3D_GATE_ACLK_ASYNCAXI_G3D));
		pwrcal_gate_disable(CLK(G3D_GATE_SCLK_ASYNCAXI_G3D));
		pwrcal_gate_disable(CLK(G3D_GATE_SCLK_AXI_LH_ASYNC_SI_G3DIRAM));
	}
}
static void disp0_post(int enable)
{
	if (enable == 1) {
		pwrcal_writel(DISP0_DRCG_EN, 0x3f);

		pwrcal_writel(CG_CTRL_MAN_ACLK_DISP0_0_400,	0);
		pwrcal_writel(CG_CTRL_MAN_ACLK_DISP0_1_400,	0);
		pwrcal_writel(CG_CTRL_MAN_ACLK_DISP0_0_400_SECURE_SFW_DISP0_0,	0);
		pwrcal_writel(CG_CTRL_MAN_ACLK_DISP0_1_400_SECURE_SFW_DISP0_1,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_DISP0_0_133,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_DISP0_0_133_HPM_APBIF_DISP0,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_DISP0_0_133_SECURE_DECON0,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_DISP0_0_133_SECURE_VPP0,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_DISP0_0_133_SECURE_SFW_DISP0_0,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_DISP0_0_133_SECURE_SFW_DISP0_1,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_DISP1_400,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_DECON0_ECLK0,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_DECON0_VCLK0,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_DECON0_VCLK1,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_HDMI_AUDIO,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_DISP0_PROMISE,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_HDMIPHY,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_MIPIDPHY0,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_MIPIDPHY1,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_MIPIDPHY2,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_DPPHY,	0);
		pwrcal_writel(CG_CTRL_MAN_OSCCLK,	0);

		pwrcal_writel(QCH_CTRL_AXI_LH_ASYNC_MI_DISP0SFR,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_CMU_DISP0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PMU_DISP0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SYSREG_DISP0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_DECON0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_VPP0_G0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_VPP0_G1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_DSIM0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_DSIM1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_DSIM2,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_HDMI,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_DP,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PPMU_DISP0_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PPMU_DISP0_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SMMU_DISP0_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SMMU_DISP0_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SFW_DISP0_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SFW_DISP0_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_LH_ASYNC_SI_R_TOP_DISP,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_LH_ASYNC_SI_TOP_DISP,	HWACG_QCH_ENABLE);

		pwrcal_writel(QSTATE_CTRL_DSIM0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_DSIM1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_DSIM2,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_HDMI,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_HDMI_AUDIO,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_DP,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_DISP0_MUX,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_HDMI_PHY,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_DISP1_400,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_DECON0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_HPM_APBIF_DISP0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PROMISE_DISP0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_DPTX_PHY,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_MIPI_DPHY_M1S0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_MIPI_DPHY_M4S0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_MIPI_DPHY_M4S4,	HWACG_QSTATE_CLOCK_ENABLE);
	}
}
static void cam1_post(int enable)
{
	if (enable == 1) {
		vclk_enable(VCLK(dvfs_cam));

		pwrcal_writel(CAM1_DRCG_EN, 0x7FFFF);

		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_SMMU_VRA));
		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_SMMU_ISPCPU));
		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_SMMU_IS_B));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_SMMU_IS_B));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_SMMU_VRA));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_SMMU_ISPCPU));
		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_SMMU_MC_SC));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_SMMU_MC_SC));
		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_ARM));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_ARM));
		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_ASYNC_CA7_TO_DRAM));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_CSIS2));
		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_CSIS2));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_CSIS3));
		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_CSIS3));
		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_VRA));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_VRA));
		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_MC_SC));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_I2C0));
		pwrcal_gate_disable(CLK(CAM1_GATE_SCLK_CAM1_ISP_IS_B_OSCCLK_I2C0_ISP));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_I2C1));
		pwrcal_gate_disable(CLK(CAM1_GATE_SCLK_CAM1_ISP_IS_B_OSCCLK_I2C1_ISP));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_I2C2));
		pwrcal_gate_disable(CLK(CAM1_GATE_SCLK_CAM1_ISP_IS_B_OSCCLK_I2C2_ISP));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_I2C3));
		pwrcal_gate_disable(CLK(CAM1_GATE_SCLK_CAM1_ISP_IS_B_OSCCLK_I2C3_ISP));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_WDT));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_MCUCTL));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_UART));
		pwrcal_gate_disable(CLK(CAM1_GATE_SCLK_ISP_PERI_IS_B_UART_EXT_CLK_ISP));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_SPI0));
		pwrcal_gate_disable(CLK(CAM1_GATE_SCLK_ISP_PERI_IS_B_SPI0_EXT_CLK_ISP));
		pwrcal_gate_disable(CLK(CAM1_GATE_SCLK_ISP_PERI_IS_B_SPI1_EXT_CLK_ISP));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_SPI1));
		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_TREX_B));
		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_TREX_CAM1));
		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_LH_SI));
		pwrcal_gate_disable(CLK(CAM1_GATE_ACLK_PDMA));
		pwrcal_gate_disable(CLK(CAM1_GATE_PCLK_PWM));
		pwrcal_gate_disable(CLK(CAM1_GATE_SCLK_ISP_PERI_IS_B_PWM_ISP));
	}
}
static void aud_post(int enable)
{
	if (enable == 1) {
		pwrcal_writel(AUD_DRCG_EN, 0xF);

		pwrcal_gate_disable(CLK(AUD_GATE_SCLK_CA5));
		pwrcal_gate_disable(CLK(AUD_GATE_ACLK_DMAC));
		pwrcal_gate_disable(CLK(AUD_GATE_ACLK_SRAMC));
		pwrcal_gate_disable(CLK(AUD_GATE_ACLK_AXI_LH_ASYNC_SI_TOP));
		pwrcal_gate_disable(CLK(AUD_GATE_ACLK_SMMU));
		pwrcal_gate_disable(CLK(AUD_GATE_ACLK_INTR));
		pwrcal_gate_disable(CLK(AUD_GATE_PCLK_SMMU));
		pwrcal_gate_disable(CLK(AUD_GATE_PCLK_SFR1));
		pwrcal_gate_disable(CLK(AUD_GATE_PCLK_TIMER));
		pwrcal_gate_disable(CLK(AUD_GATE_PCLK_DBG));
		pwrcal_gate_disable(CLK(AUD_GATE_ACLK_ATCLK_AUD));
		pwrcal_gate_disable(CLK(AUD_GATE_PCLK_I2S));
		pwrcal_gate_disable(CLK(AUD_GATE_PCLK_PCM));
		pwrcal_gate_disable(CLK(AUD_GATE_PCLK_SLIMBUS));
		pwrcal_gate_disable(CLK(AUD_GATE_SCLK_I2S));
		pwrcal_gate_disable(CLK(AUD_GATE_SCLK_I2S_BCLK));
		pwrcal_gate_disable(CLK(AUD_GATE_SCLK_PCM));
		pwrcal_gate_disable(CLK(AUD_GATE_SCLK_ASRC));
		pwrcal_gate_disable(CLK(AUD_GATE_SCLK_SLIMBUS));
		pwrcal_gate_disable(CLK(AUD_GATE_SCLK_SLIMBUS_CLKIN));
	}
}
static void fsys0_post(int enable)
{
	if (enable == 1) {
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_USBDRD30_UDRD30_PHYCLOCK_USER, 27, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_USBDRD30_UDRD30_PIPE_PCLK_USER, 27, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_UFS_TX0_SYMBOL_USER, 27, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_UFS_RX0_SYMBOL_USER, 27, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_USBHOST20_PHYCLOCK_USER, 27, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_USBHOST20PHY_REF_CLK, 27, 0);

		pwrcal_writel(CG_CTRL_MAN_ACLK_FSYS0_200,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_HPM_APBIF_FSYS0,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_USBDRD30_SUSPEND_CLK,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_MMC0,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_UFSUNIPRO_EMBEDDED,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_USBDRD30_REF_CLK,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_USBDRD30_UDRD30_PHYCLOCK,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_USBDRD30_UDRD30_PIPE_PCLK,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_UFS_TX0_SYMBOL,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_UFS_RX0_SYMBOL,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_USBHOST20_PHYCLOCK,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_USBHOST20_REF_CLK,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_PROMISE_FSYS0,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_USBHOST20PHY_REF_CLK,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_UFSUNIPRO_EMBEDDED_CFG,	0);

		pwrcal_writel(QCH_CTRL_AXI_LH_ASYNC_MI_TOP_FSYS0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_AXI_LH_ASYNC_MI_ETR_USB_FSYS0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_ETR_USB_FSYS0_ACLK,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_ETR_USB_FSYS0_PCLK,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_CMU_FSYS0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PMU_FSYS0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SYSREG_FSYS0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_USBDRD30,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_MMC0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_UFS_LINK_EMBEDDED,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_USBHOST20,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PDMA0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PDMAS,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PPMU_FSYS0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_ACEL_LH_ASYNC_SI_TOP_FSYS0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_USBDRD30_PHYCTRL,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_USBHOST20_PHYCTRL,	HWACG_QCH_ENABLE);

		pwrcal_writel(QSTATE_CTRL_USBDRD30,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_UFS_LINK_EMBEDDED,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_USBHOST20,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_USBHOST20_PHY,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_GPIO_FSYS0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_HPM_APBIF_FSYS0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PROMISE_FSYS0,	HWACG_QSTATE_CLOCK_ENABLE);

		pwrcal_writel(PAD_RETENTION_MMC0_OPTION, PAD_INITIATE_WAKEUP);
		pwrcal_writel(PAD_RETENTION_UFS_OPTION, PAD_INITIATE_WAKEUP);
		pwrcal_writel(PAD_RETENTION_USB_OPTION, PAD_INITIATE_WAKEUP);
	}
}
static void isp0_post(int enable)
{
	if (enable == 1) {
		pwrcal_writel(ISP0_DRCG_EN, 0x7);

		pwrcal_gate_disable(CLK(ISP0_GATE_CLK_AXI_LH_ASYNC_SI_TOP_ISP0));
		pwrcal_gate_disable(CLK(ISP0_GATE_CLK_C_TREX_C));
		pwrcal_gate_disable(CLK(ISP0_GATE_ACLK_SysMMU601));
		pwrcal_gate_disable(CLK(ISP0_GATE_PCLK_HPM_APBIF_ISP0));
		pwrcal_gate_disable(CLK(ISP0_GATE_PCLK_TREX_C));
		pwrcal_gate_disable(CLK(ISP0_GATE_PCLK_SysMMU601));
		pwrcal_gate_disable(CLK(ISP0_GATE_ACLK_FIMC_ISP0));
		pwrcal_gate_disable(CLK(ISP0_GATE_PCLK_FIMC_ISP0));
		pwrcal_gate_disable(CLK(ISP0_GATE_ACLK_FIMC_TPU));
		pwrcal_gate_disable(CLK(ISP0_GATE_PCLK_FIMC_TPU));
		pwrcal_gate_disable(CLK(ISP0_GATE_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D));
	}
}
static void isp1_post(int enable)
{
	if (enable == 1) {
		pwrcal_writel(ISP1_DRCG_EN, 0x3);

		pwrcal_gate_disable(CLK(ISP1_GATE_ACLK_FIMC_ISP1));
		pwrcal_gate_disable(CLK(ISP1_GATE_ACLK_XIU_N_ASYNC_SI));
		pwrcal_gate_disable(CLK(ISP1_GATE_PCLK_FIMC_ISP1));
		pwrcal_gate_disable(CLK(ISP1_GATE_PCLK_SYSREG_ISP1));
	}
}
static void mfc_post(int enable)
{
	if (enable == 1) {
		pwrcal_writel(MFC_DRCG_EN, 0x7);

		pwrcal_writel(CG_CTRL_MAN_ACLK_MFC_600,	0);
		pwrcal_writel(CG_CTRL_MAN_ACLK_MFC_600_SECURE_SFW_MFC_0,	0);
		pwrcal_writel(CG_CTRL_MAN_ACLK_MFC_600_SECURE_SFW_MFC_1,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_MFC_150,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_MFC_150_HPM_APBIF_MFC,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_MFC_150_SECURE_SFW_MFC_0,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_MFC_150_SECURE_SFW_MFC_1,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_MFC_PROMISE,	0);

		pwrcal_writel(QCH_CTRL_MFC,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_LH_M_MFC,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_CMU_MFC,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PMU_MFC,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SYSREG_MFC,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PPMU_MFC_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PPMU_MFC_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SFW_MFC_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SFW_MFC_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SMMU_MFC_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SMMU_MFC_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_LH_S_MFC_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_LH_S_MFC_1,	HWACG_QCH_ENABLE);

		pwrcal_writel(QSTATE_CTRL_HPM_APBIF_MFC,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PROMISE_MFC,	HWACG_QSTATE_CLOCK_ENABLE);
	}
}
static void disp1_post(int enable)
{
	if (enable == 1) {
		pwrcal_writel(DISP1_DRCG_EN, 0x7F);

		pwrcal_writel(CG_CTRL_MAN_ACLK_DISP1_0_400,	0);
		pwrcal_writel(CG_CTRL_MAN_ACLK_DISP1_1_400,	0);
		pwrcal_writel(CG_CTRL_MAN_ACLK_DISP1_0_400_SECURE_SFW_DISP1_0,	0);
		pwrcal_writel(CG_CTRL_MAN_ACLK_DISP1_1_400_SECURE_SFW_DISP1_1,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_DISP1_0_133,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_DISP1_0_133_HPM_APBIF_DISP1,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_DISP1_0_133_SECURE_SFW_DISP1_0,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_DISP1_0_133_SECURE_SFW_DISP1_1,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_DECON1_ECLK_0,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_DECON1_ECLK_1,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_DISP1_PROMISE,	0);

		pwrcal_writel(QCH_CTRL_AXI_LH_ASYNC_MI_DISP1SFR,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_CMU_DISP1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PMU_DISP1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SYSREG_DISP1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_VPP1_G2,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_VPP1_G3,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_DECON1_PCLK_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_DECON1_PCLK_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PPMU_DISP1_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PPMU_DISP1_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SMMU_DISP1_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SMMU_DISP1_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SFW_DISP1_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SFW_DISP1_1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_AXI_LH_ASYNC_SI_DISP1_0,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_AXI_LH_ASYNC_SI_DISP1_1,	HWACG_QCH_ENABLE);

		pwrcal_writel(QSTATE_CTRL_DECON1_ECLK_0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_DECON1_ECLK_1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_HPM_APBIF_DISP1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PROMISE_DISP1,	HWACG_QSTATE_CLOCK_ENABLE);
	}
}
static void fsys1_post(int enable)
{
	if (enable == 1) {
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_UFS_LINK_SDCARD_TX0_SYMBOL_USER, 27, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_UFS_LINK_SDCARD_RX0_SYMBOL_USER, 27, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI0_TX0_USER, 27, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI0_RX0_USER, 27, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI1_TX0_USER, 27, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI1_RX0_USER, 27, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI0_DIG_REFCLK_USER, 27, 0);
		pwrcal_setbit(CLK_CON_MUX_PHYCLK_PCIE_WIFI1_DIG_REFCLK_USER, 27, 0);

		pwrcal_writel(CG_CTRL_MAN_ACLK_FSYS1_200,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_HPM_APBIF_FSYS1,	0);
		pwrcal_writel(CG_CTRL_MAN_PCLK_COMBO_PHY_WIFI,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_MMC2,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_UFSUNIPRO_SDCARD,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_UFSUNIPRO_SDCARD_CFG,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_FSYS1_PCIE0_PHY,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_UFS_LINK_SDCARD_TX0_SYMBOL,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_UFS_LINK_SDCARD_RX0_SYMBOL,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_PCIE_WIFI0_TX0,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_PCIE_WIFI0_RX0,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_PCIE_WIFI1_TX0,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_PCIE_WIFI1_RX0,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_PCIE_WIFI0_DIG_REFCLK,	0);
		pwrcal_writel(CG_CTRL_MAN_PHYCLK_PCIE_WIFI1_DIG_REFCLK,	0);
		pwrcal_writel(CG_CTRL_MAN_SCLK_PROMISE_FSYS1,	0);

		pwrcal_writel(QCH_CTRL_AXI_LH_ASYNC_MI_TOP_FSYS1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_CMU_FSYS1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PMU_FSYS1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_SYSREG_FSYS1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_MMC2,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_UFS_LINK_SDCARD,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PPMU_FSYS1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_ACEL_LH_ASYNC_SI_TOP_FSYS1,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PCIE_RC_LINK_WIFI0_SLV,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PCIE_RC_LINK_WIFI0_DBI,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PCIE_RC_LINK_WIFI1_SLV,	HWACG_QCH_ENABLE);
		pwrcal_writel(QCH_CTRL_PCIE_RC_LINK_WIFI1_DBI,	HWACG_QCH_ENABLE);

		pwrcal_writel(QSTATE_CTRL_SROMC_FSYS1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_GPIO_FSYS1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_HPM_APBIF_FSYS1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PROMISE_FSYS1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PCIE_RC_LINK_WIFI0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PCIE_RC_LINK_WIFI1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PCIE_PCS_WIFI0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PCIE_PCS_WIFI1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PCIE_PHY_FSYS1_WIFI0,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_PCIE_PHY_FSYS1_WIFI1,	HWACG_QSTATE_CLOCK_ENABLE);
		pwrcal_writel(QSTATE_CTRL_UFS_LINK_SDCARD,	HWACG_QSTATE_CLOCK_ENABLE);

		pwrcal_writel(PAD_RETENTION_MMC2_OPTION, PAD_INITIATE_WAKEUP);
		pwrcal_writel(PAD_RETENTION_UFS_CARD_OPTION, PAD_INITIATE_WAKEUP);
		pwrcal_writel(PAD_RETENTION_PCIE_OPTION, PAD_INITIATE_WAKEUP);
	}
}

static void cam0_config(int enable)
{
	pwrcal_setf(CAM0_OPTION, 0, 0xFFFFFFFF, 0x2);
}
static void mscl_config(int enable)
{
	pwrcal_setf(MSCL_OPTION, 0, 0xFFFFFFFF, 0x2);
}
static void g3d_config(int enable)
{
	pwrcal_setf(G3D_OPTION, 0, 0xFFFFFFFF, 0x1);
	pwrcal_setf(G3D_DURATION0, 8, 0xF, 0x3);
	pwrcal_setf(G3D_DURATION0, 4, 0xF, 0x6);
	pwrcal_setf(G3D_DURATION0, 0, 0xF, 0x3);
	pwrcal_setf(MEMORY_G3D_OPTION, 0, 0xFFFFFFFF, 0x1);
}
static void disp0_config(int enable)
{
	pwrcal_setf(DISP0_OPTION, 0, 0xFFFFFFFF, 0x2);
}
static void cam1_config(int enable)
{
	pwrcal_setf(CAM1_OPTION, 0, 0xFFFFFFFF, 0x2);
}
static void aud_config(int enable)
{
	pwrcal_setf(AUD_OPTION, 0, 0xFFFFFFFF, 0x2);
	pwrcal_setf(PAD_RETENTION_AUD_OPTION, 0, 0xFFFFFFFF, 0x10000000);
}
static void fsys0_config(int enable)
{
	pwrcal_setf(FSYS0_OPTION, 0, 0xFFFFFFFF, 0x2);
}
static void isp0_config(int enable)
{
	pwrcal_setf(ISP0_OPTION, 0, 0xFFFFFFFF, 0x2);
}
static void isp1_config(int enable)
{
	pwrcal_setf(ISP1_OPTION, 0, 0xFFFFFFFF, 0x2);
}
static void mfc_config(int enable)
{
	pwrcal_setf(MFC_OPTION, 0, 0xFFFFFFFF, 0x2);
}
static void disp1_config(int enable)
{
	pwrcal_setf(DISP1_OPTION, 0, 0xFFFFFFFF, 0x2);
}
static void fsys1_config(int enable)
{
	pwrcal_setf(FSYS1_OPTION, 0, 0xFFFFFFFF, 0x2);
}



BLKPWR(blkpwr_cam0,	CAM0_CONFIGURATION,	0,	0xF,	CAM0_STATUS,	0,	0xF,	cam0_config,	cam0_prev,	cam0_post);
BLKPWR(blkpwr_mscl,	MSCL_CONFIGURATION,	0,	0xF,	MSCL_STATUS,	0,	0xF,	mscl_config,	mscl_prev,	mscl_post);
BLKPWR(blkpwr_g3d,	G3D_CONFIGURATION,	0,	0xF,	G3D_STATUS,	0,	0xF,	g3d_config,	g3d_prev,	g3d_post);
BLKPWR(blkpwr_disp0,	DISP0_CONFIGURATION,	0,	0xF,	DISP0_STATUS,	0,	0xF,	disp0_config,	disp0_prev,	disp0_post);
BLKPWR(blkpwr_cam1,	CAM1_CONFIGURATION,	0,	0xF,	CAM1_STATUS,	0,	0xF,	cam1_config,	cam1_prev,	cam1_post);
BLKPWR(blkpwr_aud,	AUD_CONFIGURATION,	0,	0xF,	AUD_STATUS,	0,	0xF,	aud_config,	aud_prev,	aud_post);
BLKPWR(blkpwr_fsys0,	FSYS0_CONFIGURATION,	0,	0xF,	FSYS0_STATUS,	0,	0xF,	fsys0_config,	fsys0_prev,	fsys0_post);
BLKPWR(blkpwr_isp0,	ISP0_CONFIGURATION,	0,	0xF,	ISP0_STATUS,	0,	0xF,	isp0_config,	isp0_prev,	isp0_post);
BLKPWR(blkpwr_isp1,	ISP1_CONFIGURATION,	0,	0xF,	ISP1_STATUS,	0,	0xF,	isp1_config,	isp1_prev,	isp1_post);
BLKPWR(blkpwr_mfc,	MFC_CONFIGURATION,	0,	0xF,	MFC_STATUS,	0,	0xF,	mfc_config,	mfc_prev,	mfc_post);
BLKPWR(blkpwr_disp1,	DISP1_CONFIGURATION,	0,	0xF,	DISP1_STATUS,	0,	0xF,	disp1_config,	disp1_prev,	disp1_post);
BLKPWR(blkpwr_fsys1,	FSYS1_CONFIGURATION,	0,	0xF,	FSYS1_STATUS,	0,	0xF,	fsys1_config,	fsys1_prev,	fsys1_post);

struct cal_pd *pwrcal_blkpwr_list[12];
unsigned int pwrcal_blkpwr_size = 12;

static int blkpwr_int(void)
{
	pwrcal_blkpwr_list[0] = &blkpwr_blkpwr_isp0;
	pwrcal_blkpwr_list[1] = &blkpwr_blkpwr_isp1;
	pwrcal_blkpwr_list[2] = &blkpwr_blkpwr_cam1;
	pwrcal_blkpwr_list[3] = &blkpwr_blkpwr_cam0;
	pwrcal_blkpwr_list[4] = &blkpwr_blkpwr_mscl;
	pwrcal_blkpwr_list[5] = &blkpwr_blkpwr_g3d;
	pwrcal_blkpwr_list[6] = &blkpwr_blkpwr_disp0;
	pwrcal_blkpwr_list[7] = &blkpwr_blkpwr_aud;
	pwrcal_blkpwr_list[8] = &blkpwr_blkpwr_fsys0;
	pwrcal_blkpwr_list[9] = &blkpwr_blkpwr_mfc;
	pwrcal_blkpwr_list[10] = &blkpwr_blkpwr_disp1;
	pwrcal_blkpwr_list[11] = &blkpwr_blkpwr_fsys1;

	return 0;
}

struct cal_pd_ops cal_pd_ops = {
	.pd_control = blkpwr_control,
	.pd_status = blkpwr_status,
	.pd_init = blkpwr_int,
};
