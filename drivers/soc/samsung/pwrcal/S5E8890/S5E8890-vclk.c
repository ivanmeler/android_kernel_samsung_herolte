#include "../pwrcal-pmu.h"
#include "../pwrcal-rae.h"
#include "S5E8890-cmu.h"
#include "S5E8890-cmusfr.h"
#include "S5E8890-vclk.h"
#include "S5E8890-vclk-internal.h"


struct pwrcal_vclk_grpgate *vclk_grpgate_list[num_of_grpgate];
struct pwrcal_vclk_m1d1g1 *vclk_m1d1g1_list[num_of_m1d1g1];
struct pwrcal_vclk_p1 *vclk_p1_list[num_of_p1];
struct pwrcal_vclk_m1 *vclk_m1_list[num_of_m1];
struct pwrcal_vclk_d1 *vclk_d1_list[num_of_d1];
struct pwrcal_vclk_pxmxdx *vclk_pxmxdx_list[num_of_pxmxdx];
struct pwrcal_vclk_umux *vclk_umux_list[num_of_umux];
struct pwrcal_vclk_dfs *vclk_dfs_list[num_of_dfs];
unsigned int vclk_grpgate_list_size = num_of_grpgate;
unsigned int vclk_m1d1g1_list_size = num_of_m1d1g1;
unsigned int vclk_p1_list_size = num_of_p1;
unsigned int vclk_m1_list_size;
unsigned int vclk_d1_list_size = num_of_d1;
unsigned int vclk_pxmxdx_list_size = num_of_pxmxdx;
unsigned int vclk_umux_list_size = num_of_umux;
unsigned int vclk_dfs_list_size = num_of_dfs;

#define ADD_LIST(to, x)		to[x & 0xFFFF] = &(vclk_##x)


