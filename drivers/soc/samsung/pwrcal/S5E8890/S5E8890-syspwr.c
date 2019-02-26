#include "../pwrcal-env.h"
#include "../pwrcal.h"
#include "../pwrcal-pmu.h"
#include "../pwrcal-rae.h"
#include "S5E8890-cmusfr.h"
#include "S5E8890-pmusfr.h"
#include "S5E8890-cmu.h"

struct cmu_backup {
	void *sfr;
	unsigned int mask;
	void *power;
	unsigned int backup;
	int backup_valid;
};

enum sys_powerdown {
	syspwr_sicd,
	syspwr_sicd_cpd,
	syspwr_sicd_aud,
	syspwr_aftr,
	syspwr_stop,
	syspwr_dstop,
	syspwr_lpd,
	syspwr_alpa,
	syspwr_sleep,
	num_of_syspwr,
};

#ifdef PWRCAL_TARGET_LINUX
extern int is_cp_aud_enabled(void);
#else
static inline int is_cp_aud_enabled(void)
{
	return 0;
}
#endif
extern void enable_cppll_sharing_bus012_disable(void);
extern void disable_cppll_sharing_bus012_enable(void);
static unsigned int mif_use_cp_pll = 0;
extern unsigned int dfsmif_paraset;

/* set_pmu_lpi_mask */
#define ASATB_MSTIF_MNGS_CAM1		(0x1 << 16)
#define ASATB_MSTIF_MNGS_AUD			(0x1 << 15)

static void set_pmu_lpi_mask(void)
{
	unsigned int tmp;

	tmp = pwrcal_readl(LPI_MASK_MNGS_ASB);
	tmp |= (ASATB_MSTIF_MNGS_AUD | ASATB_MSTIF_MNGS_CAM1);
	pwrcal_writel(LPI_MASK_MNGS_ASB, tmp);

}

#define EMULATION	(0x1 << 31)
#define ENABLE_DBGNOPWRDWN	(0x1 << 30)
#define USE_SMPEN	(0x1 << 28)
#define DBGPWRDWNREQ_EMULATION	(0x1 << 27)
#define RESET_EMULATION	(0x1 << 26)
#define CLAMP_EMULATION	(0x1 << 25)
#define USE_STANDBYWFE	(0x1 << 24)
#define IGNORE_OUTPUT_UPDATE_DONE	(0x1 << 20)
#define REQ_LAST_CPU	(0x1 << 17)
#define USE_STANDBYWFI	(0x1 << 16)
#define SKIP_DBGPWRDWN	(0x1 << 15)
#define USE_DELAYED_RESET_ASSERTION	(0x1 << 12)
#define ENABLE_GPR_CPUPWRUP	(0x1 << 9)
#define ENABLE_AUTO_WAKEUP_INT_REQ	(0x1 << 8)
#define USE_IRQCPU_FOR_PWRUP	(0x1 << 5)
#define USE_IRQCPU_FOR_PWRDOWN	(0x1 << 4)
#define USE_MEMPWR_FEEDBACK	(0x1 << 3)
#define USE_MEMPWR_COUNTER	(0x1 << 2)
#define USE_SC_FEEDBACK	(0x1 << 1)
#define USE_SC_COUNTER	(0x1 << 0)

/* init_pmu_l2_option*/
#define USE_AUTOMATIC_L2FLUSHREQ	(0x1 << 17)
#define USE_STANDBYWFIL2		(0x1 << 16)
#define USE_RETENTION			(0x1 << 4)

static void init_pmu_l2_option(void)
{
	unsigned int tmp;

	tmp = pwrcal_readl(MNGS_NONCPU_OPTION);
	tmp &= ~USE_SC_FEEDBACK;
	tmp |= USE_SC_COUNTER;
	pwrcal_writel(MNGS_NONCPU_OPTION, tmp);

	tmp = pwrcal_readl(APOLLO_NONCPU_OPTION);
	tmp &= ~USE_SC_FEEDBACK;
	tmp |= USE_SC_COUNTER;
	pwrcal_writel(APOLLO_NONCPU_OPTION, tmp);

	pwrcal_setf(MNGS_NONCPU_DURATION0, 0x8, 0xf, 0x3);
	pwrcal_setf(MNGS_NONCPU_DURATION0, 0x4, 0xf, 0x3);
	pwrcal_setf(APOLLO_NONCPU_DURATION0, 0x8, 0xf, 0x3);
	pwrcal_setf(APOLLO_NONCPU_DURATION0, 0x4, 0xf, 0x3);

	/* disable automatic L2 flush */
	/* disable L2 retention */
	/* eanble STANDBYWFIL2 MNGS only */

	tmp = pwrcal_readl(MNGS_L2_OPTION);
	tmp &= ~(USE_AUTOMATIC_L2FLUSHREQ | USE_RETENTION);
	tmp |= USE_STANDBYWFIL2;
	pwrcal_writel(MNGS_L2_OPTION, tmp);

	tmp = pwrcal_readl(APOLLO_L2_OPTION);
	tmp &= ~(USE_AUTOMATIC_L2FLUSHREQ | USE_RETENTION);
	tmp |= USE_STANDBYWFIL2;
	pwrcal_writel(APOLLO_L2_OPTION, tmp);
}

static unsigned int *pmu_cpuoption_sfrlist[] = {
	MNGS_CPU0_OPTION,
	MNGS_CPU1_OPTION,
	MNGS_CPU2_OPTION,
	MNGS_CPU3_OPTION,
	APOLLO_CPU0_OPTION,
	APOLLO_CPU1_OPTION,
	APOLLO_CPU2_OPTION,
	APOLLO_CPU3_OPTION,
};
static unsigned int *pmu_cpuduration_sfrlist[] = {
	MNGS_CPU0_DURATION0,
	MNGS_CPU1_DURATION0,
	MNGS_CPU2_DURATION0,
	MNGS_CPU3_DURATION0,
	APOLLO_CPU0_DURATION0,
	APOLLO_CPU1_DURATION0,
	APOLLO_CPU2_DURATION0,
	APOLLO_CPU3_DURATION0,
};

#define DUR_WAIT_RESET	(0xF << 20)
#define DUR_CHG_RESET	(0xF << 16)
#define DUR_MEMPWR	(0xF << 12)
#define DUR_SCPRE	(0xF << 8)
#define DUR_SCALL	(0xF << 4)
#define DUR_SCPRE_WAIT	(0xF << 0)
#define DUR_SCALL_VALUE (1 << 4)

#define SMC_PwrMgmtMode_CH0		((void *)(SMC0_BASE + 0x0238))
#define SMC_PwrMgmtMode_CH1		((void *)(SMC1_BASE + 0x0238))
#define SMC_PwrMgmtMode_CH2		((void *)(SMC2_BASE + 0x0238))
#define SMC_PwrMgmtMode_CH3		((void *)(SMC3_BASE + 0x0238))

int phycgen_ch0;
int phycgen_ch1;
int phycgen_ch2;
int phycgen_ch3;

static void smc_set_phycgen(int enable)
{
	if (enable == 1) {
		pwrcal_setbit(SMC_PwrMgmtMode_CH0, 6, phycgen_ch0);
		pwrcal_setbit(SMC_PwrMgmtMode_CH1, 6, phycgen_ch1);
		pwrcal_setbit(SMC_PwrMgmtMode_CH2, 6, phycgen_ch2);
		pwrcal_setbit(SMC_PwrMgmtMode_CH3, 6, phycgen_ch3);
	} else {
		pwrcal_setbit(SMC_PwrMgmtMode_CH0, 6, 1);
		pwrcal_setbit(SMC_PwrMgmtMode_CH1, 6, 1);
		pwrcal_setbit(SMC_PwrMgmtMode_CH2, 6, 1);
		pwrcal_setbit(SMC_PwrMgmtMode_CH3, 6, 1);
	}
}
static void smc_get_init_phycgen(void)
{
	phycgen_ch0 = pwrcal_getbit(SMC_PwrMgmtMode_CH0, 6);
	phycgen_ch1 = pwrcal_getbit(SMC_PwrMgmtMode_CH1, 6);
	phycgen_ch2 = pwrcal_getbit(SMC_PwrMgmtMode_CH2, 6);
	phycgen_ch3 = pwrcal_getbit(SMC_PwrMgmtMode_CH3, 6);
}


static void init_pmu_cpu_option(void)
{
	int cpu;
	unsigned int tmp;

	/* 8890 uses Point of no return version PMU */

	/* use both sc_counter and sc_feedback (for veloce counter only) */
	/* enable to wait for low SMP-bit at sys power down */
	for (cpu = 0; cpu < sizeof(pmu_cpuoption_sfrlist) / sizeof(pmu_cpuoption_sfrlist[0]); cpu++) {
		tmp = pwrcal_readl(pmu_cpuoption_sfrlist[cpu]);
		tmp &= ~EMULATION;
		tmp &= ~ENABLE_DBGNOPWRDWN;
		tmp |= USE_SMPEN;
		tmp &= ~DBGPWRDWNREQ_EMULATION;

		tmp &= ~RESET_EMULATION;
		tmp &= ~CLAMP_EMULATION;
		tmp &= ~USE_STANDBYWFE;
		tmp &= ~IGNORE_OUTPUT_UPDATE_DONE;
		tmp &= ~REQ_LAST_CPU;

		tmp |= USE_STANDBYWFI;

		tmp &= ~SKIP_DBGPWRDWN;
		tmp &= ~USE_DELAYED_RESET_ASSERTION;
		tmp &= ~ENABLE_GPR_CPUPWRUP;

		tmp |= ENABLE_AUTO_WAKEUP_INT_REQ;

		tmp &= ~USE_IRQCPU_FOR_PWRUP;
		tmp &= ~USE_IRQCPU_FOR_PWRDOWN;

		tmp |= USE_MEMPWR_FEEDBACK;
		tmp &= ~USE_MEMPWR_COUNTER;

		tmp &= ~USE_SC_FEEDBACK;
		tmp |= USE_SC_COUNTER;

		pwrcal_writel(pmu_cpuoption_sfrlist[cpu], tmp);

		pwrcal_setf(pmu_cpuduration_sfrlist[cpu], 0x8, 0xf, 0x3);
		pwrcal_setf(pmu_cpuduration_sfrlist[cpu], 0x4, 0xf, 0x3);
	}
}

static void init_pmu_cpuseq_option(void)
{

}

#define ENABLE_MNGS_CPU		(0x1 << 0)
#define ENABLE_APOLLO_CPU	(0x1 << 1)

static void init_pmu_up_scheduler(void)
{
	unsigned int tmp;

	/* limit in-rush current for MNGS local power up */
	tmp = pwrcal_readl(UP_SCHEDULER);
	tmp |= ENABLE_MNGS_CPU;
	pwrcal_writel(UP_SCHEDULER, tmp);
	/* limit in-rush current for APOLLO local power up */
	tmp = pwrcal_readl(UP_SCHEDULER);
	tmp |= ENABLE_APOLLO_CPU;
	pwrcal_writel(UP_SCHEDULER, tmp);
}

static unsigned int *pmu_feedback_sfrlist[] = {
	TOP_PWR_OPTION,
	TOP_PWR_MIF_OPTION,
	PWR_DDRPHY_OPTION,
	CAM0_OPTION,
	MSCL_OPTION,
	G3D_OPTION,
	DISP0_OPTION,
	CAM1_OPTION,
	AUD_OPTION,
	FSYS0_OPTION,
	BUS0_OPTION,
	ISP0_OPTION,
	ISP1_OPTION,
	MFC_OPTION,
	DISP1_OPTION,
	FSYS1_OPTION,
};


static void init_pmu_feedback(void)
{
	int i;
	unsigned int tmp;

	for (i = 0; i < sizeof(pmu_feedback_sfrlist) / sizeof(pmu_feedback_sfrlist[0]); i++) {
		tmp = pwrcal_readl(pmu_feedback_sfrlist[i]);
		tmp &= ~USE_SC_COUNTER;
		tmp |= USE_SC_FEEDBACK;
		pwrcal_writel(pmu_feedback_sfrlist[i], tmp);
	}
}

#define XXTI_DUR_STABLE				0x658 /* 1ms @ 26MHz */
#define TCXO_DUR_STABLE				0x658 /* 1ms @ 26MHz */
#define EXT_REGULATOR_DUR_STABLE	0xFDE /* 2.5ms @ 26MHz */
#define EXT_REGULATOR_MIF_DUR_STABLE	0xCB1 /* 2ms @ 26MHz */

