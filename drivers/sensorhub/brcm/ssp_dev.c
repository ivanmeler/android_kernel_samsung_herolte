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

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ssp_early_suspend(struct early_suspend *handler);
static void ssp_late_resume(struct early_suspend *handler);
#endif
#define NORMAL_SENSOR_STATE_K	0x3FEFF

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif

#if defined(CONFIG_SSP_MOTOR)
#include "ssp_motor.h"
#include "../../motor/max77854_haptic.h"
#endif

unsigned int bootmode;
EXPORT_SYMBOL(bootmode);

struct mutex shutdown_lock;
bool ssp_debug_time_flag;

static int __init bootmode_setup(char *str)
{
	get_option(&str, &bootmode);
	pr_info("[SSP] bootmode_setup = %d\n", bootmode);
	return 1;
}
__setup("bootmode=", bootmode_setup);

void ssp_enable(struct ssp_data *data, bool enable)
{
	if (enable == !data->bSspShutdown)
		return;

	pr_info("[SSP] %s, new enable = %d, old enable = %d\n",
		__func__, enable, !data->bSspShutdown);
#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING
	if (enable) {
		enable_irq(data->mcu_host_wake_irq);
		/* pr_info("[SSP_IRQ] enable irq (%d)",
				data->mcu_host_wake_irq);*/
	} else {
		disable_irq(data->mcu_host_wake_irq);
		/* pr_info("[SSP_IRQ] disable irq (%d)",
				data->mcu_host_wake_irq);*/
	}
#endif

	if (enable && data->bSspShutdown)
		data->bSspShutdown = false;
	else if (!enable && !data->bSspShutdown)
		data->bSspShutdown = true;
}

u64 get_current_timestamp(void)
{
	u64 timestamp;
	struct timespec ts;

	ts = ktime_to_timespec(ktime_get_boottime());
	timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

	return timestamp;
}

/*************************************************************************/
/* initialize sensor hub						 */
/*************************************************************************/

