/* Samsung Touchscreen Controller Driver.
 * Copyright (c) 2007-2012, Samsung Electronics
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/unaligned.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/ctype.h>
#include <linux/hrtimer.h>
#include <linux/firmware.h>
#include <linux/sec_sysfs.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include "sec_ts.h"
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
#include <linux/t-base-tui.h>
#endif

#define tostring(x) (#x)

#define FT_CMD(name, func) .cmd_name = name, .cmd_func = func

extern struct class *sec_class;

enum {
	TYPE_RAW_DATA = 0,		/*  Tota cap - offset(19) = remnant dta */
	TYPE_SIGNAL_DATA = 1,
	TYPE_AMBIENT_BASELINE = 2,	/* Cap Baseline */
	TYPE_AMBIENT_DATA = 3,		/* Cap Ambient */
	TYPE_REMV_BASELINE_DATA = 4,
	TYPE_DECODED_DATA = 5,
	TYPE_REMV_AMB_DATA = 6,
	TYPE_OFFSET_DATA_SEC = 19,	/* Cap Offset for Normal Touch */
	TYPE_OFFSET_DATA_SDC = 29,	/* Cap Offset in SDC */
	TYPE_INVALID_DATA = 0xFF, /* Invalid data type for release factory mode*/
};

enum CMD_STATUS {
	CMD_STATUS_WAITING = 0,
	CMD_STATUS_RUNNING,
	CMD_STATUS_OK,
	CMD_STATUS_FAIL,
	CMD_STATUS_NOT_APPLICABLE,
};

struct ft_cmd {
	struct list_head list;
	const char *cmd_name;
	void (*cmd_func)(void *device_data);
};

static ssize_t cmd_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count);
static ssize_t cmd_status_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t cmd_result_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t cmd_list_show(struct device *dev,
				struct device_attribute *attr, char *buf);
static ssize_t scrub_position_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t edge_x_position(struct device *dev,
		struct device_attribute *attr, char *buf);

static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, cmd_store);
static DEVICE_ATTR(cmd_status, S_IRUGO, cmd_status_show, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, cmd_result_show, NULL);
static DEVICE_ATTR(cmd_list, S_IRUGO, cmd_list_show, NULL);
static DEVICE_ATTR(scrub_pos, S_IRUGO, scrub_position_show, NULL);
static DEVICE_ATTR(edge_pos, S_IRUGO, edge_x_position, NULL);

static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_config_ver(void *device_data);
static void get_cm_spec_over(void *device_data);
static void get_threshold(void *device_data);
static void module_off_master(void *device_data);
static void module_on_master(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void set_mis_cal_spec(void *device_data);
static void get_mis_cal_info(void *device_data);
static void get_wet_mode(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void get_x_cross_routing(void *device_data);
static void get_y_cross_routing(void *device_data);
static void get_checksum_data(void *device_data);
static void get_skip_touchevent_duty(void *device_data);
static void run_reference_read(void *device_data);
static void run_reference_read_all(void *device_data);
static void get_reference(void *device_data);
static void run_rawcap_read(void *device_data);
static void run_rawcap_read_all(void *device_data);
static void get_rawcap(void *device_data);
static void run_delta_read(void *device_data);
static void run_delta_read_all(void *device_data);
static void get_delta(void *device_data);
static void run_self_reference_read(void *device_data);
static void run_self_reference_read_all(void *device_data);
static void run_self_rawcap_read(void *device_data);
static void run_self_rawcap_read_all(void *device_data);
static void run_self_delta_read(void *device_data);
static void run_self_delta_read_all(void *device_data);
static void get_selfcap(void *device_data);
static void run_force_calibration(void *device_data);
static void set_external_factory(void *device_data);
static void get_force_calibration(void *device_data);
static void set_tsp_test_result(void *device_data);
static void get_tsp_test_result(void *device_data);
static void run_trx_short_test(void *device_data);
static void increase_disassemble_count(void *device_data);
static void get_disassemble_count(void *device_data);
static void glove_mode(void *device_data);
static void hover_enable(void *device_data);
static void clear_cover_mode(void *device_data);
static void dead_zone_enable(void *device_data);
#ifdef SMARTCOVER_COVER
static void smartcover_cmd(void *device_data);
#endif
static void set_lowpower_mode(void *device_data);
static void set_wirelesscharger_mode(void *device_data);
static void set_tsp_disable_mode(void *device_data);
static void spay_enable(void *device_data);
static void set_aod_rect(void *device_data);
static void get_aod_rect(void *device_data);
#ifdef SEC_TS_SUPPORT_5MM_SAR_MODE
static void sar_enable(void *device_data);
#endif
static void aod_enable(void *device_data);
static void second_screen_enable(void *device_data);
static void set_log_level(void *device_data);
static void set_tunning_data(void *device_data);
#ifdef TWO_LEVEL_GRIP_CONCEPT
static void set_grip_data(void *device_data);
#endif
#ifdef CONFIG_TOUCHSCREEN_SUPPORT_MULTIMEDIA
static void brush_enable(void *device_data);
static void velocity_enable(void *device_data);
#endif
static void not_support_cmd(void *device_data);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
extern int tui_force_close(uint32_t arg);
#endif

struct ft_cmd ft_cmds[] = {
	{FT_CMD("fw_update", fw_update),},
	{FT_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{FT_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{FT_CMD("get_config_ver", get_config_ver),},
	{FT_CMD("get_threshold", get_threshold),},
	{FT_CMD("get_cm_spec_over", get_cm_spec_over),},
	{FT_CMD("module_off_master", module_off_master),},
	{FT_CMD("module_on_master", module_on_master),},
	/* {FT_CMD("module_off_slave", not_support_cmd),}, */
	/* {FT_CMD("module_on_slave", not_support_cmd),}, */
	{FT_CMD("get_chip_vendor", get_chip_vendor),},
	{FT_CMD("get_chip_name", get_chip_name),},
	{FT_CMD("set_mis_cal_spec", set_mis_cal_spec),},
	{FT_CMD("get_mis_cal_info", get_mis_cal_info),},
	{FT_CMD("get_wet_mode", get_wet_mode),},	
	{FT_CMD("get_x_num", get_x_num),},
	{FT_CMD("get_y_num", get_y_num),},
	{FT_CMD("get_x_cross_routing", get_x_cross_routing),},
	{FT_CMD("get_y_cross_routing", get_y_cross_routing),},
	{FT_CMD("get_checksum_data", get_checksum_data),},
	{FT_CMD("get_skip_touchevent_duty", get_skip_touchevent_duty),},
	{FT_CMD("run_reference_read", run_reference_read),},
	{FT_CMD("run_reference_read_all", run_reference_read_all),},
	{FT_CMD("get_reference", get_reference),},
	{FT_CMD("run_rawcap_read", run_rawcap_read),},
	{FT_CMD("run_rawcap_read_all", run_rawcap_read_all),},
	{FT_CMD("get_rawcap", get_rawcap),},
	{FT_CMD("run_delta_read", run_delta_read),},
	{FT_CMD("run_delta_read_all", run_delta_read_all),},
	{FT_CMD("get_delta", get_delta),},
	{FT_CMD("run_self_reference_read", run_self_reference_read),},
	{FT_CMD("run_self_reference_read_all", run_self_reference_read_all),},
	{FT_CMD("run_self_rawcap_read", run_self_rawcap_read),},
	{FT_CMD("run_self_rawcap_read_all", run_self_rawcap_read_all),},
	{FT_CMD("run_self_delta_read", run_self_delta_read),},
	{FT_CMD("run_self_delta_read_all", run_self_delta_read_all),},
	{FT_CMD("get_selfcap", get_selfcap),},
	{FT_CMD("run_force_calibration", run_force_calibration),},
	{FT_CMD("get_force_calibration", get_force_calibration),},
	{FT_CMD("set_external_factory", set_external_factory),},
	{FT_CMD("run_trx_short_test", run_trx_short_test),},
	{FT_CMD("set_tsp_test_result", set_tsp_test_result),},
	{FT_CMD("get_tsp_test_result", get_tsp_test_result),},
	{FT_CMD("increase_disassemble_count", increase_disassemble_count),},
	{FT_CMD("get_disassemble_count", get_disassemble_count),},
	{FT_CMD("glove_mode", glove_mode),},
	{FT_CMD("hover_enable", hover_enable),},
	{FT_CMD("clear_cover_mode", clear_cover_mode),},
	{FT_CMD("dead_zone_enable", dead_zone_enable),},
#ifdef SMARTCOVER_COVER
	{FT_CMD("smartcover_cmd", smartcover_cmd),},
#endif
	{FT_CMD("set_lowpower_mode", set_lowpower_mode),},
	{FT_CMD("set_wirelesscharger_mode", set_wirelesscharger_mode),},
	{FT_CMD("set_tsp_disable_mode", set_tsp_disable_mode),},
	{FT_CMD("spay_enable", spay_enable),},
	{FT_CMD("set_aod_rect", set_aod_rect),},
	{FT_CMD("get_aod_rect", get_aod_rect),},
#ifdef SEC_TS_SUPPORT_5MM_SAR_MODE
	{FT_CMD("sar_enable", sar_enable),},
#endif
	{FT_CMD("aod_enable", aod_enable),},
	{FT_CMD("second_screen_enable", second_screen_enable),},
	{FT_CMD("set_tunning_data", set_tunning_data),},
#ifdef TWO_LEVEL_GRIP_CONCEPT
	{FT_CMD("set_grip_data", set_grip_data),},
#endif
	{FT_CMD("set_log_level", set_log_level),},
#ifdef CONFIG_TOUCHSCREEN_SUPPORT_MULTIMEDIA
	{FT_CMD("velocity_enable", velocity_enable),},
	{FT_CMD("brush_enable", brush_enable),},   
#endif
	{FT_CMD("not_support_cmd", not_support_cmd),},
};

#ifdef USE_HW_PARAMETER
static ssize_t read_ito_check_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	char buff[256] = { 0 };

	input_info(true, &ts->client->dev, "%s: %02X%02X%02X%02X\n", __func__,
		ts->ito_test[0], ts->ito_test[1],
		ts->ito_test[2], ts->ito_test[3]);

	snprintf(buff, sizeof(buff), "%02X%02X%02X%02X",
		ts->ito_test[0], ts->ito_test[1],
		ts->ito_test[2], ts->ito_test[3]);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t read_raw_check_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int ii, ret = 0;
	char temp[CMD_RESULT_WORD_LEN] = { 0 };
	char *buffer = NULL;
/*
 * read rawcap / ambient data
 */

	buffer = vzalloc(ts->rx_count * ts->tx_count * 6);
	if (!buffer)
		return -ENOMEM;
	
	memset(buffer, 0x00, ts->rx_count * ts->tx_count * 6);

	for (ii = 0; ii < (ts->rx_count * ts->tx_count - 1); ii++) {
		snprintf(temp, CMD_RESULT_WORD_LEN, "%d ", ts->pFrame[ii]);
		strncat(buffer, temp, CMD_RESULT_WORD_LEN);
	
		memset(temp, 0x00, CMD_RESULT_WORD_LEN);
	}

	snprintf(temp, CMD_RESULT_WORD_LEN, "%d", ts->pFrame[ii]);
	strncat(buffer, temp, CMD_RESULT_WORD_LEN);

	ret = snprintf(buf, ts->rx_count * ts->tx_count * 6, buffer);
	vfree(buffer);

	return ret;
}

static ssize_t read_multi_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	char buff[256] = { 0 };

	input_info(true, &ts->client->dev, "%s: %d\n", __func__,
		ts->multi_count);

	snprintf(buff, sizeof(buff), "%d", ts->multi_count);

	ts->multi_count = 0;

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t read_wet_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	char buff[256] = { 0 };

	input_info(true, &ts->client->dev, "%s: %d, %d\n", __func__,
		ts->wet_count, ts->dive_count);

	snprintf(buff, sizeof(buff), "%d",
		ts->wet_count);

	ts->wet_count = 0;
	ts->dive_count= 0;

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t read_comm_err_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	char buff[256] = { 0 };

	input_info(true, &ts->client->dev, "%s: %d\n", __func__,
		ts->multi_count);

	snprintf(buff, sizeof(buff), "%d", ts->comm_err_count);

	ts->comm_err_count = 0;

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t read_module_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	char buff[256] = { 0 };

	input_info(true, &ts->client->dev, "%s: %d\n", __func__,
		ts->multi_count);

	snprintf(buff, sizeof(buff), "SE%02X%02X%02X%02X%02X",
		ts->plat_data->panel_revision, ts->plat_data->img_version_of_bin[2],
		ts->plat_data->img_version_of_bin[3], ts->nv, ts->cal_count);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static DEVICE_ATTR(ito_check, S_IRUGO, read_ito_check_show, NULL);
static DEVICE_ATTR(raw_check, S_IRUGO, read_raw_check_show, NULL);
static DEVICE_ATTR(multi_count, S_IRUGO, read_multi_count_show, NULL);
static DEVICE_ATTR(wet_mode, S_IRUGO, read_wet_mode_show, NULL);
static DEVICE_ATTR(comm_err_count, S_IRUGO, read_comm_err_count_show, NULL);
static DEVICE_ATTR(module_id, S_IRUGO, read_module_id_show, NULL);
#endif


static struct attribute *cmd_attributes[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_list.attr,
	&dev_attr_cmd_result.attr,
	&dev_attr_scrub_pos.attr,
	&dev_attr_edge_pos.attr,
#ifdef USE_HW_PARAMETER
	&dev_attr_ito_check.attr,
	&dev_attr_raw_check.attr,
	&dev_attr_multi_count.attr,
	&dev_attr_wet_mode.attr,
	&dev_attr_comm_err_count.attr,
	&dev_attr_module_id.attr,
#endif
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

static int sec_ts_check_index(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int node;

	if (ts->cmd_param[0] < 0
		|| ts->cmd_param[0] > ts->tx_count
		|| ts->cmd_param[1] < 0
		|| ts->cmd_param[1] > ts->rx_count) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		strncat(ts->cmd_result, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_FAIL;
		input_info(true, &ts->client->dev, "%s: parameter error: %u, %u\n",
				__func__, ts->cmd_param[0], ts->cmd_param[0]);
		node = -1;
		return node;
	}
	node = ts->cmd_param[1] * ts->tx_count + ts->cmd_param[0];
	input_info(true, &ts->client->dev, "%s: node = %d\n", __func__, node);
	return node;
}

static int sec_ts_check_self_index(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int node;

	if (ts->cmd_param[0] < 0
		|| ts->cmd_param[0] > (ts->tx_count + ts->rx_count)) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		strncat(ts->cmd_result, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_FAIL;
		input_info(true, &ts->client->dev, "%s: parameter error: %u, %u\n",
				__func__, ts->cmd_param[0], ts->cmd_param[0]);
		node = -1;
		return node;
	}
	node = ts->cmd_param[0];
	input_info(true, &ts->client->dev, "%s: node = %d\n", __func__, node);
	return node;
}

static void set_default_result(struct sec_ts_data *data)
{
	char delim = ':';

	memset(data->cmd_result, 0x00, CMD_RESULT_STR_LEN);
	memcpy(data->cmd_result, data->cmd, strnlen(data->cmd, CMD_STR_LEN));
	strncat(data->cmd_result, &delim, 1);

	return;
}

static void set_cmd_result(struct sec_ts_data *data, char *buf, int length)
{
	strncat(data->cmd_result, buf, length);

	return;
}

static void clear_cover_cmd_work(struct work_struct *work)
{
	struct sec_ts_data *ts = container_of(work, struct sec_ts_data,
													cover_cmd_work.work);
	if (ts->cmd_is_running) {
		schedule_delayed_work(&ts->cover_cmd_work, msecs_to_jiffies(5));
	} else {
		/* check lock   */
		mutex_lock(&ts->cmd_lock);
		ts->cmd_is_running = true;
		mutex_unlock(&ts->cmd_lock);

		ts->cmd_state = CMD_STATUS_RUNNING;
		input_info(true, &ts->client->dev, "%s param = %d, %d\n", __func__,\
		ts->delayed_cmd_param[0], ts->delayed_cmd_param[1]);

		ts->cmd_param[0] = ts->delayed_cmd_param[0];
		if (ts->delayed_cmd_param[0] > 1)
			ts->cmd_param[1] = ts->delayed_cmd_param[1];
		strcpy(ts->cmd, "clear_cover_mode");
		clear_cover_mode(ts);
	}
}
EXPORT_SYMBOL(clear_cover_cmd_work);

static void set_wirelesscharger_mode_work(struct work_struct *work)
{
	struct sec_ts_data *ts = container_of(work, struct sec_ts_data,
									set_wirelesscharger_mode_work.work);
	if (ts->cmd_is_running) {
		schedule_delayed_work(&ts->set_wirelesscharger_mode_work, msecs_to_jiffies(5));
	} else {
		/* check lock   */
		mutex_lock(&ts->cmd_lock);
		ts->cmd_is_running = true;
		mutex_unlock(&ts->cmd_lock);

		ts->cmd_state = CMD_STATUS_RUNNING;
		input_info(true, &ts->client->dev, "%s param = %d\n", __func__, ts->wirelesscharger_delayed_cmd_param[0]);

		ts->cmd_param[0] = ts->wirelesscharger_delayed_cmd_param[0];
		strcpy(ts->cmd, "set_wirelesscharger_mode");
		set_wirelesscharger_mode(ts);
	}
}

static ssize_t cmd_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned char param_cnt = 0;
	char *start;
	char *end;
	char *pos;
	char delim = ',';
	char buffer[CMD_STR_LEN];
	bool cmd_found = false;
	int *param;
	int length;
	struct ft_cmd *ft_cmd_ptr = NULL;
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	if (!ts) {
		printk(KERN_ERR "%s %s: No platform data found\n", SECLOG, __func__);
		return -EINVAL;
	}

	if (strlen(buf) >= CMD_STR_LEN) {
		input_err(true, &ts->client->dev, "%s: cmd length(strlen(buf)) is over (%d,%s)!!\n",
					__func__, (unsigned int)strlen(buf), buf);
		return -EINVAL;
	}

	if (count >= (unsigned int)CMD_STR_LEN) {
		input_err(true, &ts->client->dev, "%s %s: cmd length(count) is over (%d,%s)!!\n",
				SECLOG, __func__, (unsigned int)count, buf);
		return -EINVAL;
	}

#if 1
	if (ts->cmd_is_running == true) {
		input_err(true, &ts->client->dev, "%s: other cmd is running.\n", __func__);
		if (strncmp("clear_cover_mode", buf, 16) == 0) {
			cancel_delayed_work(&ts->cover_cmd_work);
			input_err(true, &ts->client->dev,
						"[cmd is delayed] %d, param = %d, %d\n", __LINE__, buf[17] - '0', buf[19] - '0');
			ts->delayed_cmd_param[0] = buf[17] - '0';
			if (ts->delayed_cmd_param[0] > 1)
				ts->delayed_cmd_param[1] = buf[19]-'0';
				schedule_delayed_work(&ts->cover_cmd_work, msecs_to_jiffies(10));
		}
		if (strncmp("set_wirelesscharger_mode", buf, 24) == 0) {
			cancel_delayed_work(&ts->set_wirelesscharger_mode_work);
			input_err(true, &ts->client->dev,
						"[cmd is delayed] %d, param = %d\n", __LINE__, buf[25] - '0');
			ts->wirelesscharger_delayed_cmd_param[0] = buf[25] - '0';
				schedule_delayed_work(&ts->set_wirelesscharger_mode_work, msecs_to_jiffies(10));
		}
		return -EBUSY;
	}
	else if (ts->reinit_done == false) {
		input_err(true, &ts->client->dev, "%s: reinit is working\n", __func__);
		if (strncmp("clear_cover_mode", buf, 16) == 0) {
			cancel_delayed_work(&ts->cover_cmd_work);
			input_err(true, &ts->client->dev,
						"[cmd is delayed] %d, param = %d, %d\n", __LINE__, buf[17]-'0', buf[19]-'0');
			ts->delayed_cmd_param[0] = buf[17] - '0';
			if (ts->delayed_cmd_param[0] > 1)
				ts->delayed_cmd_param[1] = buf[19]-'0';
			if(ts->delayed_cmd_param[0] == 0)
				schedule_delayed_work(&ts->cover_cmd_work, msecs_to_jiffies(300));
		}
	}
#endif
	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = true;
	mutex_unlock(&ts->cmd_lock);

	ts->cmd_state = CMD_STATUS_RUNNING;

	length = (int)count;
	if (*(buf + length - 1) == '\n')
		length--;

	memset(ts->cmd, 0x00, sizeof(ts->cmd));
	memcpy(ts->cmd, buf, length);
	memset(ts->cmd_param, 0, sizeof(ts->cmd_param));
	memset(buffer, 0x00, sizeof(buffer));

	pos = strchr(buf, (int)delim);
	if (pos)
		memcpy(buffer, buf, pos - buf);
	else
		memcpy(buffer, buf, length);

	/* find command */
	list_for_each_entry(ft_cmd_ptr, &ts->cmd_list_head, list) {
		if (!strcmp(buffer, ft_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(ft_cmd_ptr,
			&ts->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", ft_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cmd_found && pos) {
		pos++;
		start = pos;
		memset(buffer, 0x00, sizeof(buffer));
		do {
			if ((*pos == delim) || (pos - buf == length)) {
				end = pos;
				memcpy(buffer, start, end - start);
				*(buffer + strlen(buffer)) = '\0';
				param = ts->cmd_param + param_cnt;
				if (kstrtoint(buffer, 10, param) < 0)
					goto err_out;
				param_cnt++;
				memset(buffer, 0x00, sizeof(buffer));
				start = pos + 1;
			}
			pos++;
		} while ((pos - buf <= length) && (param_cnt < CMD_PARAM_NUM));
	}

	input_err(true, &ts->client->dev, "%s: Command = %s\n", __func__, buf);

	ft_cmd_ptr->cmd_func(ts);

err_out:
	return count;
}

static ssize_t cmd_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	char buffer[16];

	input_err(true, &ts->client->dev, "%s: Command status = %d\n", __func__, ts->cmd_state);

	switch (ts->cmd_state) {
	case CMD_STATUS_WAITING:
		snprintf(buffer, sizeof(buffer), "%s", tostring(WAITING));
		break;
	case CMD_STATUS_RUNNING:
		snprintf(buffer, sizeof(buffer), "%s", tostring(RUNNING));
		break;
	case CMD_STATUS_OK:
		snprintf(buffer, sizeof(buffer), "%s", tostring(OK));
		break;
	case CMD_STATUS_FAIL:
		snprintf(buffer, sizeof(buffer), "%s", tostring(FAIL));
		break;
	case CMD_STATUS_NOT_APPLICABLE:
		snprintf(buffer, sizeof(buffer), "%s", tostring(NOT_APPLICABLE));
		break;
	default:
		snprintf(buffer, sizeof(buffer), "%s", tostring(NOT_APPLICABLE));
		break;
	}

	return snprintf(buf, CMD_RESULT_STR_LEN, "%s\n", buffer);
}

static ssize_t cmd_result_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	input_info(true, &ts->client->dev, "%s: Command result = %s\n", __func__, ts->cmd_result);

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	ts->cmd_state = CMD_STATUS_WAITING;

	return snprintf(buf, CMD_RESULT_STR_LEN, "%s\n", ts->cmd_result);
}

static ssize_t cmd_list_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	char buffer[ts->cmd_buffer_size];
	char buffer_name[CMD_STR_LEN];
	int ii = 0;

	snprintf(buffer, CMD_STR_LEN, "++factory command list++\n");
	while (strncmp(ft_cmds[ii].cmd_name, "not_support_cmd", 16) != 0) {
		snprintf(buffer_name, CMD_STR_LEN, "%s\n", ft_cmds[ii].cmd_name);
		strcat(buffer, buffer_name);
		ii++;
	}

	input_info(true, &ts->client->dev,
		"%s: length : %u / %d\n", __func__,
		(unsigned int)strlen(buffer), ts->cmd_buffer_size+CMD_STR_LEN);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buffer);
}

static ssize_t scrub_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	char buff[256] = { 0 };

#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
	input_info(true, &ts->client->dev,
			"%s: scrub_id: %d\n", __func__, ts->scrub_id);
#else
	input_info(true, &ts->client->dev,
			"%s: scrub_id: %d, X:%d, Y:%d\n", __func__,
			ts->scrub_id, ts->scrub_x, ts->scrub_y);
#endif

	snprintf(buff, sizeof(buff), "%d %d %d", ts->scrub_id, ts->scrub_x, ts->scrub_y);

	ts->scrub_id = 0;
	ts->scrub_x = 0;
	ts->scrub_y = 0;

	return snprintf(buf, PAGE_SIZE, "%s", buff);
}

static ssize_t edge_x_position(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	char buff[256] = { 0 };
	int edge_position_left, edge_position_right;

	if (!ts) {
		pr_err("%s %s: No platform data found\n",
				SECLOG, __func__);
		return -EINVAL;
	}

	if (!ts->input_dev) {
		pr_err("%s %s: No input_dev data found\n",
				SECLOG, __func__);
		return -EINVAL;
	}

	edge_position_left = ts->plat_data->grip_area;
	edge_position_right = ts->plat_data->max_x + 1 - ts->plat_data->grip_area;

	input_info(true, &ts->client->dev, "%s: %d,%d\n", __func__,
			edge_position_left, edge_position_right);
	snprintf(buff, sizeof(buff), "%d,%d", edge_position_left, edge_position_right);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buff);
}
static void fw_update(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[64] = { 0 };
	int retval = 0;

	set_default_result(ts);
	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
			 __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	retval = sec_ts_firmware_update_on_hidden_menu(ts, ts->cmd_param[0]);
	if (retval < 0) {
		snprintf(buff, sizeof(buff), "%s", "NA");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_FAIL;
		input_info(true, &ts->client->dev, "%s: failed [%d]\n", __func__, retval);
	} else {
		snprintf(buff, sizeof(buff), "%s", "OK");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_OK;
		input_info(true, &ts->client->dev, "%s: success [%d]\n", __func__, retval);
	}
	return;
}