static void init_pmu_stable_counter(void)
{
	pwrcal_writel(XXTI_DURATION3, XXTI_DUR_STABLE);
	pwrcal_writel(TCXO_DURATION3, TCXO_DUR_STABLE);
	pwrcal_writel(EXT_REGULATOR_DURATION3, EXT_REGULATOR_DUR_STABLE);
	pwrcal_writel(EXT_REGULATOR_MIF_DURATION3, EXT_REGULATOR_MIF_DUR_STABLE);
}

#define ENABLE_HW_TRIP                 (0x1 << 31)
#define PS_HOLD_OUTPUT_HIGH            (0x3 << 8)
static void init_ps_hold_setting(void)
{
	unsigned int tmp;

	tmp = pwrcal_readl(PS_HOLD_CONTROL);
	tmp |= (ENABLE_HW_TRIP | PS_HOLD_OUTPUT_HIGH);
	pwrcal_writel(PS_HOLD_CONTROL, tmp);
}

static void enable_armidleclockdown(void)
{
	pwrcal_setbit(PWR_CTRL3_MNGS, 0, 1); //PWR_CTRL3[0] USE_L2QACTIVE = 1
	pwrcal_setbit(PWR_CTRL3_MNGS, 1, 1); // PWR_CTRL3[1] IGNORE_L2QREQUEST = 1
	pwrcal_setf(PWR_CTRL3_MNGS, 16, 0x7, 0x0); // PWR_CTRL3[18:16] L2QDELAY = 3'b0 (? timer ticks)
	pwrcal_setbit(PWR_CTRL3_APOLLO, 0, 1); // PWR_CTRL3[0] USE_L2QACTIVE = 1
	pwrcal_setbit(PWR_CTRL3_APOLLO, 1, 1); // PWR_CTRL3[0] IGNORE_L2QREQUEST = 1
}

static void disable_armidleclockdown(void)
{
	pwrcal_setbit(PWR_CTRL3_MNGS, 0, 0); //PWR_CTRL3[0] USE_L2QACTIVE = 0
	pwrcal_setbit(PWR_CTRL3_APOLLO, 0, 0); // PWR_CTRL3[0] USE_L2QACTIVE = 0
}

#define PAD_AUTOMATIC_WAKEUP		(0x1 << 29)
static void automatic_wakeup_init(void)
{
	pwrcal_writel(PAD_RETENTION_MIF_OPTION, PAD_AUTOMATIC_WAKEUP);
}

static void pwrcal_syspwr_init(void)
{
	init_pmu_feedback();
	init_pmu_l2_option();
	init_pmu_cpu_option();
	init_pmu_cpuseq_option();
	init_pmu_up_scheduler();
	set_pmu_lpi_mask();
	init_pmu_stable_counter();
	init_ps_hold_setting();
	enable_armidleclockdown();
	smc_get_init_phycgen();
	automatic_wakeup_init();
}


struct exynos_pmu_conf {
	void *reg;
	unsigned int val[num_of_syspwr];
};

static struct exynos_pmu_conf exynos8890_pmu_config[] = {
	/* { .addr = address, .val =					{       SICD, SICD_CPD,	SICD_AUD,AFTR,   STOP,  DSTOP,    LPD,   ALPA, SLEEP } } */
	{	MNGS_CPU0_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0x0 ,	0x8 ,	0x0 ,	0x0 ,	0x0 ,	0x8	} },
	{	DIS_IRQ_MNGS_CPU0_LOCAL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_MNGS_CPU0_CENTRAL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_MNGS_CPU0_CPUSEQUENCER_SYS_PWR_REG,		{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MNGS_CPU1_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0x0 ,	0x8 ,	0x0 ,	0x0 ,	0x0 ,	0x8	} },
	{	DIS_IRQ_MNGS_CPU1_LOCAL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_MNGS_CPU1_CENTRAL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_MNGS_CPU1_CPUSEQUENCER_SYS_PWR_REG,		{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MNGS_CPU2_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0x0 ,	0x8 ,	0x0 ,	0x0 ,	0x0 ,	0x8	} },
	{	DIS_IRQ_MNGS_CPU2_LOCAL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_MNGS_CPU2_CENTRAL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_MNGS_CPU2_CPUSEQUENCER_SYS_PWR_REG,		{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MNGS_CPU3_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0x0 ,	0x8 ,	0x0 ,	0x0 ,	0x0 ,	0x8	} },
	{	DIS_IRQ_MNGS_CPU3_LOCAL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_MNGS_CPU3_CENTRAL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_MNGS_CPU3_CPUSEQUENCER_SYS_PWR_REG,		{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	APOLLO_CPU0_SYS_PWR_REG,				{	0xF ,   0xF ,	  0xF ,	0x0 ,	0x8 ,	0x0 ,	0x0 ,	0x0 ,	0x8	} },
	{	DIS_IRQ_APOLLO_CPU0_LOCAL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_APOLLO_CPU0_CENTRAL_SYS_PWR_REG,		{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_APOLLO_CPU0_CPUSEQUENCER_SYS_PWR_REG,		{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	APOLLO_CPU1_SYS_PWR_REG,				{	0xF ,   0xF ,	  0xF ,	0x0 ,	0x8 ,	0x0 ,	0x0 ,	0x0 ,	0x8	} },
	{	DIS_IRQ_APOLLO_CPU1_LOCAL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_APOLLO_CPU1_CENTRAL_SYS_PWR_REG,		{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_APOLLO_CPU1_CPUSEQUENCER_SYS_PWR_REG,		{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	APOLLO_CPU2_SYS_PWR_REG,				{	0xF ,   0xF ,	  0xF ,	0x0 ,	0x8 ,	0x0 ,	0x0 ,	0x0 ,	0x8	} },
	{	DIS_IRQ_APOLLO_CPU2_LOCAL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_APOLLO_CPU2_CENTRAL_SYS_PWR_REG,		{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_APOLLO_CPU2_CPUSEQUENCER_SYS_PWR_REG,		{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	APOLLO_CPU3_SYS_PWR_REG,				{	0xF ,   0xF ,	  0xF ,	0x0 ,	0x8 ,	0x0 ,	0x0 ,	0x0 ,	0x8	} },
	{	DIS_IRQ_APOLLO_CPU3_LOCAL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_APOLLO_CPU3_CENTRAL_SYS_PWR_REG,		{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_APOLLO_CPU3_CPUSEQUENCER_SYS_PWR_REG,		{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MNGS_NONCPU_SYS_PWR_REG,				{	0xF ,   0x0 ,	  0xF ,	0x0 ,	0xF ,	0x0 ,	0x0 ,	0x0 ,	0x8	} },
	{	APOLLO_NONCPU_SYS_PWR_REG,				{	0xF ,   0xF ,	  0xF ,	0x0 ,	0xF ,	0x0 ,	0x0 ,	0x0 ,	0x8	} },
	{	MNGS_DBG_SYS_PWR_REG,					{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x0 ,	0x0 ,	0x2	} },
	{	CORTEXM3_APM_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	A7IS_SYS_PWR_REG,					{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_A7IS_LOCAL_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DIS_IRQ_A7IS_CENTRAL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MNGS_L2_SYS_PWR_REG,					{	0x7 ,   0x7 ,	  0x7 ,	0x0 ,	0x7 ,	0x0 ,	0x0 ,	0x0 ,	0x7	} },
	{	APOLLO_L2_SYS_PWR_REG,					{	0x7 ,   0x7 ,	  0x7 ,	0x0 ,	0x7 ,	0x0 ,	0x0 ,	0x0 ,	0x7	} },
	{	MNGS_L2_PWR_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_TOP_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_TOP_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RETENTION_CMU_TOP_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x0 ,	0x0 ,	0x3	} },
	{	RESET_CMU_TOP_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x0	} },
	{	RESET_CPUCLKSTOP_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x0	} },
	{	CLKSTOP_CMU_MIF_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x1 ,	0x0 ,	0x1	} },
	{	CLKRUN_CMU_MIF_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x1 ,	0x0 ,	0x1	} },
	{	RETENTION_CMU_MIF_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x3 ,	0x0 ,	0x3	} },
	{	RESET_CMU_MIF_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x0	} },
	{	DDRPHY_CLKSTOP_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x1 ,	0x0 ,	0x0	} },
	{	DDRPHY_ISO_SYS_PWR_REG,					{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x1 ,	0x0 ,	0x1	} },
	{	DDRPHY_SOC2_ISO_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x1	} },
	{	DDRPHY_DLL_CLK_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x1 ,	0x0 ,	0x0 ,	0x1 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_TOP_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_AUD_PLL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x1 ,	0x0	} },
	{	DISABLE_PLL_CMU_MIF_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x1 ,	0x0 ,	0x0 ,	0x1 ,	0x0 ,	0x0	} },
	{	RESET_AHEAD_CP_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3	} },
	{	TOP_BUS_SYS_PWR_REG,					{	0x7 ,   0x7 ,	  0x7 ,	0x7 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	TOP_RETENTION_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x0 ,	0x0 ,	0x3	} },
	{	TOP_PWR_SYS_PWR_REG,					{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x0 ,	0x0 ,	0x3	} },
	{	CMU_PWR_SYS_PWR_REG,					{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x3 ,	0x0 ,	0x3 } },
	{	TOP_BUS_MIF_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x7 ,	0x0 ,	0x0 ,	0x7 ,	0x0 ,	0x0	} },
	{	TOP_RETENTION_MIF_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x3 ,	0x0 ,	0x3	} },
	{	TOP_PWR_MIF_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x3 ,	0x0 ,	0x3	} },
	{	PSCDC_MIF_SYS_PWR_REG,					{	0x0 ,   0x0 ,	  0x0 ,	0x3 ,	0x3 ,	0x0 ,	0x3 ,	0x0 ,	0x0	} },
	{	LOGIC_RESET_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	OSCCLK_GATE_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	SLEEP_RESET_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x0	} },
	{	LOGIC_RESET_MIF_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x3 ,	0x0 ,	0x0	} },
	{	OSCCLK_GATE_MIF_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x1 ,	0x0 ,	0x1	} },
	{	SLEEP_RESET_MIF_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x0	} },
	{	MEMORY_TOP_SYS_PWR_REG,					{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLEANY_BUS_SYS_PWR_REG,					{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x1	} },
	{	LOGIC_RESET_CP_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3	} },
	{	TCXO_GATE_SYS_PWR_REG,					{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x1	} },
	{	RESET_ASB_CP_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3	} },
	{	RESET_ASB_MIF_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x3 ,	0x0 ,	0x0	} },
	{	MEMORY_IMEM_ALIVEIRAM_SYS_PWR_REG,			{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MEMORY_MIF_TOP_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x3 ,	0x0 ,	0x3	} },
	{	LOGIC_RESET_DDRPHY_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x3 ,	0x0 ,	0x0	} },
	{	RETENTION_DDRPHY_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x3 ,	0x0 ,	0x3	} },
	{	PWR_DDRPHY_SYS_PWR_REG,					{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x0 ,	0x3 ,	0x0 ,	0x3	} },
	{	PAD_RETENTION_LPDDR4_SYS_PWR_REG,			{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x1 ,	0x0 ,	0x0	} },
	{	PAD_RETENTION_AUD_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_RETENTION_JTAG_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x1 ,	0x0	} },
	{	PAD_RETENTION_PCIE_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_RETENTION_UFS_CARD_SYS_PWR_REG,			{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_RETENTION_MMC2_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_RETENTION_TOP_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_RETENTION_UART_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_RETENTION_MMC0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_RETENTION_EBIA_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_RETENTION_EBIB_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_RETENTION_SPI_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_RETENTION_MIF_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x1 ,	0x0 ,	0x0	} },
	{	PAD_ISOLATION_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x1	} },
	{	PAD_RETENTION_BOOTLDO_0_SYS_PWR_REG,			{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_RETENTION_UFS_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_ISOLATION_MIF_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x1 ,	0x0 ,	0x1	} },
	{	PAD_RETENTION_USB_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_RETENTION_BOOTLDO_1_SYS_PWR_REG,			{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	PAD_ALV_SEL_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	XXTI_SYS_PWR_REG,					{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x0 ,	0x1 ,	0x1 ,	0x1 ,	0x0	} },
	{	TCXO_SYS_PWR_REG,					{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x0 ,	0x1 ,	0x1 ,	0x1 ,	0x0	} },
	{	EXT_REGULATOR_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x0	} },
	{	EXT_REGULATOR_MIF_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x1 ,	0x0	} },
	{	GPIO_MODE_SYS_PWR_REG,					{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	GPIO_MODE_FSYS0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	GPIO_MODE_FSYS1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	GPIO_MODE_MIF_SYS_PWR_REG,				{	0x1 ,   0x1 ,	  0x1 ,	0x1 ,	0x1 ,	0x0 ,	0x1 ,	0x0 ,	0x0	} },
	{	GPIO_MODE_AUD_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_TOP_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_MIF_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_CAM0_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_MSCL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_G3D_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_DISP0_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_CAM1_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_AUD_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_FSYS0_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_BUS0_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_ISP0_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_ISP1_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_MFC_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_DISP1_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_OPEN_CMU_FSYS1_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CAM0_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0xF ,	0xF ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MSCL_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0xF ,	0xF ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	G3D_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0xF ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISP0_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0xF ,	0xF ,	0x0 ,	0xF ,	0x0 ,	0x0	} },
	{	CAM1_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0xF ,	0xF ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	AUD_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0xF ,	0xF ,	0x0 ,	0x0 ,	0xF ,	0x0	} },
	{	FSYS0_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0xF ,	0xF ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	BUS0_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0xF ,	0xF ,	0x0 ,	0xF ,	0x0 ,	0x0	} },
	{	ISP0_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0xF ,	0xF ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	ISP1_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0xF ,	0xF ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MFC_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0xF ,	0xF ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISP1_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0xF ,	0xF ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	FSYS1_SYS_PWR_REG,					{	0xF ,   0xF ,	  0xF ,	0xF ,	0xF ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_CAM0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_MSCL_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_G3D_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_DISP0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_CAM1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_AUD_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_FSYS0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_BUS0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_ISP0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_ISP1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_MFC_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_DISP1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKRUN_CMU_FSYS1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_CAM0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_MSCL_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_G3D_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_DISP0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_CAM1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_AUD_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_FSYS0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_BUS0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_ISP0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_ISP1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_MFC_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_DISP1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	CLKSTOP_CMU_FSYS1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_CAM0_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_MSCL_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_G3D_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_DISP0_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_CAM1_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_AUD_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_FSYS0_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_BUS0_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_ISP0_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_ISP1_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_MFC_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_DISP1_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	DISABLE_PLL_CMU_FSYS1_SYS_PWR_REG,			{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_LOGIC_CAM0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_LOGIC_MSCL_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_LOGIC_G3D_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_LOGIC_DISP0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_LOGIC_CAM1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_LOGIC_AUD_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_LOGIC_FSYS0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_LOGIC_BUS0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_LOGIC_ISP0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_LOGIC_ISP1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_LOGIC_MFC_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_LOGIC_DISP1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_LOGIC_FSYS1_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x0	} },
	{	MEMORY_CAM0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MEMORY_MSCL_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MEMORY_G3D_SYS_PWR_REG,					{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MEMORY_DISP0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MEMORY_CAM1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MEMORY_AUD_SYS_PWR_REG,					{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MEMORY_FSYS0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MEMORY_BUS0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MEMORY_ISP0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MEMORY_ISP1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MEMORY_MFC_SYS_PWR_REG,					{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MEMORY_DISP1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	MEMORY_FSYS1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_CMU_CAM0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_CMU_MSCL_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_CMU_G3D_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_CMU_DISP0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_CMU_CAM1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_CMU_AUD_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_CMU_FSYS0_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x0	} },
	{	RESET_CMU_BUS0_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x0	} },
	{	RESET_CMU_ISP0_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_CMU_ISP1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_CMU_MFC_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_CMU_DISP1_SYS_PWR_REG,				{	0x0 ,   0x0 ,	  0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0 ,	0x0	} },
	{	RESET_CMU_FSYS1_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x0	} },
	{	RESET_SLEEP_FSYS0_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x0	} },
	{	RESET_SLEEP_BUS0_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x0	} },
	{	RESET_SLEEP_FSYS1_SYS_PWR_REG,				{	0x3 ,   0x3 ,	  0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x3 ,	0x0	} },
	{ NULL, },
};
							/*	{	SICD,		SICD_CPD,	SICD_AUD	AFTR,		STOP,		DSTOP,		LPD,		ALPA,		SLEEP } }*/