static void initialize_variable(struct ssp_data *data)
{
	int iSensorIndex;

#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING
	data->cameraGyroSyncMode = false;
	data->ts_stacked_cnt = 0;
	data->ts_stacked_offset = 0;
	data->ts_irq_last = 0ULL;
#endif
	for (iSensorIndex = 0; iSensorIndex < SENSOR_MAX; iSensorIndex++) {
		data->adDelayBuf[iSensorIndex] = DEFUALT_POLLING_DELAY;
		data->batchLatencyBuf[iSensorIndex] = 0;
		data->batchOptBuf[iSensorIndex] = 0;
		data->aiCheckStatus[iSensorIndex] = INITIALIZATION_STATE;
		data->lastTimestamp[iSensorIndex] = 0;
        data->LastSensorTimeforReset[iSensorIndex] = 0;
		data->IsBypassMode[iSensorIndex] = 0;
		data->reportedData[iSensorIndex] = false;
		/* variables for conditional timestamp */
		data->first_sensor_data[iSensorIndex] = true;
		data->sensor_dump[iSensorIndex] = NULL;
	}

	atomic64_set(&data->aSensorEnable, 0);
	data->iLibraryLength = 0;
	data->uSensorState = NORMAL_SENSOR_STATE_K;
	data->uFactoryProxAvg[0] = 0;
	data->uMagCntlRegData = 1;

	data->uResetCnt = 0;
	data->uTimeOutCnt = 0;
	data->uComFailCnt = 0;
	data->uIrqCnt = 0;

	data->bFirstRef = true;
	data->bSspShutdown = true;
	data->bProximityRawEnabled = false;
	data->bGeomagneticRawEnabled = false;
	data->bBarcodeEnabled = false;
	data->bAccelAlert = false;
	data->bTimeSyncing = true;
	data->bHandlingIrq = false;
	data->resetting = false;

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

#ifdef CONFIG_SENSORS_SSP_PROX_FACTORYCAL
	data->uProxCanc = 0;
	data->uProxHiThresh = data->uProxHiThresh_default;
	data->uProxLoThresh = data->uProxLoThresh_default;
#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
	//for tmd4904
	data->uProxHiThresh_tmd4904 = data->uProxHiThresh_default_tmd4904;
	data->uProxLoThresh_tmd4904 = data->uProxLoThresh_default_tmd4904;
#endif
#endif

#ifdef CONFIG_SENSORS_SSP_GRACE
	data->uGyroDps = GYROSCOPE_DPS1000;
#else
    data->uGyroDps = GYROSCOPE_DPS2000;
#endif
	data->uIr_Current = DEFUALT_IR_CURRENT;

	data->mcu_device = NULL;
	data->acc_device = NULL;
	data->gyro_device = NULL;
	data->mag_device = NULL;
	data->prs_device = NULL;
	data->prox_device = NULL;
	data->light_device = NULL;
	data->ges_device = NULL;

#ifdef CONFIG_SENSORS_SSP_LIGHT_COLORID
	data->hiddenhole_device = NULL;
#endif
	data->voice_device = NULL;
	data->bMcuDumpMode = ssp_check_sec_dump_mode();
	INIT_LIST_HEAD(&data->pending_list);

	data->bbd_on_packet_wq =
		create_singlethread_workqueue("ssp_bbd_on_packet_wq");
	INIT_WORK(&data->work_bbd_on_packet, bbd_on_packet_work_func);

	data->bbd_mcu_ready_wq =
		create_singlethread_workqueue("ssp_bbd_mcu_ready_wq");
	INIT_WORK(&data->work_bbd_mcu_ready, bbd_mcu_ready_work_func);

#ifdef SSP_BBD_USE_SEND_WORK
	data->bbd_send_packet_wq =
		create_singlethread_workqueue("ssp_bbd_send_packet_wq");
	INIT_WORK(&data->work_bbd_send_packet, bbd_send_packet_work_func);
#endif	/* SSP_BBD_USE_SEND_WORK  */

	data->step_count_total = 0;
	data->sealevelpressure = 0;

	data->gyro_lib_state = GYRO_CALIBRATION_STATE_NOT_YET;
#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING
	/* HIFI batching wakeup */
	data->resumeTimestamp = 0;
	data->bIsResumed = false;
	data->bIsReset = false;
#endif
	initialize_function_pointer(data);
	data->uNoRespSensorCnt = 0;
	data->errorCount = 0;
	data->mcuCrashedCnt = 0;
    data->pktErrCnt = 0;
	data->mcuAbnormal = false;
    data->IsMcuCrashed = false;
	data->intendedMcuReset = false;

	data->timestamp_factor = 10; // initialize for 0.1%
	ssp_debug_time_flag = false;
    data->dhrAccelScaleRange = 0;
   	data->skipSensorData = 0;
	data->sensor_dump_cnt_light = 0;
	data->sensor_dump_flag_light = false;
	data->sensor_dump_flag_proximity = false;

	data->IsVDIS_Enabled = false;
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

#ifdef CONFIG_SENSORS_MULTIPLE_GLASS_TYPE
	iRet = set_glass_type(data);
	if (iRet < 0) {
		pr_err("[SSP]: %s - set_glass_type failed\n", __func__);
		goto out;
	}
#endif

	/* Hall IC threshold */
	if (data->hall_threshold[0]) {
		iRet = set_hall_threshold(data);
		if (iRet < 0) {
			pr_err("[SSP]: %s - set_hall_threshold failed\n",
				__func__);
			goto out;
		}
	}

	data->uSensorState = get_sensor_scanning_info(data);
	if (data->uSensorState == 0) {
		pr_err("[SSP]: %s - get_sensor_scanning_info failed\n",
			__func__);
		iRet = ERROR;
		goto out;
	}

	if (initialize_magnetic_sensor(data) < 0)
		pr_err("[SSP]: %s - initialize magnetic sensor failed\n",
			__func__);

	data->uCurFirmRev = get_firmware_rev(data);
	pr_info("[SSP] MCU Firm Rev : New = %8u\n",
		data->uCurFirmRev);

    data->dhrAccelScaleRange = get_accel_range(data);
    
#if defined(CONFIG_SENSORS_SSP_TMD3725)
    set_light_coef(data);
#endif

/* hoi: il dan mak a */
#ifndef CONFIG_SENSORS_SSP_BBD
	iRet = ssp_send_cmd(data, MSG2SSP_AP_MCU_DUMP_CHECK, 0);
#endif
out:
	return iRet;
}

