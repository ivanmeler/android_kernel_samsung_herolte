/*
 *  wacom_i2c.c - Wacom G5 Digitizer Controller (I2C bus)
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include "wacom.h"
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/sec_sysfs.h>
#include <linux/sec_batt.h>

#include "wacom_i2c_func.h"
#include "wacom_i2c_firm.h"
#define WACOM_FW_PATH_SDCARD "/sdcard/firmware/wacom_firm.bin"

static struct wacom_features wacom_feature_EMR = {
	.comstat = COM_QUERY,
	.fw_version = 0x0,
	.update_status = FW_UPDATE_PASS,
};

unsigned char screen_rotate;
unsigned char user_hand = 1;
extern unsigned int system_rev;

struct i2c_client *g_client_boot;

extern int wacom_i2c_flash(struct wacom_i2c *wac_i2c);
extern int wacom_i2c_usermode(struct wacom_i2c *wac_i2c);
extern void wacom_fullscan_check_work(struct work_struct *work);


#ifdef CONFIG_OF
char *gpios_name[] = {
	"irq",
	"pdct",
	"fwe",
	"power3p3",
};

enum {
	I_IRQ = 0,
	I_PDCT,
	I_FWE,
	I_POWER3P3,
};


#define WACOM_GET_PDATA(drv_data) (drv_data ? drv_data->pdata : NULL)

struct wacom_i2c *wacom_get_drv_data(void * data)
{
	static void * drv_data = NULL;
	if (unlikely(data))
		drv_data = data;
	return (struct wacom_i2c *)drv_data;
}

static int wacom_pinctrl_init(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;
#ifdef WACOM_USE_I2C_GPIO
	int ret;
#endif

	wac_i2c->pinctrl_irq = devm_pinctrl_get(&client->dev);
	if (IS_ERR(wac_i2c->pinctrl_irq)) {
		input_err(true, &client->dev, "%s: Failed to get pinctrl\n", __func__);
		goto err_pinctrl_get;
	}

	wac_i2c->pin_state_irq = pinctrl_lookup_state(wac_i2c->pinctrl_irq, "on_irq");
	if (IS_ERR(wac_i2c->pin_state_irq)) {
		input_err(true, &client->dev, "%s: Failed to get pinctrl state\n", __func__);
		goto err_pinctrl_get_state;
	}

#ifdef WACOM_USE_I2C_GPIO
	wac_i2c->pinctrl_i2c = devm_pinctrl_get_select_default(wac_i2c->client->dev.parent->parent);
	if (IS_ERR(wac_i2c->pinctrl_i2c)) {
		input_err(true, &client->dev, "%s: Failed to get pinctrl\n", __func__);
		goto err_pinctrl_get;
	}

	wac_i2c->pin_state_i2c = pinctrl_lookup_state(wac_i2c->pinctrl_i2c, "default");
	if (IS_ERR(wac_i2c->pin_state_i2c)) {
		input_err(true, &client->dev, "%s: Failed to get pinctrl state\n", __func__);
		goto err_pinctrl_get_state;
	}

	wac_i2c->pin_state_gpio = pinctrl_lookup_state(wac_i2c->pinctrl_i2c, "epen_gpio");
	if (IS_ERR(wac_i2c->pin_state_gpio)) {
		input_err(true, &client->dev, "%s: Failed to get pinctrl state\n", __func__);
		goto err_pinctrl_get_state;
	}

	ret = pinctrl_select_state(wac_i2c->pinctrl_i2c, wac_i2c->pin_state_gpio);
	if (ret < 0)
		input_err(true, &client->dev, "%s: Failed to configure gpio mode\n", __func__);

	ret = pinctrl_select_state(wac_i2c->pinctrl_i2c, wac_i2c->pin_state_i2c);
	if (ret < 0)
		input_err(true, &client->dev, "%s: Failed to configure i2c mode\n", __func__);
#endif

	return 0;

err_pinctrl_get_state:
	devm_pinctrl_put(wac_i2c->pinctrl_irq);
err_pinctrl_get:
	return -ENODEV;
}

int wacom_power(bool on)
{
	struct wacom_i2c *wac_i2c = wacom_get_drv_data(NULL);
	struct wacom_g5_platform_data *pdata = wac_i2c->pdata;
	struct i2c_client *client = wac_i2c->client;
	static bool wacom_power_enabled = false;
//	int ret = 0;
//	struct regulator *regulator_vdd = NULL;
	static struct timeval off_time = {0, 0};
	struct timeval cur_time = {0, 0};

	input_info(true, &client->dev, "power %s\n", on ? "enabled" : "disabled");

	if (!pdata) {
		input_err(true, &client->dev, "%s, pdata is null\n", __func__);
		return -1;
	}

	if (wacom_power_enabled == on) {
		input_info(true, &client->dev, "pwr already %s\n", on ? "enabled" : "disabled");
		return 0;
	}

	if (on) {
		long sec, usec;

		do_gettimeofday(&cur_time);
		sec = cur_time.tv_sec - off_time.tv_sec;
		usec = cur_time.tv_usec - off_time.tv_usec;
		if (!sec) {
			usec = EPEN_OFF_TIME_LIMIT - usec;
			if (usec > 500) {
				usleep_range(usec, usec);
				input_info(true, &client->dev,
						"%s, pwr on usleep %d\n", __func__, (int)usec);
			}
		}
	}

	if (on) {
		if (pdata->gpios[I_POWER3P3])
			gpio_direction_output(pdata->gpios[I_POWER3P3], 1);
		else input_err(true, &client->dev, "%s, power-gpio is null\n", __func__);
	} else {
		if (pdata->gpios[I_POWER3P3])
			gpio_direction_output(pdata->gpios[I_POWER3P3], 0);
		else input_err(true, &client->dev, "%s, power-gpio is null\n", __func__);
	}

/*
	regulator_vdd = regulator_get(NULL, "wacom_3.3v");
	if (IS_ERR_OR_NULL(regulator_vdd)) {
		input_err(true, &client->dev, " %s reg get err\n", __func__);
		regulator_put(regulator_vdd);
		return -EINVAL;
	}

	if (on) {
		ret = regulator_enable(regulator_vdd);
	} else {
		if (regulator_is_enabled(regulator_vdd)) {
			regulator_disable(regulator_vdd);
			do_gettimeofday(&off_time);
		}
	}
	regulator_put(regulator_vdd);
*/
	wacom_power_enabled = on;

	return 0;
}

static int wacom_suspend_hw(void)
{
#if 0
	struct wacom_g5_platform_data *pdata = WACOM_GET_PDATA(wacom_get_drv_data(NULL));
	if (pdata->gpios[I_RESET])
		gpio_direction_output(pdata->gpios[I_RESET], 0);
#endif
	wacom_power(0);
	return 0;
}

static int wacom_resume_hw(void)
{
	struct wacom_i2c *wac_i2c = wacom_get_drv_data(NULL);
//	struct wacom_g5_platform_data *pdata = WACOM_GET_PDATA(wacom_get_drv_data(NULL));
	int ret;
#if 0
	if (pdata->gpios[I_RESET])
		gpio_direction_output(pdata->gpios[I_RESET], 1);
	/*gpio_direction_output(pdata->gpios[I_PDCT], 1);*/
#endif
	wacom_power(1);
	ret = pinctrl_select_state(wac_i2c->pinctrl_irq, wac_i2c->pin_state_irq);
	if (ret < 0)
		input_err(true, &wac_i2c->client->dev, "%s: Failed to configure irq pin\n", __func__);
	/*msleep(100);*/
	/*gpio_direction_input(pdata->gpios[I_PDCT]);*/
	return 0;
}

static int wacom_reset_hw(void)
{
	wacom_suspend_hw();
	msleep(20);
	wacom_resume_hw();

	return 0;
}

static void wacom_compulsory_flash_mode(bool en)
{
	struct wacom_g5_platform_data *pdata =
		WACOM_GET_PDATA(wacom_get_drv_data(NULL));
	struct wacom_i2c *wac_i2c = wacom_get_drv_data(NULL);
	struct i2c_client *client = wac_i2c->client;
#ifdef WACOM_USE_I2C_GPIO
	int ret;
#endif
	if (likely(pdata->boot_on_ldo)) {
		static bool is_enabled = false;
		struct regulator *reg_fwe = NULL;
		int ret = 0;

		if (is_enabled == en) {
			input_info(true, &client->dev, "fwe already %s\n", en ? "enabled" : "disabled");
			return ;
		}

		reg_fwe = regulator_get(NULL, "wacom_fwe_1.8v");
		if (IS_ERR_OR_NULL(reg_fwe)) {
			input_err(true, &client->dev, "%s reg get err\n", __func__);
			regulator_put(reg_fwe);
			return ;
		}

		if (en) {
			ret = regulator_enable(reg_fwe);
			if (ret)
				input_err(true, &client->dev, "failed to enable fwe reg(%d)\n", ret);
		} else {
			if (regulator_is_enabled(reg_fwe))
				regulator_disable(reg_fwe);
		}
		regulator_put(reg_fwe);

		is_enabled = en;
	} else
		gpio_direction_output(pdata->gpios[I_FWE], en);

#ifdef WACOM_USE_I2C_GPIO
	if (!en) {
		ret = pinctrl_select_state(wac_i2c->pinctrl_i2c, wac_i2c->pin_state_gpio);
		if (ret < 0)
			input_err(true, &client->dev, "%s: Failed to configure gpio mode\n", __func__);

		ret = pinctrl_select_state(wac_i2c->pinctrl_i2c, wac_i2c->pin_state_i2c);
		if (ret < 0)
			input_err(true, &client->dev, "%s: Failed to configure i2c mode\n", __func__);
	}
#endif
}

static int wacom_get_irq_state(void)
{
	struct wacom_g5_platform_data *pdata =
		WACOM_GET_PDATA(wacom_get_drv_data(NULL));

	int level = gpio_get_value(pdata->gpios[I_IRQ]);

	if (pdata->irq_type & (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_LOW))
		return !level;

	return level;
}
#endif

static void wacom_enable_irq(struct wacom_i2c *wac_i2c, bool enable)
{
	static int depth;

	mutex_lock(&wac_i2c->irq_lock);
	if (enable) {
		if (depth) {
			--depth;
			enable_irq(wac_i2c->irq);
#ifndef WACOM_USE_SURVEY_MODE
#ifdef WACOM_PDCT_WORK_AROUND
			enable_irq(wac_i2c->irq_pdct);
#endif
#endif
		}
	} else {
		if (!depth) {
			++depth;
			disable_irq(wac_i2c->irq);
#ifndef WACOM_USE_SURVEY_MODE
#ifdef WACOM_PDCT_WORK_AROUND
			disable_irq(wac_i2c->irq_pdct);
#endif
#endif
		}
	}
	mutex_unlock(&wac_i2c->irq_lock);

#ifdef WACOM_IRQ_DEBUG
	input_info(true, &wac_i2c->client->dev, "Enable %d, depth %d\n", (int)enable, depth);
#endif
}

#ifdef WACOM_USE_SURVEY_MODE
static void wacom_enable_pdct_irq(struct wacom_i2c *wac_i2c, bool enable)
{
	static int pdct_depth;

	mutex_lock(&wac_i2c->irq_lock);
	if (enable) {
		if (pdct_depth) {
			--pdct_depth;
			enable_irq(wac_i2c->irq_pdct);
		}
	} else {
		if (!pdct_depth) {
			++pdct_depth;
			disable_irq(wac_i2c->irq_pdct);
		}
	}
	mutex_unlock(&wac_i2c->irq_lock);

#ifdef WACOM_IRQ_DEBUG
	input_info(true, &wac_i2c->client->dev, "Enable %d, depth %d\n", (int)enable, pdct_depth);
#endif
}
#endif