static int sec_ts_fix_tmode(struct sec_ts_data *ts, u8 mode, u8 state)
{
	int ret;
	u8 onoff[1] = {STATE_MANAGE_OFF};
	u8 tBuff[2] = { mode, state };

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_STATEMANAGE_ON, onoff, 1);
	sec_ts_delay(20);

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_CHG_SYSMODE , tBuff, sizeof(tBuff));
	sec_ts_delay(20);
	return ret;
}

static int sec_ts_release_tmode(struct sec_ts_data *ts)
{
	int ret;
	u8 onoff[1] = {STATE_MANAGE_ON};
	u8 tBuff[2] = {TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH};

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_STATEMANAGE_ON, onoff, 1);
	sec_ts_delay(20);

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_CHG_SYSMODE , tBuff, sizeof(tBuff));
	sec_ts_delay(20);

	return ret;
}

static void sec_ts_cm_spec_over_check(struct sec_ts_data *ts)
{
	int i = 0;
	int j = 0;
	int by, bx, ty1, tx1, ty2, tx2, bpos, tpos1, tpos2;
	int vb, vt1, vt2, gap1, gap2, specover_count;

	input_info(true, &ts->client->dev, "%s\n", __func__);
	input_info(true, &ts->client->dev, "rx_max(%d)  tx_max(%d) \n", ts->rx_count, ts->tx_count);

	specover_count = 0;
	for (i = 0; i < ts->rx_count-1; i++) {
		for (j = 0; j < ts->tx_count-1; j++) {
			bx = i; by = j;  bpos = (by * ts->rx_count) + bx;
			tx1 = i + 1; ty1 = j;  tpos1 = (ty1 * ts->rx_count) + tx1;
			tx2 = i; ty2 = j + 1;  tpos2 = (ty2 * ts->rx_count) + tx2;
			vb = ts->pFrame[bpos]; vt1 = ts->pFrame[tpos1]; vt2 = ts->pFrame[tpos2];

			if (vb > vt1)
				gap1 = vb - vt1;
			else
				gap1 = vt1 - vb;
			if (vb > vt2)
				gap2 = vb - vt2;
			else
				gap2 = vt2 - vb;

			/* y direction */
			gap1 = gap1 * 100 / vb;
			/*  x direction */
			gap2 = gap2 * 100 / vb;

			/*
			 * input_info(true, &ts->client->dev, "b  y(%d) x(%d) p(%d) v(%d) \n", by,bx,bpos,vb);
			 * input_info(true, &ts->client->dev, "t1 y(%d) x(%d) p(%d) v(%d)  g(%d)\n", ty1,tx1,tpos1,vt1,gap1);
			 *input_info(true, &ts->client->dev, "t2 y(%d) x(%d) p(%d) v(%d)  g(%d)\n", ty2,tx2,tpos2,vt2,gap2);
			 */
			if (by == 0 || (by == ts->rx_count-2)) {
				if (bx == 0 || bx == ts->rx_count-2) {
					if (gap1 > 18)
						specover_count++;
					if (gap2 > 18)
						specover_count++;
				} else {
					if (gap1 > 10)
						specover_count++;
					if (gap2 > 10)
						specover_count++;
				}
			} else if (bx == 0 || (bx == ts->tx_count-2)) {
				if (by == 0 || by == ts->tx_count-2) {
					if (gap1 > 18)
						specover_count++;
					if (gap2 > 18)
						specover_count++;
				} else {
					if (gap1 > 10)
						specover_count++;
					if (gap2 > 10)
						specover_count++;
				}
			} else {
				/* check 	  x : 5    / y : 6 */
				if (gap1 > 6)
					specover_count++;
				if (gap2 > 5)
					specover_count++;
			}
		}
	}
	ts->cm_specover = specover_count;
	input_info(true, &ts->client->dev, "spect out(%d)\n",specover_count);
}