static bbd_callbacks ssp_bbd_callbacks = {
// on_packet call-back func is used after N OS, drivers devided workqueue merge as one 
#if ANDROID_VERSION < 70000
	.on_packet       = NULL,
#else
	.on_packet       = callback_bbd_on_packet,
#endif
	.on_packet_alarm = callback_bbd_on_packet_alarm,
	.on_control      = callback_bbd_on_control,
	.on_mcu_ready    = callback_bbd_on_mcu_ready,
	.on_mcu_reset    = callback_bbd_on_mcu_reset
};

#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING
/* HIFI Sensor */
void ssp_reset_batching_resources(struct ssp_data *data)
{
	u64 ts = get_current_timestamp();
	pr_err("[SSP_RST] reset triggered %lld\n",ts);
	data->ts_stacked_cnt = 0;
	data->ts_stacked_offset = 0;
	data->ts_irq_last = 0ULL;
	data->resumeTimestamp = 0;
	data->bIsResumed = false;
	memset(data->ts_index_buffer, 0, sizeof(u64)*SENSOR_MAX);
	memset(data->ts_prev_index, 0, sizeof(unsigned int)*SENSOR_MAX);
	data->ts_last_enable_cmd_time = 0;
	memset(data->ts_avg_buffer, 0,
		sizeof(u64) * SIZE_MOVING_AVG_BUFFER * SENSOR_MAX);
	memset(data->ts_avg_buffer_cnt, 0, sizeof(u8)*SENSOR_MAX);
	memset(data->ts_avg_buffer_idx, 0, sizeof(u8)*SENSOR_MAX);
	memset(data->ts_avg_buffer_sum, 0, sizeof(u64)*SENSOR_MAX);
	data->bIsReset = true;

	ts = get_current_timestamp();
	pr_err("[SSP_RST] reset finished %lld\n",ts);
}

irqreturn_t ssp_mcu_host_wake_irq_handler(int irq, void *device)
{
	struct ssp_data *data = (struct ssp_data *) device;
	u64 timestamp = get_current_timestamp();
	
	/** HIFI Sensor **/
	/* Use buffer idx from 1 to 999, 0, 1 ... */
	data->ts_stacked_cnt = (data->ts_stacked_cnt + 1) % SIZE_TIMESTAMP_BUFFER;
	data->ts_index_buffer[data->ts_stacked_cnt] = timestamp;

	ssp_debug_time("[SSP_DEBUG_TIME_IRQ] ts_stacked_cnt - %d, timestamp - %lld diff - %lld\n", data->ts_stacked_cnt, timestamp, timestamp - data->ts_irq_last);
	data->ts_irq_last = timestamp;
	return IRQ_HANDLED;
}
#endif