static void wacom_i2c_reset_hw(struct wacom_i2c *wac_i2c)
{
	/* Reset IC */
	wac_i2c->pdata->suspend_platform_hw();
	msleep(50);
	wac_i2c->pdata->resume_platform_hw();
	msleep(200);
}

static void wacom_power_on(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;

	mutex_lock(&wac_i2c->lock);
	input_info(true, &client->dev, "%s start, ps:%d pen %s [0x%x]\n",
		__func__, wac_i2c->battery_saving_mode, (wac_i2c->function_result & EPEN_EVENT_PEN_INOUT) ? "out" : "in", 
		wac_i2c->wac_feature->fw_version);

	if (wac_i2c->power_enable) {
		input_info(true, &client->dev, "Pass pwr on\n");
		goto goto_resuem_work;
	}

	if (wake_lock_active(&wac_i2c->fw_wakelock)) {
		input_info(true, &client->dev, "Wake_lock active. pass pwr on\n");
		goto out_power_on;
	}

goto_resuem_work:
	/* power on */
	wac_i2c->pdata->resume_platform_hw();
	wac_i2c->power_enable = true;

#ifdef WACOM_USE_SURVEY_MODE
	if (wac_i2c->epen_blocked) {
		input_info(true, &wac_i2c->client->dev,
				"%s : epen blocked. keep Garage only mode\n", __func__);
	} else {
		if (wac_i2c->battery_saving_mode && !(wac_i2c->function_result & EPEN_EVENT_PEN_INOUT)) {
			input_info(true, &wac_i2c->client->dev,
					"%s : ps on & pen in : keep Grage_Only mode\n", __func__);
		} else {
			if( wac_i2c->survey_mode == true){
				wac_i2c->survey_mode = false;
				input_info(true, &wac_i2c->client->dev,
						"%s : survey mode(true -> false)\n", __func__);
			} else {
				input_info(true, &wac_i2c->client->dev,
						"%s : survey mode(false) \n", __func__);
			}
			wacom_i2c_exit_survey_mode(wac_i2c);
		}
	}

	if (device_may_wakeup(&client->dev))
		disable_irq_wake(wac_i2c->irq);

	if (device_may_wakeup(&client->dev))
		disable_irq_wake(wac_i2c->irq_pdct);

#endif

	cancel_delayed_work_sync(&wac_i2c->resume_work);
	schedule_delayed_work(&wac_i2c->resume_work, msecs_to_jiffies(EPEN_RESUME_DELAY));

 out_power_on:
	mutex_unlock(&wac_i2c->lock);

	input_info(true, &client->dev, "%s end\n", __func__);
}

static void wacom_power_off(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;
	mutex_lock(&wac_i2c->lock);
	input_info(true, &client->dev, "%s start, ps:%d pen %s  set(%x) ret(%x) block(%d) reset(%d)\n",
		__func__, wac_i2c->battery_saving_mode, (wac_i2c->function_result & EPEN_EVENT_PEN_INOUT) ? "out" : "in",
		wac_i2c->function_set, wac_i2c->function_result, wac_i2c->epen_blocked, wac_i2c->reset_flag);

	if (!wac_i2c->power_enable) {
		input_err(true, &client->dev, "Pass pwr off\n");
		goto out_power_off;
	}

#ifdef CONFIG_INPUT_BOOSTER
	input_booster_send_event(BOOSTER_DEVICE_PEN, BOOSTER_MODE_FORCE_OFF);
#endif
	wacom_enable_irq(wac_i2c, false);
	/* release pen, if it is pressed */
	if (wac_i2c->pen_pressed || wac_i2c->side_pressed
		|| wac_i2c->pen_prox)
		forced_release(wac_i2c);

	if (wac_i2c->soft_key_pressed[0] || wac_i2c->soft_key_pressed[1] )
		forced_release_key(wac_i2c);

	forced_release_fullscan(wac_i2c);	

	cancel_delayed_work_sync(&wac_i2c->resume_work);
	cancel_delayed_work_sync(&wac_i2c->fullscan_check_work);

#ifdef LCD_FREQ_SYNC
	cancel_work_sync(&wac_i2c->lcd_freq_work);
	cancel_delayed_work_sync(&wac_i2c->lcd_freq_done_work);
	wac_i2c->lcd_freq_wait = false;
#endif
#ifdef WACOM_USE_SOFTKEY_BLOCK
	cancel_delayed_work_sync(&wac_i2c->softkey_block_work);
	wac_i2c->block_softkey = false;
#endif

	if (wake_lock_active(&wac_i2c->fw_wakelock)) {
		input_info(true, &client->dev, "wake_lock active. pass pwr off\n");
		goto out_power_off;
	}

#ifdef WACOM_USE_SURVEY_MODE
	if (wac_i2c->epen_blocked) {
		input_info(true, &wac_i2c->client->dev,
				"%s : epen blocked. keep Garage only mode\n", __func__);
	} else {

		if (wac_i2c->reset_flag) {
			wac_i2c->reset_flag = false;
			input_info(true, &wac_i2c->client->dev, "%s : IC reset start\n", __func__);

			wacom_enable_pdct_irq(wac_i2c, false);

			wacom_suspend_hw();
			msleep(120);
			wacom_resume_hw();
			msleep(150);

			wac_i2c->pen_pdct = gpio_get_value(wac_i2c->pdata->gpios[I_PDCT]);
			input_info(true, &wac_i2c->client->dev, "%s : IC reset end, pdct(%d)\n", __func__, wac_i2c->pen_pdct);
			wacom_enable_pdct_irq(wac_i2c, true);
		}

		if (!(wac_i2c->function_set & EPEN_SETMODE_AOP)) {
			input_info(true, &client->dev, "garage only\n");
			wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_ONLY);
		}
		else if ((wac_i2c->function_set & EPEN_SETMODE_AOP) && !(wac_i2c->function_result & EPEN_EVENT_PEN_INOUT) ) {
			/*  aop on , pen in */
			if(wac_i2c->battery_saving_mode){
				input_info(true, &client->dev, "aop on, pen in, ps on\n");
				wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_ONLY);
			} else {
				input_info(true, &client->dev, "aop on  pen in, ps off\n");
				wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_AOP);
			}
		} else {
			/* aop on, pen out */
			input_info(true, &client->dev, "aop on, pen out\n");
			wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_AOP);
		}
		wac_i2c->survey_mode = true;
	}
	wac_i2c->power_enable = false;

#else
	{
		/* power off */
		wac_i2c->power_enable = false;
		wac_i2c->pdata->suspend_platform_hw();
	}
#endif

 out_power_off:
 #ifdef WACOM_USE_SURVEY_MODE
	wacom_enable_irq(wac_i2c, true);

	if (device_may_wakeup(&client->dev))
		enable_irq_wake(wac_i2c->irq);

	if (device_may_wakeup(&client->dev))
		enable_irq_wake(wac_i2c->irq_pdct);

 #endif
	mutex_unlock(&wac_i2c->lock);

	input_info(true, &client->dev, "%s end \n", __func__);
}

#if 0 //#ifdef WACOM_USE_SURVEY_MODE
static void pen_survey_resetdwork(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, pen_survey_resetdwork.work);

	input_info(true, &wac_i2c->client->dev,  "%s : %d\n", __func__, wac_i2c->pen_insert);

	/* must be TRUE state, it is checked on power_on(resumework) at normal */
	wac_i2c->survey_mode = true;
	input_info(true, &wac_i2c->client->dev,  "%s in	-survey_mode(%d)\n", __func__, wac_i2c->survey_mode);

	wacom_enable_pdct_irq(wac_i2c, false);
	/* power off */
	wac_i2c->power_enable = false;
	wac_i2c->pdata->suspend_platform_hw();
	msleep(20);
	// schedule work - check : wac_i2c->pen_prox   EPEN_RESUME_DELAY + 50ms
	wacom_power_on(wac_i2c);

	/*delay needs to remove gabage pdct irq by power on */
	msleep(150); //

	//wacom_enable_pdct_irq(wac_i2c, true);
	input_info(true, &wac_i2c->client->dev,  "%s out	-survey_mode(%d)\n", __func__, wac_i2c->survey_mode);
}
#endif

static irqreturn_t wacom_interrupt(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;
	int ret;
	
	if(wac_i2c->power_enable == false) {
		input_err(true, &wac_i2c->client->dev,  "%s : survey_mode on\n", __func__);

		/* disable-irq later to prevent sleep by pen-in */
		wake_lock_timeout(&wac_i2c->det_wakelock, 1000);

		ret = wacom_get_aop_data(wac_i2c);
		if(ret && wac_i2c->function_set & EPEN_SETMODE_AOP ){
			wac_i2c->function_result |= EPEN_EVENT_AOP;
			input_info(true, &wac_i2c->client->dev, "AOP detected and wake up device\n");

			input_report_key(wac_i2c->input_dev, KEY_WAKEUP_UNLOCK, 1);
			input_sync(wac_i2c->input_dev);
			
			input_report_key(wac_i2c->input_dev, KEY_WAKEUP_UNLOCK, 0);
			input_sync(wac_i2c->input_dev);
		}

		return IRQ_HANDLED;
	}
	wacom_i2c_coord(wac_i2c);
	return IRQ_HANDLED;
}

#if defined(WACOM_PDCT_WORK_AROUND)
static irqreturn_t wacom_interrupt_pdct(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;
	struct i2c_client *client = wac_i2c->client;

	if (wac_i2c->query_status == false)
		return IRQ_HANDLED;

	wac_i2c->pen_pdct = gpio_get_value(wac_i2c->pdata->gpios[I_PDCT]);
#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	input_info(true, &client->dev,  "pen %s\n",
		wac_i2c->pen_pdct ? "in" : "out");
#else
	input_info(true, &client->dev,  "pen %s : %d\n",
		wac_i2c->pen_pdct ? "in" : "out", wac_i2c->pdata->get_irq_state());
#endif

#ifdef WACOM_USE_SURVEY_MODE
	{
		if(wac_i2c->pen_pdct) wac_i2c->function_result &= ~EPEN_EVENT_PEN_INOUT;
		else wac_i2c->function_result |= EPEN_EVENT_PEN_INOUT;

		if(wac_i2c->power_enable == false)	wake_lock_timeout(&wac_i2c->det_wakelock, 1000);

		input_report_switch(wac_i2c->input_dev, SW_PEN_INSERT,
				(wac_i2c->function_result & EPEN_EVENT_PEN_INOUT));
		input_sync(wac_i2c->input_dev);

		if (wac_i2c->epen_blocked) {
			input_info(true, &wac_i2c->client->dev,
					"%s : epen blocked. keep Garage only mode\n", __func__);
		} else {
			if (wac_i2c->battery_saving_mode) {
				/* saving on & pen in => garage only */
				if (!(wac_i2c->function_result & EPEN_EVENT_PEN_INOUT)){ 
					input_info(true, &client->dev, "%s  ps:%d pen %s set(%x) ret(%x)\n",
						__func__, wac_i2c->battery_saving_mode, (wac_i2c->function_result & EPEN_EVENT_PEN_INOUT) ? "out" : "in",
						wac_i2c->function_set, wac_i2c->function_result);
					
					wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_ONLY);
					wac_i2c->survey_mode = true;
				} else {
					input_info(true, &client->dev,"ps on & pen out & %s mode & lcd %s\n",
							wac_i2c->survey_cmd_state ? "survey" : "normal",
							wac_i2c->power_enable ? "on" : "off");

					if(wac_i2c->power_enable && wac_i2c->survey_cmd_state == true)
						wacom_i2c_exit_survey_mode(wac_i2c);
					else input_info(true, &client->dev,"ps on & keep current mode (%d)\n",wac_i2c->survey_cmd_state);
				}
			} else {
				input_info(true, &client->dev, "%s	ps:%d pen %s	set(%x) ret(%x)\n",
					__func__, wac_i2c->battery_saving_mode, (wac_i2c->function_result & EPEN_EVENT_PEN_INOUT) ? "out" : "in",
					wac_i2c->function_set, wac_i2c->function_result);
				if(wac_i2c->power_enable &&  wac_i2c->survey_cmd_state == true) wacom_i2c_exit_survey_mode(wac_i2c);
				else input_info(true, &client->dev,"ps off & keep current mode (%s)\n",wac_i2c->survey_cmd_state ? "survey" : "normal");
			}
		}
	}