void sec_ts_print_frame(struct sec_ts_data *ts, short *min, short *max)
{
	int i = 0;
	int j = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };

	pStr = kzalloc(6 * (ts->tx_count + 1), GFP_KERNEL);
	if (pStr == NULL) {
		input_info(true, &ts->client->dev, "SEC_TS pStr kzalloc failed\n");
		return;
	}

	memset(pStr, 0x0, 6 * (ts->tx_count + 1));
	snprintf(pTmp, sizeof(pTmp), "    ");
	strncat(pStr, pTmp, 6 * ts->tx_count);

	for (i = 0; i < ts->tx_count; i++) {
		snprintf(pTmp, sizeof(pTmp), "Tx%02d  ", i);
		strncat(pStr, pTmp, 6 * ts->tx_count);
	}

	input_raw_info(true, &ts->client->dev, "SEC_TS %s\n", pStr);
	memset(pStr, 0x0, 6 * (ts->tx_count + 1));
	snprintf(pTmp, sizeof(pTmp), " +");
	strncat(pStr, pTmp, 6 * ts->tx_count);

	for (i = 0; i < ts->tx_count; i++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strncat(pStr, pTmp, 6 * ts->rx_count);
	}

	input_raw_info(true, &ts->client->dev, "SEC_TS %s\n", pStr);

	for (i = 0; i < ts->rx_count; i++) {
		memset(pStr, 0x0, 6 * (ts->tx_count + 1));
		snprintf(pTmp, sizeof(pTmp), "Rx%02d | ", i);
		strncat(pStr, pTmp, 6 * ts->tx_count);

		for (j = 0; j < ts->tx_count; j++) {
			snprintf(pTmp, sizeof(pTmp), "%5d ", ts->pFrame[(j * ts->rx_count) + i]);

			if (i > 0) {
				if (ts->pFrame[(j * ts->rx_count) + i] < *min)
					*min = ts->pFrame[(j * ts->rx_count) + i];

				if (ts->pFrame[(j * ts->rx_count) + i] > *max)
					*max = ts->pFrame[(j * ts->rx_count) + i];
			}
			strncat(pStr, pTmp, 6 * ts->rx_count);
		}
	input_raw_info(true, &ts->client->dev, "SEC_TS %s\n", pStr);
	}
	kfree(pStr);
}

int sec_ts_read_frame(struct sec_ts_data *ts, u8 type, short *min, short *max)
{
	unsigned int readbytes = 0xFF;
	unsigned char *pRead = NULL;
	u8 mode = TYPE_INVALID_DATA;
	int rc = 0;
	int ret = 0;
	int i = 0;
	int j = 0;
	short *temp = NULL;

	input_raw_info(true, &ts->client->dev, "%s: type %d\n", __func__, type);

	/* set data length, allocation buffer memory */
	readbytes = ts->rx_count * ts->tx_count * 2;

	pRead = kzalloc(readbytes, GFP_KERNEL);
	if (pRead == NULL) {
		input_info(true, &ts->client->dev, "Read frame kzalloc failed\n");
		rc = 1;
		return rc;
	}

	/* set OPCODE and data type */
	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_MUTU_RAW_TYPE, &type, 1);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "Set rawdata type failed\n");
		rc = 2;
		goto ErrorExit;
	}

	sec_ts_delay(50);

	if (type == TYPE_OFFSET_DATA_SDC) {
		/* excute selftest for real cap offset data, because real cap data is not memory data in normal touch. */
		char para = TO_TOUCH_MODE;
		disable_irq(ts->client->irq);
		execute_selftest(ts);
		ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SET_POWER_MODE, &para, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: set rawdata type failed!\n", __func__);
			enable_irq(ts->client->irq);
			goto ErrorRelease;
		}
		enable_irq(ts->client->irq);
		/* end */
	}

	/* read data */
	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_TOUCH_RAWDATA, pRead, readbytes);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read rawdata failed!\n", __func__);
		rc = 3;
		goto ErrorRelease;
	}

	memset(ts->pFrame, 0x00, readbytes);

	for (i = 0; i < readbytes; i += 2)
		ts->pFrame[i/2] = pRead[i+1] + (pRead[i] << 8);

	*min = *max = ts->pFrame[0];

#ifdef DEBUG_MSG
	input_info(true, &ts->client->dev, "02X%02X%02X readbytes=%d\n",
			pRead[0], pRead[1], pRead[2], readbytes);
#endif
	sec_ts_print_frame(ts, min, max);

	if (type == TYPE_OFFSET_DATA_SDC)
		sec_ts_cm_spec_over_check(ts);

	temp = kzalloc(readbytes, GFP_KERNEL);
	if (temp == NULL) {
		input_info(true, &ts->client->dev, "Read frame kzalloc failed\n");
		goto ErrorRelease;
	}

	memcpy(temp, ts->pFrame, readbytes);
	memset(ts->pFrame, 0x00, readbytes);

	for (i = 0; i < ts->tx_count; i++) {
		for (j = 0; j < ts->rx_count; j++)
			ts->pFrame[(j * ts->tx_count) + i] = temp[(i * ts->rx_count) + j];
	}

	kfree(temp);

ErrorRelease:
	/* release data monitory (unprepare AFE data memory) */
	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_MUTU_RAW_TYPE, &mode, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: set rawdata failed!\n", __func__);

ErrorExit:
	kfree(pRead);

	return rc;
}

void sec_ts_print_self_frame(struct sec_ts_data *ts, short *min, short *max,
			unsigned int num_long_ch, unsigned int num_short_ch)
{
	int i = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };

	pStr = kzalloc(6 * (num_short_ch + 1), GFP_KERNEL);
	if (pStr == NULL) {
		input_info(true, &ts->client->dev, "SEC_TS pStr kzalloc failed\n");
		return;
	}

	memset(pStr, 0x0, 6 * (num_short_ch + 1));
	snprintf(pTmp, sizeof(pTmp), "          ");
	strncat(pStr, pTmp, 6 * num_short_ch);

	for (i = 0; i < num_short_ch; i++) {
		snprintf(pTmp, sizeof(pTmp), "Sc%02d  ", i);
		strncat(pStr, pTmp, 6 * num_short_ch);
	}

	input_raw_info(true, &ts->client->dev, "SEC_TS %s\n", pStr);
	memset(pStr, 0x0, 6 * (num_short_ch + 1));
	snprintf(pTmp, sizeof(pTmp), "      +");
	strncat(pStr, pTmp, 6 * num_short_ch);

	for (i = 0; i < num_short_ch; i++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strncat(pStr, pTmp, 6 * num_short_ch);
	}

	input_raw_info(true, &ts->client->dev, "SEC_TS %s\n", pStr);

	memset(pStr, 0x0, 6 * (num_short_ch + 1));
	for (i = 0; i < num_short_ch; i++) {
		snprintf(pTmp, sizeof(pTmp), "%5d ", ts->sFrame[i]);
		strncat(pStr, pTmp, 6 * num_short_ch);
		if (ts->sFrame[i] < *min)
			*min = ts->sFrame[i];
		if (ts->sFrame[i] > *max)
			*max = ts->sFrame[i];
	}

	input_raw_info(true, &ts->client->dev, "SEC_TS        %s\n", pStr);

	for (i = 0; i < num_long_ch; i++) {
		memset(pStr, 0x0, 6 * (num_short_ch + 1));
		snprintf(pTmp, sizeof(pTmp), "Lc%02d | ", i);
		strncat(pStr, pTmp, 6 * num_short_ch);
		snprintf(pTmp, sizeof(pTmp), "%5d ", ts->sFrame[num_short_ch + i]);
		strncat(pStr, pTmp, 6 * num_short_ch);

		if (ts->sFrame[num_short_ch + i] < *min)
			*min = ts->sFrame[num_short_ch + i];
		if (ts->sFrame[num_short_ch + i] > *max)
			*max = ts->sFrame[num_short_ch + i];

		input_raw_info(true, &ts->client->dev, "SEC_TS %s\n", pStr);
	}
	kfree(pStr);
}

#define PRE_DEFINED_DATA_LENGTH		208
static int sec_ts_read_channel(struct sec_ts_data *ts, u8 type, short *min, short *max)
{
	unsigned char *pRead = NULL;
	int ret = 0;
	int ii = 0;
	int jj = 0;
	u8 w_data[2];
	u8 mode = TYPE_INVALID_DATA;

	input_raw_info(true, &ts->client->dev, "%s: type %d\n", __func__, type);

	pRead = kzalloc(PRE_DEFINED_DATA_LENGTH, GFP_KERNEL);
	if (IS_ERR_OR_NULL(pRead))
		return -ENOMEM;

	/* set OPCODE and data type */
	w_data[0] = type;

	ret = ts->sec_ts_i2c_write(ts,SEC_TS_CMD_SELF_RAW_TYPE, w_data, 1);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "Set rawdata type failed\n");
		goto out_read_channel;
	}

	sec_ts_delay(50);

	if (type == TYPE_OFFSET_DATA_SDC) {
		  /* excute selftest for real cap offset data, because real cap data is not memory data in normal touch. */
		  char para = TO_TOUCH_MODE;
		  disable_irq(ts->client->irq);
		  execute_selftest(ts);
		  ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SET_POWER_MODE, &para, 1);
		  if (ret < 0) {
			  input_err(true, &ts->client->dev, "%s: set rawdata type failed!\n", __func__);
			  enable_irq(ts->client->irq);
			  goto err_read_data;
		  }
		  enable_irq(ts->client->irq);
		  /* end */
	}

	/* read data */
	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_TOUCH_SELF_RAWDATA, pRead, PRE_DEFINED_DATA_LENGTH);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read rawdata failed!\n", __func__);
		goto err_read_data;
	}

	memset(ts->pFrame, 0x00, ts->tx_count * ts->rx_count * 2);

	/* d[00] ~ d[14] : TX channel
	 * d[15] ~ d[51] : none
	 * d[52] ~ d[77] : RX channel
	 * d[78] ~ d[103] : none */
	for (ii = 0; ii < PRE_DEFINED_DATA_LENGTH; ii += 2) {
		if ((ii >= (ts->tx_count * 2)) && (ii < (PRE_DEFINED_DATA_LENGTH / 2)))
			continue;
		if (ii >= ((PRE_DEFINED_DATA_LENGTH / 2) + (ts->rx_count * 2)))
			break;

		ts->pFrame[jj] = ((pRead[ii] << 8) | pRead[ii + 1]);

		if (ii == 0)
			*min = *max = ts->pFrame[jj];

		*min = min(*min, ts->pFrame[jj]);
		*max = max(*max, ts->pFrame[jj]);

		input_raw_info(true, &ts->client->dev, "%s: [%s][%d] %d\n", __func__,
				(jj < ts->tx_count) ? "TX" : "RX", jj, ts->pFrame[jj]);
		jj++;
	}
err_read_data:
	/* release data monitory (unprepare AFE data memory) */
	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SELF_RAW_TYPE, &mode, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: set rawdata failed!\n", __func__);
out_read_channel:
	kfree(pRead);
	return ret;
}


int sec_ts_read_raw_data(struct sec_ts_data *ts, struct sec_ts_test_mode *mode)
{
	int ii;
	int ret = 0;
	char temp[CMD_STR_LEN] = { 0 };
	char *buff;

	buff = kzalloc(ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		input_info(true, &ts->client->dev, "%s: failed to alloc mem\n",
				__func__);
		goto error_alloc_mem;
	}

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		goto error_power_state;
	}

	input_raw_info(true, &ts->client->dev, "%s: %d, %s\n",
			__func__, mode->type, mode->allnode ? "ALL" : "");

	ret = sec_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to fix tmode\n",
				__func__);
		goto error_test_fail;
	}

	if (mode->frame_channel)
		ret = sec_ts_read_channel(ts, mode->type, &mode->min, &mode->max);
	else
		ret = sec_ts_read_frame(ts, mode->type, &mode->min, &mode->max);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read frame\n",
				__func__);
		goto error_test_fail;
	}

	if (mode->allnode) {
		if (mode->frame_channel) {
			for (ii = 0; ii < (ts->rx_count + ts->tx_count); ii++) {
				snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", ts->pFrame[ii]);
				strncat(buff, temp, CMD_RESULT_WORD_LEN);

				memset(temp, 0x00, CMD_STR_LEN);
			}
		} else {
			for (ii = 0; ii < (ts->rx_count * ts->tx_count); ii++) {
					snprintf(temp, CMD_RESULT_WORD_LEN, "%d,",ts->pFrame[ii]);
					strncat(buff, temp, CMD_RESULT_WORD_LEN);

					memset(temp, 0x00, CMD_STR_LEN);
			}
		}
	} else {
		snprintf(buff, CMD_STR_LEN, "%d,%d", mode->min, mode->max);
	}

	sec_ts_release_all_finger(ts);

	ret = sec_ts_release_tmode(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to release tmode\n",
				__func__);
		goto error_test_fail;
	}

	set_cmd_result(ts, buff, strnlen(buff, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN));
	ts->cmd_state = CMD_STATUS_OK;

	kfree(buff);

	return ret;

error_test_fail:
error_power_state:
	kfree(buff);