#if defined(CONFIG_SENSORS_SSP_HAECHI_880)
static int haechi880_light_coef[7] = {-830, 1100, -1180, 1000, 814, 3521, 2095};
#endif
static int ssp_parse_dt(struct device *dev, struct ssp_data *data)
{
	struct device_node *np = dev->of_node;
	int errorno = 0;
	u32 len, temp;
	int i;

	if (!np) {
		pr_err("[SSP] NO dt node!!\n");
		return errorno;
	}
	
	if (of_property_read_string(np, "ssp-vdd-mcu-1p8", &data->vdd_mcu_1p8_name)) {
		data->regulator_vdd_mcu_1p8 = NULL;		
	} else {
		data->regulator_vdd_mcu_1p8 = regulator_get(NULL, data->vdd_mcu_1p8_name);
		if(IS_ERR(data->regulator_vdd_mcu_1p8)){
			pr_err("[SSP]: failed to get reulator %s ret: %ld\n", data->vdd_mcu_1p8_name, PTR_ERR(data->regulator_vdd_mcu_1p8));
		} 
	}

	data->shub_en = of_get_named_gpio(np, "ssp-pwr-en", 0);
	pr_err("[SSPBBD] ssp-pwr-en=%d\n", data->shub_en);
	if (data->shub_en < 0) {
		pr_err("[SSPBBD]: GPIO value not correct\n");
	} else {
		/* Config GPIO */	
		errorno = gpio_request(data->shub_en, "SHUB EN");
		if (errorno){
			pr_err("[SSPBBD]: failed to request SHUB EN, ret:%d", errorno);
		}
		errorno= gpio_direction_output(data->shub_en, 0);
		if (errorno) {
			pr_err("[SSPBBD]: failed set SHUB EN as output mode, ret:%d", errorno);
		}
	}
	
#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING
	data->mcu_host_wake_int = of_get_named_gpio(np,
					"ssp-batch-wake-irq", 0);
	/* pr_info("[SSP_IRQ] of_get_named_gpio 'ssp-batch-wake-irq' : %d\n", data->mcu_host_wake_int); */
	if (data->mcu_host_wake_int < 0) {
		pr_info("[SSP_IRQ] Failed to of_get_named_gpio 'ssp-batch-wake-irq'\n");
		errorno = data->mcu_host_wake_int;
		goto dt_exit;
	}

	errorno = gpio_request(data->mcu_host_wake_int, "ssp-batch-wake-irq");
	/* pr_info("[SSP_IRQ] gpio_request SSP_HOST_WAKE : %d\n", errorno); */
	if (errorno) {
		pr_info("[SSP_IRQ] Failed to request SSP_HOST_WAKE\n");
		goto dt_exit;
	}
	errorno = gpio_direction_input(data->mcu_host_wake_int);
#endif

	if (of_property_read_u32(np, "ssp-acc-position", &data->accel_position))
		data->accel_position = 0;
	if (of_property_read_u32(np, "ssp-mag-position", &data->mag_position))
		data->mag_position = 0;

	pr_info("[SSP] acc-posi[%d] mag-posi[%d]\n",
		data->accel_position, data->mag_position);

	/* acc type */
	if (of_property_read_u32(np, "ssp-acc-type", &data->acc_type))
		data->acc_type = 0;
	pr_info("[SSP] acc-type = %d\n", data->acc_type);

	/* mag type */
	if (of_property_read_u32(np, "ssp-mag-type", &data->mag_type))
		data->mag_type = 0;
	pr_info("[SSP] mag-type = %d\n", data->mag_type);

	if (of_property_read_u32(np, "ssp-ap-rev", &data->ap_rev))
		data->ap_rev = 0;

#if defined(CONFIG_SENSORS_SSP_PROX_AUTOCAL_AMS)
#ifndef CONFIG_SENSORS_SSP_LIGHT_COLORID
	if (of_property_read_u32(np, "ssp,prox-hi_thresh",
			&data->uProxHiThresh))
		data->uProxHiThresh = DEFUALT_HIGH_THRESHOLD;

	if (of_property_read_u32(np, "ssp,prox-low_thresh",
			&data->uProxLoThresh))
		data->uProxLoThresh = DEFUALT_LOW_THRESHOLD;

	pr_info("[SSP] hi-thresh[%u] low-thresh[%u]\n",
		data->uProxHiThresh, data->uProxLoThresh);

	if (of_property_read_u32(np, "ssp,prox-detect_hi_thresh",
			&data->uProxHiThresh_detect))
		data->uProxHiThresh_detect = DEFUALT_DETECT_HIGH_THRESHOLD;

	if (of_property_read_u32(np, "ssp,prox-detect_LOW_thresh",
			&data->uProxLoThresh_detect))
		data->uProxLoThresh_detect = DEFUALT_DETECT_LOW_THRESHOLD;
#if defined(CONFIG_SENSORS_SSP_HAECHI_880)
	memcpy(data->light_coef, haechi880_light_coef, sizeof(haechi880_light_coef));
#endif
	pr_info("[SSP] detect-hi[%u] detect-low[%u]\n",
		data->uProxHiThresh_detect, data->uProxLoThresh_detect);
#endif
#else /* CONFIG_SENSORS_SSP_PROX_FACTORYCAL */
	if (of_property_read_u32(np, "ssp,prox-hi_thresh",
			&data->uProxHiThresh_default))
		data->uProxHiThresh_default = DEFUALT_HIGH_THRESHOLD;


	if (of_property_read_u32(np, "ssp,prox-low_thresh",
			&data->uProxLoThresh_default))
		data->uProxLoThresh_default = DEFUALT_LOW_THRESHOLD;

	pr_info("[SSP] hi-thresh[%u] low-thresh[%u]\n",
		data->uProxHiThresh_default, data->uProxLoThresh_default);

	if (of_property_read_u32(np, "ssp,prox-cal_hi_thresh",
			&data->uProxHiThresh_cal))
		data->uProxHiThresh_cal = DEFUALT_CAL_HIGH_THRESHOLD;


	if (of_property_read_u32(np, "ssp,prox-cal_LOW_thresh",
			&data->uProxLoThresh_cal))
		data->uProxLoThresh_cal = DEFUALT_CAL_LOW_THRESHOLD;

	pr_info("[SSP] cal-hi[%u] cal-low[%u]\n",
		data->uProxHiThresh_cal, data->uProxLoThresh_cal);

#ifdef CONFIG_SENSORS_SSP_PROX_DUALIZATION
	//for tmd4904
	data->uProxHiThresh_default_tmd4904 = DEFUALT_HIGH_THRESHOLD_TMD4904;
	data->uProxLoThresh_default_tmd4904 = DEFUALT_LOW_THRESHOLD_TMD4904;
	data->uProxHiThresh_cal_tmd4904 = DEFUALT_CAL_HIGH_THRESHOLD_TMD4904;
	data->uProxLoThresh_cal_tmd4904 = DEFUALT_CAL_LOW_THRESHOLD_TMD4904;

	pr_info("[SSP] TMD4904 hi-thresh[%u] low-thresh[%u] cal-hi[%u] cal-low[%u]\n",
		data->uProxHiThresh_default_tmd4904, data->uProxLoThresh_default_tmd4904,
		data->uProxHiThresh_cal_tmd4904, data->uProxLoThresh_cal_tmd4904);
#endif
#endif

	data->uProxAlertHiThresh = DEFUALT_PROX_ALERT_HIGH_THRESHOLD;

#ifdef CONFIG_SENSORS_MULTIPLE_GLASS_TYPE
	if (of_property_read_u32(np, "ssp-glass-type", &data->glass_type))
		data->glass_type = 0;
#endif

	/* magnetic matrix */
	if(data->mag_type == 1)
	{
		if (of_property_read_u8_array(np, "ssp-mag-array",
		data->pdc_matrix, sizeof(data->pdc_matrix)))
		pr_err("no mag-array, set as 0");
	}
	else
	{
		if (!of_get_property(np, "ssp-mag-array", &len)) {
			pr_info("[SSP] No static matrix at DT for YAS532!(%p)\n",
					data->static_matrix);
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
				pr_err("[SSP] %s cannot get u32 of array[%d]!\n",
					__func__, i);
				goto dt_exit;
			}
			*(data->static_matrix+i) = (int)temp;
		}
	}

	/* Hall IC threshold */
	if (of_property_read_u16_array(np, "ssp-hall-threshold",
		data->hall_threshold, ARRAY_SIZE(data->hall_threshold))) {
		pr_err("[SSP]: %s - no hall-threshold, set as 0\n", __func__);
	} else {
		pr_info("[SSP]: %s - hall thr: %d %d %d %d %d\n", __func__,
		data->hall_threshold[0], data->hall_threshold[1],
		data->hall_threshold[2], data->hall_threshold[3],
		data->hall_threshold[4]);
	}
	
	return errorno;