#endif

	return IRQ_HANDLED;
}
#endif

#ifndef WACOM_USE_SURVEY_MODE
static void pen_insert_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, pen_insert_dwork.work);

	(wac_i2c->function_result & EPEN_EVENT_PEN_INOUT) = !gpio_get_value(wac_i2c->pdata->gpios[I_SENSE]);
	input_info(true, &wac_i2c->client->dev, "%s : %d\n",
		__func__, (wac_i2c->function_result & EPEN_EVENT_PEN_INOUT));

	input_report_switch(wac_i2c->input_dev,
		SW_PEN_INSERT, !(wac_i2c->function_result & EPEN_EVENT_PEN_INOUT));
	input_sync(wac_i2c->input_dev);

	/* batter saving mode */
	if (wac_i2c->function_result & EPEN_EVENT_PEN_INOUT) {
		if (wac_i2c->battery_saving_mode)
			wacom_power_off(wac_i2c);
	} else
		wacom_power_on(wac_i2c);
}

static irqreturn_t wacom_pen_detect(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;

	cancel_delayed_work_sync(&wac_i2c->pen_insert_dwork);
	schedule_delayed_work(&wac_i2c->pen_insert_dwork, HZ / 20);
	return IRQ_HANDLED;
}
#endif

static int init_pen_insert(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;
#ifdef WACOM_USE_SURVEY_MODE
	wac_i2c->pen_pdct = gpio_get_value(wac_i2c->pdata->gpios[I_PDCT]);
	input_info(true, &client->dev,  "pdct(%d)\n",wac_i2c->pen_pdct);

	if(wac_i2c->pen_pdct) wac_i2c->function_result &= ~EPEN_EVENT_PEN_INOUT;
	else wac_i2c->function_result |= EPEN_EVENT_PEN_INOUT;

	input_info(true, &wac_i2c->client->dev, "%s : pen is %s\n",
		__func__, (wac_i2c->function_result & EPEN_EVENT_PEN_INOUT) ? "OUT" : "IN");

	input_report_switch(wac_i2c->input_dev,
		SW_PEN_INSERT, (wac_i2c->function_result & EPEN_EVENT_PEN_INOUT));
	input_sync(wac_i2c->input_dev);
#else
	int ret = 0;
	int irq = gpio_to_irq(wac_i2c->pdata->gpios[I_SENSE]);
	INIT_DELAYED_WORK(&wac_i2c->pen_insert_dwork, pen_insert_work);

	ret =
		request_threaded_irq(
			irq, NULL,
			wacom_pen_detect,
			IRQF_DISABLED | IRQF_TRIGGER_RISING |
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"pen_insert", wac_i2c);
	if (ret < 0) {
		input_err(true, &wac_i2c->client->dev, 
			"failed to request pen insert irq\n");
		return -1;
	}
	input_info(true, &wac_i2c->client->dev, "init sense irq %d\n", irq);

	enable_irq_wake(irq);

	/* update the current status */
	schedule_delayed_work(&wac_i2c->pen_insert_dwork, HZ / 2);
#endif
	return 0;
}

static int wacom_i2c_input_open(struct input_dev *dev)
{
	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);
	int ret = 0;

	input_info(true, &wac_i2c->client->dev, "%s.\n", __func__);

#if 0 //#ifdef WACOM_USE_SURVEY_MODE
	cancel_delayed_work_sync(&wac_i2c->pen_survey_resetdwork);
#endif

#if 0
	ret = wait_for_completion_interruptible_timeout(&wac_i2c->init_done,
		msecs_to_jiffies(1 * MSEC_PER_SEC));

	if (ret < 0) {
		input_err(&wac_i2c->client->dev,
			"error while waiting for device to init (%d)\n", ret);
		ret = -ENXIO;
		goto err_open;
	}
	if (ret == 0) {
		input_err(&wac_i2c->client->dev,
			"timedout while waiting for device to init\n");
		ret = -ENXIO;
		goto err_open;
	}
#endif

	wacom_power_on(wac_i2c);

	return ret;
}

static void wacom_i2c_input_close(struct input_dev *dev)
{
	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);

	input_info(true, &wac_i2c->client->dev, "%s.\n", __func__);

	wacom_power_off(wac_i2c);
}

static void wacom_i2c_set_input_values(struct i2c_client *client,
				       struct wacom_i2c *wac_i2c,
				       struct input_dev *input_dev)
{
	/*Set input values before registering input device */

	input_dev->name = "sec_e-pen";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

	input_dev->evbit[0] |= BIT_MASK(EV_SW);
	input_set_capability(input_dev, EV_SW, SW_PEN_INSERT);

	input_dev->open = wacom_i2c_input_open;
	input_dev->close = wacom_i2c_input_close;

	__set_bit(ABS_X, input_dev->absbit);
	__set_bit(ABS_Y, input_dev->absbit);
	__set_bit(ABS_PRESSURE, input_dev->absbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(BTN_TOOL_PEN, input_dev->keybit);
	__set_bit(BTN_TOOL_SPEN_SCAN, input_dev->keybit);
	__set_bit(BTN_TOOL_RUBBER, input_dev->keybit);
	__set_bit(BTN_STYLUS, input_dev->keybit);
	__set_bit(KEY_UNKNOWN, input_dev->keybit);
	//__set_bit(KEY_PEN_PDCT, input_dev->keybit);
	__set_bit(ABS_DISTANCE, input_dev->absbit);
	__set_bit(ABS_TILT_X, input_dev->absbit);
	__set_bit(ABS_TILT_Y, input_dev->absbit);

	/*  __set_bit(BTN_STYLUS2, input_dev->keybit); */
	/*  __set_bit(ABS_MISC, input_dev->absbit); */

	/*softkey*/
	__set_bit(KEY_RECENT, input_dev->keybit);
	__set_bit(KEY_BACK, input_dev->keybit);

	/* AOP */
	__set_bit(KEY_WAKEUP_UNLOCK, input_dev->keybit);
}

static int wacom_check_emr_prox(struct wacom_g5_callbacks *cb)
{
	struct wacom_i2c *wac = container_of(cb, struct wacom_i2c, callbacks);
	input_info(true, &wac->client->dev, "%s\n", __func__);

	return wac->pen_prox;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void wacom_i2c_early_suspend(struct early_suspend *h)
{
	struct wacom_i2c *wac_i2c =
	    container_of(h, struct wacom_i2c, early_suspend);
	input_info(true, &wac_i2c->client->dev, "%s.\n", __func__);
	wacom_power_off(wac_i2c);
}

static void wacom_i2c_late_resume(struct early_suspend *h)
{
	struct wacom_i2c *wac_i2c =
		container_of(h, struct wacom_i2c, early_suspend);

	input_info(true, &wac_i2c->client->dev, "%s.\n", __func__);
	wacom_power_on(wac_i2c);
}
#endif

static void wacom_i2c_resume_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
			container_of(work, struct wacom_i2c, resume_work.work);
	u8 irq_state = 0;
	int ret = 0;

	input_info(true, &wac_i2c->client->dev, "%s start, ps:%d pen %s, reset_flag(%d)\n",
				__func__, wac_i2c->battery_saving_mode,
				(wac_i2c->function_result & EPEN_EVENT_PEN_INOUT) ? "out" : "in",
				wac_i2c->reset_flag);

	/*add and consider lpm condition. */
	if (wac_i2c->epen_blocked) {
		input_info(true, &wac_i2c->client->dev,
				"%s : epen blocked. keep Garage only mode\n", __func__);
	} else {
		if (wac_i2c->battery_saving_mode && !(wac_i2c->function_result & EPEN_EVENT_PEN_INOUT)) {
			input_info(true, &wac_i2c->client->dev,
					"ps on & pen in : keep current status(garage only)\n");
		} else {
			if(wac_i2c->survey_cmd_state == true){
				input_err(true, &wac_i2c->client->dev, "survey_cmd_state(true)-exit!\n");
				wacom_i2c_exit_survey_mode(wac_i2c);
			} else {
				input_info(true, &wac_i2c->client->dev, "survey_cmd_state(false)\n"); 
			}
		}

#ifdef WACOM_USE_SURVEY_MODE
		if(wac_i2c->reset_flag) {
			input_info(true, &wac_i2c->client->dev, "%s : IC reset start\n", __func__);
			wac_i2c->reset_flag = false;

			wacom_enable_irq(wac_i2c, false);
			wacom_enable_pdct_irq(wac_i2c, false);

			wacom_suspend_hw();
			msleep(120);
			wacom_resume_hw();
			msleep(150);

			wac_i2c->pen_pdct = gpio_get_value(wac_i2c->pdata->gpios[I_PDCT]);
			input_info(true, &wac_i2c->client->dev, "%s : IC reset end, pdct(%d)\n", __func__, wac_i2c->pen_pdct);

			if(wac_i2c->pen_pdct) wac_i2c->function_result &= ~EPEN_EVENT_PEN_INOUT;
			else wac_i2c->function_result |= EPEN_EVENT_PEN_INOUT;

			if (wac_i2c->battery_saving_mode && !(wac_i2c->function_result & EPEN_EVENT_PEN_INOUT)) {
				wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_ONLY);
				wac_i2c->survey_mode = true;
			} else {
				wac_i2c->survey_mode = false;
			}

			wacom_enable_irq(wac_i2c, true);
			wacom_enable_pdct_irq(wac_i2c, true);
		}
#endif
	}

	if (wac_i2c->wcharging_mode)
		wacom_i2c_set_sense_mode(wac_i2c);

	if(wac_i2c->tsp_noise_mode < 0){
		wac_i2c->tsp_noise_mode = set_spen_mode(0);
	}

	irq_state = wac_i2c->pdata->get_irq_state();

	if (unlikely(irq_state)) {
		u8 data[COM_COORD_NUM+1] = {0,};
		input_info(true, &wac_i2c->client->dev, "%s: irq was enabled\n", __func__);
		ret = wacom_i2c_recv(wac_i2c, data, COM_COORD_NUM+1, false);
		if (ret < 0) {
			input_err(true, &wac_i2c->client->dev, "%s failed to read i2c.L%d\n",
						__func__, __LINE__);
		}
		input_info(true, &wac_i2c->client->dev, "%x %x %x %x %x, %x %x %x %x %x, %x %x %x\n",
			data[0], data[1], data[2], data[3], data[4], data[5], data[6],
			data[7], data[8], data[9], data[10], data[11], data[12]);
	}

#ifndef WACOM_USE_SURVEY_MODE
	wacom_enable_irq(wac_i2c, true);
#endif
	ret = gpio_get_value(wac_i2c->pdata->gpios[I_PDCT]);

	input_info(true, &wac_i2c->client->dev,
			"%s : survey protect(43) - [i(%d) p(%d) set(%x) ret(%x) blocked(%d)]\n",
			__func__, irq_state, ret, wac_i2c->function_set,
			wac_i2c->function_result, wac_i2c->epen_blocked);
}