error_alloc_mem:
	snprintf(temp, CMD_STR_LEN, "FAIL");
	set_cmd_result(ts, temp, CMD_STR_LEN);
	ts->cmd_state = CMD_STATUS_FAIL;

	return ret;
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };

	set_default_result(ts);

	sprintf(buff, "SE%02X%02X%02X",
		ts->plat_data->panel_revision, ts->plat_data->img_version_of_bin[2],
		ts->plat_data->img_version_of_bin[3]);

	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };
	u8 img_ver[4];
	int ret;

	set_default_result(ts);

	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_IMG_VERSION, img_ver, 4);
	if(ret < 0)	{
		input_info(true, &ts->client->dev, "%s: Image version read error\n ", __func__);
		ts->cmd_state = CMD_STATUS_FAIL;
	}

	sprintf(buff, "SE%02X%02X%02X",
		ts->plat_data->panel_revision, img_ver[2], img_ver[3]);

	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_cm_spec_over(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[20] = { 0 };

	set_default_result(ts);

	sprintf(buff, "%3d",
			ts->cm_specover);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_config_ver(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[20] = { 0 };

	set_default_result(ts);

	sprintf(buff, "%s_SE_%02X%02X",
		ts->plat_data->project_name ?: ts->plat_data->model_name ?: SEC_TS_DEVICE_NAME,
		ts->plat_data->para_version_of_ic[2], ts->plat_data->para_version_of_ic[3]);

	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_threshold(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[20] = { 0 };

	char w_param[1];
	char r_param[2];
	int threshold = 0;
	int ret;

	set_default_result(ts);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		char buff[CMD_STR_LEN] = { 0 };
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	w_param[0] = 0;
	ret = ts->sec_ts_i2c_write(ts, SEC_TS_READ_THRESHOLD, w_param, 1 );
	if(ret < 0)
		input_err(true, &ts->client->dev, "%s: threshold write type failed. ret: %d\n", __func__, ret);

	ret = ts->sec_ts_i2c_read_bulk(ts, r_param, 2);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s threshold read failed. ret: %d\n", __func__, ret);

	threshold = (r_param[0]<<8 | r_param[1]);
	snprintf(buff, sizeof(buff), "%d", threshold);

	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void module_off_master(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[3] = { 0 };
	int ret = 0;

	mutex_lock(&ts->lock);
	if (ts->power_status) {
		disable_irq(ts->client->irq);
		ts->power_status = SEC_TS_STATE_POWER_OFF;
	}
	mutex_unlock(&ts->lock);

	if (ts->plat_data->power)
		ts->plat_data->power(ts, false);
	else
		ret = 1;

	if (ret == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else
		snprintf(buff, sizeof(buff), "%s", "NG");

	set_default_result(ts);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	if (strncmp(buff, "OK", 2) == 0)
		ts->cmd_state = CMD_STATUS_OK;
	else
		ts->cmd_state = CMD_STATUS_FAIL;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void module_on_master(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[3] = { 0 };
	int ret = 0;

	mutex_lock(&ts->lock);
	if (!ts->power_status) {
		enable_irq(ts->client->irq);
		ts->power_status = SEC_TS_STATE_POWER_ON;
	}
	mutex_unlock(&ts->lock);

	if (ts->plat_data->power) {
		ts->plat_data->power(ts, true);
		ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SENSE_ON, NULL, 0);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: fail to write Sense_on\n", __func__);
	} else
		ret = 1;

	if (ret == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else
		snprintf(buff, sizeof(buff), "%s", "NG");

	set_default_result(ts);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	if (strncmp(buff, "OK", 2) == 0)
		ts->cmd_state = CMD_STATUS_OK;
	else
		ts->cmd_state = CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };
	strncpy(buff, "SEC", sizeof(buff));
	set_default_result(ts);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_name(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };

	if (ts->plat_data->img_version_of_ic[0] == 2)
		strncpy(buff, "MC44", sizeof(buff));
	else if (ts->plat_data->img_version_of_ic[0] == 5)
		strncpy(buff, "A552", sizeof(buff));
	else if (ts->plat_data->img_version_of_ic[0] == 9)
		strncpy(buff, "Y661", sizeof(buff));

	set_default_result(ts);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_mis_cal_spec(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };
	char wreg[5] = { 0 };
	int ret;

	set_default_result(ts);

	if (ts->plat_data->mis_cal_check == 0){
		input_info(true, &ts->client->dev, "%s: [ERROR] not support, %d\n", __func__);
		goto NG;
	} else if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		goto NG;
	} else {

		if ((ts->cmd_param[0] < 0 || ts->cmd_param[0] > 255) ||
			(ts->cmd_param[1] < 0 || ts->cmd_param[1] > 255) ||
			(ts->cmd_param[2] < 0 || ts->cmd_param[2] > 255) ) {
			goto NG;
		} else {

			wreg[0] = ts->cmd_param[0];
			wreg[1] = ts->cmd_param[1];
			wreg[2] = ts->cmd_param[2];

			ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_MIS_CAL_SPEC, wreg, 3);
			if (ret < 0){
				input_err(true, &ts->client->dev, "%s nvm write failed. ret: %d\n", __func__, ret);
				goto NG;
			}else{
				input_info(true, &ts->client->dev, "%s: tx gap=%d, rx gap=%d, peak=%d\n", __func__, wreg[0], wreg[1], wreg[2]);
				sec_ts_delay(20);
			}
		}
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return;

NG:
	snprintf(buff, sizeof(buff), "%s", "NG");
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_FAIL;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

}

/*
## Mis Cal result ##
FF : initial value in Firmware.
FD : Cal fail case
F4 : i2c fail case (5F)
F3 : i2c fail case (5E)
F2 : power off state
F1 : not support mis cal concept
F0 : initial value in fucntion
03 : 0x1 | 0x2 
02 : Ambient Ambient condition check result 0 (PASS), 1(FAIL) 
01 : Wet Wet mode result 0 (PASS), 1(FAIL) 
00 : Pass
*/
static void get_mis_cal_info(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };
	char mis_cal_data = 0xF0;
	char wreg[5] = { 0 };
	int ret;

	set_default_result(ts);

	if (ts->plat_data->mis_cal_check == 0){
		input_info(true, &ts->client->dev, "%s: [ERROR] not support, %d\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	} else if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		mis_cal_data = 0xF2;
		goto NG;
	} else {
		ret = ts->sec_ts_i2c_read(ts, SEC_TS_CMD_MIS_CAL_READ, &mis_cal_data, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: i2c fail!, %d\n", __func__, ret);
			mis_cal_data = 0xF3;
			goto NG;
		}else{
			input_info(true, &ts->client->dev, "%s: miss cal data : %d\n", __func__, mis_cal_data );
		}

		ret = ts->sec_ts_i2c_read(ts, SEC_TS_CMD_MIS_CAL_SPEC, wreg, 3);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: i2c fail!, %d\n", __func__, ret);
			mis_cal_data = 0xF4;
			goto NG;
		}else{
			input_info(true, &ts->client->dev, "%s: miss cal spec : %d,%d,%d\n", __func__, \
				wreg[0], wreg[1], wreg[2] );
		}
	}

	snprintf(buff, sizeof(buff), "%d,%d,%d,%d", mis_cal_data, wreg[0], wreg[1], wreg[2]);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return;

NG:
	snprintf(buff, sizeof(buff), "%d,%d,%d,%d", mis_cal_data, 0, 0, 0);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_FAIL;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

}


static void get_wet_mode(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };
	char wet_mode_info = 0;
	int ret;

	set_default_result(ts);

	ret = ts->sec_ts_i2c_read(ts, SEC_TS_CMD_WET_MODE, &wet_mode_info, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c fail!, %d\n", __func__, ret);
		goto NG;
	}

	snprintf(buff, sizeof(buff), "%d", wet_mode_info);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_FAIL;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

}


static void get_x_num(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };

	set_default_result(ts);
	snprintf(buff, sizeof(buff), "%d", ts->tx_count);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_y_num(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };

	set_default_result(ts);
	snprintf(buff, sizeof(buff), "%d", ts->rx_count);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_x_cross_routing(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };

	set_default_result(ts);
	snprintf(buff, sizeof(buff), "NG");
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_y_cross_routing(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };
	int ret;

	set_default_result(ts);

	ret = strncmp(ts->plat_data->model_name, "G935", 4)
			&& strncmp(ts->plat_data->model_name, "N930", 4);
	if (ret == 0)
		snprintf(buff, sizeof(buff), "13,14");
	else
		snprintf(buff, sizeof(buff), "NG");
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_checksum_data(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };
	char csum_result[4] = { 0 };
	u8 nv_result;
	char data[5] = { 0 };
	u8 cal_result;
	u8 temp = 0;
	u8 csum = 0;
	int ret, i;

	set_default_result(ts);
	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		goto err;
	}

	disable_irq(ts->client->irq);

	ts->plat_data->power(ts, false);
	ts->power_status = SEC_TS_STATE_POWER_OFF;
	sec_ts_delay(50);

	ts->plat_data->power(ts, true);
	ts->power_status = SEC_TS_STATE_POWER_ON;
	sec_ts_delay(70);
	
	ret = sec_ts_wait_for_ready(ts, SEC_TS_ACK_BOOT_COMPLETE);
	if (ret < 0) {
		enable_irq(ts->client->irq);
		input_err(true, &ts->client->dev, "%s: boot complete failed\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		goto err;
	}

	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_BOOT_STATUS, &data[1], 1);
	if (ret < 0 || (data[1] != SEC_TS_STATUS_APP_MODE)) {
		enable_irq(ts->client->irq);
		input_err(true, &ts->client->dev, "%s: boot status failed, ret:%d, data:%X\n", __func__, ret, data[0]);
		snprintf(buff, sizeof(buff), "%s", "NG");
		goto err;
	}

	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_TS_STATUS, &data[2], 4);
	if (ret < 0 || (data[3] == TOUCH_SYSTEM_MODE_FLASH)) {
		enable_irq(ts->client->irq);
		input_err(true, &ts->client->dev, "%s: touch status failed, ret:%d, data:%X\n", __func__, ret, data[3]);
		snprintf(buff, sizeof(buff), "%s", "NG");
		goto err;
	}

	enable_irq(ts->client->irq);

	input_err(true, &ts->client->dev, "%s: data[0]:%X, data[1]:%X, data[3]:%X\n", __func__, data[0], data[1], data[3]);

	temp = DO_FW_CHECKSUM | DO_PARA_CHECKSUM;
	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_GET_CHECKSUM, &temp, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: send get_checksum_cmd fail!\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "SendCMDfail");
		goto err;
	}

	sec_ts_delay(20);

	ret = ts->sec_ts_i2c_read_bulk(ts, csum_result, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read get_checksum result fail!\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "ReadCSUMfail");
		goto err;
	}

	nv_result = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_FAC_RESULT);
	nv_result += get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_CAL_COUNT);

	cal_result = sec_ts_read_calibration_report(ts);

	for (i = 0; i < 4; i++)
		csum += csum_result[i];

	csum += nv_result;
	csum += cal_result;
	csum = ~csum;

	input_info(true, &ts->client->dev, "%s: checksum = %02X\n", __func__, csum);
	snprintf(buff, sizeof(buff), "%02X", csum);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	return;

err:
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
	return;
}

