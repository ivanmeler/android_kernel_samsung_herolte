/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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
 */
#include "ssp.h"
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/sec_debug.h>
#include <linux/sec_batt.h>

#define DEBUG
#define SSP_IRQ_EDGE_PROTECT
#if defined SSP_IRQ_EDGE_PROTECT
static int prevent_irq;
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
static void ssp_early_suspend(struct early_suspend *handler);
static void ssp_late_resume(struct early_suspend *handler);
#endif
#define NORMAL_SENSOR_STATE_K	0x3FEFF

unsigned int bootmode;
EXPORT_SYMBOL(bootmode);
static int __init bootmode_setup(char *str)
{
	 get_option(&str, &bootmode);
	pr_info("[SSP] bootmode_setup = %d\n",bootmode);
	 return 1;
}
__setup("bootmode=", bootmode_setup);

void ssp_enable(struct ssp_data *data, bool enable)
{
	pr_info("[SSP] %s, enable = %d, old enable = %d\n",
		__func__, enable, data->bSspShutdown);

	if (enable && data->bSspShutdown) {
		data->bSspShutdown = false;
		enable_irq(data->iIrq);
		enable_irq_wake(data->iIrq);
	} else if (!enable && !data->bSspShutdown) {
		data->bSspShutdown = true;
		disable_irq(data->iIrq);
		disable_irq_wake(data->iIrq);
	} else
		pr_err("[SSP] %s, enable error\n", __func__);
}
/************************************************************************/
/* interrupt happened due to transition/change of SSP MCU		*/
/************************************************************************/

static irqreturn_t sensordata_irq_thread_fn(int iIrq, void *dev_id)
{
	struct ssp_data *data = dev_id;

#if defined SSP_IRQ_EDGE_PROTECT
	if (prevent_irq != 0) {
		select_irq_msg(data);
		data->uIrqCnt++;
	} else {
		pr_err("[SSP] %s, irq Occured while Boot.\n", __func__);
	}
#else
	select_irq_msg(data);
	data->uIrqCnt++;
#endif

	return IRQ_HANDLED;
}

/*************************************************************************/
/* initialize sensor hub						 */
/*************************************************************************/

static void initialize_variable(struct ssp_data *data)
{
	int iSensorIndex;

	for (iSensorIndex = 0; iSensorIndex < SENSOR_MAX; iSensorIndex++) {
		data->adDelayBuf[iSensorIndex] = DEFUALT_POLLING_DELAY;
		data->batchLatencyBuf[iSensorIndex] = 0;
		data->batchOptBuf[iSensorIndex] = 0;
		data->aiCheckStatus[iSensorIndex] = INITIALIZATION_STATE;
	}

	atomic_set(&data->aSensorEnable, 0);
	data->iLibraryLength = 0;
	data->uSensorState = NORMAL_SENSOR_STATE_K;
	data->uFactoryProxAvg[0] = 0;
	data->uMagCntlRegData = 1;

	data->uResetCnt = 0;
	data->uTimeOutCnt = 0;
	data->uComFailCnt = 0;
	data->uIrqCnt = 0;

	data->bSspShutdown = true;
	data->bProximityRawEnabled = false;
	data->bGeomagneticRawEnabled = false;
	data->bBarcodeEnabled = false;
	data->bAccelAlert = false;
	data->bTimeSyncing = true;

	data->accelcal.x = 0;
	data->accelcal.y = 0;
	data->accelcal.z = 0;

	data->gyrocal.x = 0;
	data->gyrocal.y = 0;
	data->gyrocal.z = 0;

	data->magoffset.x = 0;
	data->magoffset.y = 0;
	data->magoffset.z = 0;

	data->iPressureCal = 0;
	data->uProxCanc = 0;
	data->uProxHiThresh = 0;
	data->uProxLoThresh = 0;
	data->uGyroDps = GYROSCOPE_DPS500;
	data->uIr_Current = DEFUALT_IR_CURRENT;

	data->mcu_device = NULL;
	data->acc_device = NULL;
	data->gyro_device = NULL;
	data->mag_device = NULL;
	data->prs_device = NULL;
	data->prox_device = NULL;
	data->light_device = NULL;
	data->ges_device = NULL;

	data->voice_device = NULL;
	data->bMcuDumpMode = ssp_check_sec_dump_mode();
	INIT_LIST_HEAD(&data->pending_list);

	data->sealevelpressure = 0;
	data->step_count_total = 0;
	initialize_function_pointer(data);
}

