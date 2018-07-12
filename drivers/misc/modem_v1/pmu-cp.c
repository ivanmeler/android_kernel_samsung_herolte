#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/smc.h>
#include "pmu-cp.h"
#include "regs-pmu-exynos8890.h"

#if defined(CONFIG_CP_SECURE_BOOT)
static u32 exynos_smc_read(enum cp_control reg)
{
	u32 cp_ctrl;

	cp_ctrl = exynos_smc(SMC_ID, READ_CTRL, 0, reg);
	if (!(cp_ctrl & 0xffff)) {
		cp_ctrl >>= 16;
	} else {
		pr_err("%s: ERR! read Fail: %d\n", __func__, cp_ctrl & 0xffff);

		return -1;
	}

	return cp_ctrl;
}

static u32 exynos_smc_write(enum cp_control reg, u32 value)
{
	int ret = 0;

	ret = exynos_smc(SMC_ID, WRITE_CTRL, value, reg);
	if (ret > 0) {
		pr_err("%s: ERR! CP_CTRL Write Fail: %d\n", __func__, ret);
		return -1;
	}

	return 0;
}
#endif

/* reset cp */
int exynos_cp_reset(struct modem_ctl *mc)
{
	int ret = 0;
	u32 __maybe_unused cp_ctrl;

	pr_info("%s\n", __func__);

	/* set sys_pwr_cfg registers */
	exynos_sys_powerdown_conf_cp(mc);

#if defined(CONFIG_CP_SECURE_BOOT)
	/* assert cp_reset */
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_RESET_SET);
	if (ret < 0) {
		pr_err("%s: ERR! CP Reset Fail: %d\n", __func__, ret);
		return -1;
	}
#else
	ret = regmap_update_bits(mc->pmureg, EXYNOS_PMU_CP_CTRL_NS,
			CP_RESET_SET, CP_RESET_SET);
	if (ret < 0) {
		pr_err("%s: ERR! CP Reset Fail: %d\n", __func__, ret);
		return -1;
	}
#endif

	/* some delay */
	cpu_relax();
	usleep_range(80, 100);

	return ret;
}

/* release cp */
int exynos_cp_release(struct modem_ctl *mc)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s\n", __func__);

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc_read(CP_CTRL_S);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_S, cp_ctrl | CP_START);
	if (ret < 0)
		pr_err("ERR! CP Release Fail: %d\n", ret);
	else
		pr_info("%s, cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
			exynos_smc_read(CP_CTRL_S));
#else
	ret = regmap_update_bits(mc->pmureg, EXYNOS_PMU_CP_CTRL_S,
			CP_START, CP_START);
	if (ret < 0)
		pr_err("ERR! CP Release Fail: %d\n", ret);
	else {
		regmap_read(mc->pmureg, EXYNOS_PMU_CP_CTRL_S, &cp_ctrl);
		pr_info("%s, PMU_CP_CTRL_S[0x%08x]\n", __func__, cp_ctrl);
	}
#endif

	return ret;
}

/* clear cp active */
int exynos_cp_active_clear(struct modem_ctl *mc)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s\n", __func__);

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_ACTIVE_REQ_CLR);
	if (ret < 0)
		pr_err("%s: ERR! CP active_clear Fail: %d\n", __func__, ret);
	else
		pr_info("%s: cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
			exynos_smc_read(CP_CTRL_NS));
#else
	ret = regmap_update_bits(mc->pmureg, EXYNOS_PMU_CP_CTRL_NS,
			CP_ACTIVE_REQ_CLR, CP_ACTIVE_REQ_CLR);
	if (ret < 0)
		pr_err("%s: ERR! CP active_clear Fail: %d\n", __func__, ret);
	else {
		regmap_read(mc->pmureg, EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
		pr_info("%s, PMU_CP_CTRL_NS[0x%08x]\n", __func__, cp_ctrl);
	}
#endif
	return ret;
}

/* clear cp_reset_req from cp */
int exynos_clear_cp_reset(struct modem_ctl *mc)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s\n", __func__);

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_RESET_REQ_CLR);
	if (ret < 0)
		pr_err("%s: ERR! CP clear_cp_reset Fail: %d\n", __func__, ret);
	else
		pr_info("%s: cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
			exynos_smc_read(CP_CTRL_NS));