static void get_skip_touchevent_duty(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };

	set_default_result(ts);
	snprintf(buff, sizeof(buff), "%d", SEC_TS_SKIPTSP_DUTY);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_reference_read(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct sec_ts_test_mode mode;

	set_default_result(ts);
	memset(&mode, 0x00, sizeof(struct sec_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SEC;

	sec_ts_read_raw_data(ts, &mode);
}

static void run_reference_read_all(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct sec_ts_test_mode mode;

	set_default_result(ts);

	memset(&mode, 0x00, sizeof(struct sec_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SEC;
	mode.allnode = TEST_MODE_ALL_NODE;

	sec_ts_read_raw_data(ts, &mode);
}

static void get_reference(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	set_default_result(ts);
	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = sec_ts_check_index(ts);
	if (node < 0)
		return;

	val = ts->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_rawcap_read(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct sec_ts_test_mode mode;

	set_default_result(ts);

	memset(&mode, 0x00, sizeof(struct sec_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SDC;

	sec_ts_read_raw_data(ts, &mode);
}

static void run_rawcap_read_all(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct sec_ts_test_mode mode;

	set_default_result(ts);

	memset(&mode, 0x00, sizeof(struct sec_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SDC;
	mode.allnode = TEST_MODE_ALL_NODE;

	sec_ts_read_raw_data(ts, &mode);
}

static void get_rawcap(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	set_default_result(ts);
	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = sec_ts_check_index(ts);
	if (node < 0)
		return;

	val = ts->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_delta_read(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct sec_ts_test_mode mode;

	set_default_result(ts);

	memset(&mode, 0x00, sizeof(struct sec_ts_test_mode));
	mode.type = TYPE_RAW_DATA;

	sec_ts_read_raw_data(ts, &mode);
}

static void run_delta_read_all(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct sec_ts_test_mode mode;

	set_default_result(ts);

	memset(&mode, 0x00, sizeof(struct sec_ts_test_mode));
	mode.type = TYPE_RAW_DATA;
	mode.allnode = TEST_MODE_ALL_NODE;

	sec_ts_read_raw_data(ts, &mode);
}

static void get_delta(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	set_default_result(ts);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = sec_ts_check_index(ts);
	if (node < 0)
		return;

	val = ts->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_self_reference_read(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct sec_ts_test_mode mode;

	set_default_result(ts);

	memset(&mode, 0x00, sizeof(struct sec_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SEC;
	mode.frame_channel= TEST_MODE_READ_CHANNEL;

	sec_ts_read_raw_data(ts, &mode);
}

static void run_self_reference_read_all(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct sec_ts_test_mode mode;

	set_default_result(ts);

	memset(&mode, 0x00, sizeof(struct sec_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SEC;
	mode.frame_channel= TEST_MODE_READ_CHANNEL;
	mode.allnode = TEST_MODE_ALL_NODE;

	sec_ts_read_raw_data(ts, &mode);
}

static void run_self_rawcap_read(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct sec_ts_test_mode mode;

	set_default_result(ts);

	memset(&mode, 0x00, sizeof(struct sec_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SDC;
	mode.frame_channel= TEST_MODE_READ_CHANNEL;

	sec_ts_read_raw_data(ts, &mode);
}

static void run_self_rawcap_read_all(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct sec_ts_test_mode mode;

	set_default_result(ts);

	memset(&mode, 0x00, sizeof(struct sec_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SDC;
	mode.frame_channel= TEST_MODE_READ_CHANNEL;
	mode.allnode = TEST_MODE_ALL_NODE;

	sec_ts_read_raw_data(ts, &mode);
}

static void run_self_delta_read(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct sec_ts_test_mode mode;

	set_default_result(ts);

	memset(&mode, 0x00, sizeof(struct sec_ts_test_mode));
	mode.type = TYPE_RAW_DATA;
	mode.frame_channel= TEST_MODE_READ_CHANNEL;

	sec_ts_read_raw_data(ts, &mode);
}
static void run_self_delta_read_all(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct sec_ts_test_mode mode;

	set_default_result(ts);

	memset(&mode, 0x00, sizeof(struct sec_ts_test_mode));
	mode.type = TYPE_RAW_DATA;
	mode.frame_channel= TEST_MODE_READ_CHANNEL;
	mode.allnode = TEST_MODE_ALL_NODE;

	sec_ts_read_raw_data(ts, &mode);
}

static void get_selfcap(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	set_default_result(ts);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = sec_ts_check_self_index(ts);
	if (node < 0)
		return;

	val = ts->sFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

#if defined(CONFIG_SEC_DEBUG_TSP_LOG)
void sec_ts_run_rawdata_all(struct sec_ts_data *ts)
{
	short min, max;
	int ret;

	input_raw_data_clear();
	sec_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH);
	ret = sec_ts_read_frame(ts, TYPE_OFFSET_DATA_SEC, &min, &max);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s, Offset error ## ret:%d\n", __func__, ret);
	} else {
		input_err(true, &ts->client->dev, "%s, Offset Max/Min %d,%d ##\n", __func__, max, min);
		sec_ts_release_tmode(ts);
	}

	sec_ts_delay(20);

	sec_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH);
	ret = sec_ts_read_frame(ts, TYPE_RAW_DATA, &min, &max);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s, raw error ## ret:%d\n", __func__, ret);
	} else {
		input_err(true, &ts->client->dev, "%s, raw Max/Min %d,%d ##\n", __func__, max, min);
		sec_ts_release_tmode(ts);
	}

	sec_ts_delay(20);

	sec_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH);
	ret = sec_ts_read_frame(ts, TYPE_AMBIENT_DATA, &min, &max);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s, ambient error ## ret:%d\n", __func__, ret);
	} else {
		input_err(true, &ts->client->dev, "%s, ambient Max/Min %d,%d ##\n", __func__, max, min);
		sec_ts_release_tmode(ts);
	}

	sec_ts_delay(20);

	sec_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH);
	ret = sec_ts_read_channel(ts, TYPE_RAW_DATA, &min, &max);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s, self raw error ## ret:%d\n", __func__, ret);
	} else {
		input_err(true, &ts->client->dev, "%s, self raw Max/Min %d,%d ##\n", __func__, max, min);
		sec_ts_release_tmode(ts);
	}
	
	sec_ts_delay(20);

	sec_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH);
	ret = sec_ts_read_channel(ts, TYPE_AMBIENT_DATA, &min, &max);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s, self ambient error ## ret:%d\n", __func__, ret);
	} else {
		input_err(true, &ts->client->dev, "%s, self ambient Max/Min %d,%d ##\n", __func__, max, min);
		sec_ts_release_tmode(ts);
	}
}
#endif

/* Use TSP NV area
 * buff[0] : offset from user NVM storage
 * buff[1] : length of stroed data - 1 (ex. using 1byte, value is  1 - 1 = 0)
 * buff[2] : write data
 * buff[..] : cont.
 */

void set_tsp_nvm_data_clear(struct sec_ts_data *ts, u8 offset)
{
	char buff[4] = { 0 };
	int ret;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	buff[0] = offset;
	
	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_NVM, buff, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s nvm write failed. ret: %d\n", __func__, ret);	

	sec_ts_delay(20);
}

int get_tsp_nvm_data(struct sec_ts_data *ts, u8 offset)
{
	char buff[2] = { 0 };
	int ret;

	input_info(true, &ts->client->dev, "%s, offset:%u\n", __func__, offset);

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SENSE_OFF, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: sense off failed\n", __func__);
		goto out_nvm;
	}
	input_dbg(true, &ts->client->dev, "%s: SENSE OFF\n", __func__);

	sec_ts_delay(100);

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: clear event failed\n", __func__);
		goto out_nvm;
	}
	input_dbg(true, &ts->client->dev, "%s: CLEAR EVENT STACK\n", __func__);
	
	sec_ts_delay(100);

	sec_ts_release_all_finger(ts);

	/* send NV data using command
	 * Use TSP NV area : in this model, use only one byte
	 * buff[0] : offset from user NVM storage
	 * buff[1] : length of stroed data - 1 (ex. using 1byte, value is  1 - 1 = 0)
	 */
	memset(buff, 0x00, 2);
	buff[0] = offset;
	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_NVM, buff, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s nvm send command failed. ret: %d\n", __func__, ret);
		goto out_nvm;
	}

	sec_ts_delay(10);

	/* read NV data
	 * Use TSP NV area : in this model, use only one byte
	 */
	ret = ts->sec_ts_i2c_read_bulk(ts, buff, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s nvm send command failed. ret: %d\n", __func__, ret);
		goto out_nvm;
	}

	input_info(true, &ts->client->dev, "%s: data:%X\n", __func__, buff[0]);

out_nvm:
	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SENSE_ON, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: sense on failed\n", __func__);

	input_dbg(true, &ts->client->dev, "%s: SENSE ON\n", __func__);

	return buff[0];
}

/* FACTORY TEST RESULT SAVING FUNCTION
 * bit 3 ~ 0 : OCTA Assy
 * bit 7 ~ 4 : OCTA module
 * param[0] : OCTA modue(1) / OCTA Assy(2)
 * param[1] : TEST NONE(0) / TEST FAIL(1) / TEST PASS(2) : 2 bit
 */

#define TEST_OCTA_MODULE	1
#define TEST_OCTA_ASSAY	2

#define TEST_OCTA_NONE		0
#define TEST_OCTA_FAIL		1
#define TEST_OCTA_PASS		2

static void set_tsp_test_result(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct sec_ts_test_result *result;
	char buff[CMD_STR_LEN] = { 0 };
	char r_data[1] = { 0 };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	set_default_result(ts);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP_truned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}
/*
	if ((ts->cmd_param[0] < 0) || (ts->cmd_param[0] > 1)
		|| (ts->cmd_param[1] < 0) || (ts->cmd_param[1]> 1)) {
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
		set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));

		ts->cmd_state = CMD_STATUS_FAIL;

		input_info(true, &ts->client->dev, "%s: parameter error: %u, %u\n",
				__func__, ts->cmd_param[0], ts->cmd_param[1]);

		goto err_set_result;
	}
*/

	r_data[0] = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_FAC_RESULT);
	if (r_data[0] == 0xFF)
		r_data[0] = 0;

	result = (struct sec_ts_test_result *)r_data;

	if (ts->cmd_param[0] == TEST_OCTA_ASSAY) {
		result->assy_result = ts->cmd_param[1];
		if (result->assy_count < 3)
			result->assy_count++;
	}

	if (ts->cmd_param[0] == TEST_OCTA_MODULE) {
		result->module_result = ts->cmd_param[1];
		if (result->module_count < 3)
			result->module_count++;
	}

	input_err(true, &ts->client->dev, "%s: %d, %d, %d, %d, 0x%X\n", __func__,
			result->module_result, result->module_count,
			result->assy_result, result->assy_count, result->data[0]);

	/* Use TSP NV area : in this model, use only one byte
	 * buff[0] : offset from user NVM storage
	 * buff[1] : length of stroed data - 1 (ex. using 1byte, value is  1 - 1 = 0)
	 * buff[2] : write data
	 */
	memset(buff, 0x00, CMD_STR_LEN);
	buff[2] = *result->data;

	input_err(true, &ts->client->dev, "%s command (1)%X, (2)%X: %X\n",
				__func__, ts->cmd_param[0], ts->cmd_param[1], buff[2]);

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_NVM, buff, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s nvm write failed. ret: %d\n", __func__, ret);

	sec_ts_delay(10);

	ts->nv = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_FAC_RESULT);

	snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "OK");
	ts->cmd_state = CMD_STATUS_OK;
	set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
}

static void get_tsp_test_result(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	struct sec_ts_test_result *result;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	set_default_result(ts);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP_truned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	memset(buff, 0x00, CMD_STR_LEN);
	buff[0] = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_FAC_RESULT);
	if (buff[0] == 0xFF) {
		set_tsp_nvm_data_clear(ts, SEC_TS_NVM_OFFSET_FAC_RESULT);
		buff[0] = 0;
	}

	ts->nv = buff[0];

	result = (struct sec_ts_test_result *)buff;

	input_info(true, &ts->client->dev, "%s: [0x%X][0x%X] M:%d, M:%d, A:%d, A:%d\n",
			__func__, *result->data, buff[0],
			result->module_result, result->module_count,
			result->assy_result, result->assy_count);

	snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "M:%s, M:%d, A:%s, A:%d\n",
			result->module_result == 0 ? "NONE" :
			result->module_result == 1 ? "FAIL" : "PASS", result->module_count,
			result->assy_result == 0 ? "NONE" :
			result->assy_result == 1 ? "FAIL" : "PASS", result->assy_count);

	ts->cmd_state = CMD_STATUS_OK;

	set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
}

static void increase_disassemble_count(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	set_default_result(ts);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP_truned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	buff[2] = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_DISASSEMBLE_COUNT);

	input_err(true, &ts->client->dev, "%s: disassemble count is #1 %d\n", __func__, buff[2]);

	if (buff[2] == 0xFF)
		buff[2] = 0;

	if (buff[2] < 0xFE)
		buff[2]++;

	/* Use TSP NV area : in this model, use only one byte
	 * buff[0] : offset from user NVM storage
	 * buff[1] : length of stroed data - 1 (ex. using 1byte, value is  1 - 1 = 0)
	 * buff[2] : write data
	 */
	buff[0] = SEC_TS_NVM_OFFSET_DISASSEMBLE_COUNT;
	buff[1] = 0;

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_NVM, buff, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s nvm write failed. ret: %d\n", __func__, ret);	

	sec_ts_delay(20);

	memset(buff, 0x00, 3);
	buff[0] = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_DISASSEMBLE_COUNT);
	input_info(true, &ts->client->dev, "%s: check disassemble count: %d\n", __func__, buff[0]); 

	snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "OK");

	ts->cmd_state = CMD_STATUS_OK;

	set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
}

static void get_disassemble_count(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	set_default_result(ts);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP_truned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	memset(buff, 0x00, CMD_STR_LEN);
	buff[0] = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_DISASSEMBLE_COUNT);
	if (buff[0] == 0xFF) {
		set_tsp_nvm_data_clear(ts, SEC_TS_NVM_OFFSET_DISASSEMBLE_COUNT);
		buff[0] = 0;
	}

	input_info(true, &ts->client->dev, "%s: read disassemble count: %d\n", __func__, buff[0]); 


	ts->cmd_state = CMD_STATUS_OK;
	
	snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "%d\n", buff[0]);

	set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
}

#define GLOVE_MODE_EN (1 << 0)
#define CLEAR_COVER_EN (1 << 1)
#define FAST_GLOVE_MODE_EN (1 << 2)

static void glove_mode(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	int glove_mode_enables = 0;
	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1) {
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
		ts->cmd_state = CMD_STATUS_FAIL;
	} else {
		int retval;

		if (ts->cmd_param[0])
			glove_mode_enables |= GLOVE_MODE_EN;
		else
			glove_mode_enables &= ~(GLOVE_MODE_EN);

		retval = sec_ts_glove_mode_enables(ts, glove_mode_enables);

		if (retval < 0) {
			input_err(true, &ts->client->dev, "%s failed, retval = %d\n", __func__, retval);
			snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
			ts->cmd_state = CMD_STATUS_FAIL;
		} else {
			snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "OK");
			ts->cmd_state = CMD_STATUS_OK;
		}
	}

	set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	ts->cmd_state = CMD_STATUS_WAITING;

	return;
}

static void hover_enable(void *device_data)
{
	int enables;
	int retval;
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;

	input_info(true, &ts->client->dev, "%s: enter hover enable, param = %d\n", __func__, ts->cmd_param[0]);

	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1) {
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
		ts->cmd_state = CMD_STATUS_FAIL;
	} else {
		enables = ts->cmd_param[0];
		retval = sec_ts_hover_enables(ts, enables);

		if (retval < 0) {
			input_err(true, &ts->client->dev, "%s failed, retval = %d\n", __func__, retval);
			snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
			ts->cmd_state = CMD_STATUS_FAIL;
		} else {
			snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "OK");
			ts->cmd_state = CMD_STATUS_OK;
		}
	}

	set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));
	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	ts->cmd_state = CMD_STATUS_WAITING;

	return;
}