int initialize_mcu(struct ssp_data *data)
{
	int iRet = 0;

	clean_pending_list(data);

	iRet = get_chipid(data);
	pr_info("[SSP] MCU device ID = %d, reading ID = %d\n", DEVICE_ID, iRet);
	if (iRet != DEVICE_ID) {
		if (iRet < 0) {
			pr_err("[SSP]: %s - MCU is not working : 0x%x\n",
				__func__, iRet);
		} else {
			pr_err("[SSP]: %s - MCU identification failed\n",
				__func__);
			iRet = -ENODEV;
		}
		goto out;
	}

	iRet = set_sensor_position(data);
	if (iRet < 0) {
		pr_err("[SSP]: %s - set_sensor_position failed\n", __func__);
		goto out;
	}

	data->uSensorState = get_sensor_scanning_info(data);
	if (data->uSensorState == 0) {
		pr_err("[SSP]: %s - get_sensor_scanning_info failed\n",
			__func__);
		iRet = ERROR;
		goto out;
	}
#if defined (CONFIG_SENSORS_SSP_YAS532)
	iRet = set_static_matrix(data);
	if (iRet < 0)
		pr_err("[SSP] %s - set_magnetic_static_matrix failed\n", __func__);
#endif
	data->uCurFirmRev = get_firmware_rev(data);
	pr_info("[SSP] MCU Firm Rev : New = %8u\n",
		data->uCurFirmRev);

	iRet = ssp_send_cmd(data, MSG2SSP_AP_MCU_DUMP_CHECK, 0);
out:
	return iRet;
}

static int initialize_irq(struct ssp_data *data)
{
	int iRet, iIrq;
	iIrq = gpio_to_irq(data->mcu_int1);

	pr_info("[SSP] requesting IRQ %d\n", iIrq);
	iRet = request_threaded_irq(iIrq, NULL, sensordata_irq_thread_fn,
				    IRQF_TRIGGER_FALLING|IRQF_ONESHOT, "SSP_Int", data);
	if (iRet < 0) {
		pr_err("[SSP] %s, request_irq(%d) failed for gpio %d (%d)\n",
		       __func__, iIrq, iIrq, iRet);
		goto err_request_irq;
	}

	/* start with interrupts disabled */
	data->iIrq = iIrq;
	disable_irq(data->iIrq);
	return 0;

err_request_irq:
	gpio_free(data->mcu_int1);
	return iRet;
}

static void work_function_firmware_update(struct work_struct *work)
{
	struct ssp_data *data = container_of((struct delayed_work *)work,
				struct ssp_data, work_firmware);
	int iRet;

	pr_info("[SSP] : %s\n", __func__);

	iRet = forced_to_download_binary(data, KERNEL_BINARY);
	if (iRet < 0) {
		ssp_dbg("[SSP]: %s - forced_to_download_binary failed!\n",
			__func__);
		data->uSensorState = 0;
		return;
	}

	queue_refresh_task(data, SSP_SW_RESET_TIME);
}