#ifdef LCD_FREQ_SYNC
#define SYSFS_WRITE_LCD	"/sys/class/lcd/panel/ldi_fps"
static void wacom_i2c_sync_lcd_freq(struct wacom_i2c *wac_i2c)
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *write_node;
	char freq[12] = {0, };
	int lcd_freq = wac_i2c->lcd_freq;

	mutex_lock(&wac_i2c->freq_write_lock);

	sprintf(freq, "%d", lcd_freq);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	write_node = filp_open(SYSFS_WRITE_LCD, /*O_RDONLY*/O_RDWR | O_SYNC, 0664);
	if (IS_ERR(write_node)) {
		ret = PTR_ERR(write_node);
		input_err(true, &wac_i2c->client->dev,
			"%s: node file open fail, %d\n", __func__, ret);
		goto err_open_node;
	}

	ret = write_node->f_op->write(write_node, (char __user *)freq, strlen(freq), &write_node->f_pos);
	if (ret != strlen(freq)) {
		input_err(true, &wac_i2c->client->dev,
			"%s: Can't write node data\n", __func__);
	}
	input_info(true, &wac_i2c->client->dev, "%s write freq %s\n", __func__, freq);

	filp_close(write_node, current->files);

err_open_node:
	set_fs(old_fs);
//err_read_framerate:
	mutex_unlock(&wac_i2c->freq_write_lock);

	schedule_delayed_work(&wac_i2c->lcd_freq_done_work, HZ * 5);
}

static void wacom_i2c_finish_lcd_freq_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, lcd_freq_done_work.work);

	wac_i2c->lcd_freq_wait = false;

	input_info(true, &wac_i2c->client->dev, "%s\n", __func__);
}

static void wacom_i2c_lcd_freq_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, lcd_freq_work);

	wacom_i2c_sync_lcd_freq(wac_i2c);
}
#endif

#ifdef WACOM_USE_SOFTKEY_BLOCK
static void wacom_i2c_block_softkey_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, softkey_block_work.work);

	wac_i2c->block_softkey = false;
}
#endif


#ifndef USE_OPEN_CLOSE
#ifdef CONFIG_HAS_EARLYSUSPEND
#define wacom_i2c_suspend	NULL
#define wacom_i2c_resume	NULL

#else
static int wacom_i2c_suspend(struct device *dev)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	wac_i2c->pwr_flag = false;
	if (wac_i2c->input_dev->users)
		wacom_power_off(wac_i2c);

	return 0;
}

static int wacom_i2c_resume(struct device *dev)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	wac_i2c->pwr_flag = true;
	if (wac_i2c->input_dev->users)
		wacom_power_on(wac_i2c);

	return 0;
}
static SIMPLE_DEV_PM_OPS(wacom_pm_ops, wacom_i2c_suspend, wacom_i2c_resume);
#endif
#endif

int load_fw_built_in(struct wacom_i2c *wac_i2c)
{
	int retry = 3;
	int ret;

	if (wac_i2c->pdata->fw_path == NULL) {
		input_err(true, wac_i2c->dev, 
				"Unable to open firmware. fw_path is NULL \n");
		return -1;
	}

	while (retry--) {
		ret =
			request_firmware(&wac_i2c->firm_data, wac_i2c->pdata->fw_path,
			&wac_i2c->client->dev);
		if (ret < 0) {
			input_err(true, &wac_i2c->client->dev, 
					"Unable to open firmware. ret %d retry %d\n",
					ret, retry);
			continue;
		}
		break;
	}

	if (ret < 0)
		return ret;

	wac_i2c->fw_img = (struct fw_image *)wac_i2c->firm_data->data;

	return ret;
}

int load_fw_sdcard(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;
	unsigned int nSize;
	unsigned long nSize2;
	u8 *ums_data;

	nSize = WACOM_FW_SIZE;
	nSize2 = nSize + sizeof(struct fw_image);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(WACOM_FW_PATH_SDCARD, O_RDONLY, S_IRUSR);

	if (IS_ERR(fp)) {
		input_err(true, &client->dev,  "failed to open %s.\n", WACOM_FW_PATH_SDCARD);
		ret = -ENOENT;
		set_fs(old_fs);
		return ret;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	input_info(true, &client->dev, 
		"start, file path %s, size %ld Bytes\n",
		WACOM_FW_PATH_SDCARD, fsize);

	if ((fsize != nSize) && (fsize != nSize2)) {
		input_err(true, &client->dev,  "UMS firmware size is different\n");
		ret = -EFBIG;
		goto size_error;
	}

	ums_data = kmalloc(fsize, GFP_KERNEL);
	if (IS_ERR(ums_data)) {
		input_err(true, &client->dev, 
			"%s, kmalloc failed\n", __func__);
		ret = -EFAULT;
		goto malloc_error;
	}

	nread = vfs_read(fp, (char __user *)ums_data,
		fsize, &fp->f_pos);
	input_info(true, &client->dev, "nread %ld Bytes\n", nread);
	if (nread != fsize) {
		input_err(true, &client->dev, 
			"failed to read firmware file, nread %ld Bytes\n",
			nread);
		ret = -EIO;
		kfree(ums_data);
		goto read_err;
	}

	filp_close(fp, current->files);
	set_fs(old_fs);

	wac_i2c->fw_img = (struct fw_image *)ums_data;

	return 0;

read_err:
malloc_error:
size_error:
	filp_close(fp, current->files);
	set_fs(old_fs);
	return ret;
}

int wacom_i2c_load_fw(struct wacom_i2c *wac_i2c, u8 fw_path)
{
	int ret = 0;
	struct fw_image *fw_img;

	switch (fw_path) {
	case FW_BUILT_IN:
		ret = load_fw_built_in(wac_i2c);
		break;
	case FW_IN_SDCARD:
		ret = load_fw_sdcard(wac_i2c);
		break;
	default:
		input_info(true, &wac_i2c->client->dev, "unknown path(%d)\n", fw_path);
		goto err_load_fw;
	}

	if (ret < 0)
		goto err_load_fw;

	fw_img = wac_i2c->fw_img;

	/* header check */
	if (fw_img->hdr_ver == 1 && fw_img->hdr_len == sizeof(struct fw_image)) {
		fw_data = (u8 *)fw_img->data;
		if (fw_path == FW_BUILT_IN) {
			fw_ver_file = fw_img->fw_ver1;
			memcpy(fw_chksum, fw_img->checksum, 5);
		}
	} else {
		input_info(true, &wac_i2c->client->dev, "no hdr\n");
		fw_data = (u8 *)fw_img;
	}

	return ret;

err_load_fw:
	fw_data = NULL;
	return ret;
}

void wacom_i2c_unload_fw(struct wacom_i2c *wac_i2c)
{
	switch (wac_i2c->fw_path) {
	case FW_BUILT_IN:
		release_firmware(wac_i2c->firm_data);
		break;
	case FW_IN_SDCARD:
		kfree(wac_i2c->fw_img);
		break;
	default:
		break;
	}

	wac_i2c->fw_img = NULL;
	wac_i2c->fw_path = FW_NONE;
	wac_i2c->firm_data = NULL;
	fw_data = NULL;
}

int wacom_fw_update(struct wacom_i2c *wac_i2c, u8 fw_path, bool bforced)
{
	struct i2c_client *client = wac_i2c->client;
	u32 fw_ver_ic = wac_i2c->wac_feature->fw_version;
	int ret;

	input_info(true, &client->dev, "%s\n", __func__);

	if (wake_lock_active(&wac_i2c->fw_wakelock)) {
		input_info(true, &client->dev, "update is already running. pass\n");
		return 0;
	}

	mutex_lock(&wac_i2c->update_lock);
	wacom_enable_irq(wac_i2c, false);

	ret = wacom_i2c_load_fw(wac_i2c, fw_path);
	if (ret < 0) {
		input_info(true, &client->dev, "failed to load fw data\n");
		wac_i2c->wac_feature->update_status = FW_UPDATE_FAIL;
		goto err_update_load_fw;
	}
	wac_i2c->fw_path = fw_path;

	/* firmware info */
	input_info(true, &client->dev, "wacom fw ver : 0x%x, new fw ver : 0x%x\n",
		wac_i2c->wac_feature->fw_version, fw_ver_file);

	if (!bforced) {
		if (fw_ver_ic == fw_ver_file) {
			input_info(true, &client->dev, "pass fw update\n");
			wac_i2c->do_crc_check = true;
			/*goto err_update_fw;*/
		} else if (fw_ver_ic > fw_ver_file)
			goto out_update_fw;

		/* ic < file then update */
	}

	queue_work(wac_i2c->fw_wq, &wac_i2c->update_work);
	mutex_unlock(&wac_i2c->update_lock);

	return 0;

 out_update_fw:
	wacom_i2c_unload_fw(wac_i2c);
 err_update_load_fw:
	wacom_enable_irq(wac_i2c, true);
	mutex_unlock(&wac_i2c->update_lock);

	return 0;
}

static void wacom_i2c_update_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, update_work);
	struct i2c_client *client = wac_i2c->client;
	struct wacom_features *feature = wac_i2c->wac_feature;
	int ret = 0;
#ifdef WACOM_USE_I2C_GPIO
	int retry = 10;
#else
	int retry = 3;
#endif

	if (wac_i2c->fw_path == FW_NONE)
		goto end_fw_update;

	wake_lock(&wac_i2c->fw_wakelock);

	/* CRC Check */
	if (wac_i2c->do_crc_check) {
		wac_i2c->do_crc_check = false;
		ret = wacom_checksum(wac_i2c);
		if (ret)
			goto err_update_fw;
		input_info(true, &client->dev, "crc err, do update\n");
	}

	feature->update_status = FW_UPDATE_RUNNING;

	while (retry--) {
		ret = wacom_i2c_flash(wac_i2c);
		if (ret) {
			input_info(true, &client->dev, "failed to flash fw(%d)\n", ret);
#ifdef WACOM_USE_I2C_GPIO
			msleep(500);
#endif
			continue;
		}
		break;
	}
	if (ret) {
		feature->update_status = FW_UPDATE_FAIL;
		feature->fw_version = 0;
		goto err_update_fw;
	}

	ret = wacom_i2c_query(wac_i2c);
	if (ret < 0) {
		input_info(true, &client->dev, "failed to query to IC(%d)\n", ret);
		feature->update_status = FW_UPDATE_FAIL;
		goto err_update_fw;
	}

	feature->update_status = FW_UPDATE_PASS;

 err_update_fw:
	wake_unlock(&wac_i2c->fw_wakelock);
 end_fw_update:
	wacom_i2c_unload_fw(wac_i2c);
	wacom_enable_irq(wac_i2c, true);

	return ;
}

static ssize_t epen_firm_update_status_show(struct device *dev,
struct device_attribute *attr,
	char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int status = wac_i2c->wac_feature->update_status;

	input_info(true, &wac_i2c->client->dev, "%s:(%d)\n", __func__, status);

	if (status == FW_UPDATE_PASS)
		return sprintf(buf, "PASS\n");
	else if (status == FW_UPDATE_RUNNING)
		return sprintf(buf, "DOWNLOADING\n");
	else if (status == FW_UPDATE_FAIL)
		return sprintf(buf, "FAIL\n");
	else
		return 0;
}

static ssize_t epen_firm_version_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	input_info(true, &wac_i2c->client->dev,  "%s: 0x%x|0x%X\n", __func__,
		wac_i2c->wac_feature->fw_version, fw_ver_file);

	return sprintf(buf, "%04X\t%04X\n",
		wac_i2c->wac_feature->fw_version,
		fw_ver_file);
}

#if defined(WACOM_IMPORT_FW_ALGO)
#if 0
static ssize_t epen_tuning_version_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_HA
	tuning_version = "0000";
	tuning_model = "N900";