static struct pwrcal_clk_set pxmxdx_top_grp[] = {
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_mfc_grp[] = {
	{CLK(MFC_MUX_ACLK_MFC_600_USER),	1,	0},
	{CLK(MFC_DIV_PCLK_MFC_150),	3,	-1},
	{CLK(TOP_MUXGATE_ACLK_MFC_600),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_mscl_grp[] = {
	{CLK(MSCL_MUX_ACLK_MSCL0_528_USER),	1,	0},
	{CLK(MSCL_MUX_ACLK_MSCL1_528_USER),	0,	0},
	{CLK(MSCL_MUX_ACLK_MSCL1_528),	0,	-1},
	{CLK(MSCL_DIV_PCLK_MSCL),	3,	-1},
	{CLK(TOP_MUXGATE_ACLK_MSCL0_528),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_MSCL1_528),	0,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_imem_grp[] = {
	{CLK(IMEM_MUX_ACLK_IMEM_266_USER),	1,	0},
	{CLK(IMEM_MUX_ACLK_IMEM_200_USER),	1,	0},
	{CLK(IMEM_MUX_ACLK_IMEM_100_USER),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_IMEM_266),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_IMEM_200),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_IMEM_100),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_fsys0_grp[] = {
	{CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),	1,	0},
/*	{CLK(TOP_MUXGATE_ACLK_FSYS0_200),	1,	0},	*/
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_fsys1_grp[] = {
	{CLK(FSYS1_MUX_ACLK_FSYS1_200_USER),	1,	0},
	{CLK(FSYS1_DIV_PCLK_COMBO_PHY_WIFI),	1,	-1},
	{CLK(TOP_MUXGATE_ACLK_FSYS1_200),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_disp0_grp[] = {
	{CLK(DISP0_MUX_ACLK_DISP0_0_400_USER),	1,	0},
	{CLK(DISP0_MUX_ACLK_DISP0_1_400_USER),	1,	0},
	{CLK(DISP0_MUX_ACLK_DISP0_1_400),	0,	0},
	{CLK(DISP0_DIV_PCLK_DISP0_0_133),	2,	-1},
	{CLK(DISP0_MUX_SCLK_DISP0_HDMI_AUDIO),	1,	-1},
	{CLK(DISP0_DIV_PHYCLK_HDMIPHY_TMDS_20B_CLKO),	1,	-1},
	{CLK(DISP0_DIV_PHYCLK_HDMIPHY_PIXEL_CLKO),	1,	-1},
	{CLK(TOP_MUXGATE_ACLK_DISP0_0_400),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_DISP0_1_400),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_disp1_grp[] = {
	{CLK(DISP1_MUX_ACLK_DISP1_0_400_USER),	1,	0},
	{CLK(DISP1_MUX_ACLK_DISP1_1_400_USER),	1,	0},
	{CLK(DISP1_MUX_ACLK_DISP1_1_400),	0,	0},
	{CLK(DISP1_DIV_PCLK_DISP1_0_133),	3,	-1},
	{CLK(DISP1_MUX_SCLK_DISP1_600_USER),	1,	-1},
	{CLK(TOP_MUXGATE_ACLK_DISP1_0_400),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_DISP1_1_400),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_aud_grp[] = {
	{CLK(AUD_DIV_AUD_CA5),			0,	-1},
	{CLK(AUD_DIV_AUD_CDCLK),		1,	-1},
	{CLK(AUD_DIV_ACLK_AUD),		2,	-1},
	{CLK(AUD_DIV_PCLK_DBG),		5,	-1},
	{CLK(AUD_DIV_ATCLK_AUD),		1,	-1},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_aud_cp_grp[] = {
	{CLK(AUD_MUX_CP2AP_AUD_CLK_USER),	1,	0},
	{CLK(AUD_DIV_CP_CA5),			0,	-1},
	{CLK(AUD_DIV_CP_CDCLK),		0,	-1},
	{CLK(AUD_MUX_CDCLK_AUD),		1,	0},
	{CLK(AUD_MUX_ACLK_CA5),		1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_isp0_isp0_grp[] = {
	{CLK(ISP0_MUX_ACLK_ISP0_528_USER),	1,	0},
	{CLK(ISP0_DIV_PCLK_ISP0),		1,	-1},
	{CLK(TOP_MUXGATE_ACLK_ISP0_ISP0_528),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_isp0_tpu_grp[] = {
	{CLK(ISP0_MUX_ACLK_ISP0_TPU_400_USER),	1,	0},
	{CLK(ISP0_DIV_PCLK_ISP0_TPU),		1,	-1},
	{CLK(TOP_MUXGATE_ACLK_ISP0_TPU_400),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_isp0_trex_grp[] = {
	{CLK(ISP0_MUX_ACLK_ISP0_TREX_528_USER),	1,	0},
	{CLK(ISP0_DIV_PCLK_ISP0_TREX_264),	1,	-1},
	{CLK(ISP0_DIV_PCLK_ISP0_TREX_132),	3,	-1},
	{CLK(TOP_MUXGATE_ACLK_ISP0_TREX_528),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_isp0_pxl_asbs_grp[] = {
	{CLK(ISP0_MUX_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D_USER),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_isp1_isp1_grp[] = {
	{CLK(ISP1_MUX_ACLK_ISP1_468_USER),	1,	0},
	{CLK(ISP1_DIV_PCLK_ISP1_234),		1,	-1},
	{CLK(TOP_MUXGATE_ACLK_ISP1_ISP1_468),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam0_csis0_grp[] = {
	{CLK(CAM0_MUX_ACLK_CAM0_CSIS0_414_USER),	1,	0},
	{CLK(CAM0_DIV_PCLK_CAM0_CSIS0_207),	1,	-1},
	{CLK(TOP_MUXGATE_ACLK_CAM0_CSIS0_414),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam0_csis1_grp[] = {
	{CLK(CAM0_MUX_ACLK_CAM0_CSIS1_168_USER),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_CAM0_CSIS1_168),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam0_csis2_grp[] = {
	{CLK(CAM0_MUX_ACLK_CAM0_CSIS2_234_USER),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_CAM0_CSIS2_234),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam0_csis3_grp[] = {
	{CLK(CAM0_MUX_ACLK_CAM0_CSIS3_132_USER),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_CAM0_CSIS3_132),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam0_3aa0_grp[] = {
	{CLK(CAM0_MUX_ACLK_CAM0_3AA0_414_USER),	1,	0},
	{CLK(CAM0_DIV_PCLK_CAM0_3AA0_207),	1,	-1},
	{CLK(TOP_MUXGATE_ACLK_CAM0_3AA0_414),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam0_3aa1_grp[] = {
	{CLK(CAM0_MUX_ACLK_CAM0_3AA1_414_USER),	1,	0},
	{CLK(CAM0_DIV_PCLK_CAM0_3AA1_207),	1,	-1},
	{CLK(TOP_MUXGATE_ACLK_CAM0_3AA1_414),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam0_trex_grp[] = {
	{CLK(CAM0_MUX_ACLK_CAM0_TREX_528_USER),	1,	0},
	{CLK(CAM0_DIV_PCLK_CAM0_TREX_264),	1,	-1},
	{CLK(CAM0_DIV_PCLK_CAM0_TREX_132),	3,	-1},
	{CLK(TOP_MUXGATE_ACLK_CAM0_TREX_528),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam1_arm_grp[] = {
	{CLK(CAM1_MUX_ACLK_CAM1_ARM_672_USER),	1,	0},
	{CLK(CAM1_DIV_PCLK_CAM1_ARM_168),	3,	-1},
	{CLK(TOP_MUXGATE_ACLK_CAM1_ARM_672),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam1_vra_grp[] = {
	{CLK(CAM1_MUX_ACLK_CAM1_TREX_VRA_528_USER),	1,	0},
	{CLK(CAM1_DIV_PCLK_CAM1_TREX_VRA_264),		1,	-1},
	{CLK(TOP_MUXGATE_ACLK_CAM1_TREX_VRA_528),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam1_trex_grp[] = {
	{CLK(CAM1_MUX_ACLK_CAM1_TREX_B_528_USER),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_CAM1_TREX_B_528),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam1_bus_grp[] = {
	{CLK(CAM1_MUX_ACLK_CAM1_BUS_264_USER),	1,	0},
	{CLK(CAM1_DIV_PCLK_CAM1_BUS_132),	1,	-1},
	{CLK(TOP_MUXGATE_ACLK_CAM1_BUS_264),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam1_peri_grp[] = {
	{CLK(CAM1_MUX_ACLK_CAM1_PERI_84_USER),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_CAM1_PERI_84),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam1_csis2_grp[] = {
	{CLK(CAM1_MUX_ACLK_CAM1_CSIS2_414_USER),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_CAM1_CSIS2_414),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam1_csis3_grp[] = {
	{CLK(CAM1_MUX_ACLK_CAM1_CSIS3_132_USER),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_CAM1_CSIS3_132),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_cam1_scl_grp[] = {
	{CLK(CAM1_MUX_ACLK_CAM1_SCL_566_USER),	1,	0},
	{CLK(TOP_MUXGATE_ACLK_CAM1_SCL_566),	1,	0},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_oscclk_nfc_grp[] = {
	{CLK(CLKOUT_TCXO_26M),			1,	0},
	{CLK(CLKOUT_OSCCLK_NFC),		0,	1},
	{CLK_NONE,				0,	0},
};
static struct pwrcal_clk_set pxmxdx_oscclk_aud_grp[] = {
	{CLK(CLKOUT_TCXO_IN0),			1,	0},
	{CLK(CLKOUT_TCXO_IN1),			1,	0},
	{CLK(CLKOUT_TCXO_IN2),			1,	0},
	{CLK(CLKOUT_TCXO_IN3),			1,	0},
	{CLK(CLKOUT_TCXO_IN4),			1,	0},
	{CLK(CLKOUT_CLKOUT0_DISABLE),		0,	1},
	{CLK_NONE,				0,	0},
};

PXMXDX(pxmxdx_top,	0,	pxmxdx_top_grp);
PXMXDX(pxmxdx_mfc,	gate_bus1_mfc,	pxmxdx_mfc_grp);
PXMXDX(pxmxdx_mscl,	gate_bus1_mscl,	pxmxdx_mscl_grp);
PXMXDX(pxmxdx_imem,	0,	pxmxdx_imem_grp);
PXMXDX(pxmxdx_fsys0,	gate_bus1_fsys0,	pxmxdx_fsys0_grp);
PXMXDX(pxmxdx_fsys1,	gate_bus0_fsys1,	pxmxdx_fsys1_grp);
PXMXDX(pxmxdx_disp0,	gate_bus0_display,	pxmxdx_disp0_grp);
PXMXDX(pxmxdx_disp1,	gate_bus0_display,	pxmxdx_disp1_grp);
PXMXDX(pxmxdx_aud,	0,	pxmxdx_aud_grp);
PXMXDX(pxmxdx_aud_cp,	0,	pxmxdx_aud_cp_grp);
PXMXDX(pxmxdx_isp0_isp0,	gate_bus0_cam,	pxmxdx_isp0_isp0_grp);
PXMXDX(pxmxdx_isp0_tpu,	gate_bus0_cam,	pxmxdx_isp0_tpu_grp);
PXMXDX(pxmxdx_isp0_trex,	gate_bus0_cam,	pxmxdx_isp0_trex_grp);
PXMXDX(pxmxdx_isp0_pxl_asbs,	gate_bus0_cam,	pxmxdx_isp0_pxl_asbs_grp);
PXMXDX(pxmxdx_isp1_isp1,	gate_bus0_cam,	pxmxdx_isp1_isp1_grp);
PXMXDX(pxmxdx_cam0_csis0,	gate_bus0_cam,	pxmxdx_cam0_csis0_grp);
PXMXDX(pxmxdx_cam0_csis1,	gate_bus0_cam,	pxmxdx_cam0_csis1_grp);
PXMXDX(pxmxdx_cam0_csis2,	gate_bus0_cam,	pxmxdx_cam0_csis2_grp);
PXMXDX(pxmxdx_cam0_csis3,	gate_bus0_cam,	pxmxdx_cam0_csis3_grp);
PXMXDX(pxmxdx_cam0_3aa0,	gate_bus0_cam,	pxmxdx_cam0_3aa0_grp);
PXMXDX(pxmxdx_cam0_3aa1,	gate_bus0_cam,	pxmxdx_cam0_3aa1_grp);
PXMXDX(pxmxdx_cam0_trex,	gate_bus0_cam,	pxmxdx_cam0_trex_grp);
PXMXDX(pxmxdx_cam1_arm,	gate_bus0_cam,	pxmxdx_cam1_arm_grp);
PXMXDX(pxmxdx_cam1_vra,	gate_bus0_cam,	pxmxdx_cam1_vra_grp);
PXMXDX(pxmxdx_cam1_trex,	gate_bus0_cam,	pxmxdx_cam1_trex_grp);
PXMXDX(pxmxdx_cam1_bus,	gate_bus0_cam,	pxmxdx_cam1_bus_grp);
PXMXDX(pxmxdx_cam1_peri,	gate_bus0_cam,	pxmxdx_cam1_peri_grp);
PXMXDX(pxmxdx_cam1_csis2,	gate_bus0_cam,	pxmxdx_cam1_csis2_grp);
PXMXDX(pxmxdx_cam1_csis3,	gate_bus0_cam,	pxmxdx_cam1_csis3_grp);
PXMXDX(pxmxdx_cam1_scl,	gate_bus0_cam,	pxmxdx_cam1_scl_grp);
PXMXDX(pxmxdx_oscclk_nfc,	0,	pxmxdx_oscclk_nfc_grp);
PXMXDX(pxmxdx_oscclk_aud,	0,	pxmxdx_oscclk_aud_grp);

M1D1G1(sclk_decon0_eclk0,	0,	TOP_MUX_SCLK_DISP0_DECON0_ECLK0,	TOP_DIV_SCLK_DISP0_DECON0_ECLK0,	TOP_GATE_SCLK_DISP0_DECON0_ECLK0,	DISP0_MUX_SCLK_DISP0_DECON0_ECLK0_USER);
M1D1G1(sclk_decon0_vclk0,	0,	TOP_MUX_SCLK_DISP0_DECON0_VCLK0,	TOP_DIV_SCLK_DISP0_DECON0_VCLK0,	TOP_GATE_SCLK_DISP0_DECON0_VCLK0,	DISP0_MUX_SCLK_DISP0_DECON0_VCLK0_USER);
M1D1G1(sclk_decon0_vclk1,	0,	TOP_MUX_SCLK_DISP0_DECON0_VCLK1,	TOP_DIV_SCLK_DISP0_DECON0_VCLK1,	TOP_GATE_SCLK_DISP0_DECON0_VCLK1,	DISP0_MUX_SCLK_DISP0_DECON0_VCLK1_USER);
M1D1G1(sclk_hdmi_audio,	0,	TOP_MUX_SCLK_DISP0_HDMI_AUDIO,	TOP_DIV_SCLK_DISP0_HDMI_AUDIO,	TOP_GATE_SCLK_DISP0_HDMI_AUDIO,	DISP0_MUX_SCLK_DISP0_HDMI_AUDIO_USER);
M1D1G1(sclk_decon1_eclk0,	0,	TOP_MUX_SCLK_DISP1_DECON1_ECLK0,	TOP_DIV_SCLK_DISP1_DECON1_ECLK0,	TOP_GATE_SCLK_DISP1_DECON1_ECLK0,	DISP1_MUX_SCLK_DISP1_DECON1_ECLK0_USER);
M1D1G1(sclk_decon1_eclk1,	0,	TOP_MUX_SCLK_DISP1_DECON1_ECLK1,	TOP_DIV_SCLK_DISP1_DECON1_ECLK1,	TOP_GATE_SCLK_DISP1_DECON1_ECLK1,	DISP1_MUX_SCLK_DISP1_DECON1_ECLK1_USER);
M1D1G1(sclk_usbdrd30,	0,	TOP_MUX_SCLK_FSYS0_USBDRD30,	TOP_DIV_SCLK_FSYS0_USBDRD30,	TOP_GATE_SCLK_FSYS0_USBDRD30,	FSYS0_MUX_SCLK_FSYS0_USBDRD30_USER);
M1D1G1(sclk_mmc0,	0,	TOP_MUX_SCLK_FSYS0_MMC0,	TOP_DIV_SCLK_FSYS0_MMC0,	TOP_GATE_SCLK_FSYS0_MMC0,	FSYS0_MUX_SCLK_FSYS0_MMC0_USER);
M1D1G1(sclk_ufsunipro20,	0,	TOP_MUX_SCLK_FSYS0_UFSUNIPRO20,	TOP_DIV_SCLK_FSYS0_UFSUNIPRO20,	TOP_GATE_SCLK_FSYS0_UFSUNIPRO20,	FSYS0_MUX_SCLK_FSYS0_UFSUNIPRO_EMBEDDED_USER);
M1D1G1(sclk_phy24m,	0,	TOP_MUX_SCLK_FSYS0_PHY_24M,	TOP_DIV_SCLK_FSYS0_PHY_24M,	TOP_GATE_SCLK_FSYS0_PHY_24M,	FSYS0_MUX_SCLK_FSYS0_24M_USER);
M1D1G1(sclk_ufsunipro_cfg,	0,	TOP_MUX_SCLK_FSYS0_UFSUNIPRO_CFG,	TOP_DIV_SCLK_FSYS0_UFSUNIPRO_CFG,	TOP_GATE_SCLK_FSYS0_UFSUNIPRO_CFG,	FSYS0_MUX_SCLK_FSYS0_UFSUNIPRO_EMBEDDED_CFG_USER);
M1D1G1(sclk_mmc2,	0,	TOP_MUX_SCLK_FSYS1_MMC2,	TOP_DIV_SCLK_FSYS1_MMC2,	TOP_GATE_SCLK_FSYS1_MMC2,	FSYS1_MUX_SCLK_FSYS1_MMC2_USER);
M1D1G1(sclk_ufsunipro20_sdcard,	0,	TOP_MUX_SCLK_FSYS1_UFSUNIPRO20,	TOP_DIV_SCLK_FSYS1_UFSUNIPRO20,	TOP_GATE_SCLK_FSYS1_UFSUNIPRO20,	FSYS1_MUX_SCLK_FSYS1_UFSUNIPRO_SDCARD_USER);
M1D1G1(sclk_pcie_phy,	0,	TOP_MUX_SCLK_FSYS1_PCIE_PHY,	TOP_DIV_SCLK_FSYS1_PCIE_PHY,	TOP_GATE_SCLK_FSYS1_PCIE_PHY,	FSYS1_MUX_SCLK_FSYS1_PCIE_PHY_USER);
M1D1G1(sclk_ufsunipro_sdcard_cfg,	0,	TOP_MUX_SCLK_FSYS1_UFSUNIPRO_CFG,	TOP_DIV_SCLK_FSYS1_UFSUNIPRO_CFG,	TOP_GATE_SCLK_FSYS1_UFSUNIPRO_CFG,	FSYS1_MUX_SCLK_FSYS1_UFSUNIPRO_SDCARD_CFG_USER);
M1D1G1(sclk_uart0,	0,	TOP_MUX_SCLK_PERIC0_UART0,	TOP_DIV_SCLK_PERIC0_UART0,	TOP_GATE_SCLK_PERIC0_UART0,	PERIC0_MUX_SCLK_UART0_USER);
M1D1G1(sclk_spi0,	gate_peric1_spi_i2s_pcm1_spdif_common,	TOP_MUX_SCLK_PERIC1_SPI0,	TOP_DIV_SCLK_PERIC1_SPI0,	TOP_GATE_SCLK_PERIC1_SPI0,	PERIC1_MUX_SCLK_SPI0_USER);
M1D1G1(sclk_spi1,	gate_peric1_spi_i2s_pcm1_spdif_common,	TOP_MUX_SCLK_PERIC1_SPI1,	TOP_DIV_SCLK_PERIC1_SPI1,	TOP_GATE_SCLK_PERIC1_SPI1,	PERIC1_MUX_SCLK_SPI1_USER);
M1D1G1(sclk_spi2,	gate_peric1_spi_i2s_pcm1_spdif_common,	TOP_MUX_SCLK_PERIC1_SPI2,	TOP_DIV_SCLK_PERIC1_SPI2,	TOP_GATE_SCLK_PERIC1_SPI2,	PERIC1_MUX_SCLK_SPI2_USER);
M1D1G1(sclk_spi3,	gate_peric1_spi_i2s_pcm1_spdif_common,	TOP_MUX_SCLK_PERIC1_SPI3,	TOP_DIV_SCLK_PERIC1_SPI3,	TOP_GATE_SCLK_PERIC1_SPI3,	PERIC1_MUX_SCLK_SPI3_USER);
M1D1G1(sclk_spi4,	gate_peric1_spi_i2s_pcm1_spdif_common,	TOP_MUX_SCLK_PERIC1_SPI4,	TOP_DIV_SCLK_PERIC1_SPI4,	TOP_GATE_SCLK_PERIC1_SPI4,	PERIC1_MUX_SCLK_SPI4_USER);
M1D1G1(sclk_spi5,	gate_peric1_spi_i2s_pcm1_spdif_common,	TOP_MUX_SCLK_PERIC1_SPI5,	TOP_DIV_SCLK_PERIC1_SPI5,	TOP_GATE_SCLK_PERIC1_SPI5,	PERIC1_MUX_SCLK_SPI5_USER);
M1D1G1(sclk_spi6,	gate_peric1_spi_i2s_pcm1_spdif_common,	TOP_MUX_SCLK_PERIC1_SPI6,	TOP_DIV_SCLK_PERIC1_SPI6,	TOP_GATE_SCLK_PERIC1_SPI6,	PERIC1_MUX_SCLK_SPI6_USER);
M1D1G1(sclk_spi7,	gate_peric1_spi_i2s_pcm1_spdif_common,	TOP_MUX_SCLK_PERIC1_SPI7,	TOP_DIV_SCLK_PERIC1_SPI7,	TOP_GATE_SCLK_PERIC1_SPI7,	PERIC1_MUX_SCLK_SPI7_USER);
M1D1G1(sclk_uart1,	0,	TOP_MUX_SCLK_PERIC1_UART1,	TOP_DIV_SCLK_PERIC1_UART1,	TOP_GATE_SCLK_PERIC1_UART1,	PERIC1_MUX_SCLK_UART1_USER);
M1D1G1(sclk_uart2,	0,	TOP_MUX_SCLK_PERIC1_UART2,	TOP_DIV_SCLK_PERIC1_UART2,	TOP_GATE_SCLK_PERIC1_UART2,	PERIC1_MUX_SCLK_UART2_USER);
M1D1G1(sclk_uart3,	0,	TOP_MUX_SCLK_PERIC1_UART3,	TOP_DIV_SCLK_PERIC1_UART3,	TOP_GATE_SCLK_PERIC1_UART3,	PERIC1_MUX_SCLK_UART3_USER);
M1D1G1(sclk_uart4,	0,	TOP_MUX_SCLK_PERIC1_UART4,	TOP_DIV_SCLK_PERIC1_UART4,	TOP_GATE_SCLK_PERIC1_UART4,	PERIC1_MUX_SCLK_UART4_USER);
M1D1G1(sclk_uart5,	0,	TOP_MUX_SCLK_PERIC1_UART5,	TOP_DIV_SCLK_PERIC1_UART5,	TOP_GATE_SCLK_PERIC1_UART5,	PERIC1_MUX_SCLK_UART5_USER);
M1D1G1(sclk_promise_int,	0,	TOP_MUX_SCLK_PROMISE_INT,	TOP_DIV_SCLK_PROMISE_INT,	TOP_GATE_SCLK_PROMISE_INT,	0);
M1D1G1(sclk_promise_disp,	0,	TOP_MUX_SCLK_PROMISE_DISP,	TOP_DIV_SCLK_PROMISE_DISP,	TOP_GATE_SCLK_PROMISE_DISP,	0);
M1D1G1(sclk_ap2cp_mif_pll_out,	0,	TOP_MUX_SCLK_AP2CP_MIF_PLL_OUT,	TOP_DIV_SCLK_AP2CP_MIF_PLL_OUT,	TOP_GATE_SCLK_AP2CP_MIF_PLL_OUT,	0);
M1D1G1(sclk_isp_spi0,	gate_cam1_common,	TOP_MUX_SCLK_CAM1_ISP_SPI0,	TOP_DIV_SCLK_CAM1_ISP_SPI0,	TOP_GATE_SCLK_CAM1_ISP_SPI0,	CAM1_MUX_SCLK_CAM1_ISP_SPI0_USER);
M1D1G1(sclk_isp_spi1,	gate_cam1_common,	TOP_MUX_SCLK_CAM1_ISP_SPI1,	TOP_DIV_SCLK_CAM1_ISP_SPI1,	TOP_GATE_SCLK_CAM1_ISP_SPI1,	CAM1_MUX_SCLK_CAM1_ISP_SPI1_USER);
M1D1G1(sclk_isp_uart,	0,	TOP_MUX_SCLK_CAM1_ISP_UART,	TOP_DIV_SCLK_CAM1_ISP_UART,	TOP_GATE_SCLK_CAM1_ISP_UART,	CAM1_MUX_SCLK_CAM1_ISP_UART_USER);
M1D1G1(sclk_isp_sensor0,	0,	TOP_MUX_SCLK_ISP_SENSOR0,	TOP_DIV_SCLK_ISP_SENSOR0,	TOP_GATE_SCLK_ISP_SENSOR0,	0);
M1D1G1(sclk_isp_sensor1,	0,	TOP_MUX_SCLK_ISP_SENSOR1,	TOP_DIV_SCLK_ISP_SENSOR1,	TOP_GATE_SCLK_ISP_SENSOR1,	0);
M1D1G1(sclk_isp_sensor2,	0,	TOP_MUX_SCLK_ISP_SENSOR2,	TOP_DIV_SCLK_ISP_SENSOR2,	TOP_GATE_SCLK_ISP_SENSOR2,	0);
M1D1G1(sclk_isp_sensor3,	0,	TOP_MUX_SCLK_ISP_SENSOR3,	TOP_DIV_SCLK_ISP_SENSOR3,	TOP_GATE_SCLK_ISP_SENSOR3,	0);
M1D1G1(sclk_decon0_eclk0_local,	0,	DISP0_MUX_SCLK_DISP0_DECON0_ECLK0,	DISP0_DIV_SCLK_DECON0_ECLK0,	0,	0);
M1D1G1(sclk_decon0_vclk0_local,	0,	DISP0_MUX_SCLK_DISP0_DECON0_VCLK0,	DISP0_DIV_SCLK_DECON0_VCLK0,	0,	0);
M1D1G1(sclk_decon0_vclk1_local,	0,	DISP0_MUX_SCLK_DISP0_DECON0_VCLK1,	DISP0_DIV_SCLK_DECON0_VCLK1,	0,	0);
M1D1G1(sclk_decon1_eclk0_local,	0,	DISP1_MUX_SCLK_DISP1_DECON1_ECLK0,	DISP1_DIV_SCLK_DECON1_ECLK0,	0,	0);
M1D1G1(sclk_decon1_eclk1_local,	0,	DISP1_MUX_SCLK_DISP1_DECON1_ECLK1,	DISP1_DIV_SCLK_DECON1_ECLK1,	0,	0);

P1(p1_disp_pll,	0,	DISP_PLL);
P1(p1_aud_pll,	0,	AUD_PLL);
P1(p1_mfc_pll,	0,	MFC_PLL_GATE);
P1(p1_bus3_pll,	0,	BUS3_PLL);

D1(d1_sclk_i2s_local,	pxmxdx_aud,	AUD_DIV_SCLK_I2S);
D1(d1_sclk_pcm_local,	pxmxdx_aud,	AUD_DIV_SCLK_PCM);
D1(d1_sclk_slimbus,	pxmxdx_aud,	AUD_DIV_SCLK_SLIMBUS);
D1(d1_sclk_cp_i2s,	pxmxdx_aud,	AUD_DIV_SCLK_CP_I2S);
D1(d1_sclk_asrc,	pxmxdx_aud,	AUD_DIV_SCLK_ASRC);

UMUX(umux_bus0_aclk_bus0_528,	0,	BUS0_MUX_ACLK_BUS0_528_USER);
UMUX(umux_bus0_aclk_bus0_200,	umux_bus0_aclk_bus0_528,	BUS0_MUX_ACLK_BUS0_200_USER);
UMUX(umux_bus1_aclk_bus1_528,	0,	BUS1_MUX_ACLK_BUS1_528_USER);
UMUX(umux_cam0_phyclk_rxbyteclkhs0_csis0_user,	0,	CAM0_MUX_PHYCLK_RXBYTECLKHS0_CSIS0_USER);
UMUX(umux_cam0_phyclk_rxbyteclkhs1_csis0_user,	0,	CAM0_MUX_PHYCLK_RXBYTECLKHS1_CSIS0_USER);
UMUX(umux_cam0_phyclk_rxbyteclkhs2_csis0_user,	0,	CAM0_MUX_PHYCLK_RXBYTECLKHS2_CSIS0_USER);
UMUX(umux_cam0_phyclk_rxbyteclkhs3_csis0_user,	0,	CAM0_MUX_PHYCLK_RXBYTECLKHS3_CSIS0_USER);
UMUX(umux_cam0_phyclk_rxbyteclkhs0_csis1_user,	0,	CAM0_MUX_PHYCLK_RXBYTECLKHS0_CSIS1_USER);
UMUX(umux_cam0_phyclk_rxbyteclkhs1_csis1_user,	0,	CAM0_MUX_PHYCLK_RXBYTECLKHS1_CSIS1_USER);
UMUX(umux_cam1_phyclk_rxbyteclkhs0_csis2_user,	0,	CAM1_MUX_PHYCLK_RXBYTECLKHS0_CSIS2_USER);
UMUX(umux_cam1_phyclk_rxbyteclkhs1_csis2_user,	0,	CAM1_MUX_PHYCLK_RXBYTECLKHS1_CSIS2_USER);
UMUX(umux_cam1_phyclk_rxbyteclkhs2_csis2_user,	0,	CAM1_MUX_PHYCLK_RXBYTECLKHS2_CSIS2_USER);
UMUX(umux_cam1_phyclk_rxbyteclkhs3_csis2_user,	0,	CAM1_MUX_PHYCLK_RXBYTECLKHS3_CSIS2_USER);
UMUX(umux_cam1_phyclk_rxbyteclkhs0_csis3_user,	0,	CAM1_MUX_PHYCLK_RXBYTECLKHS0_CSIS3_USER);
UMUX(umux_disp0_phyclk_hdmiphy_pixel_clko_user,	0,	DISP0_MUX_PHYCLK_HDMIPHY_PIXEL_CLKO_USER);
UMUX(umux_disp0_phyclk_hdmiphy_tmds_clko_user,	0,	DISP0_MUX_PHYCLK_HDMIPHY_TMDS_CLKO_USER);
UMUX(umux_disp0_phyclk_mipidphy0_rxclkesc0_user,	0,	DISP0_MUX_PHYCLK_MIPIDPHY0_RXCLKESC0_USER);
UMUX(umux_disp0_phyclk_mipidphy0_bitclkdiv2_user,	0,	DISP0_MUX_PHYCLK_MIPIDPHY0_BITCLKDIV2_USER);
UMUX(umux_disp0_phyclk_mipidphy0_bitclkdiv8_user,	0,	DISP0_MUX_PHYCLK_MIPIDPHY0_BITCLKDIV8_USER);
UMUX(umux_disp0_phyclk_mipidphy1_rxclkesc0_user,	0,	DISP0_MUX_PHYCLK_MIPIDPHY1_RXCLKESC0_USER);
UMUX(umux_disp0_phyclk_mipidphy1_bitclkdiv2_user,	0,	DISP0_MUX_PHYCLK_MIPIDPHY1_BITCLKDIV2_USER);
UMUX(umux_disp0_phyclk_mipidphy1_bitclkdiv8_user,	0,	DISP0_MUX_PHYCLK_MIPIDPHY1_BITCLKDIV8_USER);
UMUX(umux_disp0_phyclk_mipidphy2_rxclkesc0_user,	0,	DISP0_MUX_PHYCLK_MIPIDPHY2_RXCLKESC0_USER);
UMUX(umux_disp0_phyclk_mipidphy2_bitclkdiv2_user,	0,	DISP0_MUX_PHYCLK_MIPIDPHY2_BITCLKDIV2_USER);
UMUX(umux_disp0_phyclk_mipidphy2_bitclkdiv8_user,	0,	DISP0_MUX_PHYCLK_MIPIDPHY2_BITCLKDIV8_USER);
UMUX(umux_disp0_phyclk_dpphy_ch0_txd_clk_user,	0,	DISP0_MUX_PHYCLK_DPPHY_CH0_TXD_CLK_USER);
UMUX(umux_disp0_phyclk_dpphy_ch1_txd_clk_user,	0,	DISP0_MUX_PHYCLK_DPPHY_CH1_TXD_CLK_USER);
UMUX(umux_disp0_phyclk_dpphy_ch2_txd_clk_user,	0,	DISP0_MUX_PHYCLK_DPPHY_CH2_TXD_CLK_USER);
UMUX(umux_disp0_phyclk_dpphy_ch3_txd_clk_user,	0,	DISP0_MUX_PHYCLK_DPPHY_CH3_TXD_CLK_USER);
UMUX(umux_disp1_phyclk_mipidphy0_bitclkdiv2_user,	0,	DISP1_MUX_PHYCLK_MIPIDPHY0_BITCLKDIV2_USER);
UMUX(umux_disp1_phyclk_mipidphy1_bitclkdiv2_user,	0,	DISP1_MUX_PHYCLK_MIPIDPHY1_BITCLKDIV2_USER);
UMUX(umux_disp1_phyclk_mipidphy2_bitclkdiv2_user,	0,	DISP1_MUX_PHYCLK_MIPIDPHY2_BITCLKDIV2_USER);
UMUX(umux_disp1_phyclk_disp1_hdmiphy_pixel_clko_user,	0,	DISP1_MUX_PHYCLK_DISP1_HDMIPHY_PIXEL_CLKO_USER);
UMUX(umux_fsys0_phyclk_usbdrd30_udrd30_phyclock_user,	0,	FSYS0_MUX_PHYCLK_USBDRD30_UDRD30_PHYCLOCK_USER);
UMUX(umux_fsys0_phyclk_usbdrd30_udrd30_pipe_pclk_user,	0,	FSYS0_MUX_PHYCLK_USBDRD30_UDRD30_PIPE_PCLK_USER);
UMUX(umux_fsys0_phyclk_ufs_tx0_symbol_user,	0,	FSYS0_MUX_PHYCLK_UFS_TX0_SYMBOL_USER);
UMUX(umux_fsys0_phyclk_ufs_rx0_symbol_user,	0,	FSYS0_MUX_PHYCLK_UFS_RX0_SYMBOL_USER);
UMUX(umux_fsys0_phyclk_usbhost20_phyclock_user,	0,	FSYS0_MUX_PHYCLK_USBHOST20_PHYCLOCK_USER);
UMUX(umux_fsys0_phyclk_usbhost20_freeclk_user,	0,	0);
UMUX(umux_fsys0_phyclk_usbhost20_clk48mohci_user,	0,	0);
UMUX(umux_fsys0_phyclk_usbhost20phy_ref_clk,	0,	FSYS0_MUX_PHYCLK_USBHOST20PHY_REF_CLK);
UMUX(umux_fsys0_phyclk_ufs_rx_pwm_clk_user,	0,	0);
UMUX(umux_fsys0_phyclk_ufs_tx_pwm_clk_user,	0,	0);
UMUX(umux_fsys0_phyclk_ufs_refclk_out_soc_user,	0,	0);
UMUX(umux_fsys1_phyclk_ufs_link_sdcard_tx0_symbol_user,	0,	FSYS1_MUX_PHYCLK_UFS_LINK_SDCARD_TX0_SYMBOL_USER);
UMUX(umux_fsys1_phyclk_ufs_link_sdcard_rx0_symbol_user,	0,	FSYS1_MUX_PHYCLK_UFS_LINK_SDCARD_RX0_SYMBOL_USER);
UMUX(umux_fsys1_phyclk_pcie_wifi0_tx0_user,	0,	FSYS1_MUX_PHYCLK_PCIE_WIFI0_TX0_USER);
UMUX(umux_fsys1_phyclk_pcie_wifi0_rx0_user,	0,	FSYS1_MUX_PHYCLK_PCIE_WIFI0_RX0_USER);
UMUX(umux_fsys1_phyclk_pcie_wifi1_tx0_user,	0,	FSYS1_MUX_PHYCLK_PCIE_WIFI1_TX0_USER);
UMUX(umux_fsys1_phyclk_pcie_wifi1_rx0_user,	0,	FSYS1_MUX_PHYCLK_PCIE_WIFI1_RX0_USER);
UMUX(umux_fsys1_phyclk_pcie_wifi0_dig_refclk_user,	0,	FSYS1_MUX_PHYCLK_PCIE_WIFI0_DIG_REFCLK_USER);
UMUX(umux_fsys1_phyclk_pcie_wifi1_dig_refclk_user,	0,	FSYS1_MUX_PHYCLK_PCIE_WIFI1_DIG_REFCLK_USER);
UMUX(umux_fsys1_phyclk_ufs_link_sdcard_rx_pwm_clk_user,	0,	0);
UMUX(umux_fsys1_phyclk_ufs_link_sdcard_tx_pwm_clk_user,	0,	0);
UMUX(umux_fsys1_phyclk_ufs_link_sdcard_refclk_out_soc_user,	0,	0);


static struct pwrcal_clk *gategrp_apollo_ppmu[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_apollo_bts[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mngs_ppmu[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mngs_bts[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_aud_common[] = {
	CLK(AUD_GATE_SCLK_CA5),
	CLK(AUD_GATE_ACLK_DMAC),
	CLK(AUD_GATE_ACLK_SRAMC),
	CLK(AUD_GATE_ACLK_AXI_LH_ASYNC_SI_TOP),
	CLK(AUD_GATE_ACLK_SMMU),
	CLK(AUD_GATE_ACLK_INTR),
	CLK(AUD_GATE_PCLK_SMMU),
	CLK(AUD_GATE_PCLK_SFR1),
	CLK(AUD_GATE_PCLK_TIMER),
	CLK(AUD_GATE_PCLK_DBG),
	CLK(AUD_GATE_ACLK_ATCLK_AUD),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_aud_lpass[] = {
	CLK(AUD_MUX_ACLK_CA5),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_aud_mi2s[] = {
	CLK(AUD_GATE_PCLK_I2S),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_aud_pcm[] = {
	CLK(AUD_GATE_PCLK_PCM),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_aud_uart[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_aud_dma[] = {
	CLK(AUD_DIV_ACLK_AUD),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_aud_slimbus[] = {
	CLK(AUD_GATE_PCLK_SLIMBUS),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_aud_sysmmu[] = {
	CLK(AUD_DIV_ACLK_AUD),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_aud_ppmu[] = {
	CLK(AUD_DIV_ACLK_AUD),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_aud_bts[] = {
	CLK(AUD_DIV_ACLK_AUD),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_aud_sclk_mi2s[] = {
	CLK(AUD_GATE_SCLK_I2S),
	CLK(AUD_GATE_SCLK_I2S_BCLK),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_aud_sclk_pcm[] = {
	CLK(AUD_GATE_SCLK_PCM),
	CLK(AUD_GATE_SCLK_ASRC),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_aud_sclk_slimbus[] = {
	CLK(AUD_GATE_SCLK_SLIMBUS),
	CLK(AUD_GATE_SCLK_SLIMBUS_CLKIN),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_bus0_display[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_bus0_cam[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_bus0_fsys1[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_bus1_mfc[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_bus1_mscl[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_bus1_fsys0[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_common[] = {
	CLK(CAM0_GATE_ACLK_BNS),
	CLK(CAM0_GATE_ACLK_CSIS2),
	CLK(CAM0_GATE_ACLK_CSIS3),
	CLK(CAM0_GATE_ACLK_LH_ASYNC_SI_CAM0),
	CLK(CAM0_GATE_ACLK_TREX_A_5x1_IS_A),
	CLK(CAM0_GATE_ACLK_SysMMU6_IS_A),
	CLK(CAM0_GATE_ACLK_SFW110_IS_A),
	CLK(CAM0_GATE_PCLK_SFW110_IS_A_IS_A),
	CLK(CAM0_GATE_PCLK_SysMMU6_IS_A),
	CLK(CAM0_GATE_PCLK_TREX_A_5x1_IS_A),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_csis0[] = {
	CLK(CAM0_GATE_ACLK_CSIS0),
	CLK(CAM0_GATE_PCLK_CSIS0),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_csis1[] = {
	CLK(CAM0_GATE_ACLK_CSIS1),
	CLK(CAM0_GATE_PCLK_CSIS1),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_fimc_bns[] = {
	CLK(CAM0_GATE_ACLK_PXL_ASBS_CSIS2_int),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_fimc_3aa0[] = {
	CLK(CAM0_GATE_ACLK_3AA0),
	CLK(CAM0_GATE_PCLK_3AA0),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_fimc_3aa1[] = {
	CLK(CAM0_GATE_ACLK_3AA1),
	CLK(CAM0_GATE_PCLK_3AA1),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_hpm[] = {
	CLK(CAM0_GATE_PCLK_HPM_APBIF_CAM0),
	CLK(CAM0_GATE_SCLK_PROMISE_CAM0),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_sysmmu[] = {
	CLK(CAM0_MUX_ACLK_CAM0_TREX_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_ppmu[] = {
	CLK(CAM0_MUX_ACLK_CAM0_TREX_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_bts[] = {
	CLK(CAM0_MUX_ACLK_CAM0_TREX_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_phyclk_hs0_csis0_rx_byte[] = {
	CLK(CAM0_MUX_PHYCLK_RXBYTECLKHS0_CSIS0_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_phyclk_hs1_csis0_rx_byte[] = {
	CLK(CAM0_MUX_PHYCLK_RXBYTECLKHS1_CSIS0_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_phyclk_hs2_csis0_rx_byte[] = {
	CLK(CAM0_MUX_PHYCLK_RXBYTECLKHS2_CSIS0_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_phyclk_hs3_csis0_rx_byte[] = {
	CLK(CAM0_MUX_PHYCLK_RXBYTECLKHS3_CSIS0_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_phyclk_hs0_csis1_rx_byte[] = {
	CLK(CAM0_MUX_PHYCLK_RXBYTECLKHS0_CSIS1_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam0_phyclk_hs1_csis1_rx_byte[] = {
	CLK(CAM0_MUX_PHYCLK_RXBYTECLKHS1_CSIS1_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_common[] = {
	CLK(CAM1_GATE_ACLK_SMMU_VRA),
	CLK(CAM1_GATE_ACLK_SMMU_ISPCPU),
	CLK(CAM1_GATE_ACLK_SMMU_IS_B),
	CLK(CAM1_GATE_PCLK_SMMU_IS_B),
	CLK(CAM1_GATE_PCLK_SMMU_VRA),
	CLK(CAM1_GATE_PCLK_SMMU_ISPCPU),
	CLK(CAM1_GATE_ACLK_SMMU_MC_SC),
	CLK(CAM1_GATE_PCLK_SMMU_MC_SC),
	CLK(CAM1_GATE_ACLK_TREX_B),
	CLK(CAM1_GATE_ACLK_TREX_CAM1),
	CLK(CAM1_GATE_ACLK_LH_SI),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_isp_cpu_gic_common[] = {
	CLK(CAM1_GATE_ACLK_ARM),
	CLK(CAM1_GATE_PCLK_ARM),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_isp_cpu[] = {
	CLK(CAM1_GATE_ACLK_ASYNC_CA7_TO_DRAM),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_gic_is[] = {
	CLK(CAM1_MUX_ACLK_CAM1_BUS_264_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_csis2[] = {
	CLK(CAM1_GATE_PCLK_CSIS2),
	CLK(CAM1_GATE_ACLK_CSIS2),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_csis3[] = {
	CLK(CAM1_GATE_PCLK_CSIS3),
	CLK(CAM1_GATE_ACLK_CSIS3),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_fimc_vra[] = {
	CLK(CAM1_GATE_ACLK_VRA),
	CLK(CAM1_GATE_PCLK_VRA),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_mc_scaler[] = {
	CLK(CAM1_GATE_ACLK_MC_SC),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_i2c0_isp[] = {
	CLK(CAM1_GATE_SCLK_CAM1_ISP_IS_B_OSCCLK_I2C0_ISP),
	CLK(CAM1_GATE_PCLK_I2C0),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_i2c1_isp[] = {
	CLK(CAM1_GATE_SCLK_CAM1_ISP_IS_B_OSCCLK_I2C1_ISP),
	CLK(CAM1_GATE_PCLK_I2C1),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_i2c2_isp[] = {
	CLK(CAM1_GATE_SCLK_CAM1_ISP_IS_B_OSCCLK_I2C2_ISP),
	CLK(CAM1_GATE_PCLK_I2C2),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_i2c3_isp[] = {
	CLK(CAM1_GATE_SCLK_CAM1_ISP_IS_B_OSCCLK_I2C3_ISP),
	CLK(CAM1_GATE_PCLK_I2C3),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_wdt_isp[] = {
	CLK(CAM1_GATE_PCLK_WDT),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_mcuctl_isp[] = {
	CLK(CAM1_GATE_PCLK_MCUCTL),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_uart_isp[] = {
	CLK(CAM1_GATE_PCLK_UART),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_sclk_uart_isp[] = {
	CLK(CAM1_GATE_SCLK_ISP_PERI_IS_B_UART_EXT_CLK_ISP),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_spi0_isp[] = {
	CLK(CAM1_GATE_SCLK_ISP_PERI_IS_B_SPI0_EXT_CLK_ISP),
	CLK(CAM1_GATE_PCLK_SPI0),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_spi1_isp[] = {
	CLK(CAM1_GATE_SCLK_ISP_PERI_IS_B_SPI1_EXT_CLK_ISP),
	CLK(CAM1_GATE_PCLK_SPI1),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_pdma_is[] = {
	CLK(CAM1_GATE_ACLK_PDMA),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_pwm_isp[] = {
	CLK(CAM1_GATE_PCLK_PWM),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_sclk_pwm_isp[] = {
	CLK(CAM1_GATE_SCLK_ISP_PERI_IS_B_PWM_ISP),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_sysmmu[] = {
	CLK(CAM1_MUX_ACLK_CAM1_TREX_B_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_ppmu[] = {
	CLK(CAM1_MUX_ACLK_CAM1_TREX_B_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_bts[] = {
	CLK(CAM1_MUX_ACLK_CAM1_TREX_B_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_phyclk_hs0_csis2_rx_byte[] = {
	CLK(CAM1_MUX_PHYCLK_RXBYTECLKHS0_CSIS2_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_phyclk_hs1_csis2_rx_byte[] = {
	CLK(CAM1_MUX_PHYCLK_RXBYTECLKHS1_CSIS2_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_phyclk_hs2_csis2_rx_byte[] = {
	CLK(CAM1_MUX_PHYCLK_RXBYTECLKHS2_CSIS2_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_phyclk_hs3_csis2_rx_byte[] = {
	CLK(CAM1_MUX_PHYCLK_RXBYTECLKHS3_CSIS2_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_cam1_phyclk_hs0_csis3_rx_byte[] = {
	CLK(CAM1_MUX_PHYCLK_RXBYTECLKHS0_CSIS3_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_ccore_i2c[] = {
	CLK(CCORE_GATE_PCLK_HSI2C_BAT_AP),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_common[] = {
	CLK(DISP0_MUX_ACLK_DISP0_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_decon0[] = {
	CLK(DISP0_MUX_ACLK_DISP0_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_dsim0[] = {
	CLK(DISP0_MUX_ACLK_DISP0_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_dsim1[] = {
	CLK(DISP0_MUX_ACLK_DISP0_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_dsim2[] = {
	CLK(DISP0_MUX_ACLK_DISP0_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_hdmi[] = {
	CLK(DISP0_MUX_ACLK_DISP0_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_dp[] = {
	CLK(DISP0_MUX_ACLK_DISP0_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_hpm_apbif_disp0[] = {
	CLK(DISP0_MUX_ACLK_DISP0_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_sysmmu[] = {
	CLK(DISP0_MUX_ACLK_DISP0_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_ppmu[] = {
	CLK(DISP0_MUX_ACLK_DISP0_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_bts[] = {
	CLK(DISP0_MUX_ACLK_DISP0_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_phyclk_hdmiphy_tmds_20b_clko[] = {
	CLK(DISP0_DIV_PHYCLK_HDMIPHY_TMDS_20B_CLKO),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_phyclk_hdmiphy_tmds_10b_clko[] = {
	CLK(DISP0_MUX_PHYCLK_HDMIPHY_TMDS_CLKO_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_phyclk_hdmiphy_pixel_clko[] = {
	CLK(DISP0_DIV_PHYCLK_HDMIPHY_PIXEL_CLKO),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_phyclk_mipidphy0_rxclkesc0[] = {
	CLK(DISP0_MUX_PHYCLK_MIPIDPHY0_RXCLKESC0_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_phyclk_mipidphy0_bitclkdiv8[] = {
	CLK(DISP0_MUX_PHYCLK_MIPIDPHY0_BITCLKDIV8_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_phyclk_mipidphy1_rxclkesc0[] = {
	CLK(DISP0_MUX_PHYCLK_MIPIDPHY1_RXCLKESC0_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_phyclk_mipidphy1_bitclkdiv8[] = {
	CLK(DISP0_MUX_PHYCLK_MIPIDPHY1_BITCLKDIV8_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_phyclk_mipidphy2_rxclkesc0[] = {
	CLK(DISP0_MUX_PHYCLK_MIPIDPHY2_RXCLKESC0_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_phyclk_mipidphy2_bitclkdiv8[] = {
	CLK(DISP0_MUX_PHYCLK_MIPIDPHY2_BITCLKDIV8_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_phyclk_dpphy_ch0_txd_clk[] = {
	CLK(DISP0_MUX_PHYCLK_DPPHY_CH0_TXD_CLK_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_phyclk_dpphy_ch1_txd_clk[] = {
	CLK(DISP0_MUX_PHYCLK_DPPHY_CH1_TXD_CLK_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_phyclk_dpphy_ch2_txd_clk[] = {
	CLK(DISP0_MUX_PHYCLK_DPPHY_CH2_TXD_CLK_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_phyclk_dpphy_ch3_txd_clk[] = {
	CLK(DISP0_MUX_PHYCLK_DPPHY_CH3_TXD_CLK_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_oscclk_dp_i_clk_24m[] = {
	CLK(OSCCLK),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_oscclk_i_dptx_phy_i_ref_clk_24m[] = {
	CLK(OSCCLK),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_oscclk_i_mipi_dphy_m1s0_m_xi[] = {
	CLK(OSCCLK),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_oscclk_i_mipi_dphy_m4s0_m_xi[] = {
	CLK(OSCCLK),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp0_oscclk_i_mipi_dphy_m4s4_m_xi[] = {
	CLK(OSCCLK),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp1_common[] = {
	CLK(DISP1_MUX_ACLK_DISP1_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp1_decon1[] = {
	CLK(DISP1_MUX_ACLK_DISP1_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp1_hpmdisp1[] = {
	CLK(DISP1_MUX_ACLK_DISP1_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp1_sysmmu[] = {
	CLK(DISP1_MUX_ACLK_DISP1_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp1_ppmu[] = {
	CLK(DISP1_MUX_ACLK_DISP1_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_disp1_bts[] = {
	CLK(DISP1_MUX_ACLK_DISP1_0_400_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_common[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_usbdrd_etrusb_common[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_usbdrd30[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_etr_usb[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_usbhost20[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_mmc0_ufs_common[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_mmc0[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_ufs_linkemedded[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_pdma_common[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_pdma0[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_pdmas[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_ppmu[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_bts[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_hpm[] = {
	CLK(FSYS0_MUX_ACLK_FSYS0_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_phyclk_usbdrd30_udrd30_phyclock[] = {
	CLK(FSYS0_MUX_PHYCLK_USBDRD30_UDRD30_PHYCLOCK_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_phyclk_usbdrd30_udrd30_pipe_pclk[] = {
	CLK(FSYS0_MUX_PHYCLK_USBDRD30_UDRD30_PIPE_PCLK_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_phyclk_ufs_tx0_symbol[] = {
	CLK(FSYS0_MUX_PHYCLK_UFS_TX0_SYMBOL_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_phyclk_ufs_rx0_symbol[] = {
	CLK(FSYS0_MUX_PHYCLK_UFS_RX0_SYMBOL_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_phyclk_usbhost20_phyclock[] = {
	CLK(FSYS0_MUX_PHYCLK_USBHOST20_PHYCLOCK_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_phyclk_usbhost20_freeclk[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_phyclk_usbhost20_clk48mohci[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_phyclk_usbhost20phy_ref_clk[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_phyclk_ufs_rx_pwm_clk[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_phyclk_ufs_tx_pwm_clk[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys0_phyclk_ufs_refclk_out_soc[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_common[] = {
	CLK(FSYS1_MUX_ACLK_FSYS1_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_mmc2_ufs_common[] = {
	CLK(FSYS1_MUX_ACLK_FSYS1_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_mmc2[] = {
	CLK(FSYS1_MUX_ACLK_FSYS1_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_ufs20_sdcard[] = {
	CLK(FSYS1_MUX_ACLK_FSYS1_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_sromc[] = {
	CLK(FSYS1_MUX_ACLK_FSYS1_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_pciewifi0[] = {
	CLK(FSYS1_MUX_ACLK_FSYS1_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_pciewifi1[] = {
	CLK(FSYS1_MUX_ACLK_FSYS1_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_ppmu[] = {
	CLK(FSYS1_MUX_ACLK_FSYS1_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_bts[] = {
	CLK(FSYS1_MUX_ACLK_FSYS1_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_hpm[] = {
	CLK(FSYS1_MUX_ACLK_FSYS1_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_phyclk_ufs_link_sdcard_tx0_symbol[] = {
	CLK(FSYS1_MUX_PHYCLK_UFS_LINK_SDCARD_TX0_SYMBOL_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_phyclk_ufs_link_sdcard_rx0_symbol[] = {
	CLK(FSYS1_MUX_PHYCLK_UFS_LINK_SDCARD_RX0_SYMBOL_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_phyclk_pcie_wifi0_tx0[] = {
	CLK(FSYS1_MUX_PHYCLK_PCIE_WIFI0_TX0_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_phyclk_pcie_wifi0_rx0[] = {
	CLK(FSYS1_MUX_PHYCLK_PCIE_WIFI0_RX0_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_phyclk_pcie_wifi1_tx0[] = {
	CLK(FSYS1_MUX_PHYCLK_PCIE_WIFI1_TX0_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_phyclk_pcie_wifi1_rx0[] = {
	CLK(FSYS1_MUX_PHYCLK_PCIE_WIFI1_RX0_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_phyclk_pcie_wifi0_dig_refclk[] = {
	CLK(FSYS1_MUX_PHYCLK_PCIE_WIFI0_DIG_REFCLK_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_phyclk_pcie_wifi1_dig_refclk[] = {
	CLK(FSYS1_MUX_PHYCLK_PCIE_WIFI1_DIG_REFCLK_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_phyclk_ufs_link_sdcard_rx_pwm_clk[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_phyclk_ufs_link_sdcard_tx_pwm_clk[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_fsys1_phyclk_ufs_link_sdcard_refclk_out_soc[] = {
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_g3d_common[] = {
	CLK(G3D_GATE_ACLK_PPMU_G3D0),
	CLK(G3D_GATE_ACLK_PPMU_G3D1),
	CLK(G3D_GATE_PCLK_PPMU_G3D0),
	CLK(G3D_GATE_PCLK_PPMU_G3D1),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_g3d_g3d_common[] = {
	CLK(G3D_GATE_ACLK_G3D),
	CLK(G3D_GATE_ACLK_ASYNCAPBM_G3D),
	CLK(G3D_GATE_ACLK_XIU_G3D),
	CLK(G3D_GATE_ACLK_GRAY_DEC),
	CLK(G3D_GATE_ACLK_ASYNCAPBS_G3D),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_g3d_g3d[] = {
	CLK(G3D_GATE_ACLK_ACEL_LH_ASYNC_SI_G3D0),
	CLK(G3D_GATE_ACLK_ACEL_LH_ASYNC_SI_G3D1),
	CLK(G3D_GATE_PCLK_SYSREG_G3D),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_g3d_iram_path_test[] = {
	CLK(G3D_GATE_ACLK_AXI_DS_G3D),
	CLK(G3D_GATE_ACLK_ASYNCAXI_G3D),
	CLK(G3D_GATE_SCLK_ASYNCAXI_G3D),
	CLK(G3D_GATE_SCLK_AXI_LH_ASYNC_SI_G3DIRAM),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_g3d_ppmu[] = {
	CLK(G3D_DIV_ACLK_G3D),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_g3d_bts[] = {
	CLK(G3D_DIV_ACLK_G3D),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_imem_common[] = {
	CLK(IMEM_MUX_ACLK_IMEM_266_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_imem_apm_sss_rtic_mc_common[] = {
	CLK(IMEM_MUX_ACLK_IMEM_266_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_imem_apm[] = {
	CLK(IMEM_MUX_ACLK_IMEM_266_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_imem_sss[] = {
	CLK(IMEM_MUX_ACLK_IMEM_266_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_imem_rtic[] = {
	CLK(IMEM_MUX_ACLK_IMEM_266_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_imem_mc[] = {
	CLK(IMEM_MUX_ACLK_IMEM_266_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_imem_intmem[] = {
	CLK(IMEM_MUX_ACLK_IMEM_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_imem_intmem_alv[] = {
	CLK(IMEM_MUX_ACLK_IMEM_200_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_imem_gic400[] = {
	CLK(IMEM_MUX_ACLK_IMEM_266_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_imem_ppmu[] = {
	CLK(IMEM_MUX_ACLK_IMEM_266_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_isp0_common[] = {
	CLK(ISP0_GATE_CLK_AXI_LH_ASYNC_SI_TOP_ISP0),
	CLK(ISP0_GATE_CLK_C_TREX_C),
	CLK(ISP0_GATE_ACLK_SysMMU601),
	CLK(ISP0_GATE_PCLK_HPM_APBIF_ISP0),
	CLK(ISP0_GATE_PCLK_TREX_C),
	CLK(ISP0_GATE_PCLK_SysMMU601),
	CLK(ISP0_GATE_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_isp0_fimc_isp0[] = {
	CLK(ISP0_GATE_ACLK_FIMC_ISP0),
	CLK(ISP0_GATE_PCLK_FIMC_ISP0),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_isp0_fimc_tpu[] = {
	CLK(ISP0_GATE_ACLK_FIMC_TPU),
	CLK(ISP0_GATE_PCLK_FIMC_TPU),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_isp0_sysmmu[] = {
	CLK(ISP0_MUX_ACLK_ISP0_TREX_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_isp0_ppmu[] = {
	CLK(ISP0_MUX_ACLK_ISP0_TREX_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_isp0_bts[] = {
	CLK(ISP0_MUX_ACLK_ISP0_TREX_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_isp1_fimc_isp1[] = {
	CLK(ISP1_GATE_ACLK_FIMC_ISP1),
	CLK(ISP1_GATE_ACLK_XIU_N_ASYNC_SI),
	CLK(ISP1_GATE_PCLK_FIMC_ISP1),
	CLK(ISP1_GATE_PCLK_SYSREG_ISP1),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_isp1_sysmmu[] = {
	CLK(ISP1_MUX_ACLK_ISP1_468_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_isp1_ppmu[] = {
	CLK(ISP1_MUX_ACLK_ISP1_468_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_isp1_bts[] = {
	CLK(ISP1_MUX_ACLK_ISP1_468_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mfc_common[] = {
	CLK(MFC_MUX_ACLK_MFC_600_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mfc_mfc[] = {
	CLK(MFC_MUX_ACLK_MFC_600_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mfc_hpm[] = {
	CLK(MFC_MUX_ACLK_MFC_600_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mfc_sysmmu[] = {
	CLK(MFC_MUX_ACLK_MFC_600_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mfc_ppmu[] = {
	CLK(MFC_MUX_ACLK_MFC_600_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mscl_common[] = {
	CLK(MSCL_MUX_ACLK_MSCL0_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mscl_mscl0_jpeg_common[] = {
	CLK(MSCL_MUX_ACLK_MSCL0_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mscl_mscl0[] = {
	CLK(MSCL_MUX_ACLK_MSCL0_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mscl_jpeg[] = {
	CLK(MSCL_MUX_ACLK_MSCL0_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mscl_mscl1_g2d_common[] = {
	CLK(MSCL_MUX_ACLK_MSCL0_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mscl_mscl1[] = {
	CLK(MSCL_MUX_ACLK_MSCL0_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mscl_g2d[] = {
	CLK(MSCL_MUX_ACLK_MSCL0_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mscl_sysmmu[] = {
	CLK(MSCL_MUX_ACLK_MSCL0_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mscl_ppmu[] = {
	CLK(MSCL_MUX_ACLK_MSCL0_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_mscl_bts[] = {
	CLK(MSCL_MUX_ACLK_MSCL0_528_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric0_hsi2c0[] = {
	CLK(PERIC0_HWACG_HSI2C0),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric0_hsi2c1[] = {
	CLK(PERIC0_HWACG_HSI2C1),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric0_hsi2c4[] = {
	CLK(PERIC0_HWACG_HSI2C4),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric0_hsi2c5[] = {
	CLK(PERIC0_HWACG_HSI2C5),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric0_hsi2c9[] = {
	CLK(PERIC0_HWACG_HSI2C9),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric0_hsi2c10[] = {
	CLK(PERIC0_HWACG_HSI2C10),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric0_hsi2c11[] = {
	CLK(PERIC0_HWACG_HSI2C11),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric0_uart0[] = {
	CLK(PERIC0_HWACG_UART0),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric0_adcif[] = {
	CLK(PERIC0_HWACG_ADCIF),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric0_pwm[] = {
	CLK(PERIC0_HWACG_PWM),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric0_sclk_pwm[] = {
	CLK(PERIC0_GATE_SCLK_PWM),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_hsi2c_common[] = {
	CLK(PERIC1_MUX_ACLK_PERIC1_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_hsi2c2[] = {
	CLK(PERIC1_HWACG_HSI2C2),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_hsi2c3[] = {
	CLK(PERIC1_HWACG_HSI2C3),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_hsi2c6[] = {
	CLK(PERIC1_HWACG_HSI2C6),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_hsi2c7[] = {
	CLK(PERIC1_HWACG_HSI2C7),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_hsi2c8[] = {
	CLK(PERIC1_HWACG_HSI2C8),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_hsi2c12[] = {
	CLK(PERIC1_HWACG_HSI2C12),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_hsi2c13[] = {
	CLK(PERIC1_HWACG_HSI2C13),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_hsi2c14[] = {
	CLK(PERIC1_HWACG_HSI2C14),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_spi_i2s_pcm1_spdif_common[] = {
	CLK(PERIC1_MUX_ACLK_PERIC1_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_uart1[] = {
	CLK(PERIC1_HWACG_UART1),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_uart2[] = {
	CLK(PERIC1_HWACG_UART2),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_uart3[] = {
	CLK(PERIC1_HWACG_UART3),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_uart4[] = {
	CLK(PERIC1_HWACG_UART4),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_uart5[] = {
	CLK(PERIC1_HWACG_UART5),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_spi0[] = {
	CLK(PERIC1_HWACG_SPI0),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_spi1[] = {
	CLK(PERIC1_HWACG_SPI1),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_spi2[] = {
	CLK(PERIC1_HWACG_SPI2),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_spi3[] = {
	CLK(PERIC1_HWACG_SPI3),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_spi4[] = {
	CLK(PERIC1_HWACG_SPI4),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_spi5[] = {
	CLK(PERIC1_HWACG_SPI5),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_spi6[] = {
	CLK(PERIC1_HWACG_SPI6),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_spi7[] = {
	CLK(PERIC1_HWACG_SPI7),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_i2s1[] = {
	CLK(OSCCLK),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_pcm1[] = {
	CLK(OSCCLK),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_spdif[] = {
	CLK(OSCCLK),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_gpio_nfc[] = {
	CLK(PERIC1_MUX_ACLK_PERIC1_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_gpio_touch[] = {
	CLK(PERIC1_MUX_ACLK_PERIC1_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_gpio_fp[] = {
	CLK(PERIC1_MUX_ACLK_PERIC1_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peric1_gpio_ese[] = {
	CLK(PERIC1_MUX_ACLK_PERIC1_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peris_sfr_apbif_hdmi_cec[] = {
	CLK(PERIS_MUX_ACLK_PERIS_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peris_hpm[] = {
	CLK(PERIS_MUX_ACLK_PERIS_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peris_mct[] = {
	CLK(PERIS_MUX_ACLK_PERIS_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peris_wdt_mngs[] = {
	CLK(PERIS_MUX_ACLK_PERIS_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peris_wdt_apollo[] = {
	CLK(PERIS_MUX_ACLK_PERIS_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peris_sysreg_peris[] = {
	CLK(PERIS_MUX_ACLK_PERIS_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peris_monocnt_apbif[] = {
	CLK(PERIS_MUX_ACLK_PERIS_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peris_rtc_apbif[] = {
	CLK(PERIS_MUX_ACLK_PERIS_66_USER),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peris_top_rtc[] = {
	CLK(PERIS_HWACG_TOP_RTC),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peris_otp_con_top[] = {
	CLK(PERIS_HWACG_OTP_CON_TOP),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peris_chipid[] = {
	CLK(PERIS_HWACG_SFR_APBIF_CHIPID),
	CLK(PERIS_HWACG_CHIPID),
	CLK_NONE,
};
static struct pwrcal_clk *gategrp_peris_tmu[] = {
	CLK(PERIS_HWACG_TMU),
	CLK_NONE,
};


GRPGATE(gate_apollo_ppmu,	0,	gategrp_apollo_ppmu);
GRPGATE(gate_apollo_bts,	0,	gategrp_apollo_bts);
GRPGATE(gate_mngs_ppmu,	0,	gategrp_mngs_ppmu);
GRPGATE(gate_mngs_bts,	0,	gategrp_mngs_bts);
GRPGATE(gate_aud_common,	pxmxdx_aud,	gategrp_aud_common);
GRPGATE(gate_aud_lpass,	gate_aud_common,	gategrp_aud_lpass);
GRPGATE(gate_aud_mi2s,	gate_aud_common,	gategrp_aud_mi2s);
GRPGATE(gate_aud_pcm,	gate_aud_common,	gategrp_aud_pcm);
GRPGATE(gate_aud_uart,	gate_aud_common,	gategrp_aud_uart);
GRPGATE(gate_aud_dma,	gate_aud_common,	gategrp_aud_dma);
GRPGATE(gate_aud_slimbus,	gate_aud_common,	gategrp_aud_slimbus);
GRPGATE(gate_aud_sysmmu,	gate_aud_common,	gategrp_aud_sysmmu);
GRPGATE(gate_aud_ppmu,	gate_aud_common,	gategrp_aud_ppmu);
GRPGATE(gate_aud_bts,	gate_aud_common,	gategrp_aud_bts);
GRPGATE(gate_aud_sclk_mi2s,	d1_sclk_i2s_local,	gategrp_aud_sclk_mi2s);
GRPGATE(gate_aud_sclk_pcm,	d1_sclk_pcm_local,	gategrp_aud_sclk_pcm);
GRPGATE(gate_aud_sclk_slimbus,	d1_sclk_slimbus,	gategrp_aud_sclk_slimbus);
GRPGATE(gate_bus0_display,	umux_bus0_aclk_bus0_528,	gategrp_bus0_display);
GRPGATE(gate_bus0_cam,	umux_bus0_aclk_bus0_528,	gategrp_bus0_cam);
GRPGATE(gate_bus0_fsys1,	umux_bus0_aclk_bus0_200,	gategrp_bus0_fsys1);
GRPGATE(gate_bus1_mfc,	umux_bus1_aclk_bus1_528,	gategrp_bus1_mfc);
GRPGATE(gate_bus1_mscl,	umux_bus1_aclk_bus1_528,	gategrp_bus1_mscl);
GRPGATE(gate_bus1_fsys0,	umux_bus1_aclk_bus1_528,	gategrp_bus1_fsys0);
GRPGATE(gate_cam0_common,	gate_bus0_cam,	gategrp_cam0_common);
GRPGATE(gate_cam0_csis0,	gate_cam0_common,	gategrp_cam0_csis0);
GRPGATE(gate_cam0_csis1,	gate_cam0_common,	gategrp_cam0_csis1);
GRPGATE(gate_cam0_fimc_bns,	gate_cam0_common,	gategrp_cam0_fimc_bns);
GRPGATE(gate_cam0_fimc_3aa0,	gate_cam0_common,	gategrp_cam0_fimc_3aa0);
GRPGATE(gate_cam0_fimc_3aa1,	gate_cam0_common,	gategrp_cam0_fimc_3aa1);
GRPGATE(gate_cam0_hpm,	gate_cam0_common,	gategrp_cam0_hpm);
GRPGATE(gate_cam0_sysmmu,	gate_cam0_common,	gategrp_cam0_sysmmu);
GRPGATE(gate_cam0_ppmu,	gate_cam0_common,	gategrp_cam0_ppmu);
GRPGATE(gate_cam0_bts,	gate_cam0_common,	gategrp_cam0_bts);
GRPGATE(gate_cam0_phyclk_hs0_csis0_rx_byte,	umux_cam0_phyclk_rxbyteclkhs0_csis0_user,	gategrp_cam0_phyclk_hs0_csis0_rx_byte);
GRPGATE(gate_cam0_phyclk_hs1_csis0_rx_byte,	umux_cam0_phyclk_rxbyteclkhs1_csis0_user,	gategrp_cam0_phyclk_hs1_csis0_rx_byte);
GRPGATE(gate_cam0_phyclk_hs2_csis0_rx_byte,	umux_cam0_phyclk_rxbyteclkhs2_csis0_user,	gategrp_cam0_phyclk_hs2_csis0_rx_byte);
GRPGATE(gate_cam0_phyclk_hs3_csis0_rx_byte,	umux_cam0_phyclk_rxbyteclkhs3_csis0_user,	gategrp_cam0_phyclk_hs3_csis0_rx_byte);
GRPGATE(gate_cam0_phyclk_hs0_csis1_rx_byte,	umux_cam0_phyclk_rxbyteclkhs0_csis1_user,	gategrp_cam0_phyclk_hs0_csis1_rx_byte);
GRPGATE(gate_cam0_phyclk_hs1_csis1_rx_byte,	umux_cam0_phyclk_rxbyteclkhs1_csis1_user,	gategrp_cam0_phyclk_hs1_csis1_rx_byte);
GRPGATE(gate_cam1_common,	gate_bus0_cam,	gategrp_cam1_common);
GRPGATE(gate_cam1_isp_cpu_gic_common,	gate_cam1_common,	gategrp_cam1_isp_cpu_gic_common);
GRPGATE(gate_cam1_isp_cpu,	gate_cam1_isp_cpu_gic_common,	gategrp_cam1_isp_cpu);
GRPGATE(gate_cam1_gic_is,	gate_cam1_isp_cpu_gic_common,	gategrp_cam1_gic_is);
GRPGATE(gate_cam1_csis2,	gate_cam1_common,	gategrp_cam1_csis2);
GRPGATE(gate_cam1_csis3,	gate_cam1_common,	gategrp_cam1_csis3);
GRPGATE(gate_cam1_fimc_vra,	gate_cam1_common,	gategrp_cam1_fimc_vra);
GRPGATE(gate_cam1_mc_scaler,	gate_cam1_common,	gategrp_cam1_mc_scaler);
GRPGATE(gate_cam1_i2c0_isp,	gate_cam1_common,	gategrp_cam1_i2c0_isp);
GRPGATE(gate_cam1_i2c1_isp,	gate_cam1_common,	gategrp_cam1_i2c1_isp);
GRPGATE(gate_cam1_i2c2_isp,	gate_cam1_common,	gategrp_cam1_i2c2_isp);
GRPGATE(gate_cam1_i2c3_isp,	gate_cam1_common,	gategrp_cam1_i2c3_isp);
GRPGATE(gate_cam1_wdt_isp,	gate_cam1_common,	gategrp_cam1_wdt_isp);
GRPGATE(gate_cam1_mcuctl_isp,	gate_cam1_common,	gategrp_cam1_mcuctl_isp);
GRPGATE(gate_cam1_uart_isp,	gate_cam1_common,	gategrp_cam1_uart_isp);
GRPGATE(gate_cam1_sclk_uart_isp,	0,	gategrp_cam1_sclk_uart_isp);
GRPGATE(gate_cam1_spi0_isp,	sclk_isp_spi0,	gategrp_cam1_spi0_isp);
GRPGATE(gate_cam1_spi1_isp,	sclk_isp_spi1,	gategrp_cam1_spi1_isp);
GRPGATE(gate_cam1_pdma_isp,	gate_cam1_common,	gategrp_cam1_pdma_is);
GRPGATE(gate_cam1_pwm_isp,	gate_cam1_common,	gategrp_cam1_pwm_isp);
GRPGATE(gate_cam1_sclk_pwm_isp,	0,	gategrp_cam1_sclk_pwm_isp);
GRPGATE(gate_cam1_sysmmu,	gate_cam1_common,	gategrp_cam1_sysmmu);
GRPGATE(gate_cam1_ppmu,	gate_cam1_common,	gategrp_cam1_ppmu);
GRPGATE(gate_cam1_bts,	gate_cam1_common,	gategrp_cam1_bts);
GRPGATE(gate_cam1_phyclk_hs0_csis2_rx_byte, umux_cam1_phyclk_rxbyteclkhs0_csis2_user, gategrp_cam1_phyclk_hs0_csis2_rx_byte);
GRPGATE(gate_cam1_phyclk_hs1_csis2_rx_byte, umux_cam1_phyclk_rxbyteclkhs1_csis2_user, gategrp_cam1_phyclk_hs1_csis2_rx_byte);
GRPGATE(gate_cam1_phyclk_hs2_csis2_rx_byte, umux_cam1_phyclk_rxbyteclkhs2_csis2_user, gategrp_cam1_phyclk_hs2_csis2_rx_byte);
GRPGATE(gate_cam1_phyclk_hs3_csis2_rx_byte, umux_cam1_phyclk_rxbyteclkhs3_csis2_user, gategrp_cam1_phyclk_hs3_csis2_rx_byte);
GRPGATE(gate_cam1_phyclk_hs0_csis3_rx_byte, umux_cam1_phyclk_rxbyteclkhs0_csis3_user, gategrp_cam1_phyclk_hs0_csis3_rx_byte);
GRPGATE(gate_ccore_i2c,	0,	gategrp_ccore_i2c);
GRPGATE(gate_disp0_common,	pxmxdx_disp0,	gategrp_disp0_common);
GRPGATE(gate_disp0_decon0,	gate_disp0_common,	gategrp_disp0_decon0);
GRPGATE(gate_disp0_dsim0,	gate_disp0_common,	gategrp_disp0_dsim0);
GRPGATE(gate_disp0_dsim1,	gate_disp0_common,	gategrp_disp0_dsim1);
GRPGATE(gate_disp0_dsim2,	gate_disp0_common,	gategrp_disp0_dsim2);
GRPGATE(gate_disp0_hdmi,	gate_disp0_common,	gategrp_disp0_hdmi);
GRPGATE(gate_disp0_dp,	gate_disp0_common,	gategrp_disp0_dp);
GRPGATE(gate_disp0_hpm_apbif_disp0,	gate_disp0_common,	gategrp_disp0_hpm_apbif_disp0);
GRPGATE(gate_disp0_sysmmu,	gate_disp0_common,	gategrp_disp0_sysmmu);
GRPGATE(gate_disp0_ppmu,	gate_disp0_common,	gategrp_disp0_ppmu);
GRPGATE(gate_disp0_bts,	gate_disp0_common,	gategrp_disp0_bts);
GRPGATE(gate_disp0_phyclk_hdmiphy_tmds_20b_clko, umux_disp0_phyclk_hdmiphy_tmds_clko_user, gategrp_disp0_phyclk_hdmiphy_tmds_20b_clko);
GRPGATE(gate_disp0_phyclk_hdmiphy_tmds_10b_clko, umux_disp0_phyclk_hdmiphy_tmds_clko_user, gategrp_disp0_phyclk_hdmiphy_tmds_10b_clko);
GRPGATE(gate_disp0_phyclk_hdmiphy_pixel_clko, umux_disp0_phyclk_hdmiphy_pixel_clko_user, gategrp_disp0_phyclk_hdmiphy_pixel_clko);
GRPGATE(gate_disp0_phyclk_mipidphy0_rxclkesc0,	umux_disp0_phyclk_mipidphy0_rxclkesc0_user,	gategrp_disp0_phyclk_mipidphy0_rxclkesc0);
GRPGATE(gate_disp0_phyclk_mipidphy0_bitclkdiv8,	umux_disp0_phyclk_mipidphy0_bitclkdiv8_user,	gategrp_disp0_phyclk_mipidphy0_bitclkdiv8);
GRPGATE(gate_disp0_phyclk_mipidphy1_rxclkesc0,	umux_disp0_phyclk_mipidphy1_rxclkesc0_user,	gategrp_disp0_phyclk_mipidphy1_rxclkesc0);
GRPGATE(gate_disp0_phyclk_mipidphy1_bitclkdiv8,	umux_disp0_phyclk_mipidphy1_bitclkdiv8_user,	gategrp_disp0_phyclk_mipidphy1_bitclkdiv8);
GRPGATE(gate_disp0_phyclk_mipidphy2_rxclkesc0,	umux_disp0_phyclk_mipidphy2_rxclkesc0_user,	gategrp_disp0_phyclk_mipidphy2_rxclkesc0);
GRPGATE(gate_disp0_phyclk_mipidphy2_bitclkdiv8,	umux_disp0_phyclk_mipidphy2_bitclkdiv8_user,	gategrp_disp0_phyclk_mipidphy2_bitclkdiv8);
GRPGATE(gate_disp0_phyclk_dpphy_ch0_txd_clk,	umux_disp0_phyclk_dpphy_ch0_txd_clk_user,	gategrp_disp0_phyclk_dpphy_ch0_txd_clk);
GRPGATE(gate_disp0_phyclk_dpphy_ch1_txd_clk,	umux_disp0_phyclk_dpphy_ch1_txd_clk_user,	gategrp_disp0_phyclk_dpphy_ch1_txd_clk);
GRPGATE(gate_disp0_phyclk_dpphy_ch2_txd_clk,	umux_disp0_phyclk_dpphy_ch2_txd_clk_user,	gategrp_disp0_phyclk_dpphy_ch2_txd_clk);
GRPGATE(gate_disp0_phyclk_dpphy_ch3_txd_clk,	umux_disp0_phyclk_dpphy_ch3_txd_clk_user,	gategrp_disp0_phyclk_dpphy_ch3_txd_clk);
GRPGATE(gate_disp0_oscclk_dp_i_clk_24m,	0,	gategrp_disp0_oscclk_dp_i_clk_24m);
GRPGATE(gate_disp0_oscclk_i_dptx_phy_i_ref_clk_24m,	0,	gategrp_disp0_oscclk_i_dptx_phy_i_ref_clk_24m);
GRPGATE(gate_disp0_oscclk_i_mipi_dphy_m1s0_m_xi,	0,	gategrp_disp0_oscclk_i_mipi_dphy_m1s0_m_xi);
GRPGATE(gate_disp0_oscclk_i_mipi_dphy_m4s0_m_xi,	0,	gategrp_disp0_oscclk_i_mipi_dphy_m4s0_m_xi);
GRPGATE(gate_disp0_oscclk_i_mipi_dphy_m4s4_m_xi,	0,	gategrp_disp0_oscclk_i_mipi_dphy_m4s4_m_xi);
GRPGATE(gate_disp1_common,	pxmxdx_disp1,	gategrp_disp1_common);
GRPGATE(gate_disp1_decon1,	gate_disp1_common,	gategrp_disp1_decon1);
GRPGATE(gate_disp1_hpmdisp1,	gate_disp1_common,	gategrp_disp1_hpmdisp1);
GRPGATE(gate_disp1_sysmmu,	gate_disp1_common,	gategrp_disp1_sysmmu);
GRPGATE(gate_disp1_ppmu,	gate_disp1_common,	gategrp_disp1_ppmu);
GRPGATE(gate_disp1_bts,	gate_disp1_common,	gategrp_disp1_bts);
GRPGATE(gate_fsys0_common,	pxmxdx_fsys0,	gategrp_fsys0_common);
GRPGATE(gate_fsys0_usbdrd_etrusb_common,	gate_fsys0_common,	gategrp_fsys0_usbdrd_etrusb_common);
GRPGATE(gate_fsys0_usbdrd30,	gate_fsys0_usbdrd_etrusb_common,	gategrp_fsys0_usbdrd30);
GRPGATE(gate_fsys0_etr_usb,	gate_fsys0_usbdrd_etrusb_common,	gategrp_fsys0_etr_usb);
GRPGATE(gate_fsys0_usbhost20,	gate_fsys0_common,	gategrp_fsys0_usbhost20);
GRPGATE(gate_fsys0_mmc0_ufs_common,	gate_fsys0_common,	gategrp_fsys0_mmc0_ufs_common);
GRPGATE(gate_fsys0_mmc0,	gate_fsys0_mmc0_ufs_common,	gategrp_fsys0_mmc0);
GRPGATE(gate_fsys0_ufs_linkemedded,	gate_fsys0_mmc0_ufs_common,	gategrp_fsys0_ufs_linkemedded);
GRPGATE(gate_fsys0_pdma_common,	gate_fsys0_common,	gategrp_fsys0_pdma_common);
GRPGATE(gate_fsys0_pdma0,	gate_fsys0_pdma_common,	gategrp_fsys0_pdma0);
GRPGATE(gate_fsys0_pdmas,	gate_fsys0_pdma_common,	gategrp_fsys0_pdmas);
GRPGATE(gate_fsys0_ppmu,	gate_fsys0_common,	gategrp_fsys0_ppmu);
GRPGATE(gate_fsys0_bts,	gate_fsys0_common,	gategrp_fsys0_bts);
GRPGATE(gate_fsys0_hpm,	gate_fsys0_common,	gategrp_fsys0_hpm);
GRPGATE(gate_fsys0_phyclk_usbdrd30_udrd30_phyclock,	umux_fsys0_phyclk_usbdrd30_udrd30_phyclock_user,	gategrp_fsys0_phyclk_usbdrd30_udrd30_phyclock);
GRPGATE(gate_fsys0_phyclk_usbdrd30_udrd30_pipe_pclk,	umux_fsys0_phyclk_usbdrd30_udrd30_pipe_pclk_user,	gategrp_fsys0_phyclk_usbdrd30_udrd30_pipe_pclk);
GRPGATE(gate_fsys0_phyclk_ufs_tx0_symbol,	umux_fsys0_phyclk_ufs_tx0_symbol_user,	gategrp_fsys0_phyclk_ufs_tx0_symbol);
GRPGATE(gate_fsys0_phyclk_ufs_rx0_symbol,	umux_fsys0_phyclk_ufs_rx0_symbol_user,	gategrp_fsys0_phyclk_ufs_rx0_symbol);
GRPGATE(gate_fsys0_phyclk_usbhost20_phyclock,	umux_fsys0_phyclk_usbhost20_phyclock_user,	gategrp_fsys0_phyclk_usbhost20_phyclock);
GRPGATE(gate_fsys0_phyclk_usbhost20_freeclk,	umux_fsys0_phyclk_usbhost20_freeclk_user,	gategrp_fsys0_phyclk_usbhost20_freeclk);
GRPGATE(gate_fsys0_phyclk_usbhost20_clk48mohci,	umux_fsys0_phyclk_usbhost20_clk48mohci_user,	gategrp_fsys0_phyclk_usbhost20_clk48mohci);
GRPGATE(gate_fsys0_phyclk_usbhost20phy_ref_clk,	umux_fsys0_phyclk_usbhost20phy_ref_clk,	gategrp_fsys0_phyclk_usbhost20phy_ref_clk);
GRPGATE(gate_fsys0_phyclk_ufs_rx_pwm_clk,	umux_fsys0_phyclk_ufs_rx_pwm_clk_user,	gategrp_fsys0_phyclk_ufs_rx_pwm_clk);
GRPGATE(gate_fsys0_phyclk_ufs_tx_pwm_clk,	umux_fsys0_phyclk_ufs_tx_pwm_clk_user,	gategrp_fsys0_phyclk_ufs_tx_pwm_clk);
GRPGATE(gate_fsys0_phyclk_ufs_refclk_out_soc,	umux_fsys0_phyclk_ufs_refclk_out_soc_user,	gategrp_fsys0_phyclk_ufs_refclk_out_soc);
GRPGATE(gate_fsys1_common,	pxmxdx_fsys1,	gategrp_fsys1_common);
GRPGATE(gate_fsys1_mmc2_ufs_common,	gate_fsys1_common,	gategrp_fsys1_mmc2_ufs_common);
GRPGATE(gate_fsys1_mmc2,	gate_fsys1_mmc2_ufs_common,	gategrp_fsys1_mmc2);
GRPGATE(gate_fsys1_ufs20_sdcard,	gate_fsys1_mmc2_ufs_common,	gategrp_fsys1_ufs20_sdcard);
GRPGATE(gate_fsys1_sromc,	gate_fsys1_common,	gategrp_fsys1_sromc);
GRPGATE(gate_fsys1_pciewifi0,	gate_fsys1_common,	gategrp_fsys1_pciewifi0);
GRPGATE(gate_fsys1_pciewifi1,	gate_fsys1_common,	gategrp_fsys1_pciewifi1);
GRPGATE(gate_fsys1_ppmu,	gate_fsys1_common,	gategrp_fsys1_ppmu);
GRPGATE(gate_fsys1_bts,	gate_fsys1_common,	gategrp_fsys1_bts);
GRPGATE(gate_fsys1_hpm,	gate_fsys1_common,	gategrp_fsys1_hpm);
GRPGATE(gate_fsys1_phyclk_ufs_link_sdcard_tx0_symbol,	umux_fsys1_phyclk_ufs_link_sdcard_tx0_symbol_user,	gategrp_fsys1_phyclk_ufs_link_sdcard_tx0_symbol);
GRPGATE(gate_fsys1_phyclk_ufs_link_sdcard_rx0_symbol,	umux_fsys1_phyclk_ufs_link_sdcard_rx0_symbol_user,	gategrp_fsys1_phyclk_ufs_link_sdcard_rx0_symbol);
GRPGATE(gate_fsys1_phyclk_pcie_wifi0_tx0,	umux_fsys1_phyclk_pcie_wifi0_tx0_user,	gategrp_fsys1_phyclk_pcie_wifi0_tx0);
GRPGATE(gate_fsys1_phyclk_pcie_wifi0_rx0,	umux_fsys1_phyclk_pcie_wifi0_rx0_user,	gategrp_fsys1_phyclk_pcie_wifi0_rx0);
GRPGATE(gate_fsys1_phyclk_pcie_wifi1_tx0,	umux_fsys1_phyclk_pcie_wifi1_tx0_user,	gategrp_fsys1_phyclk_pcie_wifi1_tx0);
GRPGATE(gate_fsys1_phyclk_pcie_wifi1_rx0,	umux_fsys1_phyclk_pcie_wifi1_rx0_user,	gategrp_fsys1_phyclk_pcie_wifi1_rx0);
GRPGATE(gate_fsys1_phyclk_pcie_wifi0_dig_refclk,	umux_fsys1_phyclk_pcie_wifi0_dig_refclk_user,	gategrp_fsys1_phyclk_pcie_wifi0_dig_refclk);
GRPGATE(gate_fsys1_phyclk_pcie_wifi1_dig_refclk,	umux_fsys1_phyclk_pcie_wifi1_dig_refclk_user,	gategrp_fsys1_phyclk_pcie_wifi1_dig_refclk);
GRPGATE(gate_fsys1_phyclk_ufs_link_sdcard_rx_pwm_clk,	umux_fsys1_phyclk_ufs_link_sdcard_rx_pwm_clk_user,	gategrp_fsys1_phyclk_ufs_link_sdcard_rx_pwm_clk);
GRPGATE(gate_fsys1_phyclk_ufs_link_sdcard_tx_pwm_clk,	umux_fsys1_phyclk_ufs_link_sdcard_tx_pwm_clk_user,	gategrp_fsys1_phyclk_ufs_link_sdcard_tx_pwm_clk);
GRPGATE(gate_fsys1_phyclk_ufs_link_sdcard_refclk_out_soc,	umux_fsys1_phyclk_ufs_link_sdcard_refclk_out_soc_user,	gategrp_fsys1_phyclk_ufs_link_sdcard_refclk_out_soc);
GRPGATE(gate_g3d_common,	dvfs_g3d,	gategrp_g3d_common);
GRPGATE(gate_g3d_g3d_common,	gate_g3d_common,	gategrp_g3d_g3d_common);
GRPGATE(gate_g3d_g3d,	gate_g3d_g3d_common,	gategrp_g3d_g3d);
GRPGATE(gate_g3d_iram_path_test,	gate_g3d_g3d_common,	gategrp_g3d_iram_path_test);
GRPGATE(gate_g3d_ppmu,	gate_g3d_common,	gategrp_g3d_ppmu);
GRPGATE(gate_g3d_bts,	gate_g3d_common,	gategrp_g3d_bts);
GRPGATE(gate_imem_common,	pxmxdx_imem,	gategrp_imem_common);
GRPGATE(gate_imem_apm_sss_rtic_mc_common,	gate_imem_common,	gategrp_imem_apm_sss_rtic_mc_common);
GRPGATE(gate_imem_apm,	gate_imem_apm_sss_rtic_mc_common,	gategrp_imem_apm);
GRPGATE(gate_imem_sss,	gate_imem_apm_sss_rtic_mc_common,	gategrp_imem_sss);
GRPGATE(gate_imem_rtic,	gate_imem_apm_sss_rtic_mc_common,	gategrp_imem_rtic);
GRPGATE(gate_imem_mc,	gate_imem_apm_sss_rtic_mc_common,	gategrp_imem_mc);
GRPGATE(gate_imem_intmem,	gate_imem_common,	gategrp_imem_intmem);
GRPGATE(gate_imem_intmem_alv,	gate_imem_common,	gategrp_imem_intmem_alv);
GRPGATE(gate_imem_gic400,	gate_imem_common,	gategrp_imem_gic400);
GRPGATE(gate_imem_ppmu,	gate_imem_common,	gategrp_imem_ppmu);
GRPGATE(gate_isp0_common,	gate_bus0_cam,	gategrp_isp0_common);
GRPGATE(gate_isp0_fimc_isp0,	gate_isp0_common,	gategrp_isp0_fimc_isp0);
GRPGATE(gate_isp0_fimc_tpu,	gate_isp0_common,	gategrp_isp0_fimc_tpu);
GRPGATE(gate_isp0_sysmmu,	gate_isp0_common,	gategrp_isp0_sysmmu);
GRPGATE(gate_isp0_ppmu,	gate_isp0_common,	gategrp_isp0_ppmu);
GRPGATE(gate_isp0_bts,	gate_isp0_common,	gategrp_isp0_bts);
GRPGATE(gate_isp1_fimc_isp1,	gate_bus0_cam,	gategrp_isp1_fimc_isp1);
GRPGATE(gate_isp1_sysmmu,	gate_isp1_fimc_isp1,	gategrp_isp1_sysmmu);
GRPGATE(gate_isp1_ppmu,	gate_isp1_fimc_isp1,	gategrp_isp1_ppmu);
GRPGATE(gate_isp1_bts,	gate_isp1_fimc_isp1,	gategrp_isp1_bts);
GRPGATE(gate_mfc_common,	pxmxdx_mfc,	gategrp_mfc_common);
GRPGATE(gate_mfc_mfc,	gate_mfc_common,	gategrp_mfc_mfc);
GRPGATE(gate_mfc_hpm,	gate_mfc_common,	gategrp_mfc_hpm);
GRPGATE(gate_mfc_sysmmu,	gate_mfc_common,	gategrp_mfc_sysmmu);
GRPGATE(gate_mfc_ppmu,	gate_mfc_common,	gategrp_mfc_ppmu);
GRPGATE(gate_mscl_common,	pxmxdx_mscl,	gategrp_mscl_common);
GRPGATE(gate_mscl_mscl0_jpeg_common,	gate_mscl_common,	gategrp_mscl_mscl0_jpeg_common);
GRPGATE(gate_mscl_mscl0,	gate_mscl_mscl0_jpeg_common,	gategrp_mscl_mscl0);
GRPGATE(gate_mscl_jpeg,	gate_mscl_mscl0_jpeg_common,	gategrp_mscl_jpeg);
GRPGATE(gate_mscl_mscl1_g2d_common,	gate_mscl_common,	gategrp_mscl_mscl1_g2d_common);
GRPGATE(gate_mscl_mscl1,	gate_mscl_mscl1_g2d_common,	gategrp_mscl_mscl1);
GRPGATE(gate_mscl_g2d,	gate_mscl_mscl1_g2d_common,	gategrp_mscl_g2d);
GRPGATE(gate_mscl_sysmmu,	gate_mscl_common,	gategrp_mscl_sysmmu);
GRPGATE(gate_mscl_ppmu,	gate_mscl_common,	gategrp_mscl_ppmu);
GRPGATE(gate_mscl_bts,	gate_mscl_common,	gategrp_mscl_bts);
GRPGATE(gate_peric0_hsi2c0,	0,	gategrp_peric0_hsi2c0);
GRPGATE(gate_peric0_hsi2c1,	0,	gategrp_peric0_hsi2c1);
GRPGATE(gate_peric0_hsi2c4,	0,	gategrp_peric0_hsi2c4);
GRPGATE(gate_peric0_hsi2c5,	0,	gategrp_peric0_hsi2c5);
GRPGATE(gate_peric0_hsi2c9,	0,	gategrp_peric0_hsi2c9);
GRPGATE(gate_peric0_hsi2c10,	0,	gategrp_peric0_hsi2c10);
GRPGATE(gate_peric0_hsi2c11,	0,	gategrp_peric0_hsi2c11);
GRPGATE(gate_peric0_uart0,	sclk_uart0,	gategrp_peric0_uart0);
GRPGATE(gate_peric0_adcif,	0,	gategrp_peric0_adcif);
GRPGATE(gate_peric0_pwm,	0,	gategrp_peric0_pwm);
GRPGATE(gate_peric0_sclk_pwm,	0,	gategrp_peric0_sclk_pwm);
GRPGATE(gate_peric1_hsi2c_common,	0,	gategrp_peric1_hsi2c_common);
GRPGATE(gate_peric1_hsi2c2,	gate_peric1_hsi2c_common,	gategrp_peric1_hsi2c2);
GRPGATE(gate_peric1_hsi2c3,	gate_peric1_hsi2c_common,	gategrp_peric1_hsi2c3);
GRPGATE(gate_peric1_hsi2c6,	gate_peric1_hsi2c_common,	gategrp_peric1_hsi2c6);
GRPGATE(gate_peric1_hsi2c7,	gate_peric1_hsi2c_common,	gategrp_peric1_hsi2c7);
GRPGATE(gate_peric1_hsi2c8,	gate_peric1_hsi2c_common,	gategrp_peric1_hsi2c8);
GRPGATE(gate_peric1_hsi2c12,	gate_peric1_hsi2c_common,	gategrp_peric1_hsi2c12);
GRPGATE(gate_peric1_hsi2c13,	gate_peric1_hsi2c_common,	gategrp_peric1_hsi2c13);
GRPGATE(gate_peric1_hsi2c14,	gate_peric1_hsi2c_common,	gategrp_peric1_hsi2c14);
GRPGATE(gate_peric1_spi_i2s_pcm1_spdif_common,	0,	gategrp_peric1_spi_i2s_pcm1_spdif_common);
GRPGATE(gate_peric1_uart1,	sclk_uart1,	gategrp_peric1_uart1);
GRPGATE(gate_peric1_uart2,	sclk_uart2,	gategrp_peric1_uart2);
GRPGATE(gate_peric1_uart3,	sclk_uart3,	gategrp_peric1_uart3);
GRPGATE(gate_peric1_uart4,	sclk_uart4,	gategrp_peric1_uart4);
GRPGATE(gate_peric1_uart5,	sclk_uart5,	gategrp_peric1_uart5);
GRPGATE(gate_peric1_spi0,	sclk_spi0,	gategrp_peric1_spi0);
GRPGATE(gate_peric1_spi1,	sclk_spi1,	gategrp_peric1_spi1);
GRPGATE(gate_peric1_spi2,	sclk_spi2,	gategrp_peric1_spi2);
GRPGATE(gate_peric1_spi3,	sclk_spi3,	gategrp_peric1_spi3);
GRPGATE(gate_peric1_spi4,	sclk_spi4,	gategrp_peric1_spi4);
GRPGATE(gate_peric1_spi5,	sclk_spi5,	gategrp_peric1_spi5);
GRPGATE(gate_peric1_spi6,	sclk_spi6,	gategrp_peric1_spi6);
GRPGATE(gate_peric1_spi7,	sclk_spi7,	gategrp_peric1_spi7);
GRPGATE(gate_peric1_i2s1,	gate_peric1_spi_i2s_pcm1_spdif_common,	gategrp_peric1_i2s1);
GRPGATE(gate_peric1_pcm1,	gate_peric1_spi_i2s_pcm1_spdif_common,	gategrp_peric1_pcm1);
GRPGATE(gate_peric1_spdif,	gate_peric1_spi_i2s_pcm1_spdif_common,	gategrp_peric1_spdif);
GRPGATE(gate_peric1_gpio_nfc,	0,	gategrp_peric1_gpio_nfc);
GRPGATE(gate_peric1_gpio_touch,	0,	gategrp_peric1_gpio_touch);
GRPGATE(gate_peric1_gpio_fp,	0,	gategrp_peric1_gpio_fp);
GRPGATE(gate_peric1_gpio_ese,	0,	gategrp_peric1_gpio_ese);
GRPGATE(gate_peris_sfr_apbif_hdmi_cec,	0,	gategrp_peris_sfr_apbif_hdmi_cec);
GRPGATE(gate_peris_hpm,	0,	gategrp_peris_hpm);
GRPGATE(gate_peris_mct,	0,	gategrp_peris_mct);
GRPGATE(gate_peris_wdt_mngs,	0,	gategrp_peris_wdt_mngs);
GRPGATE(gate_peris_wdt_apollo,	0,	gategrp_peris_wdt_apollo);
GRPGATE(gate_peris_sysreg_peris,	0,	gategrp_peris_sysreg_peris);
GRPGATE(gate_peris_monocnt_apbif,	0,	gategrp_peris_monocnt_apbif);
GRPGATE(gate_peris_rtc_apbif,	0,	gategrp_peris_rtc_apbif);
GRPGATE(gate_peris_top_rtc,	0,	gategrp_peris_top_rtc);
GRPGATE(gate_peris_otp_con_top,	0,	gategrp_peris_otp_con_top);
GRPGATE(gate_peris_chipid,	0,	gategrp_peris_chipid);
GRPGATE(gate_peris_tmu,	0,	gategrp_peris_tmu);






void vclk_unused_disable(void)
{
	vclk_disable(VCLK(gate_apollo_ppmu));
	vclk_disable(VCLK(gate_apollo_bts));
	vclk_disable(VCLK(gate_mngs_ppmu));
	vclk_disable(VCLK(gate_mngs_bts));
	vclk_disable(VCLK(gate_aud_lpass));
	vclk_disable(VCLK(gate_aud_mi2s));
	vclk_disable(VCLK(gate_aud_pcm));
	vclk_disable(VCLK(gate_aud_uart));
	vclk_disable(VCLK(gate_aud_dma));
	vclk_disable(VCLK(gate_aud_slimbus));
	vclk_disable(VCLK(gate_aud_sysmmu));
	vclk_disable(VCLK(gate_aud_ppmu));
	vclk_disable(VCLK(gate_aud_bts));
	vclk_disable(VCLK(gate_aud_sclk_mi2s));
	vclk_disable(VCLK(gate_aud_sclk_pcm));
	vclk_disable(VCLK(gate_aud_sclk_slimbus));
	vclk_disable(VCLK(gate_cam0_csis0));
	vclk_disable(VCLK(gate_cam0_csis1));
	vclk_disable(VCLK(gate_cam0_fimc_bns));
	vclk_disable(VCLK(gate_cam0_fimc_3aa0));
	vclk_disable(VCLK(gate_cam0_fimc_3aa1));
	vclk_disable(VCLK(gate_cam0_hpm));
	vclk_disable(VCLK(gate_cam0_sysmmu));
	vclk_disable(VCLK(gate_cam0_ppmu));
	vclk_disable(VCLK(gate_cam0_bts));
	vclk_disable(VCLK(gate_cam1_isp_cpu));
	vclk_disable(VCLK(gate_cam1_gic_is));
	vclk_disable(VCLK(gate_cam1_csis2));
	vclk_disable(VCLK(gate_cam1_csis3));
	vclk_disable(VCLK(gate_cam1_fimc_vra));
	vclk_disable(VCLK(gate_cam1_mc_scaler));
	vclk_disable(VCLK(gate_cam1_i2c0_isp));
	vclk_disable(VCLK(gate_cam1_i2c1_isp));
	vclk_disable(VCLK(gate_cam1_i2c2_isp));
	vclk_disable(VCLK(gate_cam1_i2c3_isp));
	vclk_disable(VCLK(gate_cam1_wdt_isp));
	vclk_disable(VCLK(gate_cam1_mcuctl_isp));
	vclk_disable(VCLK(gate_cam1_uart_isp));
	vclk_disable(VCLK(gate_cam1_sclk_uart_isp));
	vclk_disable(VCLK(gate_cam1_spi0_isp));
	vclk_disable(VCLK(gate_cam1_spi1_isp));
	vclk_disable(VCLK(gate_cam1_pdma_isp));
	vclk_disable(VCLK(gate_cam1_pwm_isp));
	vclk_disable(VCLK(gate_cam1_sclk_pwm_isp));
	vclk_disable(VCLK(gate_cam1_sysmmu));
	vclk_disable(VCLK(gate_cam1_ppmu));
	vclk_disable(VCLK(gate_cam1_bts));
	vclk_disable(VCLK(gate_ccore_i2c));
	vclk_disable(VCLK(gate_disp0_decon0));
	vclk_disable(VCLK(gate_disp0_dsim0));
	vclk_disable(VCLK(gate_disp0_dsim1));
	vclk_disable(VCLK(gate_disp0_dsim2));
	vclk_disable(VCLK(gate_disp0_hdmi));
	vclk_disable(VCLK(gate_disp0_dp));
	vclk_disable(VCLK(gate_disp0_hpm_apbif_disp0));
	vclk_disable(VCLK(gate_disp0_sysmmu));
	vclk_disable(VCLK(gate_disp0_ppmu));
	vclk_disable(VCLK(gate_disp0_bts));
	vclk_disable(VCLK(gate_disp0_phyclk_hdmiphy_tmds_20b_clko));
	vclk_disable(VCLK(gate_disp0_phyclk_hdmiphy_tmds_10b_clko));
	vclk_disable(VCLK(gate_disp0_phyclk_hdmiphy_pixel_clko));
	vclk_disable(VCLK(gate_disp0_phyclk_mipidphy0_rxclkesc0));
	vclk_disable(VCLK(gate_disp0_phyclk_mipidphy0_bitclkdiv8));
	vclk_disable(VCLK(gate_disp0_phyclk_mipidphy1_rxclkesc0));
	vclk_disable(VCLK(gate_disp0_phyclk_mipidphy1_bitclkdiv8));
	vclk_disable(VCLK(gate_disp0_phyclk_mipidphy2_rxclkesc0));
	vclk_disable(VCLK(gate_disp0_phyclk_mipidphy2_bitclkdiv8));
	vclk_disable(VCLK(gate_disp0_phyclk_dpphy_ch0_txd_clk));
	vclk_disable(VCLK(gate_disp0_phyclk_dpphy_ch1_txd_clk));
	vclk_disable(VCLK(gate_disp0_phyclk_dpphy_ch2_txd_clk));
	vclk_disable(VCLK(gate_disp0_phyclk_dpphy_ch3_txd_clk));
	vclk_disable(VCLK(gate_disp0_oscclk_dp_i_clk_24m));
	vclk_disable(VCLK(gate_disp0_oscclk_i_dptx_phy_i_ref_clk_24m));
	vclk_disable(VCLK(gate_disp0_oscclk_i_mipi_dphy_m1s0_m_xi));
	vclk_disable(VCLK(gate_disp0_oscclk_i_mipi_dphy_m4s0_m_xi));
	vclk_disable(VCLK(gate_disp0_oscclk_i_mipi_dphy_m4s4_m_xi));
	vclk_disable(VCLK(gate_disp1_decon1));
	vclk_disable(VCLK(gate_disp1_hpmdisp1));
	vclk_disable(VCLK(gate_disp1_sysmmu));
	vclk_disable(VCLK(gate_disp1_ppmu));
	vclk_disable(VCLK(gate_disp1_bts));
	vclk_disable(VCLK(gate_fsys0_usbdrd30));
	vclk_disable(VCLK(gate_fsys0_etr_usb));
	vclk_disable(VCLK(gate_fsys0_usbhost20));
	vclk_disable(VCLK(gate_fsys0_mmc0));
	vclk_disable(VCLK(gate_fsys0_ufs_linkemedded));
	vclk_disable(VCLK(gate_fsys0_pdma0));
	vclk_disable(VCLK(gate_fsys0_pdmas));
	vclk_disable(VCLK(gate_fsys0_ppmu));
	vclk_disable(VCLK(gate_fsys0_bts));
	vclk_disable(VCLK(gate_fsys0_hpm));
	vclk_disable(VCLK(gate_fsys1_mmc2));
	vclk_disable(VCLK(gate_fsys1_ufs20_sdcard));
	vclk_disable(VCLK(gate_fsys1_sromc));
	vclk_disable(VCLK(gate_fsys1_pciewifi0));
	vclk_disable(VCLK(gate_fsys1_pciewifi1));
	vclk_disable(VCLK(gate_fsys1_ppmu));
	vclk_disable(VCLK(gate_fsys1_bts));
	vclk_disable(VCLK(gate_fsys1_hpm));
	vclk_disable(VCLK(gate_g3d_g3d));
	vclk_disable(VCLK(gate_g3d_iram_path_test));
	vclk_disable(VCLK(gate_g3d_ppmu));
	vclk_disable(VCLK(gate_g3d_bts));
	vclk_disable(VCLK(gate_imem_mc));
	vclk_disable(VCLK(gate_imem_ppmu));
	vclk_disable(VCLK(gate_isp0_fimc_isp0));
	vclk_disable(VCLK(gate_isp0_fimc_tpu));
	vclk_disable(VCLK(gate_isp0_sysmmu));
	vclk_disable(VCLK(gate_isp0_ppmu));
	vclk_disable(VCLK(gate_isp0_bts));
	vclk_disable(VCLK(gate_isp1_fimc_isp1));
	vclk_disable(VCLK(gate_isp1_sysmmu));
	vclk_disable(VCLK(gate_isp1_ppmu));
	vclk_disable(VCLK(gate_isp1_bts));
	vclk_disable(VCLK(gate_mfc_mfc));
	vclk_disable(VCLK(gate_mfc_hpm));
	vclk_disable(VCLK(gate_mfc_sysmmu));
	vclk_disable(VCLK(gate_mfc_ppmu));
	vclk_disable(VCLK(gate_mscl_mscl0));
	vclk_disable(VCLK(gate_mscl_jpeg));
	vclk_disable(VCLK(gate_mscl_mscl1));
	vclk_disable(VCLK(gate_mscl_g2d));
	vclk_disable(VCLK(gate_mscl_sysmmu));
	vclk_disable(VCLK(gate_mscl_ppmu));
	vclk_disable(VCLK(gate_mscl_bts));
	vclk_disable(VCLK(gate_peric0_hsi2c0));
	vclk_disable(VCLK(gate_peric0_hsi2c1));
	vclk_disable(VCLK(gate_peric0_hsi2c4));
	vclk_disable(VCLK(gate_peric0_hsi2c5));
	vclk_disable(VCLK(gate_peric0_hsi2c9));
	vclk_disable(VCLK(gate_peric0_hsi2c10));
	vclk_disable(VCLK(gate_peric0_hsi2c11));
	vclk_disable(VCLK(gate_peric0_uart0));
	vclk_disable(VCLK(gate_peric0_adcif));
	vclk_disable(VCLK(gate_peric0_sclk_pwm));
	vclk_disable(VCLK(gate_peric1_hsi2c2));
	vclk_disable(VCLK(gate_peric1_hsi2c3));
	vclk_disable(VCLK(gate_peric1_hsi2c6));
	vclk_disable(VCLK(gate_peric1_hsi2c7));
	vclk_disable(VCLK(gate_peric1_hsi2c8));
	vclk_disable(VCLK(gate_peric1_hsi2c12));
	vclk_disable(VCLK(gate_peric1_hsi2c13));
	vclk_disable(VCLK(gate_peric1_hsi2c14));
	vclk_disable(VCLK(gate_peric1_uart1));
	vclk_disable(VCLK(gate_peric1_uart3));
	vclk_disable(VCLK(gate_peric1_uart5));
	vclk_disable(VCLK(gate_peric1_spi0));
	vclk_disable(VCLK(gate_peric1_spi1));
	vclk_disable(VCLK(gate_peric1_spi2));
	vclk_disable(VCLK(gate_peric1_spi3));
	vclk_disable(VCLK(gate_peric1_spi4));
	vclk_disable(VCLK(gate_peric1_spi5));
	vclk_disable(VCLK(gate_peric1_spi6));
	vclk_disable(VCLK(gate_peric1_spi7));
	vclk_disable(VCLK(gate_peric1_i2s1));
	vclk_disable(VCLK(gate_peric1_pcm1));
	vclk_disable(VCLK(gate_peric1_spdif));
	vclk_disable(VCLK(gate_peris_sfr_apbif_hdmi_cec));
	vclk_disable(VCLK(gate_peris_hpm));
	vclk_disable(VCLK(gate_peris_tmu));
	vclk_disable(VCLK(sclk_decon0_eclk0));
	vclk_disable(VCLK(sclk_decon0_vclk0));
	vclk_disable(VCLK(sclk_decon0_vclk1));
	vclk_disable(VCLK(sclk_hdmi_audio));
	vclk_disable(VCLK(sclk_decon1_eclk0));
	vclk_disable(VCLK(sclk_decon1_eclk1));
	vclk_disable(VCLK(sclk_usbdrd30));
	vclk_disable(VCLK(sclk_mmc0));
	vclk_disable(VCLK(sclk_ufsunipro20));
	vclk_disable(VCLK(sclk_phy24m));
	vclk_disable(VCLK(sclk_ufsunipro_cfg));
	vclk_disable(VCLK(sclk_mmc2));
	vclk_disable(VCLK(sclk_ufsunipro20_sdcard));
	vclk_disable(VCLK(sclk_pcie_phy));
	vclk_disable(VCLK(sclk_ufsunipro_sdcard_cfg));
	vclk_disable(VCLK(sclk_uart0));
	vclk_disable(VCLK(sclk_spi0));
	vclk_disable(VCLK(sclk_spi1));
	vclk_disable(VCLK(sclk_spi2));
	vclk_disable(VCLK(sclk_spi3));
	vclk_disable(VCLK(sclk_spi4));
	vclk_disable(VCLK(sclk_spi5));
	vclk_disable(VCLK(sclk_spi6));
	vclk_disable(VCLK(sclk_spi7));
	vclk_disable(VCLK(sclk_uart1));
	vclk_disable(VCLK(sclk_uart3));
	vclk_disable(VCLK(sclk_uart4));
	vclk_disable(VCLK(sclk_uart5));
	vclk_disable(VCLK(sclk_isp_spi0));
	vclk_disable(VCLK(sclk_isp_spi1));
	vclk_disable(VCLK(sclk_isp_uart));
	vclk_disable(VCLK(sclk_isp_sensor0));
	vclk_disable(VCLK(sclk_isp_sensor1));
	vclk_disable(VCLK(sclk_isp_sensor2));
	vclk_disable(VCLK(sclk_isp_sensor3));
	vclk_disable(VCLK(sclk_decon0_eclk0_local));
	vclk_disable(VCLK(sclk_decon0_vclk0_local));
	vclk_disable(VCLK(sclk_decon0_vclk1_local));
	vclk_disable(VCLK(sclk_decon1_eclk0_local));
	vclk_disable(VCLK(sclk_decon1_eclk1_local));

	vclk_disable(VCLK(p1_disp_pll));
	vclk_disable(VCLK(p1_aud_pll));

	vclk_disable(VCLK(d1_sclk_i2s_local));
	vclk_disable(VCLK(d1_sclk_pcm_local));
	vclk_disable(VCLK(d1_sclk_slimbus));
	vclk_disable(VCLK(d1_sclk_cp_i2s));
	vclk_disable(VCLK(d1_sclk_asrc));

	vclk_disable(VCLK(pxmxdx_mfc));
	vclk_disable(VCLK(pxmxdx_mscl));
	vclk_disable(VCLK(pxmxdx_fsys0));
	vclk_disable(VCLK(pxmxdx_fsys1));
	vclk_disable(VCLK(pxmxdx_disp0));
	vclk_disable(VCLK(pxmxdx_disp1));
	vclk_disable(VCLK(pxmxdx_aud));
	vclk_disable(VCLK(pxmxdx_aud_cp));
	vclk_disable(VCLK(pxmxdx_isp0_isp0));
	vclk_disable(VCLK(pxmxdx_isp0_tpu));
	vclk_disable(VCLK(pxmxdx_isp0_trex));
	vclk_disable(VCLK(pxmxdx_isp0_pxl_asbs));
	vclk_disable(VCLK(pxmxdx_isp1_isp1));
	vclk_disable(VCLK(pxmxdx_cam0_csis0));
	vclk_disable(VCLK(pxmxdx_cam0_csis1));
	vclk_disable(VCLK(pxmxdx_cam0_csis2));
	vclk_disable(VCLK(pxmxdx_cam0_csis3));
	vclk_disable(VCLK(pxmxdx_cam0_3aa0));
	vclk_disable(VCLK(pxmxdx_cam0_3aa1));
	vclk_disable(VCLK(pxmxdx_cam0_trex));
	vclk_disable(VCLK(pxmxdx_cam1_arm));
	vclk_disable(VCLK(pxmxdx_cam1_vra));
	vclk_disable(VCLK(pxmxdx_cam1_trex));
	vclk_disable(VCLK(pxmxdx_cam1_bus));
	vclk_disable(VCLK(pxmxdx_cam1_peri));
	vclk_disable(VCLK(pxmxdx_cam1_csis2));
	vclk_disable(VCLK(pxmxdx_cam1_csis3));
	vclk_disable(VCLK(pxmxdx_cam1_scl));
	vclk_disable(VCLK(pxmxdx_oscclk_nfc));
	vclk_disable(VCLK(pxmxdx_oscclk_aud));
	vclk_disable(VCLK(gate_bus0_display));
	vclk_disable(VCLK(gate_bus0_cam));
	vclk_disable(VCLK(gate_bus0_fsys1));
	vclk_disable(VCLK(gate_bus1_mfc));
	vclk_disable(VCLK(gate_bus1_mscl));
	vclk_disable(VCLK(gate_bus1_fsys0));
	vclk_disable(VCLK(umux_bus0_aclk_bus0_528));
	vclk_disable(VCLK(umux_bus0_aclk_bus0_200));
	vclk_disable(VCLK(umux_bus1_aclk_bus1_528));
#if 0
	vclk_disable(VCLK(gate_peric0_pwm));
	vclk_disable(VCLK(gate_peric1_uart2));
	vclk_disable(VCLK(gate_peric1_uart4));
	vclk_disable(VCLK(sclk_uart2));
	vclk_disable(VCLK(sclk_ap2cp_mif_pll_out));
	vclk_disable(VCLK(p1_mfc_pll));
#endif
}

void vclk_init(void)
{
	ADD_LIST(vclk_grpgate_list, gate_apollo_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_apollo_bts);
	ADD_LIST(vclk_grpgate_list, gate_mngs_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_mngs_bts);
	ADD_LIST(vclk_grpgate_list, gate_aud_common);
	ADD_LIST(vclk_grpgate_list, gate_aud_lpass);
	ADD_LIST(vclk_grpgate_list, gate_aud_mi2s);
	ADD_LIST(vclk_grpgate_list, gate_aud_pcm);
	ADD_LIST(vclk_grpgate_list, gate_aud_uart);
	ADD_LIST(vclk_grpgate_list, gate_aud_dma);
	ADD_LIST(vclk_grpgate_list, gate_aud_slimbus);
	ADD_LIST(vclk_grpgate_list, gate_aud_sysmmu);
	ADD_LIST(vclk_grpgate_list, gate_aud_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_aud_bts);
	ADD_LIST(vclk_grpgate_list, gate_aud_sclk_mi2s);
	ADD_LIST(vclk_grpgate_list, gate_aud_sclk_pcm);
	ADD_LIST(vclk_grpgate_list, gate_aud_sclk_slimbus);
	ADD_LIST(vclk_grpgate_list, gate_bus0_display);
	ADD_LIST(vclk_grpgate_list, gate_bus0_cam);
	ADD_LIST(vclk_grpgate_list, gate_bus0_fsys1);
	ADD_LIST(vclk_grpgate_list, gate_bus1_mfc);
	ADD_LIST(vclk_grpgate_list, gate_bus1_mscl);
	ADD_LIST(vclk_grpgate_list, gate_bus1_fsys0);
	ADD_LIST(vclk_grpgate_list, gate_cam0_common);
	ADD_LIST(vclk_grpgate_list, gate_cam0_csis0);
	ADD_LIST(vclk_grpgate_list, gate_cam0_csis1);
	ADD_LIST(vclk_grpgate_list, gate_cam0_fimc_bns);
	ADD_LIST(vclk_grpgate_list, gate_cam0_fimc_3aa0);
	ADD_LIST(vclk_grpgate_list, gate_cam0_fimc_3aa1);
	ADD_LIST(vclk_grpgate_list, gate_cam0_hpm);
	ADD_LIST(vclk_grpgate_list, gate_cam0_sysmmu);
	ADD_LIST(vclk_grpgate_list, gate_cam0_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_cam0_bts);
	ADD_LIST(vclk_grpgate_list, gate_cam0_phyclk_hs0_csis0_rx_byte);
	ADD_LIST(vclk_grpgate_list, gate_cam0_phyclk_hs1_csis0_rx_byte);
	ADD_LIST(vclk_grpgate_list, gate_cam0_phyclk_hs2_csis0_rx_byte);
	ADD_LIST(vclk_grpgate_list, gate_cam0_phyclk_hs3_csis0_rx_byte);
	ADD_LIST(vclk_grpgate_list, gate_cam0_phyclk_hs0_csis1_rx_byte);
	ADD_LIST(vclk_grpgate_list, gate_cam0_phyclk_hs1_csis1_rx_byte);
	ADD_LIST(vclk_grpgate_list, gate_cam1_common);
	ADD_LIST(vclk_grpgate_list, gate_cam1_isp_cpu_gic_common);
	ADD_LIST(vclk_grpgate_list, gate_cam1_isp_cpu);
	ADD_LIST(vclk_grpgate_list, gate_cam1_gic_is);
	ADD_LIST(vclk_grpgate_list, gate_cam1_csis2);
	ADD_LIST(vclk_grpgate_list, gate_cam1_csis3);
	ADD_LIST(vclk_grpgate_list, gate_cam1_fimc_vra);
	ADD_LIST(vclk_grpgate_list, gate_cam1_mc_scaler);
	ADD_LIST(vclk_grpgate_list, gate_cam1_i2c0_isp);
	ADD_LIST(vclk_grpgate_list, gate_cam1_i2c1_isp);
	ADD_LIST(vclk_grpgate_list, gate_cam1_i2c2_isp);
	ADD_LIST(vclk_grpgate_list, gate_cam1_i2c3_isp);
	ADD_LIST(vclk_grpgate_list, gate_cam1_wdt_isp);
	ADD_LIST(vclk_grpgate_list, gate_cam1_mcuctl_isp);
	ADD_LIST(vclk_grpgate_list, gate_cam1_uart_isp);
	ADD_LIST(vclk_grpgate_list, gate_cam1_sclk_uart_isp);
	ADD_LIST(vclk_grpgate_list, gate_cam1_spi0_isp);
	ADD_LIST(vclk_grpgate_list, gate_cam1_spi1_isp);
	ADD_LIST(vclk_grpgate_list, gate_cam1_pdma_isp);
	ADD_LIST(vclk_grpgate_list, gate_cam1_pwm_isp);
	ADD_LIST(vclk_grpgate_list, gate_cam1_sclk_pwm_isp);
	ADD_LIST(vclk_grpgate_list, gate_cam1_sysmmu);
	ADD_LIST(vclk_grpgate_list, gate_cam1_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_cam1_bts);
	ADD_LIST(vclk_grpgate_list, gate_cam1_phyclk_hs0_csis2_rx_byte);
	ADD_LIST(vclk_grpgate_list, gate_cam1_phyclk_hs1_csis2_rx_byte);
	ADD_LIST(vclk_grpgate_list, gate_cam1_phyclk_hs2_csis2_rx_byte);
	ADD_LIST(vclk_grpgate_list, gate_cam1_phyclk_hs3_csis2_rx_byte);
	ADD_LIST(vclk_grpgate_list, gate_cam1_phyclk_hs0_csis3_rx_byte);
	ADD_LIST(vclk_grpgate_list, gate_ccore_i2c);
	ADD_LIST(vclk_grpgate_list, gate_disp0_common);
	ADD_LIST(vclk_grpgate_list, gate_disp0_decon0);
	ADD_LIST(vclk_grpgate_list, gate_disp0_dsim0);
	ADD_LIST(vclk_grpgate_list, gate_disp0_dsim1);
	ADD_LIST(vclk_grpgate_list, gate_disp0_dsim2);
	ADD_LIST(vclk_grpgate_list, gate_disp0_hdmi);
	ADD_LIST(vclk_grpgate_list, gate_disp0_dp);
	ADD_LIST(vclk_grpgate_list, gate_disp0_hpm_apbif_disp0);
	ADD_LIST(vclk_grpgate_list, gate_disp0_sysmmu);
	ADD_LIST(vclk_grpgate_list, gate_disp0_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_disp0_bts);
	ADD_LIST(vclk_grpgate_list, gate_disp0_phyclk_hdmiphy_tmds_20b_clko);
	ADD_LIST(vclk_grpgate_list, gate_disp0_phyclk_hdmiphy_tmds_10b_clko);
	ADD_LIST(vclk_grpgate_list, gate_disp0_phyclk_hdmiphy_pixel_clko);
	ADD_LIST(vclk_grpgate_list, gate_disp0_phyclk_mipidphy0_rxclkesc0);
	ADD_LIST(vclk_grpgate_list, gate_disp0_phyclk_mipidphy0_bitclkdiv8);
	ADD_LIST(vclk_grpgate_list, gate_disp0_phyclk_mipidphy1_rxclkesc0);
	ADD_LIST(vclk_grpgate_list, gate_disp0_phyclk_mipidphy1_bitclkdiv8);
	ADD_LIST(vclk_grpgate_list, gate_disp0_phyclk_mipidphy2_rxclkesc0);
	ADD_LIST(vclk_grpgate_list, gate_disp0_phyclk_mipidphy2_bitclkdiv8);
	ADD_LIST(vclk_grpgate_list, gate_disp0_phyclk_dpphy_ch0_txd_clk);
	ADD_LIST(vclk_grpgate_list, gate_disp0_phyclk_dpphy_ch1_txd_clk);
	ADD_LIST(vclk_grpgate_list, gate_disp0_phyclk_dpphy_ch2_txd_clk);
	ADD_LIST(vclk_grpgate_list, gate_disp0_phyclk_dpphy_ch3_txd_clk);
	ADD_LIST(vclk_grpgate_list, gate_disp0_oscclk_dp_i_clk_24m);
	ADD_LIST(vclk_grpgate_list, gate_disp0_oscclk_i_dptx_phy_i_ref_clk_24m);
	ADD_LIST(vclk_grpgate_list, gate_disp0_oscclk_i_mipi_dphy_m1s0_m_xi);
	ADD_LIST(vclk_grpgate_list, gate_disp0_oscclk_i_mipi_dphy_m4s0_m_xi);
	ADD_LIST(vclk_grpgate_list, gate_disp0_oscclk_i_mipi_dphy_m4s4_m_xi);
	ADD_LIST(vclk_grpgate_list, gate_disp1_common);
	ADD_LIST(vclk_grpgate_list, gate_disp1_decon1);
	ADD_LIST(vclk_grpgate_list, gate_disp1_hpmdisp1);
	ADD_LIST(vclk_grpgate_list, gate_disp1_sysmmu);
	ADD_LIST(vclk_grpgate_list, gate_disp1_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_disp1_bts);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_common);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_usbdrd_etrusb_common);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_usbdrd30);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_etr_usb);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_usbhost20);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_mmc0_ufs_common);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_mmc0);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_ufs_linkemedded);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_pdma_common);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_pdma0);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_pdmas);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_bts);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_hpm);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_phyclk_usbdrd30_udrd30_phyclock);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_phyclk_usbdrd30_udrd30_pipe_pclk);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_phyclk_ufs_tx0_symbol);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_phyclk_ufs_rx0_symbol);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_phyclk_usbhost20_phyclock);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_phyclk_usbhost20_freeclk);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_phyclk_usbhost20_clk48mohci);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_phyclk_usbhost20phy_ref_clk);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_phyclk_ufs_rx_pwm_clk);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_phyclk_ufs_tx_pwm_clk);
	ADD_LIST(vclk_grpgate_list, gate_fsys0_phyclk_ufs_refclk_out_soc);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_common);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_mmc2_ufs_common);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_mmc2);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_ufs20_sdcard);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_sromc);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_pciewifi0);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_pciewifi1);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_bts);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_hpm);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_phyclk_ufs_link_sdcard_tx0_symbol);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_phyclk_ufs_link_sdcard_rx0_symbol);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_phyclk_pcie_wifi0_tx0);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_phyclk_pcie_wifi0_rx0);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_phyclk_pcie_wifi1_tx0);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_phyclk_pcie_wifi1_rx0);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_phyclk_pcie_wifi0_dig_refclk);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_phyclk_pcie_wifi1_dig_refclk);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_phyclk_ufs_link_sdcard_rx_pwm_clk);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_phyclk_ufs_link_sdcard_tx_pwm_clk);
	ADD_LIST(vclk_grpgate_list, gate_fsys1_phyclk_ufs_link_sdcard_refclk_out_soc);
	ADD_LIST(vclk_grpgate_list, gate_g3d_common);
	ADD_LIST(vclk_grpgate_list, gate_g3d_g3d_common);
	ADD_LIST(vclk_grpgate_list, gate_g3d_g3d);
	ADD_LIST(vclk_grpgate_list, gate_g3d_iram_path_test);
	ADD_LIST(vclk_grpgate_list, gate_g3d_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_g3d_bts);
	ADD_LIST(vclk_grpgate_list, gate_imem_common);
	ADD_LIST(vclk_grpgate_list, gate_imem_apm_sss_rtic_mc_common);
	ADD_LIST(vclk_grpgate_list, gate_imem_apm);
	ADD_LIST(vclk_grpgate_list, gate_imem_sss);
	ADD_LIST(vclk_grpgate_list, gate_imem_rtic);
	ADD_LIST(vclk_grpgate_list, gate_imem_mc);
	ADD_LIST(vclk_grpgate_list, gate_imem_intmem);
	ADD_LIST(vclk_grpgate_list, gate_imem_intmem_alv);
	ADD_LIST(vclk_grpgate_list, gate_imem_gic400);
	ADD_LIST(vclk_grpgate_list, gate_imem_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_isp0_common);
	ADD_LIST(vclk_grpgate_list, gate_isp0_fimc_isp0);
	ADD_LIST(vclk_grpgate_list, gate_isp0_fimc_tpu);
	ADD_LIST(vclk_grpgate_list, gate_isp0_sysmmu);
	ADD_LIST(vclk_grpgate_list, gate_isp0_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_isp0_bts);
	ADD_LIST(vclk_grpgate_list, gate_isp1_fimc_isp1);
	ADD_LIST(vclk_grpgate_list, gate_isp1_sysmmu);
	ADD_LIST(vclk_grpgate_list, gate_isp1_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_isp1_bts);
	ADD_LIST(vclk_grpgate_list, gate_mfc_common);
	ADD_LIST(vclk_grpgate_list, gate_mfc_mfc);
	ADD_LIST(vclk_grpgate_list, gate_mfc_hpm);
	ADD_LIST(vclk_grpgate_list, gate_mfc_sysmmu);
	ADD_LIST(vclk_grpgate_list, gate_mfc_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_mscl_common);
	ADD_LIST(vclk_grpgate_list, gate_mscl_mscl0_jpeg_common);
	ADD_LIST(vclk_grpgate_list, gate_mscl_mscl0);
	ADD_LIST(vclk_grpgate_list, gate_mscl_jpeg);
	ADD_LIST(vclk_grpgate_list, gate_mscl_mscl1_g2d_common);
	ADD_LIST(vclk_grpgate_list, gate_mscl_mscl1);
	ADD_LIST(vclk_grpgate_list, gate_mscl_g2d);
	ADD_LIST(vclk_grpgate_list, gate_mscl_sysmmu);
	ADD_LIST(vclk_grpgate_list, gate_mscl_ppmu);
	ADD_LIST(vclk_grpgate_list, gate_mscl_bts);
	ADD_LIST(vclk_grpgate_list, gate_peric0_hsi2c0);
	ADD_LIST(vclk_grpgate_list, gate_peric0_hsi2c1);
	ADD_LIST(vclk_grpgate_list, gate_peric0_hsi2c4);
	ADD_LIST(vclk_grpgate_list, gate_peric0_hsi2c5);
	ADD_LIST(vclk_grpgate_list, gate_peric0_hsi2c9);
	ADD_LIST(vclk_grpgate_list, gate_peric0_hsi2c10);
	ADD_LIST(vclk_grpgate_list, gate_peric0_hsi2c11);
	ADD_LIST(vclk_grpgate_list, gate_peric0_uart0);
	ADD_LIST(vclk_grpgate_list, gate_peric0_adcif);
	ADD_LIST(vclk_grpgate_list, gate_peric0_pwm);
	ADD_LIST(vclk_grpgate_list, gate_peric0_sclk_pwm);
	ADD_LIST(vclk_grpgate_list, gate_peric1_hsi2c_common);
	ADD_LIST(vclk_grpgate_list, gate_peric1_hsi2c2);
	ADD_LIST(vclk_grpgate_list, gate_peric1_hsi2c3);
	ADD_LIST(vclk_grpgate_list, gate_peric1_hsi2c6);
	ADD_LIST(vclk_grpgate_list, gate_peric1_hsi2c7);
	ADD_LIST(vclk_grpgate_list, gate_peric1_hsi2c8);
	ADD_LIST(vclk_grpgate_list, gate_peric1_hsi2c12);
	ADD_LIST(vclk_grpgate_list, gate_peric1_hsi2c13);
	ADD_LIST(vclk_grpgate_list, gate_peric1_hsi2c14);
	ADD_LIST(vclk_grpgate_list, gate_peric1_spi_i2s_pcm1_spdif_common);
	ADD_LIST(vclk_grpgate_list, gate_peric1_uart1);
	ADD_LIST(vclk_grpgate_list, gate_peric1_uart2);
	ADD_LIST(vclk_grpgate_list, gate_peric1_uart3);
	ADD_LIST(vclk_grpgate_list, gate_peric1_uart4);
	ADD_LIST(vclk_grpgate_list, gate_peric1_uart5);
	ADD_LIST(vclk_grpgate_list, gate_peric1_spi0);
	ADD_LIST(vclk_grpgate_list, gate_peric1_spi1);
	ADD_LIST(vclk_grpgate_list, gate_peric1_spi2);
	ADD_LIST(vclk_grpgate_list, gate_peric1_spi3);
	ADD_LIST(vclk_grpgate_list, gate_peric1_spi4);
	ADD_LIST(vclk_grpgate_list, gate_peric1_spi5);
	ADD_LIST(vclk_grpgate_list, gate_peric1_spi6);
	ADD_LIST(vclk_grpgate_list, gate_peric1_spi7);
	ADD_LIST(vclk_grpgate_list, gate_peric1_i2s1);
	ADD_LIST(vclk_grpgate_list, gate_peric1_pcm1);
	ADD_LIST(vclk_grpgate_list, gate_peric1_spdif);
	ADD_LIST(vclk_grpgate_list, gate_peric1_gpio_nfc);
	ADD_LIST(vclk_grpgate_list, gate_peric1_gpio_touch);
	ADD_LIST(vclk_grpgate_list, gate_peric1_gpio_fp);
	ADD_LIST(vclk_grpgate_list, gate_peric1_gpio_ese);
	ADD_LIST(vclk_grpgate_list, gate_peris_sfr_apbif_hdmi_cec);
	ADD_LIST(vclk_grpgate_list, gate_peris_hpm);
	ADD_LIST(vclk_grpgate_list, gate_peris_mct);
	ADD_LIST(vclk_grpgate_list, gate_peris_wdt_mngs);
	ADD_LIST(vclk_grpgate_list, gate_peris_wdt_apollo);
	ADD_LIST(vclk_grpgate_list, gate_peris_sysreg_peris);
	ADD_LIST(vclk_grpgate_list, gate_peris_monocnt_apbif);
	ADD_LIST(vclk_grpgate_list, gate_peris_rtc_apbif);
	ADD_LIST(vclk_grpgate_list, gate_peris_top_rtc);
	ADD_LIST(vclk_grpgate_list, gate_peris_otp_con_top);
	ADD_LIST(vclk_grpgate_list, gate_peris_chipid);
	ADD_LIST(vclk_grpgate_list, gate_peris_tmu);

	ADD_LIST(vclk_m1d1g1_list, sclk_decon0_eclk0);
	ADD_LIST(vclk_m1d1g1_list, sclk_decon0_vclk0);
	ADD_LIST(vclk_m1d1g1_list, sclk_decon0_vclk1);
	ADD_LIST(vclk_m1d1g1_list, sclk_hdmi_audio);
	ADD_LIST(vclk_m1d1g1_list, sclk_decon1_eclk0);
	ADD_LIST(vclk_m1d1g1_list, sclk_decon1_eclk1);
	ADD_LIST(vclk_m1d1g1_list, sclk_usbdrd30);
	ADD_LIST(vclk_m1d1g1_list, sclk_mmc0);
	ADD_LIST(vclk_m1d1g1_list, sclk_ufsunipro20);
	ADD_LIST(vclk_m1d1g1_list, sclk_phy24m);
	ADD_LIST(vclk_m1d1g1_list, sclk_ufsunipro_cfg);
	ADD_LIST(vclk_m1d1g1_list, sclk_mmc2);
	ADD_LIST(vclk_m1d1g1_list, sclk_ufsunipro20_sdcard);
	ADD_LIST(vclk_m1d1g1_list, sclk_pcie_phy);
	ADD_LIST(vclk_m1d1g1_list, sclk_ufsunipro_sdcard_cfg);
	ADD_LIST(vclk_m1d1g1_list, sclk_uart0);
	ADD_LIST(vclk_m1d1g1_list, sclk_spi0);
	ADD_LIST(vclk_m1d1g1_list, sclk_spi1);
	ADD_LIST(vclk_m1d1g1_list, sclk_spi2);
	ADD_LIST(vclk_m1d1g1_list, sclk_spi3);
	ADD_LIST(vclk_m1d1g1_list, sclk_spi4);
	ADD_LIST(vclk_m1d1g1_list, sclk_spi5);
	ADD_LIST(vclk_m1d1g1_list, sclk_spi6);
	ADD_LIST(vclk_m1d1g1_list, sclk_spi7);
	ADD_LIST(vclk_m1d1g1_list, sclk_uart1);
	ADD_LIST(vclk_m1d1g1_list, sclk_uart2);
	ADD_LIST(vclk_m1d1g1_list, sclk_uart3);
	ADD_LIST(vclk_m1d1g1_list, sclk_uart4);
	ADD_LIST(vclk_m1d1g1_list, sclk_uart5);
	ADD_LIST(vclk_m1d1g1_list, sclk_promise_int);
	ADD_LIST(vclk_m1d1g1_list, sclk_promise_disp);
	ADD_LIST(vclk_m1d1g1_list, sclk_ap2cp_mif_pll_out);
	ADD_LIST(vclk_m1d1g1_list, sclk_isp_spi0);
	ADD_LIST(vclk_m1d1g1_list, sclk_isp_spi1);
	ADD_LIST(vclk_m1d1g1_list, sclk_isp_uart);
	ADD_LIST(vclk_m1d1g1_list, sclk_isp_sensor0);
	ADD_LIST(vclk_m1d1g1_list, sclk_isp_sensor1);
	ADD_LIST(vclk_m1d1g1_list, sclk_isp_sensor2);
	ADD_LIST(vclk_m1d1g1_list, sclk_isp_sensor3);
	ADD_LIST(vclk_m1d1g1_list, sclk_decon0_eclk0_local);
	ADD_LIST(vclk_m1d1g1_list, sclk_decon0_vclk0_local);
	ADD_LIST(vclk_m1d1g1_list, sclk_decon0_vclk1_local);
	ADD_LIST(vclk_m1d1g1_list, sclk_decon1_eclk0_local);
	ADD_LIST(vclk_m1d1g1_list, sclk_decon1_eclk1_local);

	ADD_LIST(vclk_p1_list, p1_disp_pll);
	ADD_LIST(vclk_p1_list, p1_aud_pll);
	ADD_LIST(vclk_p1_list, p1_mfc_pll);
	ADD_LIST(vclk_p1_list, p1_bus3_pll);

	ADD_LIST(vclk_d1_list, d1_sclk_i2s_local);
	ADD_LIST(vclk_d1_list, d1_sclk_pcm_local);
	ADD_LIST(vclk_d1_list, d1_sclk_slimbus);
	ADD_LIST(vclk_d1_list, d1_sclk_cp_i2s);
	ADD_LIST(vclk_d1_list, d1_sclk_asrc);

	ADD_LIST(vclk_pxmxdx_list, pxmxdx_top);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_mfc);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_mscl);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_imem);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_fsys0);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_fsys1);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_disp0);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_disp1);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_aud);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_aud_cp);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_isp0_isp0);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_isp0_tpu);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_isp0_trex);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_isp0_pxl_asbs);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_isp1_isp1);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam0_csis0);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam0_csis1);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam0_csis2);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam0_csis3);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam0_3aa0);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam0_3aa1);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam0_trex);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam1_arm);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam1_vra);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam1_trex);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam1_bus);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam1_peri);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam1_csis2);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam1_csis3);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_cam1_scl);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_oscclk_nfc);
	ADD_LIST(vclk_pxmxdx_list, pxmxdx_oscclk_aud);


	ADD_LIST(vclk_umux_list, umux_bus0_aclk_bus0_528);
	ADD_LIST(vclk_umux_list, umux_bus0_aclk_bus0_200);
	ADD_LIST(vclk_umux_list, umux_bus1_aclk_bus1_528);
	ADD_LIST(vclk_umux_list, umux_cam0_phyclk_rxbyteclkhs0_csis0_user);
	ADD_LIST(vclk_umux_list, umux_cam0_phyclk_rxbyteclkhs1_csis0_user);
	ADD_LIST(vclk_umux_list, umux_cam0_phyclk_rxbyteclkhs2_csis0_user);
	ADD_LIST(vclk_umux_list, umux_cam0_phyclk_rxbyteclkhs3_csis0_user);
	ADD_LIST(vclk_umux_list, umux_cam0_phyclk_rxbyteclkhs0_csis1_user);
	ADD_LIST(vclk_umux_list, umux_cam0_phyclk_rxbyteclkhs1_csis1_user);
	ADD_LIST(vclk_umux_list, umux_cam1_phyclk_rxbyteclkhs0_csis2_user);
	ADD_LIST(vclk_umux_list, umux_cam1_phyclk_rxbyteclkhs1_csis2_user);
	ADD_LIST(vclk_umux_list, umux_cam1_phyclk_rxbyteclkhs2_csis2_user);
	ADD_LIST(vclk_umux_list, umux_cam1_phyclk_rxbyteclkhs3_csis2_user);
	ADD_LIST(vclk_umux_list, umux_cam1_phyclk_rxbyteclkhs0_csis3_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_hdmiphy_pixel_clko_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_hdmiphy_tmds_clko_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_mipidphy0_rxclkesc0_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_mipidphy0_bitclkdiv2_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_mipidphy0_bitclkdiv8_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_mipidphy1_rxclkesc0_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_mipidphy1_bitclkdiv2_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_mipidphy1_bitclkdiv8_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_mipidphy2_rxclkesc0_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_mipidphy2_bitclkdiv2_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_mipidphy2_bitclkdiv8_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_dpphy_ch0_txd_clk_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_dpphy_ch1_txd_clk_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_dpphy_ch2_txd_clk_user);
	ADD_LIST(vclk_umux_list, umux_disp0_phyclk_dpphy_ch3_txd_clk_user);
	ADD_LIST(vclk_umux_list, umux_disp1_phyclk_mipidphy0_bitclkdiv2_user);
	ADD_LIST(vclk_umux_list, umux_disp1_phyclk_mipidphy1_bitclkdiv2_user);
	ADD_LIST(vclk_umux_list, umux_disp1_phyclk_mipidphy2_bitclkdiv2_user);
	ADD_LIST(vclk_umux_list, umux_disp1_phyclk_disp1_hdmiphy_pixel_clko_user);
	ADD_LIST(vclk_umux_list, umux_fsys0_phyclk_usbdrd30_udrd30_phyclock_user);
	ADD_LIST(vclk_umux_list, umux_fsys0_phyclk_usbdrd30_udrd30_pipe_pclk_user);
	ADD_LIST(vclk_umux_list, umux_fsys0_phyclk_ufs_tx0_symbol_user);
	ADD_LIST(vclk_umux_list, umux_fsys0_phyclk_ufs_rx0_symbol_user);
	ADD_LIST(vclk_umux_list, umux_fsys0_phyclk_usbhost20_phyclock_user);
	ADD_LIST(vclk_umux_list, umux_fsys0_phyclk_usbhost20_freeclk_user);
	ADD_LIST(vclk_umux_list, umux_fsys0_phyclk_usbhost20_clk48mohci_user);
	ADD_LIST(vclk_umux_list, umux_fsys0_phyclk_usbhost20phy_ref_clk);
	ADD_LIST(vclk_umux_list, umux_fsys0_phyclk_ufs_rx_pwm_clk_user);
	ADD_LIST(vclk_umux_list, umux_fsys0_phyclk_ufs_tx_pwm_clk_user);
	ADD_LIST(vclk_umux_list, umux_fsys0_phyclk_ufs_refclk_out_soc_user);
	ADD_LIST(vclk_umux_list, umux_fsys1_phyclk_ufs_link_sdcard_tx0_symbol_user);
	ADD_LIST(vclk_umux_list, umux_fsys1_phyclk_ufs_link_sdcard_rx0_symbol_user);
	ADD_LIST(vclk_umux_list, umux_fsys1_phyclk_pcie_wifi0_tx0_user);
	ADD_LIST(vclk_umux_list, umux_fsys1_phyclk_pcie_wifi0_rx0_user);
	ADD_LIST(vclk_umux_list, umux_fsys1_phyclk_pcie_wifi1_tx0_user);
	ADD_LIST(vclk_umux_list, umux_fsys1_phyclk_pcie_wifi1_rx0_user);
	ADD_LIST(vclk_umux_list, umux_fsys1_phyclk_pcie_wifi0_dig_refclk_user);
	ADD_LIST(vclk_umux_list, umux_fsys1_phyclk_pcie_wifi1_dig_refclk_user);
	ADD_LIST(vclk_umux_list, umux_fsys1_phyclk_ufs_link_sdcard_rx_pwm_clk_user);
	ADD_LIST(vclk_umux_list, umux_fsys1_phyclk_ufs_link_sdcard_tx_pwm_clk_user);
	ADD_LIST(vclk_umux_list, umux_fsys1_phyclk_ufs_link_sdcard_refclk_out_soc_user);

	ADD_LIST(vclk_dfs_list, dvfs_big);
	ADD_LIST(vclk_dfs_list, dvfs_little);
	ADD_LIST(vclk_dfs_list, dvfs_g3d);
	ADD_LIST(vclk_dfs_list, dvfs_mif);
	ADD_LIST(vclk_dfs_list, dvfs_int);
	ADD_LIST(vclk_dfs_list, dvfs_cam);
	ADD_LIST(vclk_dfs_list, dvfs_disp);
	ADD_LIST(vclk_dfs_list, dvs_g3dm);

	/* This code is for reference count sync. Initial state of BUS3_PLL is enabled. */
	vclk_enable(VCLK(p1_bus3_pll));
	vclk_enable(VCLK(p1_bus3_pll));
}