dt_exit:
	if (data->static_matrix != NULL)
		kfree(data->static_matrix);
	return errorno;
}

#if defined(CONFIG_MUIC_NOTIFIER)
static int exynos_cpuidle_muic_notifier(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct ssp_data *ssp_data;
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;

	ssp_data = container_of(nb, struct ssp_data, cpuidle_muic_nb);

	switch (attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			ssp_data->jig_is_attached = false;
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			ssp_data->jig_is_attached = true;
		else
			pr_err("[SSP] %s: ACTION Error!\n", __func__);
		break;
	default:
		break;
	}

	pr_info("[SSP] %s: dev=%d, action=%lu\n",
			__func__, attached_dev, action);

	return NOTIFY_DONE;
}
#endif

#if defined (CONFIG_SENSORS_SSP_VLTE)
static int ssp_hall_ic_notify(struct notifier_block *nb,
				unsigned long action, void *v)
{
	pr_info("[SSP] %s is called : fold state %lu\n", __func__, action);
	ssp_ckeck_lcd((int) action);
	return 0;
}
#endif

#if defined(CONFIG_SSP_MOTOR)
static struct ssp_data *ssp_data_info = NULL;
void set_ssp_data_info(struct ssp_data *data)
{
	if (data != NULL)
		ssp_data_info = data;
	else
		pr_info("[SSP] %s : ssp data info is null\n", __func__);
}

int ssp_motor_callback(int state)
{
	int iRet = 0;
	
	ssp_data_info->motor_state = state;

	queue_work(ssp_data_info->ssp_motor_wq,
			&ssp_data_info->work_ssp_motor);

	pr_info("[SSP] %s : Motor state %d\n",__func__, state);

	return iRet;
}
int get_current_motor_state(void)
{
	return ssp_data_info->motor_state;
}
int (*getMotorCallback(void))(int)
{
	pr_info("[SSP] %s : called \n",__func__);
	return ssp_motor_callback;
}