#endif
	input_info(true, &wac_i2c->client->dev,  "%s: %s\n", __func__,
		tuning_version);

	return sprintf(buf, "%s_%s\n",
		tuning_model,
		tuning_version);
}
#endif

static ssize_t epen_rotation_store(struct device *dev,
struct device_attribute *attr,
	const char *buf, size_t count)
{
	static bool factory_test;
	static unsigned char last_rotation;
	unsigned int val;

	sscanf(buf, "%u", &val);

	/* Fix the rotation value to 0(Portrait) when factory test(15 mode) */
	if (val == 100 && !factory_test) {
		factory_test = true;
		screen_rotate = 0;
		input_info(true, &wac_i2c->client->dev, "%s, enter factory test mode\n",
			__func__);
	} else if (val == 200 && factory_test) {
		factory_test = false;
		screen_rotate = last_rotation;
		input_info(true, &wac_i2c->client->dev, "%s, exit factory test mode\n",
			__func__);
	}

	/* Framework use index 0, 1, 2, 3 for rotation 0, 90, 180, 270 */
	/* Driver use same rotation index */
	if (val >= 0 && val <= 3) {
		if (factory_test)
			last_rotation = val;
		else
			screen_rotate = val;
	}

	/* 0: Portrait 0, 1: Landscape 90, 2: Portrait 180 3: Landscape 270 */
	input_info(true, &wac_i2c->client->dev, "%s: rotate=%d\n", __func__, screen_rotate);

	return count;
}

static ssize_t epen_hand_store(struct device *dev,
struct device_attribute *attr, const char *buf,
	size_t count)
{
	unsigned int val;

	sscanf(buf, "%u", &val);

	if (val == 0 || val == 1)
		user_hand = (unsigned char)val;

	/* 0:Left hand, 1:Right Hand */
	input_info(true, &wac_i2c->client->dev, "%s: hand=%u\n", __func__, user_hand);

	return count;
}
#endif

static ssize_t epen_firmware_update_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	u8 fw_path = FW_NONE;

	input_info(true, &wac_i2c->client->dev,  "%s\n", __func__);

	switch (*buf) {
	case 'i':
	case 'I':
		fw_path = FW_IN_SDCARD;
		break;
	case 'k':
	case 'K':
		fw_path = FW_BUILT_IN;
		break;
	default:
		input_err(true, &wac_i2c->client->dev,  "wrong parameter\n");
		return count;
	}

	wacom_fw_update(wac_i2c, fw_path, true);

	return count;
}

static ssize_t epen_reset_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val;

	sscanf(buf, "%d", &val);

	if (val == 1) {
		wacom_enable_irq(wac_i2c, false);

		/* Reset IC */
		/*wacom_i2c_reset_hw(wac_i2c->pdata);*/
		wacom_i2c_reset_hw(wac_i2c);
		/* I2C Test */
		wacom_i2c_query(wac_i2c);

		wacom_enable_irq(wac_i2c, true);

		input_info(true, &wac_i2c->client->dev,  "%s, result %d\n", __func__,
		       wac_i2c->query_status);
	}

	return count;
}

static ssize_t epen_reset_result_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	if (wac_i2c->query_status) {
		input_info(true, &wac_i2c->client->dev, "%s, PASS\n", __func__);
		return sprintf(buf, "PASS\n");
	} else {
		input_info(true, &wac_i2c->client->dev, "%s, FAIL\n", __func__);
		return sprintf(buf, "FAIL\n");
	}
}

#ifdef WACOM_USE_AVE_TRANSITION
static ssize_t epen_ave_store(struct device *dev,
struct device_attribute *attr,
	const char *buf, size_t count)
{
	int v1, v2, v3, v4, v5;
	int height;

	sscanf(buf, "%d%d%d%d%d%d", &height, &v1, &v2, &v3, &v4, &v5);

	if (height < 0 || height > 2) {
		input_info(true, &wac_i2c->client->dev, "Height err %d\n", height);
		return count;
	}

	g_aveLevel_C[height] = v1;
	g_aveLevel_X[height] = v2;
	g_aveLevel_Y[height] = v3;
	g_aveLevel_Trs[height] = v4;
	g_aveLevel_Cor[height] = v5;

	input_info(true, &wac_i2c->client->dev,  "%s, v1 %d v2 %d v3 %d v4 %d\n",
		__func__, v1, v2, v3, v4);

	return count;
}

static ssize_t epen_ave_result_show(struct device *dev,
struct device_attribute *attr,
	char *buf)
{
	input_info(true, &wac_i2c->client->dev,  "%s\n%d %d %d %d\n"
		"%d %d %d %d\n%d %d %d %d\n",
		__func__,
		g_aveLevel_C[0], g_aveLevel_X[0],
		g_aveLevel_Y[0], g_aveLevel_Trs[0],
		g_aveLevel_C[1], g_aveLevel_X[1],
		g_aveLevel_Y[1], g_aveLevel_Trs[1],
		g_aveLevel_C[2], g_aveLevel_X[2],
		g_aveLevel_Y[2], g_aveLevel_Trs[2]);
	return sprintf(buf, "%d %d %d %d\n%d %d %d %d\n%d %d %d %d\n",
		g_aveLevel_C[0], g_aveLevel_X[0],
		g_aveLevel_Y[0], g_aveLevel_Trs[0],
		g_aveLevel_C[1], g_aveLevel_X[1],
		g_aveLevel_Y[1], g_aveLevel_Trs[1],
		g_aveLevel_C[2], g_aveLevel_X[2],
		g_aveLevel_Y[2], g_aveLevel_Trs[2]);
}
#endif

static ssize_t epen_checksum_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val;

	sscanf(buf, "%d", &val);

	if (val != 1) {
		input_err(true, &wac_i2c->client->dev, " wrong cmd %d\n", val);
		return count;
	}

	wacom_enable_irq(wac_i2c, false);
	wacom_checksum(wac_i2c);
	wacom_enable_irq(wac_i2c, true);

	input_info(true, &wac_i2c->client->dev,  "%s, result %d\n",
		__func__, wac_i2c->checksum_result);

	return count;
}

static ssize_t epen_checksum_result_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	if (wac_i2c->checksum_result) {
		input_info(true, &wac_i2c->client->dev, "checksum, PASS\n");
		return sprintf(buf, "PASS\n");
	} else {
		input_info(true, &wac_i2c->client->dev, "checksum, FAIL\n");
		return sprintf(buf, "FAIL\n");
	}
}

static int wacom_open_test(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;
	u8 cmd = 0;
	u8 buf[4] = {0,};
	int ret = 0, retry = 10;

	cmd = WACOM_I2C_STOP;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1, false);
	if (ret <= 0) {
		input_err(true, &client->dev,  "failed to send stop command\n");
		return -1;
	}

	usleep_range(500, 500);

	cmd = WACOM_I2C_GRID_CHECK;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1, false);
	if (ret <= 0) {
		input_err(true, &client->dev,  "failed to send stop command\n");
		return -1;
	}

	msleep(2000);

	cmd = WACOM_STATUS;
	do {
		input_info(true, &client->dev, "read status, retry %d\n", retry);
		ret = wacom_i2c_send(wac_i2c, &cmd, 1, false);
		if (ret != 1) {
			input_err(true, &client->dev, "failed to send cmd(ret:%d)\n", ret);
			continue;
		}
		usleep_range(500, 500);
		ret = wacom_i2c_recv(wac_i2c, buf, 4, false);
		if (ret != 4) {
			input_err(true, &client->dev, "failed to recv data(ret:%d)\n", ret);
			continue;
		}

		/*
		*	status value
		*	0 : data is not ready
		*	1 : PASS
		*	2 : Fail (coil function error)
		*	3 : Fail (All coil function error)
		*/
		if (buf[0] == 1) {
			input_info(true, &client->dev, "Pass\n");
			break;
		}

		msleep(50);
	} while (retry--);

	if (ret > 0)
		wac_i2c->connection_check = (1 == buf[0]);
	else {
		wac_i2c->connection_check = false;
		return -1;
	}

	wac_i2c->fail_channel = buf[1];
	wac_i2c->min_adc_val = buf[2]<<8 | buf[3];

	input_info(true, &client->dev, 
	       "epen_connection : %s buf[0]:%d, buf[1]:%d, buf[2]:%d, buf[3]:%d\n",
	       wac_i2c->connection_check ? "Pass" : "Fail", buf[0], buf[1], buf[2], buf[3]);

	cmd = WACOM_I2C_STOP;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1, false);
	if (ret <= 0) {
		input_err(true, &client->dev,  "failed to send stop cmd for end\n");
		return -2;
	}

	cmd = WACOM_I2C_START;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1, false);
	if (ret <= 0) {
		input_err(true, &client->dev,  "failed to send start cmd for end\n");
		return -2;
	}

	if(retry <= 0)
		return -1;

	return 0;
}

static ssize_t epen_connection_show(struct device *dev,
struct device_attribute *attr,
	char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	struct i2c_client *client = wac_i2c->client;
	int retry = 2;
	int ret,module_ver =1;

	mutex_lock(&wac_i2c->lock);

	input_info(true, &client->dev, "%s\n", __func__);
	wacom_enable_irq(wac_i2c, false);
	wacom_enable_pdct_irq(wac_i2c, false);

	while (retry--) {
		ret = wacom_open_test(wac_i2c);
		if (!ret)
			break;

		input_err(true, &client->dev, "failed. retry %d\n", retry);

		wacom_suspend_hw();
		msleep(120);
		wacom_resume_hw();

		if (ret == -1) {
			msleep(150);
			continue;
		} else if (ret == -2) {
			break;
		}
	}

	wacom_enable_irq(wac_i2c, true);
	wacom_enable_pdct_irq(wac_i2c, true);

	if (false == wac_i2c->power_enable) {
		wac_i2c->pdata->suspend_platform_hw();
		input_info(true, &client->dev, "pwr off\n");
	}
	mutex_unlock(&wac_i2c->lock);

	input_info(true, &client->dev, 
		"connection_check : %d\n",
		wac_i2c->connection_check);

	return sprintf(buf, "%s %d %d %d\n",
		wac_i2c->connection_check ?
		"OK" : "NG",module_ver, wac_i2c->fail_channel,wac_i2c->min_adc_val);
}

static ssize_t epen_saving_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	u8 irq_state = 0;
	int val, ret = 0;
	static int call_count;
	if (call_count++ > 1000) call_count = -10;

#ifdef CONFIG_SEC_FACTORY
	if (sscanf(buf, "%u", &val) == 1) {
		input_info(true, &wac_i2c->client->dev,
				"%s : Not support power saving mode(%d) in Factory Bin\n",
				__func__, val);
	}
	return count;
#endif

	if (sscanf(buf, "%u", &val) == 1)
		wac_i2c->battery_saving_mode = !!val;

	input_info(true, &wac_i2c->client->dev, "%s, ps %d, pen(%s) [%d]\n",
				__func__, wac_i2c->battery_saving_mode,
				(wac_i2c->function_result & EPEN_EVENT_PEN_INOUT) ? "out" : "in", call_count);

	if (!wac_i2c->pwr_flag) {
		input_info(true, &wac_i2c->client->dev, "pass pwr control\n");
		return count;
	}