static struct exynos_pmu_conf exynos8890_pmu_option[] = {
	{PMU_SYNC_CTRL,						{	0x1,		0x1,		0x1,		0x1,		0x1,		0x1,		0x1,		0x1,		0x1} },
	{CENTRAL_SEQ_OPTION,					{	0xFF0000,	0xFF0000,	0xFF0000,	0xFF0000,	0xFF0002,	0xFF0002,	0xFF0002,	0xFF0002,	0xFF0002} },
	{CENTRAL_SEQ_OPTION1,					{	0x10000000,	0x10000000,	0x10000000,	0x10000000,	0x0,		0x0,		0x0,		0x10000000,	0x0} },
	{CENTRAL_SEQ_MIF_OPTION,				{	0x10,		0x10,		0x30,		0x10,		0x10,		0x10,		0x10,		0x30,		0x10} },
	{WAKEUP_MASK_MIF,					{	0x13,		0x13,		0x3,		0x13,		0x13,		0x13,		0x13,		0x3,		0x13} },
	{NULL,},
};


void set_pmu_sys_pwr_reg(enum sys_powerdown mode)
{
	int i;

	for (i = 0; exynos8890_pmu_config[i].reg != NULL; i++)
		pwrcal_writel(exynos8890_pmu_config[i].reg,
				exynos8890_pmu_config[i].val[mode]);

	for (i = 0; exynos8890_pmu_option[i].reg != NULL; i++)
		pwrcal_writel(exynos8890_pmu_option[i].reg,
				exynos8890_pmu_option[i].val[mode]);
}

#define	CENTRALSEQ_PWR_CFG	0x10000
#define	CENTRALSEQ__MIF_PWR_CFG	0x10000

void set_pmu_central_seq(bool enable)
{
	unsigned int tmp;

	/* central sequencer */
	tmp = pwrcal_readl(CENTRAL_SEQ_CONFIGURATION);
	if (enable)
		tmp &= ~CENTRALSEQ_PWR_CFG;
	else
		tmp |= CENTRALSEQ_PWR_CFG;
	pwrcal_writel(CENTRAL_SEQ_CONFIGURATION, tmp);

}

void set_pmu_central_seq_mif(bool enable)
{
	unsigned int tmp;

	/* central sequencer MIF */
	tmp = pwrcal_readl(CENTRAL_SEQ_MIF_CONFIGURATION);
	if (enable)
		tmp &= ~CENTRALSEQ_PWR_CFG;
	else
		tmp |= CENTRALSEQ_PWR_CFG;
	pwrcal_writel(CENTRAL_SEQ_MIF_CONFIGURATION, tmp);
}


static int syspwr_hwacg_control(int mode)
{
	if (mode == syspwr_sicd || mode == syspwr_sicd_cpd || mode == syspwr_sicd_aud)
		return 0;

	/* all QCH_CTRL in CMU_IMEM */
	pwrcal_setbit(QCH_CTRL_AXI_LH_ASYNC_MI_IMEM, 0, 1);
	pwrcal_setbit(QCH_CTRL_SSS, 0, 1);
	pwrcal_setbit(QCH_CTRL_RTIC, 0, 1);
	pwrcal_setbit(QCH_CTRL_INT_MEM, 0, 1);
	pwrcal_setbit(QCH_CTRL_INT_MEM_ALV, 0, 1);
	pwrcal_setbit(QCH_CTRL_MCOMP, 0, 1);
	pwrcal_setbit(QCH_CTRL_CMU_IMEM, 0, 1);
	pwrcal_setbit(QCH_CTRL_PMU_IMEM, 0, 1);
	pwrcal_setbit(QCH_CTRL_SYSREG_IMEM, 0, 1);
	pwrcal_setbit(QCH_CTRL_PPMU_SSSX, 0, 1);
	pwrcal_setbit(QCH_CTRL_LH_ASYNC_SI_IMEM, 0, 1);
	pwrcal_setbit(QCH_CTRL_APM, 0, 1);
	pwrcal_setbit(QCH_CTRL_CM3_APM, 0, 1);

	/* all QCH_CTRL in CMU_PERIS */
	pwrcal_setbit(QCH_CTRL_AXILHASYNCM_PERIS, 0, 1);
	pwrcal_setbit(QCH_CTRL_CMU_PERIS, 0, 1);
	pwrcal_setbit(QCH_CTRL_PMU_PERIS, 0, 1);
	pwrcal_setbit(QCH_CTRL_SYSREG_PERIS, 0, 1);
	pwrcal_setbit(QCH_CTRL_MONOCNT_APBIF, 0, 1);
	/* all QCH_CTRL in CMU_PERIC0 */
	pwrcal_setbit(QCH_CTRL_AXILHASYNCM_PERIC0, 0, 1);
	pwrcal_setbit(QCH_CTRL_CMU_PERIC0, 0, 1);
	pwrcal_setbit(QCH_CTRL_PMU_PERIC0, 0, 1);
	pwrcal_setbit(QCH_CTRL_SYSREG_PERIC0, 0, 1);
	/* all QCH_CTRL in CMU_PERIC1 */
	pwrcal_setbit(QCH_CTRL_AXILHASYNCM_PERIC1, 0, 1);
	pwrcal_setbit(QCH_CTRL_CMU_PERIC1, 0, 1);
	pwrcal_setbit(QCH_CTRL_PMU_PERIC1, 0, 1);
	pwrcal_setbit(QCH_CTRL_SYSREG_PERIC1, 0, 1);
	/* all QCH_CTRL in CMU_FSYS0 */
	if (pwrcal_getf(FSYS0_STATUS, 0, 0xF) == 0xF) {
		pwrcal_setbit(QCH_CTRL_AXI_LH_ASYNC_MI_TOP_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_AXI_LH_ASYNC_MI_ETR_USB_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_ETR_USB_FSYS0_ACLK, 0, 1);
		pwrcal_setbit(QCH_CTRL_ETR_USB_FSYS0_PCLK, 0, 1);
		pwrcal_setbit(QCH_CTRL_CMU_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_PMU_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_SYSREG_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_USBDRD30, 0, 0);
		pwrcal_setbit(QCH_CTRL_MMC0, 0, 1);
		pwrcal_setbit(QCH_CTRL_UFS_LINK_EMBEDDED, 0, 1);
		pwrcal_setbit(QCH_CTRL_USBHOST20, 0, 0);
		pwrcal_setbit(QCH_CTRL_PDMA0, 0, 0);
		pwrcal_setbit(QCH_CTRL_PDMAS, 0, 0);
		pwrcal_setbit(QCH_CTRL_PPMU_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_ACEL_LH_ASYNC_SI_TOP_FSYS0, 0, 1);
		pwrcal_setbit(QCH_CTRL_USBDRD30_PHYCTRL, 0, 0);
		pwrcal_setbit(QCH_CTRL_USBHOST20_PHYCTRL, 0, 0);
	}
	/* all QCH_CTRL in CMU_FSYS1 */
	if (pwrcal_getf(FSYS1_STATUS, 0, 0xF) == 0xF) {
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
	}
	pwrcal_setf(QSTATE_CTRL_ATB_APL_MNGS, 0, 0x3, 0x3);
	pwrcal_setf(QSTATE_CTRL_ASYNCAHBM_SSS_ATLAS, 0, 0x3, 0x1);

	return 0;
}
static int syspwr_hwacg_control_post(int mode)
{
	return 0;
}