#else
	ret = regmap_update_bits(mc->pmureg, EXYNOS_PMU_CP_CTRL_NS,
			CP_RESET_REQ_CLR, CP_RESET_REQ_CLR);
	if (ret < 0)
		pr_err("%s: ERR! CP clear_cp_reset Fail: %d\n", __func__, ret);
	else {
		regmap_read(mc->pmureg, EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
		pr_info("%s, PMU_CP_CTRL_NS[0x%08x]\n", __func__, cp_ctrl);
	}
#endif

	return ret;
}

int exynos_get_cp_power_status(struct modem_ctl *mc)
{
	u32 cp_state;

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_state = exynos_smc_read(CP_CTRL_NS);
	if (cp_state == -1)
		return -1;
#else
	regmap_read(mc->pmureg, EXYNOS_PMU_CP_CTRL_NS, &cp_state);
#endif

	if (cp_state & CP_PWRON)
		return 1;
	else
		return 0;
}

int exynos_cp_init(struct modem_ctl *mc)
{
	u32 __maybe_unused cp_ctrl;
	int ret = 0;

	pr_info("%s: cp_ctrl init\n", __func__);

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl & ~CP_RESET & ~CP_PWRON);
	if (ret < 0)
		pr_err("%s: ERR! write Fail: %d\n", __func__, ret);

	cp_ctrl = exynos_smc_read(CP_CTRL_S);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_S, cp_ctrl & ~CP_START);
	if (ret < 0)
		pr_err("%s: ERR! write Fail: %d\n", __func__, ret);
#else
	ret = regmap_update_bits(mc->pmureg, EXYNOS_PMU_CP_CTRL_NS,
			CP_RESET_SET, 0);
	if (ret < 0)
		pr_err("%s: ERR! CP_RESET_SET Fail: %d\n", __func__, ret);

	ret = regmap_update_bits(mc->pmureg, EXYNOS_PMU_CP_CTRL_NS,
			CP_PWRON, 0);
	if (ret < 0)
		pr_err("%s: ERR! CP_PWRON Fail: %d\n", __func__, ret);

	ret = regmap_update_bits(mc->pmureg, EXYNOS_PMU_CP_CTRL_S,
			CP_START, 0);
	if (ret < 0)
		pr_err("%s: ERR! CP_START Fail: %d\n", __func__, ret);
#endif
	return ret;
}

int exynos_set_cp_power_onoff(struct modem_ctl *mc, enum cp_mode mode)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s: mode[%d]\n", __func__, mode);

#if defined(CONFIG_CP_SECURE_BOOT)
	/* set power on/off */
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	if (mode == CP_POWER_ON) {
		if (!(cp_ctrl & CP_PWRON)) {
			ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_PWRON);
			if (ret < 0)
				pr_err("%s: ERR! write Fail: %d\n", __func__, ret);
			else
				pr_info("%s: CP Power: [0x%08X] -> [0x%08X]\n",
					__func__, cp_ctrl, exynos_smc_read(CP_CTRL_NS));
		}
		cp_ctrl = exynos_smc_read(CP_CTRL_S);
		if (cp_ctrl == -1)
			return -1;

		ret = exynos_smc_write(CP_CTRL_S, cp_ctrl | CP_START);
		if (ret < 0)
			pr_err("%s: ERR! write Fail: %d\n", __func__, ret);
		else
			pr_info("%s: CP Start: [0x%08X] -> [0x%08X]\n", __func__,
				cp_ctrl, exynos_smc_read(CP_CTRL_S));
	} else {
		ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl & ~CP_PWRON);
		if (ret < 0)
			pr_err("ERR! write Fail: %d\n", ret);
		else
			pr_info("%s: CP Power Down: [0x%08X] -> [0x%08X]\n", __func__,
				cp_ctrl, exynos_smc_read(CP_CTRL_NS));
	}
#else
	regmap_read(mc->pmureg, EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
	if (mode == CP_POWER_ON) {
		if (!(cp_ctrl & CP_PWRON)) {
			ret = regmap_update_bits(mc->pmureg,
				EXYNOS_PMU_CP_CTRL_NS, CP_PWRON, CP_PWRON);
			if (ret < 0)
				pr_err("%s: ERR! write Fail: %d\n",
						__func__, ret);
		}

		ret = regmap_update_bits(mc->pmureg,
			EXYNOS_PMU_CP_CTRL_S, CP_START, CP_START);
		if (ret < 0)
			pr_err("%s: ERR! write Fail: %d\n", __func__, ret);
	} else {
		ret = regmap_update_bits(mc->pmureg,
			EXYNOS_PMU_CP_CTRL_NS, CP_PWRON, 0);
		if (ret < 0)
			pr_err("ERR! write Fail: %d\n", ret);
	}
#endif

	return ret;
}