#ifdef WACOM_USE_SURVEY_MODE
	if (wac_i2c->battery_saving_mode) {
		if (wac_i2c->function_result & EPEN_EVENT_PEN_INOUT) {	//PEN OUT -Notthing to do.
			input_info(true, &wac_i2c->client->dev,
				"ps on & pen out => keep current status(normal)\n");
		} else {					// PEN IN - GRAGE ONLY mode
			input_info(true, &wac_i2c->client->dev,
				"ps on & pen in : set survey mode(garage only)\n");

			wacom_enable_irq(wac_i2c, false);
			
			if (wac_i2c->pen_pressed || wac_i2c->side_pressed
				|| wac_i2c->pen_prox)
				forced_release(wac_i2c);
			if (wac_i2c->soft_key_pressed[0] || wac_i2c->soft_key_pressed[1] )
				forced_release_key(wac_i2c);
			
			wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_ONLY);
			wac_i2c->survey_mode = true;

			irq_state = wac_i2c->pdata->get_irq_state();
			msleep(10);
			wacom_enable_irq(wac_i2c, true);

			/* clear unexpected irq - when select power save mode by pen */
			msleep(30);
			if (unlikely(irq_state)) {
				u8 data[COM_COORD_NUM+1] = {0,};
				input_info(true, &wac_i2c->client->dev, "%s : irq was enabled\n", __func__);
				ret = wacom_i2c_recv(wac_i2c, data, COM_COORD_NUM+1, false);
				if (ret < 0) {
					input_err(true, &wac_i2c->client->dev, "%s failed to read i2c.L%d\n",
								__func__, __LINE__);
				}
			}

			wacom_set_scan_mode(wac_i2c,0);
		}

	} else {
		if (wac_i2c->function_result & EPEN_EVENT_PEN_INOUT) {	//PEN OUT -Notthing to do.
			input_info(true, &wac_i2c->client->dev,
				"ps off & pen out => keep current status(survey:%s)\n",
				wac_i2c->survey_mode ? "on" : "off");
		} else {					// PEN IN - GRAGE AOP mode

			wacom_enable_irq(wac_i2c, false);

			if(!wac_i2c->power_enable){  // lcd off
				input_info(true, &wac_i2c->client->dev,
						"ps off & pen in & survey mode(%d) lcd on(%d)\n",
						wac_i2c->survey_mode,wac_i2c->power_enable);
				if(wac_i2c->survey_cmd_state == true) {
					wacom_i2c_exit_survey_mode(wac_i2c);
					msleep(100);
				}
				if(wac_i2c->function_set & EPEN_SETMODE_AOP)
					wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_AOP);
				else
					wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_ONLY);
				wac_i2c->survey_mode = true;
			} else { // lcd on
				input_info(true, &wac_i2c->client->dev,
						"ps off & pen in & survey mode(%d) lcd on(%d)\n,",
						wac_i2c->survey_mode, wac_i2c->power_enable);
				if(wac_i2c->survey_cmd_state == false) {
					wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_ONLY);
					msleep(100);
				}
				wacom_i2c_exit_survey_mode(wac_i2c);
				wac_i2c->survey_mode = false;
			}

			irq_state = wac_i2c->pdata->get_irq_state();
			msleep(10);
			wacom_enable_irq(wac_i2c, true);

			/* clear unexpected irq - when select power save mode by pen */
			msleep(30);
			if (unlikely(irq_state)) {
				u8 data[COM_COORD_NUM+1] = {0,};
				input_info(true, &wac_i2c->client->dev, "irq was enabled\n");
				ret = wacom_i2c_recv(wac_i2c, data, COM_COORD_NUM+1, false);
				if (ret < 0) {
					input_err(true, &wac_i2c->client->dev, "%s failed to read i2c.L%d\n",
								__func__, __LINE__);
				}
			}
		}
	}
#else
	if (wac_i2c->battery_saving_mode) {
		if (wac_i2c->function_result & EPEN_EVENT_PEN_INOUT)
			wacom_power_off(wac_i2c);
	} else
		wacom_power_on(wac_i2c);
#endif

	return count;
}

static ssize_t epen_aop_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

#if 1	// remove 20160620
	input_info(true, &wac_i2c->client->dev, "%s, Not support AOP\n", __func__);
#else
	int val, aopmode = 0;

	if (sscanf(buf, "%u", &val) == 1)
		aopmode = val;

	if(val) wac_i2c->function_set |= EPEN_SETMODE_AOP;
	else wac_i2c->function_set &= (~EPEN_SETMODE_AOP);

	input_info(true, &wac_i2c->client->dev, "%s, ps %d, aop(%d) set(%x) ret(%x)\n",
				__func__, wac_i2c->battery_saving_mode, aopmode,
				wac_i2c->function_set, wac_i2c->function_result);

	//consider lcd on & off, bacause  setting is too late
	if (wac_i2c->survey_mode) {

		if( (wac_i2c->function_set & EPEN_SETMODE_AOP) && (wac_i2c->function_result & EPEN_EVENT_PEN_INOUT )){
			/* aop on, pen out */
			wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_AOP);
		}
		else if( (wac_i2c->function_set & EPEN_SETMODE_AOP) && !(wac_i2c->function_result & EPEN_EVENT_PEN_INOUT )){
			/* aop on ,  pen in */
			if (wac_i2c->battery_saving_mode)	/* aop on ,  pen in, ps on */
				wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_ONLY);
			else								/* aop on ,  pen in, ps off */
				wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_AOP);
		}
		else
			wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_ONLY);
	}
#endif
	return count;
}

static ssize_t epen_wcharging_mode_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	input_info(true, &wac_i2c->client->dev, "%s: %s\n",
			__func__, !wac_i2c->wcharging_mode ? "NORMAL" : "LOWSENSE");

	return sprintf(buf, "%s\n", !wac_i2c->wcharging_mode ? "NORMAL" : "LOWSENSE");
}

static ssize_t epen_wcharging_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int retval = 0;

	if (sscanf(buf, "%u", &retval) == 1)
		wac_i2c->wcharging_mode = retval;

	input_info(true, &wac_i2c->client->dev, "%s, val %d\n",
		__func__, wac_i2c->wcharging_mode);


	if (!wac_i2c->power_enable) {
		input_err(true, &wac_i2c->client->dev,
				"%s: power off, save & return\n", __func__);
		return count;
	}

	retval = wacom_i2c_set_sense_mode(wac_i2c);
	if (retval < 0)
		input_err(true, &wac_i2c->client->dev,
				"%s: do not set sensitivity mode, %d\n",
				__func__, retval);

	return count;

}

/* epen_disable_mode : Use in Secure mode(TUI) and Factory mode */
void epen_disable_mode(int mode) {
	/* 0 : enable wacom, 1 : disable wacom */
	struct wacom_i2c *wac_i2c = wacom_get_drv_data(NULL);

	input_info(true, &wac_i2c->client->dev, "%s %d\n", __func__, mode);

#ifdef WACOM_USE_SURVEY_MODE
	wacom_enable_irq(wac_i2c, false);
	if (mode) {
		wac_i2c->epen_blocked = true;
		wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_GARAGE_ONLY);
		
	} else {
		if (wac_i2c->power_enable)
			wacom_i2c_exit_survey_mode(wac_i2c);
		else
			input_info(true, &wac_i2c->client->dev,
					"%s: lcd is off now. "
					"disable epen_blocked but not exit survey mode\n",
					__func__);
		wac_i2c->epen_blocked = false;
	}
	wacom_enable_irq(wac_i2c, true);
#else
	if (mode)
		wacom_power_off(wac_i2c);
	else
		wacom_power_on(wac_i2c);
#endif

}
EXPORT_SYMBOL(epen_disable_mode);

static ssize_t epen_disable_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val = 0;
	sscanf(buf, "%u", &val);

	if (val != 0 && val != 1) {
		input_info(true, &wac_i2c->client->dev,
				"%s: not available param %d\n", __func__, val);
		return count;
	}

	input_info(true, &wac_i2c->client->dev, "%s1 %d\n", __func__, val);

	epen_disable_mode(val);

	return count;
}

#if defined(CONFIG_INPUT_BOOSTER)
static ssize_t epen_boost_level(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int level;

	sscanf(buf, "%d", &level);

	if (level < 0 || level >= BOOSTER_LEVEL_MAX) {
		input_err(true, &wac_i2c->client->dev, "err to set boost_level %d\n", level);
		return count;
	}
	change_booster_level_for_pen(level);

	input_info(true, &wac_i2c->client->dev, "%s %d\n", __func__, level);

	return count;

}
#endif

/* firmware update */
static DEVICE_ATTR(epen_firm_update,
			S_IWUSR | S_IWGRP, NULL, epen_firmware_update_store);
/* return firmware update status */
static DEVICE_ATTR(epen_firm_update_status,
			S_IRUGO, epen_firm_update_status_show, NULL);
/* return firmware version */
static DEVICE_ATTR(epen_firm_version, S_IRUGO, epen_firm_version_show, NULL);
#if defined(WACOM_IMPORT_FW_ALGO)
/* return tuning data version */
#if 0
static DEVICE_ATTR(epen_tuning_version, S_IRUGO,
			epen_tuning_version_show, NULL);
#endif
/* screen rotation */
static DEVICE_ATTR(epen_rotation, S_IWUSR | S_IWGRP, NULL, epen_rotation_store);
/* hand type */
static DEVICE_ATTR(epen_hand, S_IWUSR | S_IWGRP, NULL, epen_hand_store);
#endif

/* For SMD Test */
static DEVICE_ATTR(epen_reset, S_IWUSR | S_IWGRP, NULL, epen_reset_store);
static DEVICE_ATTR(epen_reset_result,
			S_IRUSR | S_IRGRP, epen_reset_result_show, NULL);

/* For SMD Test. Check checksum */
static DEVICE_ATTR(epen_checksum, S_IWUSR | S_IWGRP, NULL, epen_checksum_store);
static DEVICE_ATTR(epen_checksum_result, S_IRUSR | S_IRGRP,
			epen_checksum_result_show, NULL);
#ifdef CONFIG_INPUT_BOOSTER
static DEVICE_ATTR(boost_level, S_IWUSR | S_IWGRP, NULL, epen_boost_level);
#endif

#ifdef WACOM_USE_AVE_TRANSITION
static DEVICE_ATTR(epen_ave, S_IWUSR | S_IWGRP, NULL, epen_ave_store);
static DEVICE_ATTR(epen_ave_result, S_IRUSR | S_IRGRP,
	epen_ave_result_show, NULL);
#endif

static DEVICE_ATTR(epen_connection,
			S_IRUGO, epen_connection_show, NULL);

static DEVICE_ATTR(epen_saving_mode,
			S_IWUSR | S_IWGRP, NULL, epen_saving_mode_store);

static DEVICE_ATTR(epen_aop_mode,
			S_IWUSR | S_IWGRP, NULL, epen_aop_mode_store);

static DEVICE_ATTR(epen_wcharging_mode,
			S_IRUGO | S_IWUSR | S_IWGRP, epen_wcharging_mode_show, epen_wcharging_mode_store);

static DEVICE_ATTR(epen_disable_mode,
			S_IWUSR | S_IWGRP, NULL, epen_disable_mode_store);

static struct attribute *epen_attributes[] = {
	&dev_attr_epen_firm_update.attr,
	&dev_attr_epen_firm_update_status.attr,
	&dev_attr_epen_firm_version.attr,
#if defined(WACOM_IMPORT_FW_ALGO)
#if 0
	&dev_attr_epen_tuning_version.attr,
#endif
	&dev_attr_epen_rotation.attr,
	&dev_attr_epen_hand.attr,
#endif
	&dev_attr_epen_reset.attr,
	&dev_attr_epen_reset_result.attr,
	&dev_attr_epen_checksum.attr,
	&dev_attr_epen_checksum_result.attr,
#ifdef WACOM_USE_AVE_TRANSITION
	&dev_attr_epen_ave.attr,
	&dev_attr_epen_ave_result.attr,
#endif
	&dev_attr_epen_connection.attr,
	&dev_attr_epen_saving_mode.attr,
	&dev_attr_epen_aop_mode.attr,
	&dev_attr_epen_wcharging_mode.attr,
	&dev_attr_epen_disable_mode.attr,
#ifdef CONFIG_INPUT_BOOSTER
	&dev_attr_boost_level.attr,
#endif
	NULL,
};