static void clear_cover_mode(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	input_info(true, &ts->client->dev, "%s: start clear_cover_mode %s\n", __func__, buff);
	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		ts->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (ts->cmd_param[0] > 1) {
			ts->flip_enable = true;
			ts->cover_type = ts->cmd_param[1];
			ts->cover_cmd = (u8)ts->cover_type;
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
			if(TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()){
				sec_ts_delay(100);
				tui_force_close(1);
				sec_ts_delay(200);
				if(TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()){
					trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
					trustedui_set_mode(TRUSTEDUI_MODE_OFF);
				}
			}
#endif // CONFIG_TRUSTONIC_TRUSTED_UI
		} else {
			ts->flip_enable = false;
		}

		if (!(ts->power_status == SEC_TS_STATE_POWER_OFF) && ts->reinit_done) {
			if (ts->flip_enable)
				sec_ts_set_cover_type(ts, true);
			else
				sec_ts_set_cover_type(ts, false);
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		ts->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_WAITING;

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
};

static void dead_zone_enable(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int ret;
	u8 data[2] = { 0 };

	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		ts->cmd_state = CMD_STATUS_FAIL;
		goto out;
	}

	data[0] = ts->cmd_param[0];

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_EDGE_DEADZONE, data, 1);
	if (ret < 0){
		input_err(true, &ts->client->dev, "%s: write edge deadzone failed!\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		ts->cmd_state = CMD_STATUS_FAIL;
		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	ts->cmd_state = CMD_STATUS_OK;
out:
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return;
};

#ifdef SMARTCOVER_COVER
void change_smartcover_table(struct sec_ts_data *ts)
{
	//struct factory_data *info = ts->f_data;
	u8 i, j, k, h, temp, temp_sum;

	for (i = 0; i < MAX_BYTE ; i++)
		for (j = 0; j < MAX_TX; j++)
			ts->changed_table[j][i] = ts->smart_cover[i][j];

#if 1 // debug
	input_info(true, &ts->client->dev, "%s smart_cover value\n", __func__);
	for (i = 0; i < MAX_BYTE; i++){
		pr_cont("[sec_ts] ");
		for (j = 0; j < MAX_TX; j++)
			pr_cont("%d ", ts->smart_cover[i][j]);
		pr_cont("\n");
	}

	input_info(true, &ts->client->dev, "%s changed_table value\n", __func__);
	for (j = 0; j < MAX_TX; j++){
		pr_cont("[sec_ts] ");
		for (i = 0; i < MAX_BYTE; i++)
			pr_cont("%d ", ts->changed_table[j][i]);
		pr_cont("\n");
	}
#endif

	input_info(true, &ts->client->dev, "%s %d\n", __func__, __LINE__);

	for (i = 0; i < MAX_TX; i++)
		for (j = 0; j < 4; j++)
			ts->send_table[i][j] = 0;
	input_info(true, &ts->client->dev, "%s %d\n", __func__, __LINE__);

	for (i = 0; i < MAX_TX; i++){
		temp = 0;
		for (j = 0; j < MAX_BYTE; j++)
			temp += ts->changed_table[i][j];
		if (temp == 0) continue;

		for(k = 0; k < 4; k++) {
			temp_sum = 0;
			for(h = 0; h < 8; h++)
				temp_sum += ((u8)(ts->changed_table[i][h+8*k])) << (7-h);

			ts->send_table[i][k] = temp_sum;
		}

		input_info(true, &ts->client->dev, "i:%2d, %2X %2X %2X %2X \n",
			i,ts->send_table[i][0],ts->send_table[i][1],ts->send_table[i][2],ts->send_table[i][3]);
	}
	input_info(true, &ts->client->dev, "%s %d\n", __func__, __LINE__);
}

void set_smartcover_mode(struct sec_ts_data *ts, bool on)
{
	//struct factory_data *info = ts->f_data;
	int ret;
	unsigned char regMon[2] = {0xC1, 0x0A};
	unsigned char regMoff[2] = {0xC2, 0x0A};

	if (on == 1){
		ret = ts->sec_ts_i2c_write(ts, regMon[0], regMon+1, 1);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s mode on failed. ret: %d\n", __func__, ret);
	} else {
		ret = ts->sec_ts_i2c_write(ts, regMoff[0], regMoff+1, 1);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s mode off failed. ret: %d\n", __func__, ret);
	}
}

void set_smartcover_clear(struct sec_ts_data *ts)
{
	//struct factory_ts *info = ts->f_ts;
	int ret;
	unsigned char regClr[6] = {0xC5, 0xFF, 0x00, 0x00, 0x00, 0x00};

	ret = ts->sec_ts_i2c_write(ts, regClr[0], regClr+1, 5);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s ts clear failed. ret: %d\n", __func__, ret);
}

void set_smartcover_data(struct sec_ts_data *ts)
{
	int ret;
	u8 i, j;
	u8 temp=0;
	unsigned char regData[6] = {0xC5, 0x00, 0x00, 0x00, 0x00, 0x00};


	for (i = 0; i < MAX_TX; i++) {
		temp = 0;
		for (j = 0; j < 4; j++)
			temp += ts->send_table[i][j];
		if (temp == 0) continue;

		regData[1] = i;

		for (j = 0; j < 4; j++)
			regData[2+j] = ts->send_table[i][j];

		input_info(true, &ts->client->dev, "i:%2d, %2X %2X %2X %2X \n",
			regData[1],regData[2],regData[3],regData[4], regData[5]);

		// data write
		ret = ts->sec_ts_i2c_write(ts, regData[0], regData+1, 5);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s data write[%d] failed. ret: %d\n", __func__,i, ret);
	}
}

/* ####################################################
        func : smartcover_cmd [0] [1] [2] [3]
        index 0
                vlaue 0 : off (normal)
                vlaue 1 : off (globe mode)
                vlaue 2 :  X
                vlaue 3 : on
                                clear -> data send(send_table value) ->  mode on
                vlaue 4 : clear smart_cover value
                vlaue 5 : data save to smart_cover value
                        index 1 : tx channel num
                        index 2 : data 0xFF
                        index 3 : data 0xFF
                value 6 : table value change, smart_cover -> changed_table -> send_table

        ex)
        // clear
        echo smartcover_cmd,4 > cmd
        // data write (hart)
        echo smartcover_cmd,5,3,16,16 > cmd
        echo smartcover_cmd,5,4,56,56 > cmd
        echo smartcover_cmd,5,5,124,124 > cmd
        echo smartcover_cmd,5,6,126,252 > cmd
        echo smartcover_cmd,5,7,127,252 > cmd
        echo smartcover_cmd,5,8,63,248 > cmd
        echo smartcover_cmd,5,9,31,240 > cmd
        echo smartcover_cmd,5,10,15,224 > cmd
        echo smartcover_cmd,5,11,7,192 > cmd
        echo smartcover_cmd,5,12,3,128 > cmd
        // data change
        echo smartcover_cmd,6 > cmd
        // mode on
        echo smartcover_cmd,3 > cmd

###################################################### */
void smartcover_cmd(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	//struct factory_data *info = ts->f_data;
	int retval;
	char buff[CMD_STR_LEN] = { 0 };
	u8 i, j, t;

	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 6) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		ts->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (ts->cmd_param[0] == 0) {		// off
			set_smartcover_mode(ts, 0);
			input_info(true, &ts->client->dev, "%s mode off, normal\n", __func__);
		} else if(ts->cmd_param[0] == 1) {	// off, globe mode
			set_smartcover_mode(ts, 0);
			input_info(true, &ts->client->dev, "%s mode off, globe mode\n", __func__);

			if(ts->glove_enables & (0x1 << 3)) {//SEC_TS_BIT_SETFUNC_GLOVE )
				retval = sec_ts_glove_mode_enables(ts, 1);
				if (retval < 0) {
					input_err(true, &ts->client->dev,
						"%s: glove mode enable failed, retval = %d\n", __func__, retval);
					snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
					ts->cmd_state = CMD_STATUS_FAIL;
				} else {
					snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "OK");
					ts->cmd_state = CMD_STATUS_OK;
				}
			} else if (ts->glove_enables & (0x1 << 1)) {//SEC_TS_BIT_SETFUNC_HOVER )
				retval = sec_ts_hover_enables(ts, 1);
				if (retval < 0)	{
					input_err(true, &ts->client->dev,
						"%s: hover enable failed, retval = %d\n", __func__, retval);
					snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
					ts->cmd_state = CMD_STATUS_FAIL;
				} else {
					snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "OK");
					ts->cmd_state = CMD_STATUS_OK;
				}
			}
			/*
			if (info->fast_mshover_enabled)
			fts_command(info, FTS_CMD_SET_FAST_GLOVE_MODE);
			else if (info->mshover_enabled)
			fts_command(info, FTS_CMD_MSHOVER_ON);
			*/
		} else if (ts->cmd_param[0] == 3) {	// on
			set_smartcover_clear(ts);
			set_smartcover_data(ts);
			input_info(true, &ts->client->dev, "%s data send\n", __func__);
			set_smartcover_mode(ts, 1);
			input_info(true, &ts->client->dev, "%s mode on\n", __func__);

		} else if (ts->cmd_param[0] == 4) {	// clear
			for (i = 0; i < MAX_BYTE; i++)
				for (j = 0; j < MAX_TX; j++)
					ts->smart_cover[i][j] = 0;
			input_info(true, &ts->client->dev, "%s data clear\n", __func__);
		} else if (ts->cmd_param[0] == 5) {	// data write
			if (ts->cmd_param[1] < 0 || ts->cmd_param[1] >= 32) {
				input_info(true, &ts->client->dev, "%s data tx size is over[%d]\n",
					__func__,ts->cmd_param[1]);
				snprintf(buff, sizeof(buff), "%s", "NG");
				ts->cmd_state = CMD_STATUS_FAIL;
				goto fail;
			}
			input_info(true, &ts->client->dev, "%s data %2X, %2X, %2X\n", __func__, 
				ts->cmd_param[1],ts->cmd_param[2],ts->cmd_param[3] );

			t = ts->cmd_param[1];
			ts->smart_cover[t][0] = (ts->cmd_param[2]&0x80)>>7;
			ts->smart_cover[t][1] = (ts->cmd_param[2]&0x40)>>6;
			ts->smart_cover[t][2] = (ts->cmd_param[2]&0x20)>>5;
			ts->smart_cover[t][3] = (ts->cmd_param[2]&0x10)>>4;
			ts->smart_cover[t][4] = (ts->cmd_param[2]&0x08)>>3;
			ts->smart_cover[t][5] = (ts->cmd_param[2]&0x04)>>2;
			ts->smart_cover[t][6] = (ts->cmd_param[2]&0x02)>>1;
			ts->smart_cover[t][7] = (ts->cmd_param[2]&0x01);
			ts->smart_cover[t][8] = (ts->cmd_param[3]&0x80)>>7;
			ts->smart_cover[t][9] = (ts->cmd_param[3]&0x40)>>6;
			ts->smart_cover[t][10] = (ts->cmd_param[3]&0x20)>>5;
			ts->smart_cover[t][11] = (ts->cmd_param[3]&0x10)>>4;
			ts->smart_cover[t][12] = (ts->cmd_param[3]&0x08)>>3;
			ts->smart_cover[t][13] = (ts->cmd_param[3]&0x04)>>2;
			ts->smart_cover[t][14] = (ts->cmd_param[3]&0x02)>>1;
			ts->smart_cover[t][15] = (ts->cmd_param[3]&0x01);

		} else if (ts->cmd_param[0] == 6) {	// data change
			change_smartcover_table(ts);
			input_info(true, &ts->client->dev, "%s data change\n", __func__);
		} else {
			input_info(true, &ts->client->dev, "%s cmd[%d] not use\n", __func__, ts->cmd_param[0] );
			snprintf(buff, sizeof(buff), "%s", "NG");
			ts->cmd_state = CMD_STATUS_FAIL;
			goto fail;
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		ts->cmd_state = CMD_STATUS_OK;
	}
fail:
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
	ts->cmd_state = CMD_STATUS_WAITING;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
};
#endif


static void sec_ts_swap(u8 *a, u8 *b)
{
	u8 temp = *a;
	*a = *b;
	*b = temp;
}

static void rearrange_sft_result(u8 *data, int length)
{
	int i;

	for(i = 0; i < length; i+=4)
	{
		sec_ts_swap(&data[i], &data[i+3]);
		sec_ts_swap(&data[i+1], &data[i+2]);
	}
}

int execute_selftest(struct sec_ts_data *ts)
{
	int rc;
	u8 tpara = 0x23;
	u8 *rBuff;
	int i;
	int result = 0;
	int result_size = SEC_TS_SELFTEST_REPORT_SIZE + ts->tx_count * ts->rx_count * 2;

	input_info(true, &ts->client->dev, "%s: Self test start!\n", __func__);
	rc = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SELFTEST, &tpara, 1);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: Send selftest cmd failed!\n", __func__);
		goto err_exit;
	}
	sec_ts_delay(350);

	rc = sec_ts_wait_for_ready(ts, SEC_TS_ACK_SELF_TEST_DONE);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: Selftest execution time out!\n", __func__);
		goto err_exit;
	}

	input_info(true, &ts->client->dev, "%s: Self test done!\n", __func__);

	rBuff = kzalloc(result_size, GFP_KERNEL);
	if (!rBuff) {
		input_err(true, &ts->client->dev, "%s: allocation failed!\n", __func__);
		goto err_exit;
	}

	rc = ts->sec_ts_i2c_read(ts, SEC_TS_READ_SELFTEST_RESULT, rBuff, result_size);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: Selftest execution time out!\n", __func__);
		goto err_exit;
	}
	rearrange_sft_result(rBuff, result_size);

	for(i = 0; i < 80; i+=4){
		if(i%8==0) pr_cont("\n");
		if(i%4==0) pr_cont("%s sec_ts : ", SECLOG);

		if(i/4==0) pr_cont("SIG");
		else if(i/4==1) pr_cont("VER");
		else if(i/4==2) pr_cont("SIZ");
		else if(i/4==3) pr_cont("CRC");
		else if(i/4==4) pr_cont("RES");
		else if(i/4==5) pr_cont("COU");
		else if(i/4==6) pr_cont("PAS");
		else if(i/4==7) pr_cont("FAI");
		else if(i/4==8) pr_cont("CHA");
		else if(i/4==9) pr_cont("AMB");
		else if(i/4==10) pr_cont("RXS");
		else if(i/4==11) pr_cont("TXS");
		else if(i/4==12) pr_cont("RXO");
		else if(i/4==13) pr_cont("TXO");
		else if(i/4==14) pr_cont("RXG");
		else if(i/4==15) pr_cont("TXG");
		else if(i/4==16) pr_cont("RXR");
		else if(i/4==17) pr_cont("TXT");
		else if(i/4==18) pr_cont("RXT");
		else if(i/4==19) pr_cont("TXR");

		pr_cont(" %2X, %2X, %2X, %2X  ",rBuff[i],rBuff[i+1],rBuff[i+2],rBuff[i+3]);


		if (i/4 == 4) {
			if ((rBuff[i+3]&0x30) != 0)	// RX, RX open check.
				result = 0;
			else
				result = 1;
		}
	}

	return result;
err_exit:

	return 0;

}

static void run_trx_short_test(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = {0};
	int rc;
	char para = TO_TOUCH_MODE;

	set_default_result(ts);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	disable_irq(ts->client->irq);

	rc = execute_selftest(ts);
	if (rc) {

		ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SET_POWER_MODE, &para, 1);
		enable_irq(ts->client->irq);
		snprintf(buff, sizeof(buff), "%s", "OK");
		ts->cmd_state = CMD_STATUS_OK;
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

		mutex_lock(&ts->cmd_lock);
		ts->cmd_is_running = false;
		mutex_unlock(&ts->cmd_lock);

		input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
		return;
	}


	ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SET_POWER_MODE, &para, 1);
	enable_irq(ts->client->irq);

	snprintf(buff, sizeof(buff), "%s", "NG");
	ts->cmd_state = CMD_STATUS_FAIL;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return;

}


int sec_ts_execute_force_calibration(struct sec_ts_data *ts, int cal_mode)
{
	int rc = -1;
	u8 cmd;

	input_info(true, &ts->client->dev, "%s %d\n", __func__, cal_mode);

	if (cal_mode == OFFSET_CAL_SEC)
		cmd = SEC_TS_CMD_CALIBRATION_OFFSET_SEC;
	else if (cal_mode == AMBIENT_CAL)
		cmd = SEC_TS_CMD_CALIBRATION_AMBIENT;
	else
		return rc;

	if (ts->sec_ts_i2c_write(ts, cmd, NULL, 0) < 0) {
		input_err(true, &ts->client->dev, "%s: Write Cal commend failed!\n", __func__);
		return rc;
	}

	sec_ts_delay(1000);

	rc = sec_ts_wait_for_ready(ts, SEC_TS_ACK_OFFSET_CAL_DONE);

	ts->cal_status = sec_ts_read_calibration_report(ts);
	return rc;
}

