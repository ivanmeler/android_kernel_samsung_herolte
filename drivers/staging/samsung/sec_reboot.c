#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <linux/gpio.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#endif
#include <linux/sec_debug.h>
#include <linux/battery/sec_battery.h>
#include <linux/sec_batt.h>

#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-powermode.h>
#include <linux/soc/samsung/exynos-soc.h>

#define EXYNOS_INFORM2 0x808
#define EXYNOS_INFORM3 0x80c
#define EXYNOS_PS_HOLD_CONTROL 0x330c


void (*mach_restart)(char str, const char *cmd);
EXPORT_SYMBOL(mach_restart);

static void sec_power_off(void)
{
	int poweroff_try = 0;
	unsigned int ps_hold_control;
	union power_supply_propval ac_val, usb_val, wpc_val;

#ifdef CONFIG_OF
	int powerkey_gpio = -1;
	struct device_node *np, *pp;

	np = of_find_node_by_path("/gpio_keys");
	if (!np)
		return;
	for_each_child_of_node(np, pp) {
		uint keycode = 0;
		if (!of_find_property(pp, "gpios", NULL))
			continue;
		of_property_read_u32(pp, "linux,code", &keycode);
		if (keycode == KEY_POWER) {
			pr_info("%s: <%u>\n", __func__,  keycode);
			powerkey_gpio = of_get_gpio(pp, 0);
			break;
		}
	}
	of_node_put(np);

	if (!gpio_is_valid(powerkey_gpio)) {
		pr_err("Couldn't find power key node\n");
		return;
	}
#else
	int powerkey_gpio = GPIO_nPOWER;
#endif

	local_irq_disable();

	psy_do_property("ac", get, POWER_SUPPLY_PROP_ONLINE, ac_val);
	psy_do_property("usb", get, POWER_SUPPLY_PROP_ONLINE, usb_val);
	psy_do_property("wireless", get, POWER_SUPPLY_PROP_ONLINE, wpc_val);
	pr_info("[%s] AC[%d] : USB[%d], WPC[%d]\n", __func__,
		ac_val.intval, usb_val.intval, wpc_val.intval);

	while (1) {
		/* Check reboot charging */
		if (ac_val.intval || usb_val.intval || wpc_val.intval || (poweroff_try >= 5)) {

			pr_emerg("%s: charger connected() or power"
			     "off failed(%d), reboot!\n",
			     __func__, poweroff_try);

#ifdef CONFIG_SEC_DEBUG
			sec_debug_reboot_handler();
#endif
			/* To enter LP charging */
			exynos_pmu_write(EXYNOS_INFORM2, 0x0);

			flush_cache_all();
			//outer_flush_all();
			mach_restart(0, 0);

			pr_emerg("%s: waiting for reboot\n", __func__);
			while (1)
				;
		}

		/* wait for power button release */
		if (gpio_get_value(powerkey_gpio)) {
#ifdef CONFIG_SEC_DEBUG
			/* Clear magic code in power off */
			pr_emerg("%s: Clear magic code in power off!\n", __func__);
			sec_debug_reboot_handler();
			flush_cache_all();
#endif
			pr_emerg("%s: set PS_HOLD low\n", __func__);

			/* power off code
			 * PS_HOLD Out/High -->
			 * Low PS_HOLD_CONTROL, R/W, 0x1002_330C
			 */
			exynos_pmu_read(EXYNOS_PS_HOLD_CONTROL,&ps_hold_control);
			exynos_pmu_write( EXYNOS_PS_HOLD_CONTROL, ps_hold_control & 0xFFFFFEFF);

			++poweroff_try;
			pr_emerg
			    ("%s: Should not reach here! (poweroff_try:%d)\n",
			     __func__, poweroff_try);
		} else {
		/* if power button is not released, wait and check TA again */
			pr_info("%s: PowerButton is not released.\n", __func__);
		}
		mdelay(1000);
	}
}