void exynos_sys_powerdown_conf_cp(struct modem_ctl *mc)
{
	pr_info("%s\n", __func__);

	regmap_write(mc->pmureg, EXYNOS_PMU_CENTRAL_SEQ_CP_CONFIGURATION, 0);
	regmap_write(mc->pmureg, EXYNOS_PMU_RESET_AHEAD_CP_SYS_PWR_REG, 0);
	regmap_write(mc->pmureg, EXYNOS_PMU_LOGIC_RESET_CP_SYS_PWR_REG, 0);
	regmap_write(mc->pmureg, EXYNOS_PMU_RESET_ASB_CP_SYS_PWR_REG, 0);
	regmap_write(mc->pmureg, EXYNOS_PMU_TCXO_GATE_SYS_PWR_REG, 0);
	regmap_write(mc->pmureg, EXYNOS_PMU_CLEANY_BUS_SYS_PWR_REG, 0);
}

#if !defined(CONFIG_CP_SECURE_BOOT)

#define MEMSIZE		136
#define SHDMEM_BASE	0xF0000000
#define MEMSIZE_OFFSET	16
#define MEMBASE_ADDR_OFFSET	0

static void __init set_shdmem_size(struct modem_ctl *mc, int memsz)
{
	u32 tmp;

	pr_info("[Modem_IF]Set shared mem size: %dMB\n", memsz);

	memsz /= 4;
	regmap_update_bits(mc->pmureg, EXYNOS_PMU_CP2AP_MEM_CONFIG,
			0x1ff << MEMSIZE_OFFSET, memsz << MEMSIZE_OFFSET);

	regmap_read(mc->pmureg, EXYNOS_PMU_CP2AP_MEM_CONFIG, &tmp);
	pr_info("[Modem_IF] EXYNOS_PMU_CP2AP_MEM_CONFIG: 0x%x\n", tmp);
}

static void __init set_shdmem_base(struct modem_ctl *mc)
{
	u32 tmp, base_addr;

	pr_info("[Modem_IF]Set shared mem baseaddr : 0x%x\n", SHDMEM_BASE);

	base_addr = (SHDMEM_BASE >> 22);

	regmap_update_bits(mc->pmureg, EXYNOS_PMU_CP2AP_MEM_CONFIG,
			0x3fff << MEMBASE_ADDR_OFFSET,
			base_addr << MEMBASE_ADDR_OFFSET);
	regmap_read(mc->pmureg, EXYNOS_PMU_CP2AP_MEM_CONFIG, &tmp);
	pr_info("[Modem_IF] EXYNOS_PMU_CP2AP_MEM_CONFIG: 0x%x\n", tmp);
}

static void set_batcher(struct modem_ctl *mc)
{
	regmap_write(mc->pmureg, EXYNOS_PMU_MODAPIF_CONFIG, 0x3);
}
#endif

int exynos_pmu_cp_init(struct modem_ctl *mc)
{

#if !defined(CONFIG_CP_SECURE_BOOT)
	set_shdmem_size(mc, 136);
	set_shdmem_base(mc);
	set_batcher(mc);

#ifdef CONFIG_SOC_EXYNOS8890
	/* set access window for CP */
	regmap_write(mc->pmureg, EXYNOS_PMU_CP2AP_MIF0_PERI_ACCESS_CON,
			0xffffffff);
	regmap_write(mc->pmureg, EXYNOS_PMU_CP2AP_MIF1_PERI_ACCESS_CON,
			0xffffffff);
	regmap_write(mc->pmureg, EXYNOS_PMU_CP2AP_MIF2_PERI_ACCESS_CON,
			0xffffffff);
	regmap_write(mc->pmureg, EXYNOS_PMU_CP2AP_MIF3_PERI_ACCESS_CON,
			0xffffffff);
	regmap_write(mc->pmureg, EXYNOS_PMU_CP2AP_CCORE_PERI_ACCESS_CON,
			0xffffffff);
#endif

#endif

	return 0;
}