static int ssp_parse_dt(struct device *dev,struct  ssp_data *data)
{
	struct device_node *np = dev->of_node;
	int errorno = 0;
#if defined(CONFIG_SENSORS_SSP_YAS532)
	u32 len, temp;
	int i;
#endif

	if (!np) {
		pr_err("[SSP] NO dt node!!\n");
		return errorno;
	}

	data->mcu_int1 = of_get_named_gpio(np, "ssp-irq", 0);
	if (data->mcu_int1 < 0) {
		errorno = data->mcu_int1;
		goto dt_exit;
	}

	data->mcu_int2 = of_get_named_gpio(np, "ssp-irq2",	0);
	if (data->mcu_int2 < 0) {
		errorno = data->mcu_int2;
		goto dt_exit;
	}

	data->ap_int = of_get_named_gpio(np, "ssp-ap-int", 0);
	if (data->ap_int < 0) {
		errorno = data->ap_int;
		goto dt_exit;
	}

	data->rst = of_get_named_gpio(np, "ssp-reset", 0);
	if (data->rst < 0) {
		errorno = data->rst ;
		goto dt_exit;
	}

	if (of_property_read_u32(np, "ssp-acc-position", &data->accel_position))
		data->accel_position = 0;
	if (of_property_read_u32(np, "ssp-mag-position", &data->mag_position))
		data->mag_position = 0;
	if (of_property_read_u32(np, "ssp-sns-combination", &data->sns_combination))
		data->sns_combination = 0;
	if (of_property_read_u32(np, "ssp-ap-rev", &data->ap_rev))
		data->ap_rev = 0;

	pr_info("[SSP] acc-posi[%d] mag-posi[%d] ap-rev[%d]\n", data->accel_position,
				data->mag_position, data->ap_rev);

	errorno = gpio_request(data->mcu_int1, "mcu_ap_int1");
	if (errorno) {
		pr_err("[SSP] failed to request MCU_INT1 for SSP\n");
		goto dt_exit;
	}
	errorno = gpio_direction_input(data->mcu_int1);
	if (errorno) {
		pr_err("[SSP] failed to set mcu_int1 as input\n");
		goto dt_exit;
	}
	errorno = gpio_request(data->mcu_int2, "MCU_INT2");
	if (errorno) {
		pr_err("[SSP] failed to request MCU_INT2 for SSP\n");
		goto dt_exit;
	}
	gpio_direction_input(data->mcu_int2);

	errorno = gpio_request(data->ap_int, "AP_MCU_INT");
	if (errorno) {
		pr_err("[SSP] failed to request AP_INT for SSP\n");
		goto dt_exit;
	}
	gpio_direction_output(data->ap_int, 1);

	errorno = gpio_request(data->rst, "MCU_RST");
	if (errorno) {
		pr_err("[SSP] failed to request MCU_RST for SSP\n");
		goto dt_exit;
	}
	if(!gpio_get_value(data->rst))
		pr_err("[SSP] rst level was LOW at BL !!\n");
	gpio_direction_output(data->rst, 1);

#if defined(CONFIG_SENSORS_SSP_YAS532)
	if (!of_get_property(np, "ssp-mag-array", &len)) {
		pr_info("[SSP] No static matrix at DT for YAS532!(%p)\n", data->static_matrix);
		goto dt_exit;
	}
	if (len/4 != 9) {
		pr_err("[SSP] Length/4:%d should be 9 for YAS532!\n", len/4);
		goto dt_exit;
	}
	data->static_matrix = kzalloc(9*sizeof(s16), GFP_KERNEL);
	pr_info("[SSP] static matrix Length:%d, Len/4=%d\n", len, len/4);

	for (i = 0; i < 9; i++) {
		if (of_property_read_u32_index(np, "ssp-mag-array", i, &temp)) {
			pr_err("[SSP] %s cannot get u32 of array[%d]!\n", __func__, i);
			goto dt_exit;
		}
		*(data->static_matrix+i) = (int)temp;
	}
#endif

dt_exit:
	return errorno;
}