void ssp_motor_work_func(struct work_struct *work)
{
	int iRet = 0;
	struct ssp_data *data = container_of(work,
					struct ssp_data, work_ssp_motor);

	iRet = send_motor_state(data);
	pr_info("[SSP] %s : Motor state %d, iRet %d\n",__func__, data->motor_state, iRet);
}
#endif

#if ANDROID_VERSION >= 80000
void ssp_timestamp_sync_work_func(struct work_struct *work)
{
	struct ssp_data *data = container_of((struct delayed_work *)work,
					struct ssp_data, work_ssp_tiemstamp_sync);
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = MSG2AP_INST_TIMESTAMP_OFFSET;
	msg->length = sizeof(data->timestamp_offset);
	msg->options = AP2HUB_WRITE;
	msg->buffer = (char *) kzalloc(sizeof(data->timestamp_offset), GFP_KERNEL);

	pr_info("handle_timestamp_sync: %lld\n", data->timestamp_offset);
	memcpy(msg->buffer, &(data->timestamp_offset), sizeof(data->timestamp_offset));

	ssp_spi_sync(data, msg, 1000);
	//pr_info("[SSP] %s : Motor state %d, iRet %d\n",__func__, data->motor_state, iRet);
}
#endif

void ssp_reset_work_func(struct work_struct *work)
{
	struct ssp_data *data = container_of((struct delayed_work *)work,
					struct ssp_data, work_ssp_reset);
	u64 current_timestamp = get_current_timestamp();

	pr_err("[SSP]: resumetimestamp %lld, current_timestamp %lld\n", data->resumeTimestamp, current_timestamp);
	if (data->resetting == false && current_timestamp - data->resumeTimestamp < 3000000000ULL) {
		mutex_lock(&data->ssp_enable_mutex);
		pr_err("[SSP]: reset scenario, flip cover issue.\n");
		reset_mcu(data);
		mutex_unlock(&data->ssp_enable_mutex);
	}
}

static int ssp_probe(struct spi_device *spi)
{
	struct ssp_data *data;
	struct ssp_platform_data *pdata;
	int iRet = 0;

	pr_info("[SSP] %s, is called\n", __func__);

	data = kzalloc(sizeof(*data), GFP_KERNEL);

	if (data == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to allocate memory for data\n",
			__func__);
		goto exit;
	}

	if (spi->dev.of_node) {
		iRet = ssp_parse_dt(&spi->dev, data);
		if (iRet) {
			pr_err("[SSP]: %s - Failed to parse DT\n", __func__);
			goto err_setup;
		}
		/* data->ssp_changes = SSP_MCU_L5; *//* K330, MPU L5*/
	} else {
		pdata = spi->dev.platform_data;
		if (pdata == NULL) {
			pr_err("[SSP] %s, platform_data is null\n", __func__);
			iRet = -ENOMEM;
			goto err_setup;
		}

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

	spi->mode = SPI_MODE_3;
	if (spi_setup(spi)) {
		pr_err("[SSP] %s, failed to setup spi\n", __func__);
		iRet = -ENODEV;
		goto err_setup;
	}

	data->bProbeIsDone = false;
	data->spi = spi;
	spi_set_drvdata(spi, data);

#ifdef CONFIG_SENSORS_SSP_SHTC1
	mutex_init(&data->cp_temp_adc_lock);
	mutex_init(&data->bulk_temp_read_lock);
#endif

	mutex_init(&data->comm_mutex);
	mutex_init(&data->pending_mutex);
	mutex_init(&data->enable_mutex);
	mutex_init(&data->ssp_enable_mutex);

	if (spi->dev.of_node == NULL) {
		pr_err("[SSP] %s, function callback is null\n", __func__);
		iRet = -EIO;
		goto err_reset_null;
	}

	pr_info("\n#####################################################\n");

	initialize_variable(data);
	wake_lock_init(&data->ssp_wake_lock,
		WAKE_LOCK_SUSPEND, "ssp_wake_lock");

	wake_lock_init(&data->ssp_comm_wake_lock,
		WAKE_LOCK_SUSPEND, "ssp_comm_wake_lock");

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

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	/* init sensorhub device */
	iRet = ssp_sensorhub_initialize(data);
	if (iRet < 0) {
		pr_err("%s: ssp_sensorhub_initialize err(%d)\n",
			__func__, iRet);
		ssp_sensorhub_remove(data);
	}
#endif
#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING
	/* HIFI Sensor + Batch Support */
	mutex_init(&data->batch_events_lock);

	data->batch_wq = create_singlethread_workqueue("ssp_batch_wq");
	if (!data->batch_wq) {
		iRet = -1;
		pr_err("[SSP]: %s - could not create batch workqueue\n",
			__func__);
		goto err_create_batch_workqueue;
	}

	/* HIFI Sensor + Normal Indexing */
	data->mcu_host_wake_irq = gpio_to_irq(data->mcu_host_wake_int);

	iRet = request_irq(data->mcu_host_wake_irq,
			ssp_mcu_host_wake_irq_handler,
			IRQF_TRIGGER_FALLING, "ssp-batch-wake-irq", data);

	/* pr_info("[SSP_IRQ]: request_irq(%d) for gpio %d (%d)\n",
		data->mcu_host_wake_int,
		data->mcu_host_wake_irq,
		iRet); */

	if (iRet < 0) {
		pr_info("[SSP_IRQ]: request_irq(%d) failed for gpio %d (%d)\n",
				data->mcu_host_wake_int,
				data->mcu_host_wake_irq,
				iRet);
		iRet = -ENODEV;
		goto err_request_irq;
	}
	disable_irq(data->mcu_host_wake_irq);
	data->ts_stacked_cnt = 0;
#endif
	bbd_register(data, &ssp_bbd_callbacks);

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.suspend = ssp_early_suspend;
	data->early_suspend.resume = ssp_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

#if defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&data->cpuidle_muic_nb,
		exynos_cpuidle_muic_notifier, MUIC_NOTIFY_DEV_CPUIDLE);