#ifdef CONFIG_LCD_RES
enum lcd_res_type {
	LCD_RES_DEFAULT = 0,
	LCD_RES_FHD = 1920,
	LCD_RES_HD = 1280,
	LCD_RES_MAX
};
#endif

#define REBOOT_MODE_PREFIX	0x12345670
#define REBOOT_MODE_NONE	0
#define REBOOT_MODE_DOWNLOAD	1
#define REBOOT_MODE_UPLOAD	2
#define REBOOT_MODE_CHARGING	3
#define REBOOT_MODE_RECOVERY	4
#define REBOOT_MODE_FOTA	5
#define REBOOT_MODE_FOTA_BL	6	/* update bootloader */
#define REBOOT_MODE_SECURE	7	/* image secure check fail */
#define REBOOT_MODE_FWUP	9	/* emergency firmware update */
#define REBOOT_MODE_EM_FUSE	10	/* EMC market fuse */

#define REBOOT_SET_PREFIX	0xabc00000
#define REBOOT_SET_LCD_RES	0x000b0000
#define REBOOT_SET_DEBUG	0x000d0000
#define REBOOT_SET_SWSEL	0x000e0000
#define REBOOT_SET_SUD		0x000f0000

static void sec_reboot(char str, const char *cmd)
{
	local_irq_disable();

	pr_emerg("%s (%d, %s)\n", __func__, str, cmd ? cmd : "(null)");

	exynos_pmu_write(EXYNOS_INFORM2, 0x12345678);

	if (!cmd) {
		exynos_pmu_write(EXYNOS_INFORM3, REBOOT_MODE_PREFIX | REBOOT_MODE_NONE);
	} else {
		unsigned long value;
		if (!strcmp(cmd, "fota"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA);
		else if (!strcmp(cmd, "fota_bl"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA_BL);
		else if (!strcmp(cmd, "recovery"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_RECOVERY);
		else if (!strcmp(cmd, "download"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_DOWNLOAD);
		else if (!strcmp(cmd, "upload"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_UPLOAD);
		else if (!strcmp(cmd, "secure"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_SECURE);
		else if (!strcmp(cmd, "fwup"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_FWUP);
		else if (!strcmp(cmd, "em_mode_force_user"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_EM_FUSE);
#ifdef CONFIG_LCD_RES
		else if (!strncmp(cmd, "lcdres_", 7)) {
			if( !strcmp(cmd, "lcdres_fhd") ) value = LCD_RES_FHD;
			else if( !strcmp(cmd, "lcdres_hd") ) value = LCD_RES_HD;
			else value = LCD_RES_DEFAULT;
			pr_info( "%s : lcd_res, %d\n", __func__, (int)value );
			exynos_pmu_write(EXYNOS_INFORM3, REBOOT_SET_PREFIX | REBOOT_SET_LCD_RES | value);
		}
#endif
		else if (!strncmp(cmd, "debug", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_SET_PREFIX | REBOOT_SET_DEBUG | value);
		else if (!strncmp(cmd, "swsel", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_SET_PREFIX | REBOOT_SET_SWSEL | value);
		else if (!strncmp(cmd, "sud", 3)
			 && !kstrtoul(cmd + 3, 0, &value))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_SET_PREFIX | REBOOT_SET_SUD | value);
		else if (!strncmp(cmd, "emergency", 9))
			exynos_pmu_write(EXYNOS_INFORM3, 0x0);
		else if (!strncmp(cmd, "panic", 5)){
			/*
			 * This line is intentionally blanked because the INFORM3 is used for upload cause
			 * in sec_debug_set_upload_cause() only in case of  panic() .
			 */
		}
		else
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_NONE);
	}

	flush_cache_all();
	//outer_flush_all();

	mach_restart(0, 0);

	pr_emerg("%s: waiting for reboot\n", __func__);
	while (1)
		;
}

static int __init sec_reboot_init(void)
{
	mach_restart = (void*)arm_pm_restart;
	pm_power_off = sec_power_off;
	arm_pm_restart = (void*)sec_reboot;
	return 0;
}

subsys_initcall(sec_reboot_init);