static void get_force_calibration(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = {0};
	char cal_result[4] = { 0 };

	set_default_result(ts);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	cal_result[0] = sec_ts_read_calibration_report(ts);

	if (cal_result[0] == SEC_TS_STATUS_CALIBRATION_SEC) {
		snprintf(buff, sizeof(buff), "%s", "OK");
		ts->cmd_state = CMD_STATUS_OK;
	} else {
		snprintf(buff, sizeof(buff), "%s", "NG");
	}

	input_info(true, &ts->client->dev, "%s: %d, %d\n", __func__, cal_result[0],(cal_result[0] == SEC_TS_STATUS_CALIBRATION_SEC) );

	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_external_factory(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = {0};

	set_default_result(ts);

	ts->external_factory = true;
	snprintf(buff, sizeof(buff), "%s", "OK");

	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_force_calibration(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = {0};
	int rc;

	set_default_result(ts);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	sec_ts_read_calibration_report(ts);

	if (ts->touch_count > 0) {
		snprintf(buff, sizeof(buff), "%s", "NG_FINGER_ON");
		ts->cmd_state = CMD_STATUS_FAIL;
		goto out_force_cal;
	}

	disable_irq(ts->client->irq);

	rc = sec_ts_execute_force_calibration(ts, OFFSET_CAL_SEC);
	if (rc < 0) {
		snprintf(buff, sizeof(buff), "%s", "FAIL");
		ts->cmd_state = CMD_STATUS_FAIL;
	} else {
#ifdef CALIBRATION_BY_FACTORY
		buff[1] = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_CAL_COUNT);
		if (buff[1] == 0xFF)
			buff[1] = 0;

		/* count the number of calibration */
		if (buff[1] < 0xFE)
			ts->cal_count = buff[1] + 1;

		/* Use TSP NV area : in this model, use only one byte
		 * buff[0] : offset from user NVM storage
		 * buff[1] : length of stored data - 1 (ex. using 1byte, value is  1 - 1 = 0)
		 * buff[2] : write data
		 */
		buff[0] = SEC_TS_NVM_OFFSET_CAL_COUNT;
		buff[1] = 0;
		buff[2] = ts->cal_count;

		input_info(true, &ts->client->dev, "%s: write to nvm %X\n",
					__func__, buff[2]);

		rc = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_NVM, buff, 3);
		if (rc < 0) {
			input_err(true, &ts->client->dev,
				"%s nvm write failed. ret: %d\n", __func__, rc);
		}

		sec_ts_delay(20);

		ts->cal_count = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_CAL_COUNT);
#endif

		/* Save nv data   cal_position 
		 * 0xE0 :  External
		 * 0x01 :  LCiA
		 * 0xFF :  Developer/Tech -Default
		 */
		if (ts->external_factory)  ts->cal_pos = 0xE0;
		else  ts->cal_pos = 0x01;
		buff[0] = SEC_TS_NVM_OFFSET_CAL_POS;
		buff[1] = 0;
		buff[2] = ts->cal_pos;
		input_info(true, &ts->client->dev, "%s: write to nvm cal_pos(0x%2X)\n",
					__func__, buff[2]);
		
		rc = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_NVM, buff, 3);
		if (rc < 0) {
			input_err(true, &ts->client->dev,
				"%s: nvm write failed. ret: %d\n", __func__, rc);
		}
		sec_ts_delay(20);

		if(ts->plat_data->mis_cal_check){

			buff[0] = 0;			
			rc = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_STATEMANAGE_ON, buff, 1);
			if (rc < 0) {
				input_info(true, &ts->client->dev,
					"%s sec_ts. mis_cal_check error[1] ret: %d\n", __func__, rc);
			}

			buff[0] = 0x2;
			buff[1] = 0x2;
			rc = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_CHG_SYSMODE, buff, 2);
			if (rc < 0) {
				input_info(true, &ts->client->dev,
					"%s sec_ts. mis_cal_check error[2] ret: %d\n", __func__, rc);
			}

			input_err(true, &ts->client->dev, "%s try mis Cal. check \n", __func__);
			rc = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_MIS_CAL_CHECK, NULL, 0);
			if (rc < 0) {
				input_info(true, &ts->client->dev,
					"%s sec_ts. mis_cal_check error[3] ret: %d\n", __func__, rc);
			}
			sec_ts_delay(200);
			
			buff[0] = 1;
			rc = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_STATEMANAGE_ON, buff, 1);
			if (rc < 0) {
				input_info(true, &ts->client->dev,
					"%s sec_ts. mis_cal_check error[4] ret: %d\n", __func__, rc);
			}
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		ts->cmd_state = CMD_STATUS_OK;
	}

	enable_irq(ts->client->irq);

out_force_cal:
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->external_factory = false;

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}
static void set_log_level(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	char tBuff[2] = { 0 };
	int ret;

	set_default_result(ts);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_FAIL;
		return;
	}

	if ( (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1) ||
		(ts->cmd_param[1] < 0 || ts->cmd_param[1] > 1) ||
		(ts->cmd_param[2] < 0 || ts->cmd_param[2] > 1) ||
		(ts->cmd_param[3] < 0 || ts->cmd_param[3] > 1) ) {
		input_err(true, &ts->client->dev, "%s: para out of range\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "Para out of range");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_FAIL;
		return;
	}

	ret = ts->sec_ts_i2c_read(ts, SEC_TS_CMD_STATUS_EVENT_TYPE, tBuff, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Read Event type enable status fail\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "Read Stat Fail");
		goto err;
	}

	input_info(true, &ts->client->dev, "%s: STATUS_EVENT enable = 0x%02X, 0x%02X\n",
		__func__, tBuff[0], tBuff[1]);

	tBuff[0] = 0x0;
	tBuff[1] = BIT_STATUS_EVENT_ACK(ts->cmd_param[0]) |
		BIT_STATUS_EVENT_ERR(ts->cmd_param[1]) |
		BIT_STATUS_EVENT_INFO(ts->cmd_param[2]) |
		BIT_STATUS_EVENT_GEST(ts->cmd_param[3]);

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_STATUS_EVENT_TYPE, tBuff, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Write Event type enable status fail\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "Write Stat Fail");
		goto err;
	}
	input_info(true, &ts->client->dev, "%s: ACK : %d, ERR : %d, INFO : %d, GEST : %d\n",
			__func__, ts->cmd_param[0], ts->cmd_param[1], ts->cmd_param[2], ts->cmd_param[3]);

	snprintf(buff, sizeof(buff), "%s", "OK");
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	return;
err:
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
}

bool check_lowpower_flag(struct sec_ts_data *ts)
{
	bool ret = 0;
	unsigned char flag = ts->lowpower_flag & 0xFF;

	if (flag)
		ret = 1;

	input_info(true, &ts->client->dev,
		"%s: lowpower_mode flag : %d, ret:%d\n", __func__, flag, ret);

	if (flag & SEC_TS_LOWP_FLAG_AOD)
		input_info(true, &ts->client->dev, "%s: aod cmd on\n", __func__);
	if (flag & SEC_TS_LOWP_FLAG_SPAY)
		input_info(true, &ts->client->dev, "%s: spay cmd on\n", __func__);
	if (flag & SEC_TS_LOWP_FLAG_SIDE_GESTURE)
		input_info(true, &ts->client->dev, "%s: side cmd on\n", __func__);

	return ret;
}

static void set_lowpower_mode(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
/*
	char para[1] = { 0 };
	int ret;
	char tBuff[4] = { 0 };
*/

	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1) {
		goto NG;
	} else {
		if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
			input_err(true, &ts->client->dev, "%s: ERR, POWER OFF\n", __func__);
			goto NG;
		}

		ts->lowpower_mode = ts->cmd_param[0];
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	ts->cmd_state = CMD_STATUS_OK;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	return ;

NG:
	snprintf(buff, sizeof(buff), "%s", "NG");
	ts->cmd_state = CMD_STATUS_FAIL;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	return ;
}

static void set_wirelesscharger_mode(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	int ret, mode;
	u8 w_data[2]={0x00,0x00};

	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 5) {
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
		goto OUT;
	} else {
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "OK");
	}

	if (ts->cmd_param[0] == 1 || ts->cmd_param[0] == 3 || ts->cmd_param[0] == 5)
		mode = true;
	else
		mode = false;

	if(ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: fail to enable w-charger status, POWER_STATUS=OFF \n",__func__);
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
		goto NG;
	}

	if(ts->cmd_param[0] == 3) w_data[0] = 0x01;
	else if(ts->cmd_param[0] == 5) w_data[0] = 0x02;
	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SET_CHARGERTYPE, w_data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command 74", __func__);
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
		goto NG;
	}

	if (mode)
		ts->touch_functions = (ts->touch_functions|SEC_TS_BIT_SETFUNC_WIRELESSCHARGER|SEC_TS_BIT_SETFUNC_MUTUAL);
	else 
		ts->touch_functions = ((ts->touch_functions&(~SEC_TS_BIT_SETFUNC_WIRELESSCHARGER))|SEC_TS_BIT_SETFUNC_MUTUAL);

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, &ts->touch_functions, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command 63", __func__);
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
		goto NG;
	}

NG:
	(mode)? (ts->touch_functions = ts->touch_functions|SEC_TS_BIT_SETFUNC_WIRELESSCHARGER|SEC_TS_BIT_SETFUNC_MUTUAL):
	 (ts->touch_functions = (ts->touch_functions&(~SEC_TS_BIT_SETFUNC_WIRELESSCHARGER))|SEC_TS_BIT_SETFUNC_MUTUAL);
	  input_err(true, &ts->client->dev, "%s: %s, status =%x\n",
	   __func__, (mode) ? "wireless enable" : "wireless disable", ts->touch_functions);
OUT:
	set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	ts->cmd_state = CMD_STATUS_WAITING;
};

#ifdef CONFIG_EPEN_WACOM_W9018
int set_spen_mode(int mode)
{
	int ret;
	u8 w_data[2]={0x00,0x00};

	if(ts_dup == NULL) return 0;
		
	if(ts_dup->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts_dup->client->dev, "%s: fail to send status, POWER_STATUS=OFF \n",__func__);
		return -EINVAL;
	}

	w_data[0] = mode;
	ret = ts_dup->sec_ts_i2c_write(ts_dup, SEC_TS_CMD_SET_SPENMODE, w_data, 1);
	if (ret < 0) {
		input_err(true, &ts_dup->client->dev, "%s: Failed to send command 75", __func__);
		return -EINVAL;
	}

	input_info(true, &ts_dup->client->dev, "%s: mode(%d) sended \n", __func__, mode);
	return mode;
}

EXPORT_SYMBOL(set_spen_mode);
#endif

static void set_tsp_disable_mode(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;

	char buff[CMD_STR_LEN] = { 0 };
	u8 w_data[2]={0x00, 0x00};
	int ret;

	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1)
		goto NG;

	if(ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: fail to send status, POWER_STATUS=OFF \n",__func__);
		goto NG;
	}

	if(ts->cmd_param[0]) w_data[0] = 0x01;
	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SET_SPENMODE, w_data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command 75", __func__);
		goto NG;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	ts->cmd_state = CMD_STATUS_OK;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
	return ;

NG:
	snprintf(buff, sizeof(buff), "%s", "NG");
	ts->cmd_state = CMD_STATUS_FAIL;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
	return ;
}

static void spay_enable(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;

	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1)
		goto NG;

	if (ts->cmd_param[0]) {
		if (ts->use_sponge)
			ts->lowpower_flag |= SEC_TS_MODE_SPONGE_SPAY;			
		else
			ts->lowpower_flag |= SEC_TS_LOWP_FLAG_SPAY;
	} else {
		if (ts->use_sponge)
			ts->lowpower_flag &= ~(SEC_TS_MODE_SPONGE_SPAY);
		else		
			ts->lowpower_flag &= ~(SEC_TS_LOWP_FLAG_SPAY);
	}

	ts->lowpower_mode = check_lowpower_flag(ts);

	input_info(true, &ts->client->dev, "%s: %02X\n", __func__, ts->lowpower_mode);

	snprintf(buff, sizeof(buff), "%s", "OK");
	ts->cmd_state = CMD_STATUS_OK;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
	return ;

NG:
	snprintf(buff, sizeof(buff), "%s", "NG");
	ts->cmd_state = CMD_STATUS_FAIL;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
	return ;
}

#ifdef SEC_TS_SUPPORT_5MM_SAR_MODE
static void sar_enable(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;

	char buff[CMD_STR_LEN] = { 0 };
	int ret;
	char mode;

	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1)
		goto NG;

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: ERR, POWER OFF\n", __func__);
		goto NG;
	}

	mode = ts->cmd_param[0]; 

	ret = ts->sec_ts_i2c_write(ts, 0x62, &mode, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to write mode\n", __func__);
		goto NG;
	}

	input_info(true, &ts->client->dev, "%s: %02X\n", __func__, mode);

	snprintf(buff, sizeof(buff), "%s", "OK");
	ts->cmd_state = CMD_STATUS_OK;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	return ;

NG:
	snprintf(buff, sizeof(buff), "%s", "NG");
	ts->cmd_state = CMD_STATUS_FAIL;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	return ;
}
#endif

static void aod_enable(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;

	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1)
		goto NG;

	if (ts->cmd_param[0]) {
		if (ts->use_sponge)
			ts->lowpower_flag |= SEC_TS_MODE_SPONGE_AOD;
		else
			ts->lowpower_flag |= SEC_TS_LOWP_FLAG_AOD;
	} else {
		if (ts->use_sponge)
			ts->lowpower_flag &= ~SEC_TS_MODE_SPONGE_AOD;
		else
			ts->lowpower_flag &= ~SEC_TS_LOWP_FLAG_AOD;
	}

	ts->lowpower_mode = check_lowpower_flag(ts);

	input_info(true, &ts->client->dev, "%s: %02X\n", __func__, ts->lowpower_flag);

	snprintf(buff, sizeof(buff), "%s", "OK");
	ts->cmd_state = CMD_STATUS_OK;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
	return ;

NG:
	snprintf(buff, sizeof(buff), "%s", "NG");
	ts->cmd_state = CMD_STATUS_FAIL;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
	return ;
}

static void set_aod_rect(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	u8 data[10] = {0x02, 0};
	int ret, i;

	set_default_result(ts);

	input_info(true, &ts->client->dev, "%s: w:%d, h:%d, x:%d, y:%d\n",
			__func__, ts->cmd_param[0], ts->cmd_param[1],
			ts->cmd_param[2], ts->cmd_param[3]);

	for (i = 0; i < 4; i++) {
		data[i * 2 + 2] = ts->cmd_param[i] & 0xFF;
		data[i * 2 + 3] = (ts->cmd_param[i] >> 8) & 0xFF;
	}

	if (ts->use_sponge) {
		disable_irq(ts->client->irq);
		ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_WRITE_PARAM, &data[0], 10);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to write offset\n", __func__);
			goto NG;
		}

		ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_NOTIFY_PACKET, NULL, 0);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to send notify\n", __func__);
			goto NG;
		}
		enable_irq(ts->client->irq);
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	ts->cmd_state = CMD_STATUS_OK;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
	return;