static struct attribute_group epen_attr_group = {
	.attrs = epen_attributes,
};

static int wacom_i2c_remove(struct i2c_client *client)
{
	struct wacom_i2c *wac_i2c = i2c_get_clientdata(client);

	input_info(true, &wac_i2c->client->dev, "%s called!\n", __func__);

	input_unregister_device(wac_i2c->input_dev);
	wacom_power_off(wac_i2c);

	disable_irq(wac_i2c->irq);
	disable_irq(wac_i2c->irq_pdct);
	free_irq(wac_i2c->irq, wac_i2c);
	free_irq(wac_i2c->irq_pdct, wac_i2c);

	wac_i2c->pdata->suspend_platform_hw();

	destroy_workqueue(wac_i2c->fw_wq);
	wake_lock_destroy(&wac_i2c->det_wakelock);
	wake_lock_destroy(&wac_i2c->fw_wakelock);
	mutex_destroy(&wac_i2c->irq_lock);
	mutex_destroy(&wac_i2c->update_lock);
	mutex_destroy(&wac_i2c->lock);

	sysfs_delete_link(&wac_i2c->dev->kobj, &wac_i2c->input_dev->dev.kobj, "input");
	sysfs_remove_group(&wac_i2c->dev->kobj, &epen_attr_group);
	sec_device_destroy(wac_i2c->dev->devt);

	wac_i2c->input_dev = NULL;
	wac_i2c = NULL;
	kfree(wac_i2c);

	return 0;
}

static void wacom_init_abs_params(struct wacom_i2c *wac_i2c)
{
	int min_x, min_y;
	int max_x, max_y;
	int pressure;

	min_x = wac_i2c->pdata->min_x;
	max_x = wac_i2c->pdata->max_x;
	min_y = wac_i2c->pdata->min_y;
	max_y = wac_i2c->pdata->max_y;
	pressure = wac_i2c->pdata->max_pressure;

	if (wac_i2c->pdata->xy_switch) {
		input_set_abs_params(wac_i2c->input_dev, ABS_X, min_y,
			max_y, 4, 0);
		input_set_abs_params(wac_i2c->input_dev, ABS_Y, min_x,
			max_x, 4, 0);
	} else {
		input_set_abs_params(wac_i2c->input_dev, ABS_X, min_x,
			max_x, 4, 0);
		input_set_abs_params(wac_i2c->input_dev, ABS_Y, min_y,
			max_y, 4, 0);
	}
	input_set_abs_params(wac_i2c->input_dev, ABS_PRESSURE, 0,
		pressure, 0, 0);
	input_set_abs_params(wac_i2c->input_dev, ABS_DISTANCE, 0,
		1024, 0, 0);
	input_set_abs_params(wac_i2c->input_dev, ABS_TILT_X, -63,
		63, 0, 0);
	input_set_abs_params(wac_i2c->input_dev, ABS_TILT_Y, -63,
		63, 0, 0);
}

#ifdef WACOM_IMPORT_FW_ALGO
static void wacom_init_fw_algo(struct wacom_i2c *wac_i2c)
{
#if defined(CONFIG_MACH_T0)
	int digitizer_type = 0;

	/*Set data by digitizer type*/
	digitizer_type = wacom_i2c_get_digitizer_type();

	if (digitizer_type == EPEN_DTYPE_B746) {
		input_info(true, &wac_i2c->client->dev, "Use Box filter\n");
		wac_i2c->use_aveTransition = true;
	} else if (digitizer_type == EPEN_DTYPE_B713) {
		input_info(true, &wac_i2c->client->dev, "Reset tilt for B713\n");

		/*Change tuning version for B713*/
		tuning_version = tuning_version_B713;

		memcpy(tilt_offsetX, tilt_offsetX_B713, sizeof(tilt_offsetX));
		memcpy(tilt_offsetY, tilt_offsetY_B713, sizeof(tilt_offsetY));
	} else if (digitizer_type == EPEN_DTYPE_B660) {
		input_info(true, &wac_i2c->client->dev, "Reset tilt and origin for B660\n");

		origin_offset[0] = EPEN_B660_ORG_X;
		origin_offset[1] = EPEN_B660_ORG_Y;
		memset(tilt_offsetX, 0, sizeof(tilt_offsetX));
		memset(tilt_offsetY, 0, sizeof(tilt_offsetY));
		wac_i2c->use_offset_table = false;
	}

	/*Set switch type*/
	wac_i2c->invert_pen_insert = wacom_i2c_invert_by_switch_type();
#endif

}
#endif

#ifdef CONFIG_OF
static int  wacom_request_gpio(struct i2c_client *client, struct wacom_g5_platform_data *pdata)
{
	int ret;
	int i;
	char name[16] = {0, };

	for (i = 0 ; i < WACOM_GPIO_CNT; ++i) {
		if (!pdata->gpios[i])
			continue;
		if (pdata->boot_on_ldo) {
			if (i == I_FWE)
				continue;
		}
		sprintf(name, "wacom_%s", gpios_name[i]);
		ret = gpio_request(pdata->gpios[i], name);
		if (ret) {
			input_err(true, &client->dev, "%s: unable to request %s [%d]\n",
				__func__, name, pdata->gpios[i]);
			return ret;
		}
	}

	return 0;
}

static int wacom_parse_dt(struct i2c_client *client, struct wacom_g5_platform_data *pdata)
{
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	int ret;
	int i;
	char name[20] = {0, };
	u32 tmp[5] = {0, };

	if (!np)
		return -ENOENT;

	if (ARRAY_SIZE(gpios_name) != WACOM_GPIO_CNT)
		input_err(true, dev, "array size error, gpios_name\n");

	for (i = 0 ; i < WACOM_GPIO_CNT; ++i) {
		sprintf(name, "wacom,%s-gpio", gpios_name[i]);

		if (of_find_property(np, name, NULL)) {
			pdata->gpios[i] = of_get_named_gpio_flags(np, name,
				0, &pdata->flag_gpio[i]);

			/*input_info(true, &wac_i2c->client->dev, "%s:%d\n", gpios_name[i], pdata->gpios[i]);*/

			if (pdata->gpios[i] < 0) {
				input_err(true, dev, "failed to get gpio %s, %d\n", name, pdata->gpios[i]);
				pdata->gpios[i] = 0;
			}
		} else {
			input_info(true, dev, "theres no prop %s\n", name);
			pdata->gpios[i] = 0;
		}
	}

	/* get features */
	ret = of_property_read_u32(np, "wacom,irq_type", tmp);
	if (ret) {
		input_err(true, dev, "failed to read trigger type %d\n", ret);
		return -EINVAL;
	}
	pdata->irq_type = tmp[0];

	ret = of_property_read_u32_array(np, "wacom,origin", pdata->origin, 2);
	if (ret) {
		input_err(true, dev, "failed to read origin %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "wacom,max_coords", tmp, 2);
	if (ret) {
		input_err(true, dev, "failed to read max coords %d\n", ret);
		return -EINVAL;
	}
	pdata->max_x = tmp[0];
	pdata->max_y = tmp[1];

	ret = of_property_read_u32(np, "wacom,max_pressure", tmp);
	if (ret) {
		input_err(true, dev, "failed to read max pressure %d\n", ret);
		return -EINVAL;
	}
	pdata->max_pressure = tmp[0];

	ret = of_property_read_u32_array(np, "wacom,invert", tmp, 3);
	if (ret) {
		input_err(true, dev, "failed to read inverts %d\n", ret);
		return -EINVAL;
	}
	pdata->x_invert = tmp[0];
	pdata->y_invert = tmp[1];
	pdata->xy_switch = tmp[2];

	ret = of_property_read_u32(np, "wacom,ic_type", &pdata->ic_type);
	if (ret) {
		input_err(true, dev, "failed to read ic_type %d\n", ret);
		pdata->ic_type = WACOM_IC_9012;
	}

	ret = of_property_read_string(np, "wacom,fw_path", (const char **)&pdata->fw_path);
	if (ret) {
		input_err(true, dev, "failed to read fw_path %d\n", ret);
	}

	ret = of_property_read_string_index(np, "wacom,project_name", 0, (const char **)&pdata->project_name);
	if (ret)
		input_err(true, dev, "failed to read project name %d\n", ret);
	ret = of_property_read_string_index(np, "wacom,project_name", 1, (const char **)&pdata->model_name);
	if (ret)
		input_err(true, dev, "failed to read model name %d\n", ret);

	if (of_find_property(np, "wacom,boot_on_ldo", NULL)) {
		ret = of_property_read_u32(np, "wacom,boot_on_ldo", &pdata->boot_on_ldo);
		if (ret) {
			input_err(true, dev, "failed to read boot_on_ldo %d\n", ret);
			pdata->boot_on_ldo = 0;
		}
	}

	if (of_find_property(np, "wacom,use_query_cmd", NULL)) {
		pdata->use_query_cmd = true;
		input_info(true, dev, "use query cmd\n");
	}
	input_info(true, dev, "int type %d\n", pdata->irq_type);

	return 0;
}
#endif

static int wacom_i2c_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct wacom_g5_platform_data *pdata = client->dev.platform_data;
	struct wacom_i2c *wac_i2c;
	struct input_dev *input;
	int ret = 0;
	bool bforced = false;

	/* init platform data */
	if (pdata == NULL) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			input_err(true, &client->dev, "failed to allocate platform data\n");
			return -EINVAL;
		}
		client->dev.platform_data = pdata;

		ret =  wacom_parse_dt(client, pdata);
		if (ret) {
			input_err(true, &client->dev, "failed to parse dt\n");
			return -EINVAL;
		}

		ret = wacom_request_gpio(client, pdata);
		if (ret) {
			input_err(true, &client->dev, "failed to request gpio\n");
			return -EINVAL;
		}
	}

	/*Check I2C functionality */
	ret = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (!ret) {
		input_err(true, &client->dev, "No I2C functionality found\n");
		ret = -ENODEV;
		goto err_i2c_fail;
	}

	/*Obtain kernel memory space for wacom i2c */
	wac_i2c = kzalloc(sizeof(struct wacom_i2c), GFP_KERNEL);
	if (NULL == wac_i2c) {
		input_err(true, &client->dev, "failed to allocate wac_i2c.\n");
		ret = -ENOMEM;
		goto err_alloc_mem;
	}

	if (!g_client_boot) {
		wac_i2c->client_boot = i2c_new_dummy(client->adapter,
			WACOM_I2C_BOOT);
		if (!wac_i2c->client_boot) {
			input_err(true, &client->dev, "Fail to register sub client[0x%x]\n",
				WACOM_I2C_BOOT);
		}
	}
	else
		wac_i2c->client_boot = g_client_boot;


	input = input_allocate_device();
	if (NULL == input) {
		input_err(true, &client->dev,  "failed to allocate input device.\n");
		ret = -ENOMEM;
		goto err_alloc_input_dev;
	}

	client->irq = gpio_to_irq(pdata->gpios[I_IRQ]);

	wacom_i2c_set_input_values(client, wac_i2c, input);

	wac_i2c->wac_feature = &wacom_feature_EMR;
	wac_i2c->pdata = pdata;
	wac_i2c->input_dev = input;
	wac_i2c->client = client;
	wac_i2c->irq = client->irq;
	/* init_completion(&wac_i2c->init_done); */
	wac_i2c->irq_pdct = gpio_to_irq(pdata->gpios[I_PDCT]);
	wac_i2c->pen_pdct = PDCT_NOSIGNAL;
	wac_i2c->fw_img = NULL;
	wac_i2c->fw_path = FW_NONE;
	wac_i2c->fullscan_mode = false;
	wac_i2c->wacom_noise_state = WACOM_NOISE_LOW;
	wac_i2c->tsp_noise_mode = 0;