static struct cmu_backup cmu_backup_list[] = {
	{BUS0_PLL_LOCK,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS1_PLL_LOCK,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS2_PLL_LOCK,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS3_PLL_LOCK,	0xFFFFFFFF,	NULL,	0,	0},
	{MFC_PLL_LOCK,	0xFFFFFFFF,	NULL,	0,	0},
	{ISP_PLL_LOCK,	0xFFFFFFFF,	NULL,	0,	0},
	{AUD_PLL_LOCK,	0xFFFFFFFF,	NULL,	0,	0},
	{G3D_PLL_LOCK,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS0_PLL_CON0,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS0_PLL_CON1,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS0_PLL_FREQ_DET,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS1_PLL_CON0,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS1_PLL_CON1,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS1_PLL_FREQ_DET,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS2_PLL_CON0,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS2_PLL_CON1,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS2_PLL_FREQ_DET,	0xFFFFFFFF,	NULL,	0,	0},
	{MFC_PLL_CON0,	0xFFFFFFFF,	NULL,	0,	0},
	{MFC_PLL_CON1,	0xFFFFFFFF,	NULL,	0,	0},
	{MFC_PLL_FREQ_DET,	0xFFFFFFFF,	NULL,	0,	0},
	{ISP_PLL_CON0,	0xFFFFFFFF,	NULL,	0,	0},
	{ISP_PLL_CON1,	0xFFFFFFFF,	NULL,	0,	0},
	{ISP_PLL_FREQ_DET,	0xFFFFFFFF,	NULL,	0,	0},
	{AUD_PLL_CON0,	0xFFFFFFFF,	NULL,	0,	0},
	{AUD_PLL_CON1,	0xFFFFFFFF,	NULL,	0,	0},
	{AUD_PLL_CON2,	0xFFFFFFFF,	NULL,	0,	0},
	{AUD_PLL_FREQ_DET,	0xFFFFFFFF,	NULL,	0,	0},
	{G3D_PLL_CON0,	0xFFFFFFFF,	NULL,	0,	0},
	{G3D_PLL_CON1,	0xFFFFFFFF,	NULL,	0,	0},
	{G3D_PLL_FREQ_DET,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_BUS0_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_BUS1_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_BUS2_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_BUS3_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_MFC_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ISP_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_AUD_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_G3D_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS0_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS1_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS2_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS3_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_MFC_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_ISP_PLL,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_800,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_264,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_G3D_800,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_528,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_132,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_PCLK_CCORE_66,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_BUS0_528,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_BUS0_200,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_PCLK_BUS0_132,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_BUS1_528,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_PCLK_BUS1_132,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_DISP0_0_400,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_DISP0_1_400_TOP,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_DISP1_0_400,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_DISP1_1_400_TOP,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_MFC_600,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_MSCL0_528,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_MSCL1_528_TOP,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_IMEM_266,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_IMEM_200,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_IMEM_100,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_FSYS0_200,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_FSYS1_200,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_PERIS_66,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_PERIC0_66,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_PERIC1_66,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_ISP0_ISP0_528,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_ISP0_TPU_400,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_ISP0_TREX_528,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_ISP1_ISP1_468,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D_TOP,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_CSIS0_414,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_CSIS1_168,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_CSIS2_234,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_3AA0_414,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_3AA1_414,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_CSIS3_132,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_TREX_528,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_ARM_672,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_TREX_VRA_528,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_TREX_B_528,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_BUS_264,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_PERI_84,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_CSIS2_414,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_CSIS3_132,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_SCL_566,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_DECON0_ECLK0_TOP,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK0_TOP,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK1_TOP,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_HDMI_AUDIO_TOP,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK0_TOP,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK1_TOP,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_USBDRD30,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_MMC0,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_UFSUNIPRO20,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_PHY_24M,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_UFSUNIPRO_CFG,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS1_MMC2,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS1_UFSUNIPRO20,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS1_PCIE_PHY,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS1_UFSUNIPRO_CFG,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC0_UART0,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI0,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI1,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI2,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI3,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI4,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI5,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI6,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI7,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_UART1,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_UART2,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_UART3,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_UART4,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_UART5,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_CAM1_ISP_SPI0,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_CAM1_ISP_SPI1,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_CAM1_ISP_UART,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_AP2CP_MIF_PLL_OUT,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_PSCDC_400,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS_PLL_MNGS,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS_PLL_APOLLO,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS_PLL_MIF,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS_PLL_G3D,	0xFFDFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D_TOP,	0xFFDFFFFF, NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CCORE_800,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CCORE_264,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CCORE_G3D_800,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CCORE_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CCORE_132,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_PCLK_CCORE_66,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_BUS0_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_BUS0_200,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_PCLK_BUS0_132,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_BUS1_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_PCLK_BUS1_132,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_DISP0_0_400,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_DISP0_1_400,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_DISP1_0_400,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_DISP1_1_400,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_MFC_600,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_MSCL0_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_MSCL1_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_IMEM_266,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_IMEM_200,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_IMEM_100,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_FSYS0_200,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_FSYS1_200,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_PERIS_66,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_PERIC0_66,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_PERIC1_66,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_ISP0_ISP0_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_ISP0_TPU_400,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_ISP0_TREX_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_ISP1_ISP1_468,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM0_CSIS0_414,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM0_CSIS1_168,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM0_CSIS2_234,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM0_3AA0_414,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM0_3AA1_414,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM0_CSIS3_132,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM0_TREX_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM1_ARM_672,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM1_TREX_VRA_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM1_TREX_B_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM1_BUS_264,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM1_PERI_84,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM1_CSIS2_414,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM1_CSIS3_132,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_CAM1_SCL_566,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_DISP0_DECON0_ECLK0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_DISP0_DECON0_VCLK0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_DISP0_DECON0_VCLK1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_DISP0_HDMI_AUDIO,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_DISP1_DECON1_ECLK0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_DISP1_DECON1_ECLK1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_FSYS0_USBDRD30,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_FSYS0_MMC0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_FSYS0_UFSUNIPRO20,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_FSYS0_PHY_24M,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_FSYS0_UFSUNIPRO_CFG,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_FSYS1_MMC2,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_FSYS1_UFSUNIPRO20,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_FSYS1_PCIE_PHY,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_FSYS1_UFSUNIPRO_CFG,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC0_UART0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC1_SPI0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC1_SPI1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC1_SPI2,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC1_SPI3,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC1_SPI4,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC1_SPI5,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC1_SPI6,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC1_SPI7,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC1_UART1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC1_UART2,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC1_UART3,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC1_UART4,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PERIC1_UART5,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_CAM1_ISP_SPI0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_CAM1_ISP_SPI1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_CAM1_ISP_UART,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_AP2CP_MIF_PLL_OUT,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_PSCDC_400,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_BUS_PLL_MNGS,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_BUS_PLL_APOLLO,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_BUS_PLL_MIF,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_BUS_PLL_G3D,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_ISP_SENSOR0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_ISP_SENSOR0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_ISP_SENSOR0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_ISP_SENSOR1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_ISP_SENSOR1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_ISP_SENSOR1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_ISP_SENSOR2,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_ISP_SENSOR2,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_ISP_SENSOR2,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_ISP_SENSOR3,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_ISP_SENSOR3,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_ISP_SENSOR3,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PROMISE_INT,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PROMISE_INT,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PROMISE_INT,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PROMISE_DISP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_PROMISE_DISP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PROMISE_DISP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_TOP0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_TOP0_DIV_STAT,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_TOP1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_TOP1_DIV_STAT,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_TOP2,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_TOP2_DIV_STAT,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_TOP__CLKOUT0,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_TOP__CLKOUT1,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_TOP__CLKOUT2,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_TOP__CLKOUT3,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_CP2AP_MIF_CLK_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{AP2CP_CLK_CTRL,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_PDN_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{TOP_ROOTCLKEN,	0xFFFFFFFF,	NULL,	0,	0},
	{TOP0_ROOTCLKEN_ON_GATE,	0xFFFFFFFF,	NULL,	0,	0},
	{TOP1_ROOTCLKEN_ON_GATE,	0xFFFFFFFF,	NULL,	0,	0},
	{TOP2_ROOTCLKEN_ON_GATE,	0xFFFFFFFF,	NULL,	0,	0},
	{TOP3_ROOTCLKEN_ON_GATE,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_AUD_PLL_USER,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_I2S,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_PCM,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_MUX_CP2AP_AUD_CLK_USER,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CA5,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_MUX_CDCLK_AUD,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_DIV_AUD_CA5,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_DIV_ACLK_AUD,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_DBG,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_DIV_ATCLK_AUD,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_DIV_AUD_CDCLK,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_DIV_SCLK_I2S,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_DIV_SCLK_PCM,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_DIV_SCLK_SLIMBUS,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_DIV_SCLK_CP_I2S,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_DIV_SCLK_ASRC,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_DIV_CP_CA5,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_DIV_CP_CDCLK,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_CA5,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_AUD,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_AUD,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_ATCLK_AUD,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_I2S,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_PCM,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_SLIMBUS,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_CP_I2S,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_ASRC,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_SLIMBUS_CLKIN,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_I2S_BCLK,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLKOUT_CMU_AUD,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLKOUT_CMU_AUD_DIV_STAT,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CMU_AUD_SPARE0,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CMU_AUD_SPARE1,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_ENABLE_PDN_AUD,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{AUD_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	AUD_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_BUS0_528_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_BUS0_200_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_PCLK_BUS0_132_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_BUS0_528_BUS0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_BUS0_200_BUS0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_PCLK_BUS0_132_BUS0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_BUS0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_BUS0_DIV_STAT,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_BUS0_SPARE0,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_BUS0_SPARE1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_PDN_BUS0,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS0_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_BUS1_528_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_PCLK_BUS1_132_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_BUS1_528_BUS1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_PCLK_BUS1_132_BUS1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_BUS1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_BUS1_DIV_STAT,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_BUS1_SPARE0,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_BUS1_SPARE1,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS1_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_CSIS0_414_USER,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_CSIS1_168_USER,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_CSIS2_234_USER,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_CSIS3_132_USER,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_3AA0_414_USER,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_3AA1_414_USER,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_TREX_528_USER,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_RXBYTECLKHS0_CSIS0_USER,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_RXBYTECLKHS1_CSIS0_USER,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_RXBYTECLKHS2_CSIS0_USER,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_RXBYTECLKHS3_CSIS0_USER,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_RXBYTECLKHS0_CSIS1_USER,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_RXBYTECLKHS1_CSIS1_USER,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_CAM0_CSIS0_207,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_CAM0_3AA0_207,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_CAM0_3AA1_207,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_CAM0_TREX_264,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_CAM0_TREX_132,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_CSIS0_414,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM0_CSIS0_207,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_CSIS1_168_CAM0,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_CSIS2_234_CAM0,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_CSIS3_132_CAM0,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_3AA0_414_CAM0,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM0_3AA0_207,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_3AA1_414_CAM0,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM0_3AA1_207,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_TREX_528_CAM0,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM0_TREX_264,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM0_TREX_132,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_PROMISE_CAM0,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PHYCLK_HS0_CSIS0_RX_BYTE,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PHYCLK_HS1_CSIS0_RX_BYTE,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PHYCLK_HS2_CSIS0_RX_BYTE,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PHYCLK_HS3_CSIS0_RX_BYTE,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PHYCLK_HS0_CSIS1_RX_BYTE,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PHYCLK_HS1_CSIS1_RX_BYTE,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_HPM_APBIF_CAM0,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLKOUT_CMU_CAM0,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLKOUT_CMU_CAM0_DIV_STAT,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CMU_CAM0_SPARE0,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CMU_CAM0_SPARE1,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PDN_CAM0,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CAM0_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_CSIS0_414_LOCAL,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM0_CSIS0_207_LOCAL,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_CSIS1_168_LOCAL,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_CSIS2_234_LOCAL,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_CSIS3_132_LOCAL,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_3AA0_414_LOCAL,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM0_3AA0_207_LOCAL,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_3AA1_414_LOCAL,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM0_3AA1_207_LOCAL,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM0_TREX_264_LOCAL,	0xFFFFFFFF,	CAM0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_ARM_672_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_TREX_VRA_528_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_TREX_B_528_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_BUS_264_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_PERI_84_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_CSIS2_414_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_CSIS3_132_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_SCL_566_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_CAM1_ISP_SPI0_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_CAM1_ISP_SPI1_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_CAM1_ISP_UART_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_RXBYTECLKHS0_CSIS2_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_RXBYTECLKHS1_CSIS2_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_RXBYTECLKHS2_CSIS2_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_RXBYTECLKHS3_CSIS2_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_RXBYTECLKHS0_CSIS3_USER,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_CAM1_ARM_168,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_CAM1_TREX_VRA_264,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_CAM1_BUS_132,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_ARM_672_CAM1,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM1_ARM_168,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_TREX_VRA_528_CAM1,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM1_TREX_VRA_264,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_TREX_B_528_CAM1,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_BUS_264_CAM1,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM1_BUS_132,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM1_PERI_84,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_CSIS2_414_CAM1,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_CSIS3_132_CAM1,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_SCL_566_CAM1,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_CAM1_SCL_283,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_CAM1_ISP_SPI0_CAM1,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_CAM1_ISP_SPI1_CAM1,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_CAM1_ISP_UART_CAM1,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_ISP_PERI_IS_B,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_PHYCLK_HS0_CSIS2_RX_BYTE,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_PHYCLK_HS1_CSIS2_RX_BYTE,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_PHYCLK_HS2_CSIS2_RX_BYTE,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_PHYCLK_HS3_CSIS2_RX_BYTE,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_PHYCLK_HS0_CSIS3_RX_BYTE,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLKOUT_CMU_CAM1,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLKOUT_CMU_CAM1_DIV_STAT,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CMU_CAM1_SPARE0,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CMU_CAM1_SPARE1,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_ENABLE_PDN_CAM1,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CAM1_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	CAM1_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_800_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_264_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_G3D_800_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_528_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_132_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_PCLK_CCORE_66_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_DIV_SCLK_HPM_CCORE,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CCORE0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CCORE1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CCORE2,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CCORE3,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CCORE4,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CCORE_AP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CCORE_CP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_PCLK_CCORE_AP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_PCLK_CCORE_CP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_HPM_CCORE,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_CCORE,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_CCORE_DIV_STAT,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_PDN_CCORE,	0xFFFFFFFF,	NULL,	0,	0},
	{CCORE_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	NULL,	0,	0},
	{PSCDC_CTRL_CCORE,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_STOPCTRL_CCORE,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_CCORE_SPARE0,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_CCORE_SPARE1,	0xFFFFFFFF,	NULL,	0,	0},
	{DISP_PLL_LOCK,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{DISP_PLL_CON0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{DISP_PLL_CON1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{DISP_PLL_FREQ_DET,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_DISP_PLL,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_DISP0_0_400_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_DISP0_1_400_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_DECON0_ECLK0_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK0_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK1_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_HDMI_AUDIO_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_HDMIPHY_PIXEL_CLKO_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_HDMIPHY_TMDS_CLKO_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_MIPIDPHY0_RXCLKESC0_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_MIPIDPHY0_BITCLKDIV2_USER_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_MIPIDPHY0_BITCLKDIV8_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_MIPIDPHY1_RXCLKESC0_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_MIPIDPHY1_BITCLKDIV2_USER_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_MIPIDPHY1_BITCLKDIV8_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_MIPIDPHY2_RXCLKESC0_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_MIPIDPHY2_BITCLKDIV2_USER_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_MIPIDPHY2_BITCLKDIV8_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_DPPHY_CH0_TXD_CLK_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_DPPHY_CH1_TXD_CLK_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_DPPHY_CH2_TXD_CLK_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_DPPHY_CH3_TXD_CLK_USER,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_DISP0_1_400_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_DECON0_ECLK0_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK0_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK1_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_HDMI_AUDIO_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_DISP0_0_133,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_DIV_SCLK_DECON0_ECLK0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_DIV_SCLK_DECON0_VCLK0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_DIV_SCLK_DECON0_VCLK1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_DIV_PHYCLK_HDMIPHY_PIXEL_CLKO,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_DIV_PHYCLK_HDMIPHY_TMDS_20B_CLKO,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_DSM_DIV_M_SCLK_HDMI_AUDIO,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_DSM_DIV_N_SCLK_HDMI_AUDIO,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_DISP0_0_400,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_DISP0_1_400,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_DISP0_0_400_SECURE_SFW_DISP0_0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_DISP0_1_400_SECURE_SFW_DISP0_1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_DISP0_0_133,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_DISP0_0_133_HPM_APBIF_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_DISP0_0_133_SECURE_DECON0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_DISP0_0_133_SECURE_VPP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_DISP0_0_133_SECURE_SFW_DISP0_0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_DISP0_0_133_SECURE_SFW_DISP0_1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_DISP1_400,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_DECON0_ECLK0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_DECON0_VCLK0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_DECON0_VCLK1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_HDMI_AUDIO,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_DISP0_PROMISE,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_HDMIPHY,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_MIPIDPHY0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_MIPIDPHY1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_MIPIDPHY2,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_DPPHY,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_VAL_OSCCLK,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLKOUT_CMU_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLKOUT_CMU_DISP0_DIV_STAT,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{DISP0_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CMU_DISP0_SPARE0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CMU_DISP0_SPARE1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_DISP0_0_400,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_DISP0_1_400,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_DISP0_0_400_SECURE_SFW_DISP0_0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_DISP0_1_400_SECURE_SFW_DISP0_1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_DISP0_0_133,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_DISP0_0_133_HPM_APBIF_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_DISP0_0_133_SECURE_DECON0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_DISP0_0_133_SECURE_VPP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_DISP0_0_133_SECURE_SFW_DISP0_0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_DISP0_0_133_SECURE_SFW_DISP0_1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_DISP1_400,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_DECON0_ECLK0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_DECON0_VCLK0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_DECON0_VCLK1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_HDMI_AUDIO,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_DISP0_PROMISE,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_HDMIPHY,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_MIPIDPHY0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_MIPIDPHY1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_MIPIDPHY2,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_DPPHY,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CG_CTRL_MAN_OSCCLK,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_AXI_LH_ASYNC_MI_DISP0SFR,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_CMU_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_PMU_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_SYSREG_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_DECON0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_VPP0_G0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_VPP0_G1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_DSIM0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_DSIM1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_DSIM2,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_HDMI,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_DP,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_PPMU_DISP0_0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_PPMU_DISP0_1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_SMMU_DISP0_0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_SMMU_DISP0_1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_SFW_DISP0_0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_SFW_DISP0_1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_LH_ASYNC_SI_R_TOP_DISP,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QCH_CTRL_LH_ASYNC_SI_TOP_DISP,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_DSIM0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_DSIM1,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_DSIM2,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_HDMI,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_HDMI_AUDIO,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_DP,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_DISP0_MUX,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_HDMI_PHY,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_DISP1_400,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_DECON0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_HPM_APBIF_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_PROMISE_DISP0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_DPTX_PHY,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_MIPI_DPHY_M1S0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_MIPI_DPHY_M4S0,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{QSTATE_CTRL_MIPI_DPHY_M4S4,	0xFFFFFFFF,	DISP0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_DISP1_0_400_USER,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_DISP1_1_400_USER,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK0_USER,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK1_USER,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DISP1_600_USER,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_MIPIDPHY0_BITCLKDIV2_USER_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_MIPIDPHY1_BITCLKDIV2_USER_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_MIPIDPHY2_BITCLKDIV2_USER_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_DISP1_HDMIPHY_PIXEL_CLKO_USER,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_DISP1_1_400_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK0_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK1_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_DECON1_ECLK1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_DISP1_0_133,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_DIV_SCLK_DECON1_ECLK0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_DIV_SCLK_DECON1_ECLK1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_DISP1_0_400,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_DISP1_1_400,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_DISP1_0_400_SECURE_SFW_DISP1_0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_DISP1_1_400_SECURE_SFW_DISP1_1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_DISP1_0_133,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_DISP1_0_133_HPM_APBIF_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_DISP1_0_133_SECURE_SFW_DISP1_0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_DISP1_0_133_SECURE_SFW_DISP1_1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_DECON1_ECLK_0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_DECON1_ECLK_1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_DISP1_PROMISE,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLKOUT_CMU_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLKOUT_CMU_DISP1_DIV_STAT,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{DISP1_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CMU_DISP1_SPARE0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CMU_DISP1_SPARE1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_DISP1_0_400,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_DISP1_1_400,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_DISP1_0_400_SECURE_SFW_DISP1_0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_DISP1_1_400_SECURE_SFW_DISP1_1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_DISP1_0_133,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_DISP1_0_133_HPM_APBIF_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_DISP1_0_133_SECURE_SFW_DISP1_0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_DISP1_0_133_SECURE_SFW_DISP1_1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_DECON1_ECLK_0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_DECON1_ECLK_1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_DISP1_PROMISE,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_AXI_LH_ASYNC_MI_DISP1SFR,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_CMU_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_PMU_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_SYSREG_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_VPP1_G2,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_VPP1_G3,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_DECON1_PCLK_0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_DECON1_PCLK_1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_PPMU_DISP1_0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_PPMU_DISP1_1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_SMMU_DISP1_0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_SMMU_DISP1_1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_SFW_DISP1_0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_SFW_DISP1_1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_AXI_LH_ASYNC_SI_DISP1_0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QCH_CTRL_AXI_LH_ASYNC_SI_DISP1_1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QSTATE_CTRL_DECON1_ECLK_0,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QSTATE_CTRL_DECON1_ECLK_1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QSTATE_CTRL_HPM_APBIF_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{QSTATE_CTRL_PROMISE_DISP1,	0xFFFFFFFF,	DISP1_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_FSYS0_200_USER,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_USBDRD30_USER,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_MMC0_USER,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_UFSUNIPRO_EMBEDDED_USER,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_24M_USER,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_UFSUNIPRO_EMBEDDED_CFG_USER,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_USBDRD30_UDRD30_PHYCLOCK_USER,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_USBDRD30_UDRD30_PIPE_PCLK_USER,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_UFS_TX0_SYMBOL_USER,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_UFS_RX0_SYMBOL_USER,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_USBHOST20_PHYCLOCK_USER,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_USBHOST20PHY_REF_CLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_FSYS0_200,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_HPM_APBIF_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_USBDRD30_SUSPEND_CLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_MMC0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_UFSUNIPRO_EMBEDDED,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_USBDRD30_REF_CLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_USBDRD30_UDRD30_PHYCLOCK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_USBDRD30_UDRD30_PIPE_PCLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_UFS_TX0_SYMBOL,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_UFS_RX0_SYMBOL,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_USBHOST20_PHYCLOCK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_USBHOST20_REF_CLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_PROMISE_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_USBHOST20PHY_REF_CLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_UFSUNIPRO_EMBEDDED_CFG,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLKOUT_CMU_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLKOUT_CMU_FSYS0_DIV_STAT,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{FSYS0_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CMU_FSYS0_SPARE0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CMU_FSYS0_SPARE1,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_FSYS0_200,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_HPM_APBIF_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_USBDRD30_SUSPEND_CLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_MMC0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_UFSUNIPRO_EMBEDDED,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_USBDRD30_REF_CLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_USBDRD30_UDRD30_PHYCLOCK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_USBDRD30_UDRD30_PIPE_PCLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_UFS_TX0_SYMBOL,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_UFS_RX0_SYMBOL,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_USBHOST20_PHYCLOCK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_USBHOST20_REF_CLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_PROMISE_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_USBHOST20PHY_REF_CLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_UFSUNIPRO_EMBEDDED_CFG,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_AXI_LH_ASYNC_MI_TOP_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_AXI_LH_ASYNC_MI_ETR_USB_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_ETR_USB_FSYS0_ACLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_ETR_USB_FSYS0_PCLK,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_CMU_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_PMU_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_SYSREG_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_USBDRD30,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_MMC0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_UFS_LINK_EMBEDDED,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_USBHOST20,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_PDMA0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_PDMAS,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_PPMU_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_ACEL_LH_ASYNC_SI_TOP_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_USBDRD30_PHYCTRL,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QCH_CTRL_ACEL_LH_ASYNC_SI_TOP_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QSTATE_CTRL_USBDRD30,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QSTATE_CTRL_UFS_LINK_EMBEDDED,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QSTATE_CTRL_USBHOST20,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QSTATE_CTRL_USBHOST20_PHY,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QSTATE_CTRL_GPIO_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QSTATE_CTRL_HPM_APBIF_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{QSTATE_CTRL_PROMISE_FSYS0,	0xFFFFFFFF,	FSYS0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_FSYS1_200_USER,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS1_MMC2_USER,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS1_UFSUNIPRO_SDCARD_USER,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS1_UFSUNIPRO_SDCARD_CFG_USER,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS1_PCIE_PHY_USER,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_UFS_LINK_SDCARD_TX0_SYMBOL_USER,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_UFS_LINK_SDCARD_RX0_SYMBOL_USER,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_PCIE_WIFI0_TX0_USER,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_PCIE_WIFI0_RX0_USER,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_PCIE_WIFI1_TX0_USER,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_PCIE_WIFI1_RX0_USER,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_PCIE_WIFI0_DIG_REFCLK_USER,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_MUX_PHYCLK_PCIE_WIFI1_DIG_REFCLK_USER,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_COMBO_PHY_WIFI,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_FSYS1_200,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_HPM_APBIF_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_COMBO_PHY_WIFI,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_MMC2,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_UFSUNIPRO_SDCARD,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_UFSUNIPRO_SDCARD_CFG,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_FSYS1_PCIE0_PHY,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_UFS_LINK_SDCARD_TX0_SYMBOL,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_UFS_LINK_SDCARD_RX0_SYMBOL,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_PCIE_WIFI0_TX0,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_PCIE_WIFI0_RX0,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_PCIE_WIFI1_TX0,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_PCIE_WIFI1_RX0,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_PCIE_WIFI0_DIG_REFCLK,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_PHYCLK_PCIE_WIFI1_DIG_REFCLK,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_PROMISE_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLKOUT_CMU_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLKOUT_CMU_FSYS1_DIV_STAT,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{FSYS1_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CMU_FSYS1_SPARE0,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CMU_FSYS1_SPARE1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_FSYS1_200,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_HPM_APBIF_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_COMBO_PHY_WIFI,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_MMC2,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_UFSUNIPRO_SDCARD,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_UFSUNIPRO_SDCARD_CFG,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_FSYS1_PCIE0_PHY,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_UFS_LINK_SDCARD_TX0_SYMBOL,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_UFS_LINK_SDCARD_RX0_SYMBOL,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_PCIE_WIFI0_TX0,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_PCIE_WIFI0_RX0,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_PCIE_WIFI1_TX0,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_PCIE_WIFI1_RX0,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_PCIE_WIFI0_DIG_REFCLK,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_PHYCLK_PCIE_WIFI1_DIG_REFCLK,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_PROMISE_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QCH_CTRL_AXI_LH_ASYNC_MI_TOP_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QCH_CTRL_CMU_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QCH_CTRL_PMU_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QCH_CTRL_SYSREG_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QCH_CTRL_MMC2,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QCH_CTRL_UFS_LINK_SDCARD,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QCH_CTRL_PPMU_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QCH_CTRL_ACEL_LH_ASYNC_SI_TOP_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QCH_CTRL_PCIE_RC_LINK_WIFI0_SLV,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QCH_CTRL_PCIE_RC_LINK_WIFI0_DBI,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QCH_CTRL_PCIE_RC_LINK_WIFI1_SLV,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QCH_CTRL_PCIE_RC_LINK_WIFI1_DBI,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QSTATE_CTRL_SROMC_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QSTATE_CTRL_GPIO_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QSTATE_CTRL_HPM_APBIF_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QSTATE_CTRL_PROMISE_FSYS1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QSTATE_CTRL_PCIE_RC_LINK_WIFI0,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QSTATE_CTRL_PCIE_RC_LINK_WIFI1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QSTATE_CTRL_PCIE_PCS_WIFI0,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QSTATE_CTRL_PCIE_PCS_WIFI1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QSTATE_CTRL_PCIE_PHY_FSYS1_WIFI0,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QSTATE_CTRL_PCIE_PHY_FSYS1_WIFI1,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{QSTATE_CTRL_UFS_LINK_SDCARD,	0xFFFFFFFF,	FSYS1_STATUS,	0,	0},
	{CLK_CON_MUX_G3D_PLL_USER,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_CON_MUX_BUS_PLL_USER_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_CON_MUX_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_CON_MUX_PCLK_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_CON_DIV_ACLK_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_CON_DIV_SCLK_HPM_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_CON_DIV_SCLK_ATE_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_G3D_BUS,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_HPM_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_ATE_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLKOUT_CMU_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLKOUT_CMU_G3D_DIV_STAT,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_ENABLE_PDN_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{G3D_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_STOPCTRL_G3D,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CMU_G3D_SPARE0,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CMU_G3D_SPARE1,	0xFFFFFFFF,	G3D_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_IMEM_266_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_IMEM_200_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_IMEM_100_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_IMEM_266,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_IMEM_266_SECURE_SSS,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_IMEM_266_SECURE_RTIC,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_IMEM_200,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_PCLK_IMEM_200_SECURE_SSS,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_PCLK_IMEM_200_SECURE_RTIC,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_IMEM_100,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_IMEM_100_SECURE_CM3_APM,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_IMEM_100_SECURE_AHB_BUSMATRIX_APM,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_IMEM,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_IMEM_DIV_STAT,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_IMEM_SPARE0,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_IMEM_SPARE1,	0xFFFFFFFF,	NULL,	0,	0},
	{IMEM_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_IMEM_266,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_IMEM_266_SECURE_SSS,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_IMEM_266_SECURE_RTIC,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_IMEM_200,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_PCLK_IMEM_200_SECURE_SSS,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_PCLK_IMEM_200_SECURE_RTIC,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_IMEM_100,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_IMEM_100_SECURE_CM3_APM,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_IMEM_100_SECURE_AHB_BUSMATRIX_APM,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_AXI_LH_ASYNC_MI_IMEM,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_SSS,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_RTIC,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_INT_MEM,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_INT_MEM_ALV,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_MCOMP,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_CMU_IMEM,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_PMU_IMEM,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_SYSREG_IMEM,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_PPMU_SSSX,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_LH_ASYNC_SI_IMEM,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_APM,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_CM3_APM,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_GIC,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_ASYNCAHBM_SSS_ATLAS,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_ISP0_528_USER,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_ISP0_TPU_400_USER,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_ISP0_TREX_528_USER,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D_USER,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_ISP0,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_ISP0_TPU,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_ISP0_TREX_264,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_ISP0_TREX_132,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_STAT_MUX_ACLK_ISP0_528_USER,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_STAT_MUX_ACLK_ISP0_TPU_400_USER,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_STAT_MUX_ACLK_ISP0_TREX_528_USER,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_STAT_MUX_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D_USER,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_ISP0,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_ISP0,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_ISP0_TPU,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_ISP0_TPU,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_ISP0_TREX,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_TREX_264,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_HPM_APBIF_ISP0,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_TREX_132,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_PROMISE_ISP0,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLKOUT_CMU_ISP0,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLKOUT_CMU_ISP0_DIV_STAT,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CMU_ISP0_SPARE0,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CMU_ISP0_SPARE1,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_ENABLE_PDN_ISP0,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{ISP0_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	ISP0_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_ISP1_468_USER,	0xFFFFFFFF,	ISP1_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_ISP1_234,	0xFFFFFFFF,	ISP1_STATUS,	0,	0},
	{CLK_ENABLE_ACLK_ISP1,	0xFFFFFFFF,	ISP1_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_ISP1_234,	0xFFFFFFFF,	ISP1_STATUS,	0,	0},
	{CLK_ENABLE_PCLK_HPM_APBIF_ISP1,	0xFFFFFFFF,	ISP1_STATUS,	0,	0},
	{CLK_ENABLE_SCLK_PROMISE_ISP1,	0xFFFFFFFF,	ISP1_STATUS,	0,	0},
	{CLKOUT_CMU_ISP1,	0xFFFFFFFF,	ISP1_STATUS,	0,	0},
	{CLKOUT_CMU_ISP1_DIV_STAT,	0xFFFFFFFF,	ISP1_STATUS,	0,	0},
	{CMU_ISP1_SPARE0,	0xFFFFFFFF,	ISP1_STATUS,	0,	0},
	{CMU_ISP1_SPARE1,	0xFFFFFFFF,	ISP1_STATUS,	0,	0},
	{CLK_ENABLE_PDN_ISP1,	0xFFFFFFFF,	ISP1_STATUS,	0,	0},
	{ISP1_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	ISP1_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_MFC_600_USER,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_MFC_150,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_MFC_600,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_MFC_600_SECURE_SFW_MFC_0,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_MFC_600_SECURE_SFW_MFC_1,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_MFC_150,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_MFC_150_HPM_APBIF_MFC,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_MFC_150_SECURE_SFW_MFC_0,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_MFC_150_SECURE_SFW_MFC_1,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_VAL_SCLK_MFC_PROMISE,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CLKOUT_CMU_MFC,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CLKOUT_CMU_MFC_DIV_STAT,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{MFC_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CMU_MFC_SPARE0,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CMU_MFC_SPARE1,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_MFC_600,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_MFC_600_SECURE_SFW_MFC_0,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_MFC_600_SECURE_SFW_MFC_1,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_MFC_150,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_MFC_150_HPM_APBIF_MFC,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_MFC_150_SECURE_SFW_MFC_0,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_MFC_150_SECURE_SFW_MFC_1,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CG_CTRL_MAN_SCLK_MFC_PROMISE,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QCH_CTRL_MFC,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QCH_CTRL_LH_M_MFC,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QCH_CTRL_CMU_MFC,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QCH_CTRL_PMU_MFC,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QCH_CTRL_SYSREG_MFC,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QCH_CTRL_PPMU_MFC_0,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QCH_CTRL_PPMU_MFC_1,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QCH_CTRL_SFW_MFC_0,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QCH_CTRL_SFW_MFC_1,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QCH_CTRL_SMMU_MFC_0,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QCH_CTRL_SMMU_MFC_1,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QCH_CTRL_LH_S_MFC_0,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QCH_CTRL_LH_S_MFC_1,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QSTATE_CTRL_HPM_APBIF_MFC,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{QSTATE_CTRL_PROMISE_MFC,	0xFFFFFFFF,	MFC_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_MSCL0_528_USER,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_MSCL1_528_USER,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_MSCL1_528_MSCL,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CLK_CON_DIV_PCLK_MSCL,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_MSCL0_528,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_MSCL0_528_SECURE_SFW_MSCL_0,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_MSCL1_528,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_VAL_ACLK_MSCL1_528_SECURE_SFW_MSCL_1,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_MSCL,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_MSCL_SECURE_SFW_MSCL_0,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_VAL_PCLK_MSCL_SECURE_SFW_MSCL_1,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CLKOUT_CMU_MSCL,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CLKOUT_CMU_MSCL_DIV_STAT,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{MSCL_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CMU_MSCL_SPARE0,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CMU_MSCL_SPARE1,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_MSCL0_528,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_MSCL0_528_SECURE_SFW_MSCL_0,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_MSCL1_528,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_MAN_ACLK_MSCL1_528_SECURE_SFW_MSCL_1,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_MSCL,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_MSCL_SECURE_SFW_MSCL_0,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CG_CTRL_MAN_PCLK_MSCL_SECURE_SFW_MSCL_1,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_LH_ASYNC_MI_MSCLSFR,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_CMU_MSCL,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_PMU_MSCL,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_SYSREG_MSCL,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_MSCL_0,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_MSCL_1,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_JPEG,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_G2D,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_SMMU_MSCL_0,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_SMMU_MSCL_1,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_SMMU_JPEG,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_SMMU_G2D,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_PPMU_MSCL_0,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_PPMU_MSCL_1,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_SFW_MSCL_0,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_SFW_MSCL_1,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_LH_ASYNC_SI_MSCL_0,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{QCH_CTRL_LH_ASYNC_SI_MSCL_1,	0xFFFFFFFF,	MSCL_STATUS,	0,	0},
	{CLK_CON_MUX_ACLK_PERIC0_66_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_UART0_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_PERIC0_66,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_UART0,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_PWM,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_PERIC0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_PERIC0_DIV_STAT,	0xFFFFFFFF,	NULL,	0,	0},
	{PERIC0_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_PERIC0_SPARE0,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_PERIC0_SPARE1,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_PERIC0_66,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_UART0,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_PWM,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_AXILHASYNCM_PERIC0,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_CMU_PERIC0,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_PMU_PERIC0,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_SYSREG_PERIC0,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_GPIO_BUS0,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_UART0,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_ADCIF,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_PWM,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C0,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C1,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C4,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C5,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C9,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C10,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C11,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_PERIC1_66_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_SPI0_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_SPI1_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_SPI2_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_SPI3_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_SPI4_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_SPI5_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_SPI6_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_SPI7_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_UART1_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_UART2_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_UART3_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_UART4_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_UART5_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_PERIC1_66,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_PERIC1_66_HSI2C,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_SPI0,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_SPI1,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_SPI2,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_SPI3,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_SPI4,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_SPI5,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_SPI6,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_SPI7,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_UART1,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_UART2,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_UART3,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_UART4,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_UART5,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_PERIC1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_PERIC1_DIV_STAT,	0xFFFFFFFF,	NULL,	0,	0},
	{PERIC1_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_PERIC1_SPARE0,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_PERIC1_SPARE1,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_PERIC1_66,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_PERIC1_66_HSI2C,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_SPI0,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_SPI1,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_SPI2,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_SPI3,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_SPI4,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_SPI5,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_SPI6,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_SPI7,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_UART1,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_UART2,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_UART3,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_UART4,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_UART5,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_AXILHASYNCM_PERIC1,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_CMU_PERIC1,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_PMU_PERIC1,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_SYSREG_PERIC1,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_GPIO_PERIC1,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_GPIO_NFC,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_GPIO_TOUCH,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_GPIO_FF,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_GPIO_ESE,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_UART1,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_UART2,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_UART3,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_UART4,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_UART5,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_SPI0,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_SPI1,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_SPI2,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_SPI3,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_SPI4,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_SPI5,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_SPI6,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_SPI7,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C2,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C3,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C6,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C7,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C8,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C12,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C13,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HSI2C14,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_PERIS_66_USER,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_PERIS,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_PERIS_HPM_APBIF_PERIS,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_PERIS_SECURE_TZPC,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_PERIS_SECURE_RTC,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_PERIS_SECURE_OTP,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_ACLK_PERIS_SECURE_CHIPID,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_PERIS_SECURE_CHIPID,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_PERIS,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_VAL_SCLK_PERIS_PROMISE,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_PERIS,	0xFFFFFFFF,	NULL,	0,	0},
	{CLKOUT_CMU_PERIS_DIV_STAT,	0xFFFFFFFF,	NULL,	0,	0},
	{PERIS_SFR_IGNORE_REQ_SYSCLK,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_PERIS_SPARE0,	0xFFFFFFFF,	NULL,	0,	0},
	{CMU_PERIS_SPARE1,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_PERIS,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_PERIS_HPM_APBIF_PERIS,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_PERIS_SECURE_TZPC,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_PERIS_SECURE_RTC,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_PERIS_SECURE_OTP,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_ACLK_PERIS_SECURE_CHIPID,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_PERIS_SECURE_CHIPID,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_PERIS,	0xFFFFFFFF,	NULL,	0,	0},
	{CG_CTRL_MAN_SCLK_PERIS_PROMISE,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_AXILHASYNCM_PERIS,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_CMU_PERIS,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_PMU_PERIS,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_SYSREG_PERIS,	0xFFFFFFFF,	NULL,	0,	0},
	{QCH_CTRL_MONOCNT_APBIF,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_MCT,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_WDT_MNGS,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_WDT_APOLLO,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_RTC_APBIF,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_SFR_APBIF_TMU,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_SFR_APBIF_HDMI_CEC,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_HPM_APBIF_PERIS,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_0,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_1,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_2,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_3,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_4,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_5,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_6,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_7,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_8,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_9,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_10,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_11,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_12,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_13,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_14,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TZPC_15,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TOP_RTC,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_OTP_CON_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_SFR_APBIF_CHIPID,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_TMU,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_CHIPID,	0xFFFFFFFF,	NULL,	0,	0},
	{QSTATE_CTRL_PROMISE_PERIS,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CCORE_800,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CCORE_264,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CCORE_G3D_800,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CCORE_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CCORE_132,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_PCLK_CCORE_66,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_BUS0_528_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_BUS0_200_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_PCLK_BUS0_132_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_BUS1_528_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_PCLK_BUS1_132_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_DISP0_0_400,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_DISP0_1_400,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_DISP1_0_400,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_DISP1_1_400,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_MFC_600,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_MSCL0_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_MSCL1_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_IMEM_266,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_IMEM_200,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_IMEM_100,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_FSYS0_200,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_FSYS1_200,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_PERIS_66,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_PERIC0_66,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_PERIC1_66,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_ISP0_ISP0_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_ISP0_TPU_400,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_ISP0_TREX_528,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_ISP1_ISP1_468,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_CSIS1_414,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_CSIS1_168_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_CSIS2_234_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_3AA0_414_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_3AA1_414_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_CSIS3_132_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM0_TREX_528_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_ARM_672_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_TREX_VRA_528_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_TREX_B_528_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_BUS_264_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_PERI_84,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_CSIS2_414_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_CSIS3_132_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_CAM1_SCL_566_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_DISP0_DECON0_ECLK0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_DISP0_DECON0_VCLK0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_DISP0_DECON0_VCLK1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_DISP0_HDMI_ADUIO,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_DISP1_DECON1_ECLK0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_DISP1_DECON1_ECLK1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_FSYS0_USBDRD30,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_FSYS0_MMC0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_FSYS0_UFSUNIPRO20,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_FSYS0_PHY_24M,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_FSYS0_UFSUNIPRO_CFG,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_FSYS1_MMC2,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_FSYS1_UFSUNIPRO20,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_FSYS1_PCIE_PHY,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_FSYS1_UFSUNIPRO_CFG,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC0_UART0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC1_SPI0,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC1_SPI1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC1_SPI2,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC1_SPI3,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC1_SPI4,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC1_SPI5,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC1_SPI6,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC1_SPI7,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC1_UART1,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC1_UART2,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC1_UART3,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC1_UART4,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_PERIC1_UART5,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_CAM1_ISP_SPI0_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_CAM1_ISP_SPI1_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_CAM1_ISP_UART_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_AP2CP_MIF_PLL_OUT,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_PSCDC_400,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_BUS_PLL_MNGS,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_BUS_PLL_APOLLO,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_BUS_PLL_MIF,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_SCLK_BUS_PLL_G3D,	0xFFFFFFFF,	NULL,	0,	0},
	{CLK_ENABLE_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D_TOP,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS3_PLL_CON0,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS3_PLL_CON1,	0xFFFFFFFF,	NULL,	0,	0},
	{BUS3_PLL_FREQ_DET,	0xFFFFFFFF,	NULL,	0,	0},
};

static struct cmu_backup cmu_backup_topmux_list[] = {
	{CLK_CON_MUX_BUS0_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_BUS1_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_BUS2_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_BUS3_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_MFC_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ISP_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_AUD_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_G3D_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS0_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS1_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS2_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS3_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_MFC_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_ISP_PLL,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_800,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_264,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_G3D_800,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_528,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CCORE_132,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_PCLK_CCORE_66,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_BUS0_528,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_BUS0_200,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_PCLK_BUS0_132,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_BUS1_528,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_PCLK_BUS1_132,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_DISP0_0_400,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_DISP0_1_400_TOP,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_DISP1_0_400,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_DISP1_1_400_TOP,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_MFC_600,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_MSCL0_528,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_MSCL1_528_TOP,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_IMEM_266,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_IMEM_200,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_IMEM_100,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_FSYS0_200,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_FSYS1_200,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_PERIS_66,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_PERIC0_66,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_PERIC1_66,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_ISP0_ISP0_528,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_ISP0_TPU_400,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_ISP0_TREX_528,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_ISP1_ISP1_468,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D_TOP,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_CSIS0_414,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_CSIS1_168,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_CSIS2_234,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_3AA0_414,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_3AA1_414,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_CSIS3_132,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM0_TREX_528,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_ARM_672,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_TREX_VRA_528,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_TREX_B_528,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_BUS_264,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_PERI_84,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_CSIS2_414,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_CSIS3_132,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_CAM1_SCL_566,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_DECON0_ECLK0_TOP,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK0_TOP,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK1_TOP,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_DISP0_HDMI_AUDIO_TOP,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK0_TOP,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK1_TOP,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_USBDRD30,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_MMC0,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_UFSUNIPRO20,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_PHY_24M,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS0_UFSUNIPRO_CFG,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS1_MMC2,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS1_UFSUNIPRO20,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS1_PCIE_PHY,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_FSYS1_UFSUNIPRO_CFG,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC0_UART0,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI0,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI1,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI2,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI3,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI4,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI5,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI6,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_SPI7,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_UART1,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_UART2,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_UART3,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_UART4,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_PERIC1_UART5,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_CAM1_ISP_SPI0,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_CAM1_ISP_SPI1,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_CAM1_ISP_UART,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_AP2CP_MIF_PLL_OUT,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_ACLK_PSCDC_400,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS_PLL_MNGS,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS_PLL_APOLLO,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS_PLL_MIF,	0x00200000,	NULL,	0,	0},
	{CLK_CON_MUX_SCLK_BUS_PLL_G3D,	0x00200000,	NULL,	0,	0},
};

static void save_cmusfr(int mode)
{
	int i;
	if (mode == syspwr_sleep) {
		for (i = 0; i < ARRAY_SIZE(cmu_backup_list); i++) {
			cmu_backup_list[i].backup_valid = 0;
			if (cmu_backup_list[i].power) {
				if (pwrcal_getf(cmu_backup_list[i].power, 0, 0xF) != 0xF)
					continue;
			}
			cmu_backup_list[i].backup = pwrcal_readl(cmu_backup_list[i].sfr);
			cmu_backup_list[i].backup_valid = 1;
		}
	}
}

static void restore_cmusfr(int mode)
{
	int i;
	if (mode == syspwr_sleep) {
		for (i = 0; i < ARRAY_SIZE(cmu_backup_list); i++) {
			if (cmu_backup_list[i].backup_valid == 0)
				continue;

			if (cmu_backup_list[i].power)
				if (pwrcal_getf(cmu_backup_list[i].power, 0, 0xF) != 0xF)
					continue;

			pwrcal_setf(cmu_backup_list[i].sfr, 0, cmu_backup_list[i].mask, cmu_backup_list[i].backup);
		}
	}
}

static void save_topmuxgate(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cmu_backup_topmux_list); i++) {
		cmu_backup_topmux_list[i].backup_valid = 0;
		if (cmu_backup_list[i].power)
			if (pwrcal_getf(cmu_backup_topmux_list[i].power, 0, 0xF) != 0xF)
				continue;

		cmu_backup_topmux_list[i].backup = pwrcal_readl(cmu_backup_topmux_list[i].sfr);
		cmu_backup_topmux_list[i].backup_valid = 1;
	}
}

static void restore_topmuxgate(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cmu_backup_topmux_list); i++) {
		if (cmu_backup_topmux_list[i].backup_valid == 0)
			continue;

		if (cmu_backup_topmux_list[i].power)
			if (pwrcal_getf(cmu_backup_topmux_list[i].power, 0, 0xF) != 0xF)
				continue;

		pwrcal_setf(cmu_backup_topmux_list[i].sfr, 0, cmu_backup_topmux_list[i].mask, cmu_backup_topmux_list[i].backup);
	}
}

static void topmux_enable(void)
{
	pwrcal_setbit(CLK_CON_MUX_BUS0_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_BUS1_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_BUS2_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_BUS3_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_MFC_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ISP_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_AUD_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_G3D_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_BUS0_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_BUS1_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_BUS2_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_BUS3_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_MFC_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_ISP_PLL,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CCORE_800,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CCORE_264,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CCORE_G3D_800,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CCORE_528,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CCORE_132,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_PCLK_CCORE_66,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_BUS0_528,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_BUS0_200,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_PCLK_BUS0_132,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_BUS1_528,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_PCLK_BUS1_132,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_DISP0_0_400,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_DISP0_1_400_TOP,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_DISP1_0_400,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_DISP1_1_400_TOP,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_MFC_600,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_MSCL0_528,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_MSCL1_528_TOP,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_IMEM_266,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_IMEM_200,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_IMEM_100,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_FSYS0_200,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_FSYS1_200,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_PERIS_66,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_PERIC0_66,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_PERIC1_66,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_ISP0_ISP0_528,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_ISP0_TPU_400,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_ISP0_TREX_528,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_ISP1_ISP1_468,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM0_CSIS0_414,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM0_CSIS1_168,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM0_CSIS2_234,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM0_3AA0_414,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM0_3AA1_414,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM0_CSIS3_132,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM0_TREX_528,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM1_ARM_672,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM1_TREX_VRA_528,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM1_TREX_B_528,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM1_BUS_264,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM1_PERI_84,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM1_CSIS2_414,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM1_CSIS3_132,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_CAM1_SCL_566,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_ECLK0_TOP,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK0_TOP,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_DECON0_VCLK1_TOP,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_DISP0_HDMI_AUDIO_TOP,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK0_TOP,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_DISP1_DECON1_ECLK1_TOP,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_FSYS0_USBDRD30,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_FSYS0_MMC0,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_FSYS0_UFSUNIPRO20,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_FSYS0_PHY_24M,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_FSYS0_UFSUNIPRO_CFG,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_FSYS1_MMC2,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_FSYS1_UFSUNIPRO20,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_FSYS1_PCIE_PHY,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_FSYS1_UFSUNIPRO_CFG,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC0_UART0,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC1_SPI0,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC1_SPI1,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC1_SPI2,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC1_SPI3,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC1_SPI4,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC1_SPI5,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC1_SPI6,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC1_SPI7,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC1_UART1,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC1_UART2,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC1_UART3,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC1_UART4,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PERIC1_UART5,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_CAM1_ISP_SPI0,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_CAM1_ISP_SPI1,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_CAM1_ISP_UART,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_AP2CP_MIF_PLL_OUT,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_PSCDC_400,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_BUS_PLL_MNGS,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_BUS_PLL_APOLLO,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_BUS_PLL_MIF,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_BUS_PLL_G3D,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_ACLK_ISP0_PXL_ASBS_IS_C_FROM_IS_D_TOP,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_ISP_SENSOR0,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_ISP_SENSOR1,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_ISP_SENSOR2,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_ISP_SENSOR3,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PROMISE_INT,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_SCLK_PROMISE_DISP,	21,	1);
	pwrcal_setbit(CLK_CON_MUX_CP2AP_MIF_CLK_USER,	21,	1);
}


static void pwrcal_syspwr_prepare(int mode)
{
	save_cmusfr(mode);
	save_topmuxgate();
	topmux_enable();
	syspwr_hwacg_control(mode);
	disable_armidleclockdown();

	set_pmu_sys_pwr_reg(mode);
	set_pmu_central_seq(true);

	switch (mode) {
	case syspwr_stop:
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 2, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 1, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 0, 0);
		pwrcal_setbit(FSYS0_OPTION, 31, 0);
		pwrcal_setbit(FSYS0_OPTION, 30, 0);
		pwrcal_setbit(FSYS0_OPTION, 29, 0);
		pwrcal_setbit(FSYS1_OPTION, 31, 0);
		pwrcal_setbit(FSYS1_OPTION, 30, 0);
		pwrcal_setbit(FSYS1_OPTION, 29, 0);
		pwrcal_setbit(G3D_OPTION, 31, 1);
		pwrcal_setbit(G3D_OPTION, 30, 1);
		pwrcal_setbit(WAKEUP_MASK, 30, 1);
		set_pmu_central_seq_mif(true);
		break;
	case syspwr_aftr:
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 2, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 1, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 0, 0);
		pwrcal_setbit(FSYS0_OPTION, 31, 0);
		pwrcal_setbit(FSYS0_OPTION, 30, 0);
		pwrcal_setbit(FSYS0_OPTION, 29, 0);
		pwrcal_setbit(FSYS1_OPTION, 31, 0);
		pwrcal_setbit(FSYS1_OPTION, 30, 0);
		pwrcal_setbit(FSYS1_OPTION, 29, 0);
		pwrcal_setbit(G3D_OPTION, 31, 0);
		pwrcal_setbit(G3D_OPTION, 30, 0);
		pwrcal_setbit(WAKEUP_MASK, 30, 1);
		break;
	case syspwr_sicd:
	case syspwr_sicd_cpd:
	case syspwr_sicd_aud:
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 2, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 1, 1);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 0, 1);
		pwrcal_setbit(FSYS0_OPTION, 31, 0);
		pwrcal_setbit(FSYS0_OPTION, 30, 0);
		pwrcal_setbit(FSYS0_OPTION, 29, 0);
		pwrcal_setbit(FSYS1_OPTION, 31, 0);
		pwrcal_setbit(FSYS1_OPTION, 30, 0);
		pwrcal_setbit(FSYS1_OPTION, 29, 0);
		pwrcal_setbit(G3D_OPTION, 31, 0);
		pwrcal_setbit(G3D_OPTION, 30, 0);
		pwrcal_setbit(WAKEUP_MASK, 30, 1);
		smc_set_phycgen(false);
		set_pmu_central_seq_mif(true);
		break;
	case syspwr_alpa:
	case syspwr_dstop:
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 2, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 1, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 0, 0);
		pwrcal_setbit(FSYS0_OPTION, 31, 0);
		pwrcal_setbit(FSYS0_OPTION, 30, 0);
		pwrcal_setbit(FSYS0_OPTION, 29, 0);
		pwrcal_setbit(FSYS1_OPTION, 31, 1);
		pwrcal_setbit(FSYS1_OPTION, 30, 1);
		pwrcal_setbit(FSYS1_OPTION, 29, 0);
		pwrcal_setbit(G3D_OPTION, 31, 0);
		pwrcal_setbit(G3D_OPTION, 30, 0);
		pwrcal_setbit(WAKEUP_MASK, 30, 1);
		set_pmu_central_seq_mif(true);
		break;
	case syspwr_lpd:
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 2, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 1, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 0, 0);
		pwrcal_setbit(FSYS0_OPTION, 31, 0);
		pwrcal_setbit(FSYS0_OPTION, 30, 0);
		pwrcal_setbit(FSYS0_OPTION, 29, 0);
		pwrcal_setbit(FSYS1_OPTION, 31, 1);
		pwrcal_setbit(FSYS1_OPTION, 30, 1);
		pwrcal_setbit(FSYS1_OPTION, 29, 0);
		pwrcal_setbit(G3D_OPTION, 31, 0);
		pwrcal_setbit(G3D_OPTION, 30, 0);
		pwrcal_setbit(WAKEUP_MASK, 30, 1);
		break;
	case syspwr_sleep:
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 2, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 1, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 0, 0);
		pwrcal_setbit(FSYS0_OPTION, 31, 1);
		pwrcal_setbit(FSYS0_OPTION, 30, 1);
		pwrcal_setbit(FSYS0_OPTION, 29, 1);
		pwrcal_setbit(FSYS1_OPTION, 31, 1);
		pwrcal_setbit(FSYS1_OPTION, 30, 1);
		pwrcal_setbit(FSYS1_OPTION, 29, 1);
		pwrcal_setbit(G3D_OPTION, 31, 0);
		pwrcal_setbit(G3D_OPTION, 30, 0);
		pwrcal_setbit(WAKEUP_MASK, 30, 1);
		pwrcal_setbit(MEMORY_TOP_OPTION, 4, 0);
		set_pmu_central_seq_mif(true);

		pr_info("%s : %s mode \n", __func__,
				is_cp_aud_enabled()? "CP_CALL": "SLEEP");
		if (is_cp_aud_enabled() &&
			(!(pwrcal_readl(MAILBOX_EVS_MODE)) && !(pwrcal_readl(MAILBOX_UMTS_MODE)))) {
			mif_use_cp_pll = 1;
			enable_cppll_sharing_bus012_disable();
		}
		break;
	default:
		break;
	}
}

#define PAD_INITIATE_WAKEUP	(0x1 << 28)

void set_pmu_pad_retention_release(void)
{
	/*pwrcal_writel(PAD_RETENTION_AUD_OPTION, PAD_INITIATE_WAKEUP);*/
	pwrcal_writel(PAD_RETENTION_PCIE_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_UFS_CARD_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_MMC2_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_TOP_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_UART_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_MMC0_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_EBIA_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_EBIB_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_SPI_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_BOOTLDO_0_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_UFS_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_USB_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_BOOTLDO_1_OPTION, PAD_INITIATE_WAKEUP);
}

static void pwrcal_syspwr_post(int mode)
{
	syspwr_hwacg_control_post(mode);
	enable_armidleclockdown();

	if (pwrcal_getbit(CENTRAL_SEQ_CONFIGURATION , 16) == 0x1)
		set_pmu_pad_retention_release();
	else
		set_pmu_central_seq(false);

	switch (mode) {
	case syspwr_sleep:
		set_pmu_lpi_mask();
		pwrcal_setbit(MEMORY_TOP_OPTION, 4, 1);
		mif_use_cp_pll = 0;
		dfsmif_paraset = 0;
	case syspwr_stop:
	case syspwr_aftr:
	case syspwr_alpa:
	case syspwr_lpd:
	case syspwr_dstop:
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 2, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 1, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 0, 0);
		pwrcal_setbit(FSYS0_OPTION, 31, 0);
		pwrcal_setbit(FSYS0_OPTION, 30, 0);
		pwrcal_setbit(FSYS0_OPTION, 29, 0);
		pwrcal_setbit(FSYS1_OPTION, 31, 0);
		pwrcal_setbit(FSYS1_OPTION, 30, 0);
		pwrcal_setbit(FSYS1_OPTION, 29, 0);
		pwrcal_setbit(G3D_OPTION, 31, 0);
		pwrcal_setbit(G3D_OPTION, 30, 0);
		pwrcal_setbit(WAKEUP_MASK, 30, 0);
		set_pmu_central_seq_mif(false);
		break;
	case syspwr_sicd:
	case syspwr_sicd_cpd:
	case syspwr_sicd_aud:
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 2, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 1, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 0, 0);
		pwrcal_setbit(FSYS0_OPTION, 31, 0);
		pwrcal_setbit(FSYS0_OPTION, 30, 0);
		pwrcal_setbit(FSYS0_OPTION, 29, 0);
		pwrcal_setbit(FSYS1_OPTION, 31, 0);
		pwrcal_setbit(FSYS1_OPTION, 30, 0);
		pwrcal_setbit(FSYS1_OPTION, 29, 0);
		pwrcal_setbit(G3D_OPTION, 31, 0);
		pwrcal_setbit(G3D_OPTION, 30, 0);
		pwrcal_setbit(WAKEUP_MASK, 30, 0);
		smc_set_phycgen(true);
		set_pmu_central_seq_mif(false);
		break;
	default:
		break;
	}
	restore_cmusfr(mode);
	restore_topmuxgate();
}
static void pwrcal_syspwr_earlywakeup(int mode)
{
	set_pmu_central_seq(false);

	syspwr_hwacg_control_post(mode);
	enable_armidleclockdown();

	switch (mode) {
	case syspwr_sleep:
		pwrcal_setbit(MEMORY_TOP_OPTION, 4, 1);
		if (mif_use_cp_pll) {
			mif_use_cp_pll = 0;
			disable_cppll_sharing_bus012_enable();
		}
	case syspwr_stop:
	case syspwr_aftr:
	case syspwr_alpa:
	case syspwr_lpd:
	case syspwr_dstop:
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 2, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 1, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 0, 0);
		pwrcal_setbit(FSYS0_OPTION, 31, 0);
		pwrcal_setbit(FSYS0_OPTION, 30, 0);
		pwrcal_setbit(FSYS0_OPTION, 29, 0);
		pwrcal_setbit(FSYS1_OPTION, 31, 0);
		pwrcal_setbit(FSYS1_OPTION, 30, 0);
		pwrcal_setbit(FSYS1_OPTION, 29, 0);
		pwrcal_setbit(G3D_OPTION, 31, 0);
		pwrcal_setbit(G3D_OPTION, 30, 0);
		pwrcal_setbit(WAKEUP_MASK, 30, 0);
		set_pmu_central_seq_mif(false);
		break;
	case syspwr_sicd:
	case syspwr_sicd_cpd:
	case syspwr_sicd_aud:
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 2, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 1, 0);
		pwrcal_setbit(TOP_BUS_MIF_OPTION, 0, 0);
		pwrcal_setbit(FSYS0_OPTION, 31, 0);
		pwrcal_setbit(FSYS0_OPTION, 30, 0);
		pwrcal_setbit(FSYS0_OPTION, 29, 0);
		pwrcal_setbit(FSYS1_OPTION, 31, 0);
		pwrcal_setbit(FSYS1_OPTION, 30, 0);
		pwrcal_setbit(FSYS1_OPTION, 29, 0);
		pwrcal_setbit(G3D_OPTION, 31, 0);
		pwrcal_setbit(G3D_OPTION, 30, 0);
		pwrcal_setbit(WAKEUP_MASK, 30, 0);
		smc_set_phycgen(true);
		set_pmu_central_seq_mif(false);
		break;
	default:
		break;
	}

	restore_cmusfr(mode);
	restore_topmuxgate();
}


struct cal_pm_ops cal_pm_ops = {
	.pm_enter = pwrcal_syspwr_prepare,
	.pm_exit = pwrcal_syspwr_post,
	.pm_earlywakeup = pwrcal_syspwr_earlywakeup,
	.pm_init = pwrcal_syspwr_init,
};