NG:
	enable_irq(ts->client->irq);
	snprintf(buff, sizeof(buff), "%s", "NG");
	ts->cmd_state = CMD_STATUS_FAIL;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

}

static void get_aod_rect(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	u8 data[8] = {0x02, 0};
	u16 rect_data[4] = {0, };
	int ret, i;

	set_default_result(ts);

	if (ts->use_sponge) {
		disable_irq(ts->client->irq);
		ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_READ_PARAM, data, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to write offset\n", __func__);
			goto NG;
		}

		ret = ts->sec_ts_i2c_read(ts, SEC_TS_CMD_SPONGE_READ_PARAM, data, 8);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to read rect\n", __func__);
			goto NG;
		}
		enable_irq(ts->client->irq);
	}

	for (i = 0; i < 4; i++)
		rect_data[i] = (data[i * 2 + 1] & 0xFF) << 8 | (data[i * 2] & 0xFF);

	input_info(true, &ts->client->dev, "%s: w:%d, h:%d, x:%d, y:%d\n",
			__func__, rect_data[0], rect_data[1], rect_data[2], rect_data[3]);

	snprintf(buff, sizeof(buff), "%s", "OK");
	ts->cmd_state = CMD_STATUS_OK;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
	return;
NG:
	enable_irq(ts->client->irq);
	snprintf(buff, sizeof(buff), "%s", "NG");
	ts->cmd_state = CMD_STATUS_FAIL;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
}

static void second_screen_enable(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;

	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1)
		goto NG;

	if (ts->cmd_param[0])
		ts->lowpower_flag |= SEC_TS_LOWP_FLAG_SIDE_GESTURE;
	else
		ts->lowpower_flag &= ~SEC_TS_LOWP_FLAG_SIDE_GESTURE;

	ts->lowpower_mode = check_lowpower_flag(ts);

	input_info(true, &ts->client->dev, "%s: %02X\n", __func__, ts->lowpower_flag);

	snprintf(buff, sizeof(buff), "%s", "OK");
	ts->cmd_state = CMD_STATUS_OK;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
	return ;

NG:
	snprintf(buff, sizeof(buff), "%s", "NG");
	ts->cmd_state = CMD_STATUS_FAIL;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
	return ;
}

static void set_tunning_data(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int i, ret;
	u8 data[2] = { 0 };

	set_default_result(ts);
	memset(buff, 0, sizeof(buff));
	memset(data, 0, sizeof(data));

#ifdef TWO_LEVEL_GRIP_CONCEPT
	if(!(ts->plat_data->grip_concept & 0x1)){
		input_info(true, &ts->client->dev, "%s: can't set, because %d concept\n", \
			__func__, ts->plat_data->grip_concept);
		goto out;
	}
#endif

	for (i = 0; i < 4; i++) {
		if (ts->cmd_param[i]) {
			if (ts->cmd_param[i] > ts->plat_data->max_x)
				continue;

			data[0] = (ts->cmd_param[i] >> 8) & 0xFF;
			data[1] = ts->cmd_param[i] & 0xFF;

			ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_DEADZONE_RANGE + i, data, 2);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "%s: write tunning data failed!\n", __func__);
				snprintf(buff, sizeof(buff), "%s", "NG");
				ts->cmd_state = CMD_STATUS_FAIL;
				goto out;
			}
		}
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	ts->cmd_state = CMD_STATUS_OK;
out:
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
}

#ifdef TWO_LEVEL_GRIP_CONCEPT

/* ######################################################
    flag     1  :  set edge handler
              2  :  set (portrait, normal) edge zone data
              4  :  set (portrait, normal) dead zone data
              8  :  set landscape mode data
              16 :  mode clear
    data
              0x30, FFF (y start), FFF (y end),  FF(direction) 
              0x31, FFFF (edge zone)
              0x32, FF (up x), FF (down x), FFFF (y)
              0x33, FF (mode), FFF (edge), FFF (dead zone)
    case
           edge handler set :  0x30....
           booting time :  0x30...  + 0x31... 
           normal mode : 0x32...  (+0x31...)
           landscape mode : 0x33... 
           landscape -> normal (if same with old data) : 0x33, 0
           landscape -> normal (etc) : 0x32....  + 0x33, 0
    ###################################################### */

void set_grip_data_to_ic(struct sec_ts_data *ts, u8 flag) {
	u8 data[8] = { 0 };

	input_info(true, &ts->client->dev, "%s: flag: %2X (clr,lan,nor,edg,han) \n", __func__, flag);

	if (flag & G_SET_EDGE_HANDLER) {
		if (ts->grip_edgehandler_direction == 0) {
			data[0] = 0x0;			
			data[1] = 0x0;
			data[2] = 0x0;
			data[3] = 0x0;
		} else {
			data[0] = (ts->grip_edgehandler_start_y >> 4) & 0xFF;
			data[1] = (ts->grip_edgehandler_start_y << 4 & 0xF0) | ((ts->grip_edgehandler_end_y >> 8) & 0xF);
			data[2] = ts->grip_edgehandler_end_y & 0xFF;
			data[3] = (ts->grip_edgehandler_direction) & 0x3;
		}
		ts->sec_ts_i2c_write(ts, ts->gripreg_edge_handler, data, 4); 	
		input_info(true, &ts->client->dev, "%s: 0x%2X %2X,%2X,%2X,%2X \n", __func__, \
			ts->gripreg_edge_handler, data[0], data[1], data[2], data[3]);
	}

	if (flag & G_SET_EDGE_ZONE) {
		data[0] = (ts->grip_edge_range >> 8) & 0xFF;
		data[1] = ts->grip_edge_range  & 0xFF;
		ts->sec_ts_i2c_write(ts, ts->gripreg_edge_area, data, 2);		
		input_info(true, &ts->client->dev, "%s: 0x%2X %2X,%2X \n", __func__, \
			ts->gripreg_edge_area, data[0], data[1]);
	}

	if (flag & G_SET_NORMAL_MODE) {
		data[0] = ts->grip_deadzone_up_x & 0xFF;
		data[1] = ts->grip_deadzone_dn_x & 0xFF;
		data[2] = (ts->grip_deadzone_y >> 8) & 0xFF;
		data[3] = ts->grip_deadzone_y & 0xFF;
		ts->sec_ts_i2c_write(ts, ts->gripreg_dead_zone, data, 4); 	
		input_info(true, &ts->client->dev, "%s: 0x%2X %2X,%2X,%2X,%2X \n", __func__, \
			ts->gripreg_dead_zone, data[0], data[1], data[2], data[3]);
	}

	if (flag & G_SET_LANDSCAPE_MODE) {
		data[0] = ts->grip_landscape_mode & 0x1;
		data[1] = (ts->grip_landscape_edge >> 4) & 0xFF;
		data[2] = (ts->grip_landscape_edge << 4 & 0xF0) | ((ts->grip_landscape_deadzone >> 8) & 0xF);
		data[3] = ts->grip_landscape_deadzone & 0xFF;
		ts->sec_ts_i2c_write(ts, ts->gripreg_landscape_mode, data, 4); 	
		input_info(true, &ts->client->dev, "%s: 0x%2X %2X,%2X,%2X,%2X \n", __func__, \
			ts->gripreg_landscape_mode, data[0], data[1], data[2], data[3]);
	}
	
	if (flag & G_CLR_LANDSCAPE_MODE) {
		data[0] = ts->grip_landscape_mode;
		ts->sec_ts_i2c_write(ts, ts->gripreg_landscape_mode, data, 1); 	
		input_info(true, &ts->client->dev, "%s: 0x%2X %2X \n", __func__, \
			ts->gripreg_landscape_mode, data[0]);
	}

}
/* ######################################################
    index  0 :  set edge handler
              1 :  portrait (normal) mode
              2 :  landscape mode
    data
              0, X (direction), X (y start), X (y end) 
                     direction : 0 (off), 1 (left), 2 (right)
                     ex) echo set_grip_data,0,2,600,900 > cmd

              1, X (edge zone), X (dead zone up x), X (dead zone down x), X (dead zone y)
                     ex) echo set_grip_data,1,200,10,50,1500 > cmd

              2, 1 (landscape mode), X (edge zone), X (dead zone)
                     ex) echo set_grip_data,2,1,200,100 > cmd

              2, 0 (portrait mode) 
                     ex) echo set_grip_data,2,0  > cmd
    ###################################################### */

static void set_grip_data(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	u8 mode = G_NONE;

	set_default_result(ts);

	memset(buff, 0, sizeof(buff));

	if(!(ts->plat_data->grip_concept & 0x6)){	// 0x2 : grace, 0x4 : dual mode for hero2
		input_info(true, &ts->client->dev, "%s: can't set, because %d concept\n", \
			__func__, ts->plat_data->grip_concept);
		goto err_grip_data;
	}

	if (ts->cmd_param[0] == 0) {			// [ edge handler ]
		if (ts->cmd_param[1] == 0) {		// clear edge handler
			ts->grip_edgehandler_direction = 0;	

		} else if (ts->cmd_param[1] < 3) {	// set edge handler
			ts->grip_edgehandler_direction 	= ts->cmd_param[1];
			ts->grip_edgehandler_start_y 	= ts->cmd_param[2];	
			ts->grip_edgehandler_end_y 		= ts->cmd_param[3];

		} else {
			input_info(true, &ts->client->dev, "%s: cmd1 is abnormal, %d", __func__,ts->cmd_param[1]);
			goto err_grip_data;
		}

		mode = mode | G_SET_EDGE_HANDLER;
		set_grip_data_to_ic(ts, mode);

	} else if (ts->cmd_param[0] == 1) {		// [ normal mode ]

		if (ts->grip_edge_range != ts->cmd_param[1]) {	// edge data chnage case
			mode = mode | G_SET_EDGE_ZONE;
		}
		ts->grip_edge_range 	= ts->cmd_param[1];
		ts->grip_deadzone_up_x 	= ts->cmd_param[2];
		ts->grip_deadzone_dn_x	= ts->cmd_param[3];
		ts->grip_deadzone_y 	= ts->cmd_param[4];
		mode = mode | G_SET_NORMAL_MODE;

		if (ts->grip_landscape_mode == 1) {	// landscape -> normal mode
			ts->grip_landscape_mode = 0;
			mode = mode | G_CLR_LANDSCAPE_MODE;
		}
		set_grip_data_to_ic(ts, mode);

	} else if (ts->cmd_param[0] == 2) {		// [ landscape mode ]
	
		if (ts->cmd_param[1] == 0) { 		// landscape -> normal mode, edge/dead zone data is same
			ts->grip_landscape_mode = 0;
			mode = mode | G_CLR_LANDSCAPE_MODE;

		} else if (ts->cmd_param[1] == 1) {
			ts->grip_landscape_mode 	= 1;
			ts->grip_landscape_edge 	= ts->cmd_param[2];
			ts->grip_landscape_deadzone = ts->cmd_param[3];
			mode = mode | G_SET_LANDSCAPE_MODE;

		} else {
			input_info(true, &ts->client->dev, "%s: cmd1 is abnormal, %d", __func__,ts->cmd_param[1]);
			goto err_grip_data;
		}
		set_grip_data_to_ic(ts, mode);

	} else {
		input_info(true, &ts->client->dev, "%s: cmd0 is abnormal, %d", __func__,ts->cmd_param[0]);
		goto err_grip_data;

	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	ts->cmd_state = CMD_STATUS_OK;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	return;

err_grip_data:

	snprintf(buff, sizeof(buff), "%s", "NG");
	ts->cmd_state = CMD_STATUS_FAIL;
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	return;

}
#endif

#ifdef CONFIG_TOUCHSCREEN_SUPPORT_MULTIMEDIA
static void brush_enable(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1) {
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
		ts->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (ts->brush_enable != (ts->cmd_param[0] ? true : false)) {
					ts->brush_enable = ts->cmd_param[0] ? true : false;
		}
		input_err(true, &ts->client->dev, "%s enable ts = %d \n", __func__, ts->brush_enable );
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "OK");
		ts->cmd_state = CMD_STATUS_OK;
	}

	set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));
	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	ts->cmd_state = CMD_STATUS_WAITING;

	return;
}

static void velocity_enable(void *device_data)
{

	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1) {
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
		ts->cmd_state = CMD_STATUS_FAIL;
	} else {
		
		if (ts->velocity_enable != (ts->cmd_param[0] ? true : false)) {
					ts->velocity_enable = ts->cmd_param[0] ? true : false;
		}
		input_err(true, &ts->client->dev, " %s enable ts = %d \n", __func__, ts->velocity_enable );
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "OK");
		ts->cmd_state = CMD_STATUS_OK;
	}

	set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));
	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	ts->cmd_state = CMD_STATUS_WAITING;

	return;
}
#endif

static void not_support_cmd(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;

	set_default_result(ts);
	snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "%s", tostring(NA));

	set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));
	ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
}

int sec_ts_fn_init(struct sec_ts_data *ts)
{
	int retval;
	unsigned short ii;

	INIT_LIST_HEAD(&ts->cmd_list_head);

	//++factory command list++
	ts->cmd_buffer_size = 30;
	for (ii = 0; ii < ARRAY_SIZE(ft_cmds); ii++) {
		list_add_tail(&ft_cmds[ii].list, &ts->cmd_list_head);
		if (ft_cmds[ii].cmd_name)
			ts->cmd_buffer_size += strlen(ft_cmds[ii].cmd_name) + 1;
	}

	mutex_init(&ts->cmd_lock);
	ts->cmd_is_running = false;

	ts->fac_dev_ts = sec_device_create(ts, "tsp");
	retval = IS_ERR(ts->fac_dev_ts);
	if (retval) {
		input_err(true, &ts->client->dev, "%s: Failed to create device for the sysfs\n", __func__);
		retval = IS_ERR(ts->fac_dev_ts);
		goto exit;
	}

	dev_set_drvdata(ts->fac_dev_ts, ts);

	retval = sysfs_create_group(&ts->fac_dev_ts->kobj, &cmd_attr_group);
	if (retval < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to create sysfs attributes\n", __func__);
		goto exit;
	}

	retval = sysfs_create_link(&ts->fac_dev_ts->kobj,
				&ts->input_dev->dev.kobj, "input");

	if (retval < 0)	{
		input_err(true, &ts->client->dev, "%s: fail - sysfs_create_link\n", __func__);
		goto exit;
	}
	ts->reinit_done = true;

	INIT_DELAYED_WORK(&ts->cover_cmd_work, clear_cover_cmd_work);
	INIT_DELAYED_WORK(&ts->set_wirelesscharger_mode_work, set_wirelesscharger_mode_work);

	return 0;

exit:
	//kfree(factory_data);
	return retval;
}