static int ssp_probe(struct spi_device *spi)
{
	struct ssp_data *data;
	struct ssp_platform_data *pdata;
	int iRet = 0;

	pr_info("[SSP] %s, is called\n", __func__);

	if (lpcharge == 1 || bootmode == 2) {
		pr_err("[SSP] probe exit : lpm %d (recovery)bootmode:%d \n", lpcharge, bootmode);
		return -ENODEV;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		pr_err("[SSP] %s, failed to allocate memory for data\n", __func__);
		iRet = -ENOMEM;
		goto exit;
	}

	if (spi->dev.of_node) {
		iRet = ssp_parse_dt(&spi->dev, data);
		if (iRet) {
			pr_err("[SSP]: %s - Failed to parse DT\n", __func__);
			goto err_setup;
		}
	} else {
		pdata = spi->dev.platform_data;
		if (pdata == NULL) {
			pr_err("[SSP] %s, platform_data is null\n", __func__);
			iRet = -ENOMEM;
			goto err_setup;
		}
		data->wakeup_mcu = pdata->wakeup_mcu;
		data->set_mcu_reset = pdata->set_mcu_reset;
		data->ap_int = pdata->ap_int;
		data->mcu_int1 = pdata->mcu_int1;
		data->mcu_int2 = pdata->mcu_int2;

		/* AP system_rev */
		if (pdata->check_ap_rev)
			data->ap_rev = pdata->check_ap_rev();
		else
			data->ap_rev = 0;

		/* Get sensor positions */
		if (pdata->get_positions) {
			pdata->get_positions(&data->accel_position,
				&data->mag_position);
		} else {
			data->accel_position = 0;
			data->mag_position = 0;
		}
		if (pdata->mag_matrix) {
			data->mag_matrix_size = pdata->mag_matrix_size;
			data->mag_matrix = pdata->mag_matrix;
		}
	}

#if 0//def CONFIG_SENSORS_SSP_SHTC1
	/* Get pdata for cp thermister */
	if (pdata->cp_thm_adc_table) {
		data->cp_thm_adc_channel = pdata->cp_thm_adc_channel;
		data->cp_thm_adc_arr_size= pdata->cp_thm_adc_arr_size;
		data->cp_thm_adc_table = pdata->cp_thm_adc_table;
	}
	/* Get pdata for batt thermister */
	if (pdata->batt_thm_adc_table) {
		data->batt_thm_adc_arr_size= pdata->batt_thm_adc_arr_size;
		data->batt_thm_adc_table = pdata->batt_thm_adc_table;
	}
	/* Get pdata for chg thermister */
	if (pdata->chg_thm_adc_table) {
		data->chg_thm_adc_arr_size= pdata->chg_thm_adc_arr_size;
		data->chg_thm_adc_table = pdata->chg_thm_adc_table;
	}
#endif

	spi->mode = SPI_MODE_1;
	if (spi_setup(spi)) {
		pr_err("[SSP] %s, failed to setup spi\n", __func__);
		goto err_setup;
	}

	data->bProbeIsDone = false;
	data->fw_dl_state = FW_DL_STATE_NONE;
	data->spi = spi;
	spi_set_drvdata(spi, data);

#ifdef CONFIG_SENSORS_SSP_SHTC1
	mutex_init(&data->cp_temp_adc_lock);
	mutex_init(&data->bulk_temp_read_lock);
#endif
#ifdef CONFIG_SENSORS_SSP_ATMEL
	mutex_init(&data->comm_mutex);
	mutex_init(&data->pending_mutex);
#endif

	if (((data->wakeup_mcu == NULL)
		|| (data->set_mcu_reset == NULL))
		&& (spi->dev.of_node == NULL)) {
		pr_err("[SSP] %s, function callback is null\n", __func__);
		iRet = -EIO;
		goto err_reset_null;
	}
	pr_info("\n#####################################################\n");

	INIT_DELAYED_WORK(&data->work_firmware, work_function_firmware_update);

	wake_lock_init(&data->ssp_wake_lock,
		WAKE_LOCK_SUSPEND, "ssp_wake_lock");

	iRet = initialize_input_dev(data);
	if (iRet < 0) {
		pr_err("[SSP]: %s - could not create input device\n", __func__);
		goto err_input_register_device;
	}

	iRet = initialize_debug_timer(data);
	if (iRet < 0) {
		pr_err("[SSP]: %s - could not create workqueue\n", __func__);
		goto err_create_workqueue;
	}

	iRet = initialize_irq(data);
	if (iRet < 0) {
		pr_err("[SSP]: %s - could not create irq\n", __func__);
		goto err_setup_irq;
	}

	iRet = initialize_sysfs(data);
	if (iRet < 0) {
		pr_err("[SSP]: %s - could not create sysfs\n", __func__);
		goto err_sysfs_create;
	}

	iRet = initialize_event_symlink(data);
	if (iRet < 0) {
		pr_err("[SSP]: %s - could not create symlink\n", __func__);
		goto err_symlink_create;
	}

	initialize_variable(data);
	msleep(40);
#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	/* init sensorhub device */
	iRet = ssp_sensorhub_initialize(data);
	if (iRet < 0) {
		pr_err("%s: ssp_sensorhub_initialize err(%d)", __func__, iRet);
		ssp_sensorhub_remove(data);
	}
#endif

	ssp_enable(data, true);
#if defined SSP_IRQ_EDGE_PROTECT
	prevent_irq = 1;
#endif
	/* check boot loader binary */
	data->fw_dl_state = check_fwbl(data);

	if (data->fw_dl_state == FW_DL_STATE_NONE) {
		iRet = initialize_mcu(data);
		if (iRet == ERROR) {
			toggle_mcu_reset(data);
		} else if (iRet < ERROR) {
			pr_err("[SSP]: %s - initialize_mcu failed\n", __func__);
			goto err_read_reg;
		}
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.suspend = ssp_early_suspend;
	data->early_suspend.resume = ssp_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	pr_info("[SSP]: %s - probe success!\n", __func__);

	enable_debug_timer(data);

	if (data->fw_dl_state == FW_DL_STATE_NEED_TO_SCHEDULE) {
		pr_info("[SSP] Firmware update is scheduled\n");
		schedule_delayed_work(&data->work_firmware,
				msecs_to_jiffies(1000));
		data->fw_dl_state = FW_DL_STATE_SCHEDULED;
	} else if (data->fw_dl_state == FW_DL_STATE_FAIL) {
		data->bSspShutdown = true;
	}

	data->bProbeIsDone = true;
	iRet = 0;

	goto exit;

err_read_reg:
err_symlink_create:
	remove_sysfs(data);
err_sysfs_create:
	free_irq(data->iIrq, data);
	gpio_free(data->mcu_int1);
err_setup_irq:
	destroy_workqueue(data->debug_wq);
err_create_workqueue:
	remove_input_dev(data);
err_input_register_device:
	wake_lock_destroy(&data->ssp_wake_lock);
err_reset_null:
#ifdef CONFIG_SENSORS_SSP_ATMEL
	mutex_destroy(&data->comm_mutex);
	mutex_destroy(&data->pending_mutex);
#endif
#ifdef CONFIG_SENSORS_SSP_SHTC1
	mutex_destroy(&data->bulk_temp_read_lock);
	mutex_destroy(&data->cp_temp_adc_lock);
#endif

err_setup:
	kfree(data);
	pr_err("[SSP] %s, probe failed!\n", __func__);
exit:
	pr_info("#####################################################\n\n");
	return iRet;
}

static void ssp_shutdown(struct spi_device *spi)
{
	struct ssp_data *data = spi_get_drvdata(spi);

	pr_err("[SSP] %s, data->fw_dl_state[%d]*******************!\n",
			__func__,data->fw_dl_state);
	pr_err("[SSP]  lpm %d recovery \n",lpcharge /*, recovery_mode*/);
	func_dbg();
	if (data->bProbeIsDone == false)
		goto exit;

	disable_debug_timer(data);

	if (data->fw_dl_state >= FW_DL_STATE_SCHEDULED &&
		data->fw_dl_state < FW_DL_STATE_DONE) {
		pr_err("[SSP] %s, cancel_delayed_work_sync state = %d\n",
			__func__, data->fw_dl_state);
		cancel_delayed_work_sync(&data->work_firmware);
	}

	if (SUCCESS != ssp_send_cmd(data, MSG2SSP_AP_STATUS_SHUTDOWN, 0))
		pr_err("[SSP]: %s MSG2SSP_AP_STATUS_SHUTDOWN failed\n",
			__func__);

	ssp_enable(data, false);
	clean_pending_list(data);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif

	free_irq(data->iIrq, data);
	gpio_free(data->mcu_int1);

	remove_event_symlink(data);
	remove_sysfs(data);
	remove_input_dev(data);

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	ssp_sensorhub_remove(data);
#endif

	del_timer_sync(&data->debug_timer);
	cancel_work_sync(&data->work_debug);
	cancel_delayed_work_sync(&data->work_refresh);
	destroy_workqueue(data->debug_wq);
	wake_lock_destroy(&data->ssp_wake_lock);
#ifdef CONFIG_SENSORS_SSP_SHTC1
	mutex_destroy(&data->bulk_temp_read_lock);
	mutex_destroy(&data->cp_temp_adc_lock);
#endif
#ifdef CONFIG_SENSORS_SSP_ATMEL
	mutex_destroy(&data->comm_mutex);
	mutex_destroy(&data->pending_mutex);
#endif
	toggle_mcu_reset(data);
	pr_info("[SSP] %s done\n", __func__);
exit:
	kfree(data);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ssp_early_suspend(struct early_suspend *handler)
{
	struct ssp_data *data;
	data = container_of(handler, struct ssp_data, early_suspend);

	func_dbg();
	disable_debug_timer(data);

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	/* give notice to user that AP goes to sleep */
	ssp_sensorhub_report_notice(data, MSG2SSP_AP_STATUS_SLEEP);
	ssp_sleep_mode(data);
	data->uLastAPState = MSG2SSP_AP_STATUS_SLEEP;
#else
	if (atomic_read(&data->aSensorEnable) > 0)
		ssp_sleep_mode(data);
#endif
}

static void ssp_late_resume(struct early_suspend *handler)
{
	struct ssp_data *data;
	data = container_of(handler, struct ssp_data, early_suspend);

	func_dbg();
	enable_debug_timer(data);

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	/* give notice to user that AP goes to sleep */
	ssp_sensorhub_report_notice(data, MSG2SSP_AP_STATUS_WAKEUP);
	ssp_resume_mode(data);
	data->uLastAPState = MSG2SSP_AP_STATUS_WAKEUP;
#else
	if (atomic_read(&data->aSensorEnable) > 0)
		ssp_resume_mode(data);
#endif
}

#else /* no early suspend */

static int ssp_suspend(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct ssp_data *data = spi_get_drvdata(spi);

	func_dbg();
	data->uLastResumeState = MSG2SSP_AP_STATUS_SUSPEND;
	disable_debug_timer(data);

	if (SUCCESS != ssp_send_cmd(data, MSG2SSP_AP_STATUS_SUSPEND, 0))
		pr_err("[SSP]: %s MSG2SSP_AP_STATUS_SUSPEND failed\n",
			__func__);
	data->bTimeSyncing = false;
	disable_irq(data->iIrq);
	return 0;
}

static int ssp_resume(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct ssp_data *data = spi_get_drvdata(spi);

	enable_irq(data->iIrq);
	func_dbg();
	enable_debug_timer(data);

	if (SUCCESS != ssp_send_cmd(data, MSG2SSP_AP_STATUS_RESUME, 0))
		pr_err("[SSP]: %s MSG2SSP_AP_STATUS_RESUME failed\n",
			__func__);
	data->uLastResumeState = MSG2SSP_AP_STATUS_RESUME;

	return 0;
}

static const struct dev_pm_ops ssp_pm_ops = {
	.suspend = ssp_suspend,
	.resume = ssp_resume
};
#endif /* CONFIG_HAS_EARLYSUSPEND */

static const struct spi_device_id ssp_id[] = {
	{"ssp", 0},
	{}
};

MODULE_DEVICE_TABLE(spi, ssp_id);
#ifdef CONFIG_OF
static struct of_device_id ssp_match_table[] = {
	{ .compatible = "ssp,STM32F",},
	{},
};
#endif
static struct spi_driver ssp_driver = {
	.probe = ssp_probe,
	.shutdown = ssp_shutdown,
	.id_table = ssp_id,
	.driver = {
#ifndef CONFIG_HAS_EARLYSUSPEND
		   .pm = &ssp_pm_ops,
#endif
		   .owner = THIS_MODULE,
		   .name = "ssp",
#ifdef CONFIG_OF
		   .of_match_table = ssp_match_table
#endif
		},
};

module_spi_driver(ssp_driver);
MODULE_DESCRIPTION("ssp spi driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