#endif

#if defined (CONFIG_SENSORS_SSP_VLTE)
	data->hall_ic_nb.notifier_call = ssp_hall_ic_notify;
	hall_ic_register_notify(&data->hall_ic_nb);
#endif


	pr_info("[SSP]: %s - probe success!\n", __func__);

	enable_debug_timer(data);
	data->bProbeIsDone = true;
	iRet = 0;
	mutex_init(&shutdown_lock);

#ifdef CONFIG_SSP_MOTOR
	pr_info("[SSP]: %s motor callback set!", __func__);
	set_ssp_data_info(data);
	//register motor
	setMotorCallback(ssp_motor_callback);

	data->ssp_motor_wq =
		create_singlethread_workqueue("ssp_motor_wq");
	if (!data->ssp_motor_wq) {
		iRet = -1;
		pr_err("[SSP]: %s - could not create motor workqueue\n",
			__func__);
		goto err_create_motor_workqueue;
	}
	
	INIT_WORK(&data->work_ssp_motor, ssp_motor_work_func);
#endif
#if ANDROID_VERSION >= 80000
	INIT_DELAYED_WORK(&data->work_ssp_tiemstamp_sync, ssp_timestamp_sync_work_func);
#endif
        INIT_DELAYED_WORK(&data->work_ssp_reset, ssp_reset_work_func);
	goto exit;

#ifdef CONFIG_SSP_MOTOR
err_create_motor_workqueue:
	destroy_workqueue(data->ssp_motor_wq);
#endif
#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING
/* Test PIN for Camera - Gyro Sync */
err_request_irq:
	gpio_free(data->mcu_host_wake_int);
err_create_batch_workqueue:
	destroy_workqueue(data->batch_wq);
	mutex_destroy(&data->batch_events_lock);
#endif
err_symlink_create:
	remove_sysfs(data);
err_sysfs_create:
	destroy_workqueue(data->debug_wq);
err_create_workqueue:
	remove_input_dev(data);
err_input_register_device:
	wake_lock_destroy(&data->ssp_comm_wake_lock);
	wake_lock_destroy(&data->ssp_wake_lock);