#ifdef CONFIG_OF
	wacom_get_drv_data(wac_i2c);

	wac_i2c->pdata->compulsory_flash_mode = wacom_compulsory_flash_mode;
	wac_i2c->pdata->suspend_platform_hw = wacom_suspend_hw;
	wac_i2c->pdata->resume_platform_hw = wacom_resume_hw;
	wac_i2c->pdata->reset_platform_hw = wacom_reset_hw;
	wac_i2c->pdata->get_irq_state = wacom_get_irq_state;
#endif
	/*Register callbacks */
	wac_i2c->callbacks.check_prox = wacom_check_emr_prox;
	if (wac_i2c->pdata->register_cb)
		wac_i2c->pdata->register_cb(&wac_i2c->callbacks);

	/* Firmware Feature */
	/*wacom_i2c_init_firm_data();*/

	/* pinctrl */
	ret = wacom_pinctrl_init(wac_i2c);
	if (ret < 0)
		goto err_init_pinctrl;

	/* compensation to protect from flash mode	*/
	wac_i2c->pdata->compulsory_flash_mode(true);
	wac_i2c->pdata->compulsory_flash_mode(false);

	/* Power on */
	wac_i2c->pdata->resume_platform_hw();
	if (false == pdata->boot_on_ldo)
		msleep(200);
	wac_i2c->power_enable = true;
	wac_i2c->pwr_flag = true;
#ifdef WACOM_USE_SURVEY_MODE
	wac_i2c->survey_mode = false;
	wac_i2c->survey_cmd_state = false;
	/* Control AOP mode on & off */
	wac_i2c->function_set = 0x00 | EPEN_SETMODE_GARAGE;	// EPEN_SETMODE_AOP : remove 20160620
	wac_i2c->function_result = 0x01;
#ifdef CONFIG_SEC_FACTORY
	wac_i2c->battery_saving_mode = 0;
#else
	wac_i2c->battery_saving_mode = 1;
#endif
	wac_i2c->reset_flag = false;
#endif
	msleep(100);
	wacom_i2c_query(wac_i2c);

	wacom_init_abs_params(wac_i2c);
	input_set_drvdata(input, wac_i2c);

	/*Set client data */
	i2c_set_clientdata(client, wac_i2c);
	i2c_set_clientdata(wac_i2c->client_boot, wac_i2c);

	/*Initializing for semaphor */
	mutex_init(&wac_i2c->lock);
	mutex_init(&wac_i2c->update_lock);
	mutex_init(&wac_i2c->irq_lock);
	wake_lock_init(&wac_i2c->fw_wakelock, WAKE_LOCK_SUSPEND, "wacom");
	wake_lock_init(&wac_i2c->det_wakelock, WAKE_LOCK_SUSPEND, "wacom det");

	INIT_DELAYED_WORK(&wac_i2c->resume_work, wacom_i2c_resume_work);
	INIT_DELAYED_WORK(&wac_i2c->fullscan_check_work, wacom_fullscan_check_work);
#if 0 //#ifdef WACOM_USE_SURVEY_MODE
	INIT_DELAYED_WORK(&wac_i2c->pen_survey_resetdwork, pen_survey_resetdwork);
#endif
#ifdef LCD_FREQ_SYNC
	mutex_init(&wac_i2c->freq_write_lock);
	INIT_WORK(&wac_i2c->lcd_freq_work, wacom_i2c_lcd_freq_work);
	INIT_DELAYED_WORK(&wac_i2c->lcd_freq_done_work, wacom_i2c_finish_lcd_freq_work);
	wac_i2c->use_lcd_freq_sync = true;
#endif
#ifdef WACOM_USE_SOFTKEY_BLOCK
	INIT_DELAYED_WORK(&wac_i2c->softkey_block_work, wacom_i2c_block_softkey_work);
	wac_i2c->block_softkey = false;
#endif
	INIT_WORK(&wac_i2c->update_work, wacom_i2c_update_work);

	/*Before registering input device, data in each input_dev must be set */
	ret = input_register_device(input);
	if (ret) {
		input_err(true, &client->dev,
				"%s sec_epen: failed to register input device.\n", SECLOG);
		goto err_register_device;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	wac_i2c->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	wac_i2c->early_suspend.suspend = wacom_i2c_early_suspend;
	wac_i2c->early_suspend.resume = wacom_i2c_late_resume;
	register_early_suspend(&wac_i2c->early_suspend);
#endif

	wac_i2c->dev = sec_device_create(wac_i2c, "sec_epen");
	if (IS_ERR(wac_i2c->dev)) {
		input_err(true, &client->dev, "Failed to create device(wac_i2c->dev)!\n");
		ret = -ENODEV;
		goto err_create_device;
	}

	dev_set_drvdata(wac_i2c->dev, wac_i2c);

	ret = sysfs_create_group(&wac_i2c->dev->kobj, &epen_attr_group);
	if (ret) {
		input_err(true, &client->dev,
			    "failed to create sysfs group\n");
		goto err_sysfs_create_group;
	}

	ret = sysfs_create_link(&wac_i2c->dev->kobj, &input->dev.kobj, "input");
	if (ret) {
		input_err(true, &client->dev,
			"failed to create sysfs link\n");
		goto err_create_symlink;
	}

	wac_i2c->fw_wq = create_singlethread_workqueue(client->name);
	if (!wac_i2c->fw_wq) {
		input_err(true, &client->dev, "fail to create workqueue for fw_wq\n");
		ret = -ENOMEM;
		goto err_create_fw_wq;
	}

	/*Request IRQ */
	ret =
		request_threaded_irq(wac_i2c->irq, NULL, wacom_interrupt,
					IRQF_DISABLED | wac_i2c->pdata->irq_type |
					IRQF_ONESHOT, "sec_epen_irq", wac_i2c);
	if (ret < 0) {
		input_err(true, &client->dev,
			    "failed to request irq(%d) - %d\n",
			    wac_i2c->irq, ret);
		goto err_request_irq;
	}
	input_info(true, &client->dev, "init irq %d\n", wac_i2c->irq);

	ret =
		request_threaded_irq(wac_i2c->irq_pdct, NULL,
				wacom_interrupt_pdct,
				IRQF_DISABLED | IRQF_TRIGGER_RISING |
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"sec_epen_pdct", wac_i2c);
	if (ret < 0) {
		input_err(true, &client->dev,
			"failed to request irq(%d) - %d\n",
			wac_i2c->irq_pdct, ret);
		goto err_request_pdct_irq;
	}
	input_info(true, &client->dev, "init pdct %d\n", wac_i2c->irq_pdct);

	init_pen_insert(wac_i2c);

	wacom_fw_update(wac_i2c, FW_BUILT_IN, bforced);
	/*complete_all(&wac_i2c->init_done);*/
	device_init_wakeup(&client->dev, true);

#ifdef WACOM_USE_SURVEY_MODE
	wacom_enable_pdct_irq(wac_i2c, true);
#endif

	return 0;

err_request_pdct_irq:
	free_irq(wac_i2c->irq, wac_i2c);
err_request_irq:
	destroy_workqueue(wac_i2c->fw_wq);
err_create_fw_wq:
	sysfs_delete_link(&wac_i2c->dev->kobj, &input->dev.kobj, "input");
err_create_symlink:
	sysfs_remove_group(&wac_i2c->dev->kobj,
		&epen_attr_group);
err_sysfs_create_group:
	sec_device_destroy(wac_i2c->dev->devt);
err_create_device:
	input_unregister_device(input);
	input = NULL;
err_register_device:
	wake_lock_destroy(&wac_i2c->det_wakelock);
	wake_lock_destroy(&wac_i2c->fw_wakelock);
#ifdef LCD_FREQ_SYNC
	mutex_destroy(&wac_i2c->freq_write_lock);
#endif
	mutex_destroy(&wac_i2c->irq_lock);
	mutex_destroy(&wac_i2c->update_lock);
	mutex_destroy(&wac_i2c->lock);
	input_free_device(input);
err_init_pinctrl:
err_alloc_input_dev:
	kfree(wac_i2c);
	wac_i2c = NULL;
err_alloc_mem:
err_i2c_fail:
	return ret;
}

void wacom_i2c_shutdown(struct i2c_client *client)
{
	struct wacom_i2c *wac_i2c = i2c_get_clientdata(client);

	if (!wac_i2c)
		return;

	wac_i2c->pdata->suspend_platform_hw();

	input_info(true, &wac_i2c->client->dev, "%s\n", __func__);

	sec_device_destroy(wac_i2c->dev->devt);
}

static const struct i2c_device_id wacom_i2c_id[] = {
	{"wacom_epen", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, wacom_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id wacom_dt_ids[] = {
	{ .compatible = "wacom,w9010" },
	{ }
};
#endif

/*Create handler for wacom_i2c_driver*/
static struct i2c_driver wacom_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "wacom_epen",
#ifndef USE_OPEN_CLOSE
#ifdef CONFIG_PM
		.pm = &wacom_pm_ops,
#endif
#endif
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(wacom_dt_ids),
#endif
		},
	.probe = wacom_i2c_probe,
	.remove = wacom_i2c_remove,
	.shutdown = &wacom_i2c_shutdown,
	.id_table = wacom_i2c_id,
};

static int __init wacom_i2c_init(void)
{
	int ret = 0;
#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		pr_notice("%s sec_epen: %s : Do not load driver due to : lpm %d\n",
				SECLOG, __func__, lpcharge);
		return 0;
	}
#endif
	ret = i2c_add_driver(&wacom_i2c_driver);
	if (ret)
		pr_err("%s sec_epen: %s : fail to i2c_add_driver\n", SECLOG, __func__);
	return ret;
}

static void __exit wacom_i2c_exit(void)
{
	i2c_del_driver(&wacom_i2c_driver);
}

module_init(wacom_i2c_init);
module_exit(wacom_i2c_exit);

/* flash driver for i2c-gpio flash */
static int wacom_flash_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	g_client_boot = client;
	input_info(true, &client->dev, "%s\n", __func__);
	return 0;
}
void wacom_flash_shutdown(struct i2c_client *client)
{
	input_info(true, &client->dev, "%s\n", __func__);
}

static int wacom_flash_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id wacom_flash_id[] = {
	{"wacom_g5sp_flash", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, wacom_flash_id);

#ifdef CONFIG_OF
static struct of_device_id wacom_flash_dt_ids[] = {
	{ .compatible = "wacom,flash" },
	{ }
};
#endif

static struct i2c_driver wacom_flash_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "wacom_g5sp_flash",
#ifndef USE_OPEN_CLOSE
#ifdef CONFIG_PM
		.pm = &wacom_pm_ops,
#endif
#endif
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(wacom_flash_dt_ids),
#endif
	},
	.probe = wacom_flash_probe,
	.remove = wacom_flash_remove,
	.shutdown = &wacom_flash_shutdown,
	.id_table = wacom_flash_id,
};

static int __init wacom_flash_init(void)
{
	int ret = 0;
#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		pr_info("%s sec_epen: %s : Do not load driver due to : lpm %d\n",
				SECLOG, __func__, lpcharge);
		return 0;
	}
#endif
	ret = i2c_add_driver(&wacom_flash_driver);
	if (ret)
		pr_err("%s sec_epen: fail to i2c_add_driver\n", SECLOG);
	return ret;
}

static void __exit wacom_flash_exit(void)
{
	i2c_del_driver(&wacom_flash_driver);
}

fs_initcall(wacom_flash_init);
module_exit(wacom_flash_exit);

MODULE_AUTHOR("Samsung");
MODULE_DESCRIPTION("Driver for Wacom G5SP Digitizer Controller");

MODULE_LICENSE("GPL");