err_reset_null:
	mutex_destroy(&data->comm_mutex);
	mutex_destroy(&data->pending_mutex);
	mutex_destroy(&data->enable_mutex);
	mutex_destroy(&data->ssp_enable_mutex);
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

	pr_err("[SSP] lpm %d recovery\n", lpcharge);
	func_dbg();
	if (data->bProbeIsDone == false)
		goto exit;

	disable_debug_timer(data);

	/* hoi
	if (SUCCESS != ssp_send_cmd(data, MSG2SSP_AP_STATUS_SHUTDOWN, 0))
		pr_err("[SSP]: %s MSG2SSP_AP_STATUS_SHUTDOWN failed\n",
			__func__);
	*/
#if defined (CONFIG_SENSORS_SSP_VLTE)
	// hall_ic unregister
	hall_ic_unregister_notify(&data->hall_ic_nb);
#endif
	mutex_lock(&data->ssp_enable_mutex);
	ssp_enable(data, false);
	clean_pending_list(data);
	mutex_unlock(&data->ssp_enable_mutex);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif

	bbd_register(NULL, NULL);

	mutex_lock(&shutdown_lock);

	cancel_work_sync(&data->work_bbd_on_packet); /* should be cancel before removing iio dev */
	destroy_workqueue(data->bbd_on_packet_wq);
	cancel_work_sync(&data->work_bbd_mcu_ready);
	destroy_workqueue(data->bbd_mcu_ready_wq);
#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING
	gpio_free(data->mcu_host_wake_int); /* HIFI */
	destroy_workqueue(data->batch_wq); /* HIFI */
	mutex_destroy(&data->batch_events_lock);
#endif
	data->bbd_on_packet_wq = NULL;
	data->bbd_mcu_ready_wq = NULL;

#ifdef CONFIG_SSP_MOTOR
	cancel_work_sync(&data->work_ssp_motor);
	destroy_workqueue(data->ssp_motor_wq);

	data->ssp_motor_wq = NULL;
#endif

	mutex_unlock(&shutdown_lock);

	remove_event_symlink(data);
	remove_sysfs(data);
	remove_input_dev(data);

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	ssp_sensorhub_remove(data);
#endif

	del_timer_sync(&data->debug_timer);
	cancel_work_sync(&data->work_debug);
	destroy_workqueue(data->debug_wq);
	wake_lock_destroy(&data->ssp_comm_wake_lock);
	wake_lock_destroy(&data->ssp_wake_lock);
#ifdef CONFIG_SENSORS_SSP_SHTC1
	mutex_destroy(&data->bulk_temp_read_lock);
	mutex_destroy(&data->cp_temp_adc_lock);
#endif
	mutex_destroy(&data->comm_mutex);
	mutex_destroy(&data->pending_mutex);
	mutex_destroy(&data->enable_mutex);
	mutex_destroy(&data->ssp_enable_mutex);
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
	if (atomic64_read(&data->aSensorEnable) > 0)
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
	if (atomic64_read(&data->aSensorEnable) > 0)
		ssp_resume_mode(data);
#endif
}

#else /* no early suspend */

static int ssp_suspend(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct ssp_data *data = spi_get_drvdata(spi);

	func_dbg();

	if (SUCCESS != ssp_send_cmd(data, MSG2SSP_AP_STATUS_SUSPEND, 0))
		pr_err("[SSP]: %s MSG2SSP_AP_STATUS_SUSPEND failed\n",
			__func__);

	data->uLastResumeState = MSG2SSP_AP_STATUS_SUSPEND;
	disable_debug_timer(data);

	data->bTimeSyncing = false;
	pr_info("[SSP]: isHandlingIrq:%d\n", data->bHandlingIrq);

	return 0;
}

static int ssp_resume(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct ssp_data *data = spi_get_drvdata(spi);

	func_dbg();
	enable_debug_timer(data);
#ifdef CONFIG_SENSORS_SSP_HIFI_BATCHING
	data->resumeTimestamp = get_current_timestamp();
	data->bIsResumed = true;
#endif
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
	{"ssp-spi", 0},
	{}
};

MODULE_DEVICE_TABLE(spi, ssp_id);

static struct spi_driver ssp_driver = {
	.probe = ssp_probe,
	.shutdown = ssp_shutdown,
	.id_table = ssp_id,
	.driver = {
#ifndef CONFIG_HAS_EARLYSUSPEND
		   .pm = &ssp_pm_ops,
#endif
		   .owner = THIS_MODULE,
		   .name = "ssp-spi",
	},
};

struct spi_driver *pssp_driver = &ssp_driver;
module_spi_driver(ssp_driver);
MODULE_DESCRIPTION("ssp spi driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
