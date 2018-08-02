/*
 * cypress_touchkey.c - Platform data for cypress touchkey driver
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "cypress-touchkey.h"
#ifdef  TK_HAS_FIRMWARE_UPDATE
#include "cy8cmbr_swd.h"
static u8 module_divider[] = {0, 0xff};
#endif

#ifdef CONFIG_TOUCHKEY_LIGHT_EFS
/**************************************************/
#define LIGHT_VERSION			160719
#define LIGHT_TABLE_MAX			6
struct light_info tkey_light_voltage_table[LIGHT_TABLE_MAX] =
{
	/* octa id, voltage */
	{ WINDOW_COLOR_BLACK_UTYPE,	3300},
	{ WINDOW_COLOR_BLACK,		3300},
	{ WINDOW_COLOR_GOLD,		3300},
	{ WINDOW_COLOR_SILVER,		3300},
	{ WINDOW_COLOR_BLUE,		3300},
	{ WINDOW_COLOR_PINKGOLD,	3300},
};
/**************************************************/
#endif

static const u8 fac_reg_index[] = {
	TK_THRESHOLD,
	TK_DIFF_DATA,
	TK_RAW_DATA,
	TK_IDAC,
	TK_COMP_IDAC,
	TK_BASELINE_DATA,
};

static int touchkey_keycode[] = { 0,
	KEY_RECENT, KEY_BACK,
};
static const int touchkey_count = ARRAY_SIZE(touchkey_keycode);

static char *str_states[] = {"on_irq", "off_irq", "on_i2c", "off_i2c"};
static int touchkey_led_status;
static int touchled_cmd_reversed;
static bool touchkey_probe;
#ifdef CONFIG_KEYBOARD_THD_CHANGE_AS_FINGERPRINT
static struct touchkey_i2c *tkey_info;
#endif

int touchkey_mode_change(struct touchkey_i2c *tkey_i2c, int cmd);
#ifdef CONFIG_TOUCHKEY_LIGHT_EFS
static int efs_read_light_table_version(struct touchkey_i2c *tkey_i2c);
#endif
static void change_touch_key_led_voltage(struct device *dev, int vol_mv)
{
	struct regulator *tled_regulator;
	int ret;

	tled_regulator = regulator_get(NULL, TK_LED_REGULATOR_NAME);
	if (IS_ERR(tled_regulator)) {
		input_err(true, dev, "%s: failed to get resource %s\n",
				__func__, TK_LED_REGULATOR_NAME);
		return;
	}
	ret = regulator_set_voltage(tled_regulator, vol_mv * 1000, vol_mv * 1000);
	if (ret)
		input_err(true, dev, "%s: failed to set key led %d mv, %d\n",
				__func__, vol_mv, ret);
	regulator_put(tled_regulator);
	input_info(true, dev, "%s: %dmV\n", __func__, vol_mv);
}

static ssize_t brightness_control(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data;

	if (sscanf(buf, "%d\n", &data) == 1)
		change_touch_key_led_voltage(&tkey_i2c->client->dev, data);
	else
		input_err(true, &tkey_i2c->client->dev, "%s Error\n", __func__);

	return size;
}

#ifdef CONFIG_TOUCHKEY_LIGHT_EFS
static int read_window_type(void)
{
	struct file *type_filp = NULL;
	mm_segment_t old_fs;
	int ret = 0;
	char window_type[10] = {0, };

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	type_filp = filp_open("/sys/class/lcd/panel/window_type", O_RDONLY, 0440);
	if (IS_ERR(type_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(type_filp);
		return ret;
	}

	ret = type_filp->f_op->read(type_filp, window_type,
			sizeof(window_type), &type_filp->f_pos);
	if (ret != 9 * sizeof(char)) {
		pr_err("%s touchkey %s: fd read fail\n", SECLOG, __func__);
		ret = -EIO;
		goto out;
	}

	if (window_type[1] < '0' || window_type[1] >= 'f') {
		ret = -EAGAIN;
		goto out;
	}

	ret = (window_type[1] - '0') & 0x0f;
	pr_info("%s touchkey %s: %d\n", SECLOG, __func__, ret);
out:
	filp_close(type_filp, current->files);
	set_fs(old_fs);

	return ret;
}

static int efs_calculate_crc (struct touchkey_i2c *tkey_i2c)
{
	struct file *temp_file = NULL;
	int crc = tkey_i2c->light_version_efs;
	mm_segment_t old_fs;
	char predefine_value_path[LIGHT_TABLE_PATH_LEN];
	int ret = 0, i;
	char temp_vol[LIGHT_CRC_SIZE] = {0, };
	int table_size;

	efs_read_light_table_version(tkey_i2c);
	table_size = (int)strlen(tkey_i2c->light_version_full_efs) - 8;

	for (i = 0; i < table_size; i++) {
		char octa_temp = tkey_i2c->light_version_full_efs[8 + i];
		int octa_temp_i;

		if (octa_temp >= 'A')
			octa_temp_i = octa_temp - 'A' + 0x0A;
		else
			octa_temp_i = octa_temp - '0';
		
		input_info(true, &tkey_i2c->client->dev, "%s: octa %d\n", __func__, octa_temp_i);

		snprintf(predefine_value_path, LIGHT_TABLE_PATH_LEN, "%s%d",
				LIGHT_TABLE_PATH, octa_temp_i);
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		temp_file = filp_open(predefine_value_path, O_RDONLY, 0440);
		if (!IS_ERR(temp_file)) {
			temp_file->f_op->read(temp_file, temp_vol,
					sizeof(temp_vol), &temp_file->f_pos);
			filp_close(temp_file, current->files);
			if (kstrtoint(temp_vol, 0, &ret) < 0) {
				ret = -EIO;
			} else {
				crc += octa_temp_i;
				crc += ret;
				ret = 0;
			}
		}
		set_fs(old_fs);
	}

	if (!ret)
		ret = crc;

	return ret;
}

static int efs_read_crc(struct touchkey_i2c *tkey_i2c)
{
	struct file *temp_file = NULL;
	char crc[LIGHT_CRC_SIZE] = {0, };
	mm_segment_t old_fs;
	int ret = 0;
	
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	temp_file = filp_open(LIGHT_CRC_PATH, O_RDONLY, 0440);
	if (IS_ERR(temp_file)) {
		ret = PTR_ERR(temp_file);
		input_info(true, &tkey_i2c->client->dev,
				"%s: failed to open efs file %d\n", __func__, ret);
	} else {
		temp_file->f_op->read(temp_file, crc, sizeof(crc), &temp_file->f_pos);
		filp_close(temp_file, current->files);
		if (kstrtoint(crc, 0, &ret) < 0)
			ret = -EIO;
	}
	set_fs(old_fs);

	return ret;
}


static bool check_light_table_crc(struct touchkey_i2c *tkey_i2c)
{
	int crc_efs = efs_read_crc(tkey_i2c);

	if (tkey_i2c->light_version_efs == LIGHT_VERSION) {
		/* compare efs crc file with binary crc*/
		input_info(true, &tkey_i2c->client->dev,
				"%s: efs:%d, bin:%d\n",
				__func__, crc_efs, tkey_i2c->light_table_crc);
		if (crc_efs != tkey_i2c->light_table_crc)
			return false;
	}

	return true;
}

static int efs_write_light_table_crc(struct touchkey_i2c *tkey_i2c, int crc_cal)
{
	struct file *temp_file = NULL;
	char crc[LIGHT_CRC_SIZE] = {0, };
	mm_segment_t old_fs;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	temp_file = filp_open(LIGHT_CRC_PATH, O_TRUNC | O_RDWR | O_CREAT, 0660);
	if (IS_ERR(temp_file)) {
		ret = PTR_ERR(temp_file);
		input_info(true, &tkey_i2c->client->dev,
				"%s: failed to open efs file %d\n", __func__, ret);
	} else {
		snprintf(crc, sizeof(crc), "%d", crc_cal);
		temp_file->f_op->write(temp_file, crc, sizeof(crc), &temp_file->f_pos);
		filp_close(temp_file, current->files);
		input_info(true, &tkey_i2c->client->dev, "%s: %s\n", __func__, crc);
	}
	set_fs(old_fs);
	return ret;
}

static int efs_write_light_table_version(struct touchkey_i2c *tkey_i2c, char *full_version)
{
	struct file *temp_file = NULL;
	mm_segment_t old_fs;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	temp_file = filp_open(LIGHT_VERSION_PATH, O_TRUNC | O_RDWR | O_CREAT, 0660);
	if (IS_ERR(temp_file)) {
		ret = -ENOENT;
	} else {
		temp_file->f_op->write(temp_file, full_version,
				LIGHT_VERSION_LEN, &temp_file->f_pos);
		filp_close(temp_file, current->files);
		input_info(true, &tkey_i2c->client->dev, "%s: version = %s\n",
				__func__, full_version);
	}
	set_fs(old_fs);
	return ret;
}

static int efs_write_light_table(struct touchkey_i2c *tkey_i2c, struct light_info table)
{
	struct file *type_filp = NULL;
	mm_segment_t old_fs;
	int ret = 0;
	char predefine_value_path[LIGHT_TABLE_PATH_LEN];
	char vol_mv[LIGHT_DATA_SIZE] = {0, };

	snprintf(predefine_value_path, LIGHT_TABLE_PATH_LEN,
			"%s%d", LIGHT_TABLE_PATH, table.octa_id);
	snprintf(vol_mv, sizeof(vol_mv), "%d", table.vol_mv);

	input_info(true, &tkey_i2c->client->dev, "%s: make %s\n", __func__, predefine_value_path);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	type_filp = filp_open(predefine_value_path, O_TRUNC | O_RDWR | O_CREAT, 0660);
	if (IS_ERR(type_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(type_filp);
		input_err(true, &tkey_i2c->client->dev, "%s: open fail :%d\n",
			__func__, ret);
		return ret;
	}

	type_filp->f_op->write(type_filp, vol_mv, sizeof(vol_mv), &type_filp->f_pos);
	filp_close(type_filp, current->files);
	set_fs(old_fs);

	return ret;
}

static int efs_write(struct touchkey_i2c *tkey_i2c)
{
	int ret = 0;
	int i, crc_cal;

	ret = efs_write_light_table_version(tkey_i2c, tkey_i2c->light_version_full_bin);
	if (ret < 0)
		return ret;
	tkey_i2c->light_version_efs = LIGHT_VERSION;

	for (i = 0; i < LIGHT_TABLE_MAX; i++) {
		ret = efs_write_light_table(tkey_i2c, tkey_light_voltage_table[i]);
		if (ret < 0)
			break;
	}
	if (ret < 0)
		return ret;

	crc_cal = efs_calculate_crc(tkey_i2c);
	if (crc_cal < 0)
		return crc_cal;

	ret = efs_write_light_table_crc(tkey_i2c, crc_cal);
	if (ret < 0)
		return ret;

	if (!check_light_table_crc(tkey_i2c))
		ret = -EIO;

	return ret;
}

static int pick_light_table_version(char* str)
{
	static char* str_addr;
	char* token = NULL;
	int ret = 0;
	
	if (str != NULL)
		str_addr = str;
	else if (str_addr == NULL)
		return 0;

	token = str_addr;
	while (true) {
		if (!(*str_addr)) {
			break;
 		} else if (*str_addr == 'T') {
			*str_addr = '0';
		} else if (*str_addr == '.') {
			*str_addr = '\0';
			str_addr = str_addr + 1;
			break;
		}
		str_addr++;
	}

	if (kstrtoint(token + 1, 0, &ret) < 0)
		return 0;

	return ret;
}


static int efs_read_light_table_version(struct touchkey_i2c *tkey_i2c)
{
	struct file *temp_file = NULL;
	char version[LIGHT_VERSION_LEN] = {0, };
	mm_segment_t old_fs;
	int ret = 0;
	
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	temp_file = filp_open(LIGHT_VERSION_PATH, O_RDONLY, 0440);
	if (IS_ERR(temp_file)) {
		ret = PTR_ERR(temp_file);
	} else {
		temp_file->f_op->read(temp_file, version, sizeof(version), &temp_file->f_pos);
		filp_close(temp_file, current->files);
		input_info(true, &tkey_i2c->client->dev,
				"%s: table full version = %s\n", __func__, version);
		snprintf(tkey_i2c->light_version_full_efs,
				sizeof(tkey_i2c->light_version_full_efs), version);
		tkey_i2c->light_version_efs = pick_light_table_version(version);
		input_dbg(true, &tkey_i2c->client->dev,
				"%s: table version = %d\n", __func__, tkey_i2c->light_version_efs);
	}
	set_fs(old_fs);

	return ret;
}

static int efs_read_light_table(struct touchkey_i2c *tkey_i2c, int octa_id)
{
	struct file *type_filp = NULL;
	mm_segment_t old_fs;
	char predefine_value_path[LIGHT_TABLE_PATH_LEN];
	char voltage[LIGHT_DATA_SIZE] = {0, };
	int ret;

	snprintf(predefine_value_path, LIGHT_TABLE_PATH_LEN,
		"%s%d", LIGHT_TABLE_PATH, octa_id);

	input_info(true, &tkey_i2c->client->dev, "%s: %s\n", __func__, predefine_value_path);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	type_filp = filp_open(predefine_value_path, O_RDONLY, 0440);
	if (IS_ERR(type_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(type_filp);
		input_err(true, &tkey_i2c->client->dev,
				"%s: fail to open light data %d\n", __func__, ret);
		return ret;
	}

	type_filp->f_op->read(type_filp, voltage, sizeof(voltage), &type_filp->f_pos);
	filp_close(type_filp, current->files);
	set_fs(old_fs);

	if (kstrtoint(voltage, 0, &ret) < 0)
		return -EIO;

	return ret;
}

static int efs_read_light_table_with_default(struct touchkey_i2c *tkey_i2c, int octa_id)
{
	bool set_default = false;
	int ret;

retry:
	if (set_default)
		octa_id = WINDOW_COLOR_DEFAULT;

	ret = efs_read_light_table(tkey_i2c, octa_id);
	if (ret < 0) {
		if (!set_default) {
			set_default = true;
			goto retry;
		}
	}

	return ret;
}


static bool need_update_light_table(struct touchkey_i2c *tkey_i2c)
{
	/* Check version file exist*/
	if (efs_read_light_table_version(tkey_i2c) < 0) {
		return true;
	}

	/* Compare version */
	input_info(true, &tkey_i2c->client->dev,
			"%s: efs:%d, bin:%d\n", __func__,
			tkey_i2c->light_version_efs, LIGHT_VERSION);
	if (tkey_i2c->light_version_efs < LIGHT_VERSION)
		return true;

	/* Check CRC */
	if (!check_light_table_crc(tkey_i2c)) {
		input_info(true, &tkey_i2c->client->dev,
				"%s: crc is diffrent\n", __func__);
		return true;
	}

	return false;
}

static void touchkey_efs_open_work(struct work_struct *work)
{
	struct touchkey_i2c *tkey_i2c =
			container_of(work, struct touchkey_i2c, efs_open_work.work);
	int window_type;
	static int count = 0;
	int vol_mv;

	if (need_update_light_table(tkey_i2c)) {
		if (efs_write(tkey_i2c) < 0)
			goto out;
	}

	window_type = read_window_type();
	if (window_type < 0)
		goto out;

	vol_mv = efs_read_light_table_with_default(tkey_i2c, window_type);
	if (vol_mv >= LIGHT_VOLTAGE_MIN_VAL) {
		change_touch_key_led_voltage(&tkey_i2c->client->dev, vol_mv);
		input_info(true, &tkey_i2c->client->dev,
				"%s: read done for %d\n", __func__, window_type);
	} else {
		input_err(true, &tkey_i2c->client->dev,
				"%s: fail. voltage is %d\n", __func__, vol_mv);
	}
	return;

out:
	if (count < 50) {
		schedule_delayed_work(&tkey_i2c->efs_open_work, msecs_to_jiffies(2000));
		count++;
 	} else {
		input_err(true, &tkey_i2c->client->dev,
				"%s: retry %d times but can't check efs\n", __func__, count);
 	}
}
#endif
static int i2c_touchkey_read(struct i2c_client *client,
		u8 reg, u8 *val, unsigned int len)
{
	int ret = 0;
	int retry = 3;
	struct touchkey_i2c *tkey_i2c;

	if (client == NULL) {
		pr_err("%s client is null\n", SECLOG);
		return -ENODEV;
	}
	tkey_i2c = i2c_get_clientdata(client);

	mutex_lock(&tkey_i2c->i2c_lock);

	if (!tkey_i2c->enabled) {
		input_err(true, &client->dev, "Touchkey is not enabled. %d\n",
				__LINE__);
		ret = -ENODEV;
		goto out_i2c_read;
	}

	while (retry--) {
		ret = i2c_smbus_read_i2c_block_data(client,
				reg, len, val);
		if (ret < 0) {
			input_err(true, &client->dev, "%s:error(%d)\n", __func__, ret);
			usleep_range(10000, 10000);
			continue;
		}
		break;
	}

out_i2c_read:
	mutex_unlock(&tkey_i2c->i2c_lock);
	return ret;
}

static int i2c_touchkey_write(struct i2c_client *client,
		u8 *val, unsigned int len)
{
	int ret = 0;
	int retry = 3;
	struct touchkey_i2c *tkey_i2c;

	if (client == NULL) {
		pr_err("%s client is null\n", SECLOG);
		return -ENODEV;
	}
	tkey_i2c = i2c_get_clientdata(client);

	mutex_lock(&tkey_i2c->i2c_lock);

	if (!tkey_i2c->enabled) {
		input_err(true, &client->dev, "Touchkey is not enabled. %d\n",
				__LINE__);
		ret = -ENODEV;
		goto out_i2c_write;
	}

	while (retry--) {
		ret = i2c_smbus_write_i2c_block_data(client,
				BASE_REG, len, val);
		if (ret < 0) {
			input_err(true, &client->dev, "%s:error(%d)\n", __func__, ret);
			usleep_range(10000, 10000);
			continue;
		}
		break;
	}

out_i2c_write:
	mutex_unlock(&tkey_i2c->i2c_lock);
	return ret;
}

static int touchkey_i2c_check(struct touchkey_i2c *tkey_i2c)
{
	char data[4] = { 0, };
	int ret = 0;
	int retry  = 3;

	while (retry--) {
		ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 4);
		if (ret < 0) {
			input_err(true, &tkey_i2c->client->dev,
					"Failed to read Module version retry %d\n", retry);
			if (retry == 1) {
				tkey_i2c->fw_ver_ic = 0;
				tkey_i2c->md_ver_ic = 0;
				tkey_i2c->device_ver = 0;
				return ret;
			}
			msleep(30);
			continue;
		}
		break;
	}

	tkey_i2c->fw_ver_ic = data[1];
	tkey_i2c->md_ver_ic = data[2];
	tkey_i2c->device_ver = data[3];

#ifdef CRC_CHECK_INTERNAL
	ret = i2c_touchkey_read(tkey_i2c->client, 0x30, data, 2);
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev, "Failed to read crc\n");
		tkey_i2c->crc = 0;
		return ret;
	}

	tkey_i2c->crc = ((0xFF & data[1]) << 8) | data[0];
#endif

	input_info(true, &tkey_i2c->client->dev,
			"%s: ic_fw_ver = 0x%02x, module_ver = 0x%02x, CY device = 0x%02x\n",
			__func__, tkey_i2c->fw_ver_ic, tkey_i2c->md_ver_ic, tkey_i2c->device_ver);

	return ret;
}

static int touchkey_pinctrl_init(struct touchkey_i2c *tkey_i2c)
{
	struct device *dev = &tkey_i2c->client->dev;
	int i;

	tkey_i2c->pinctrl_irq = devm_pinctrl_get(dev);
	if (IS_ERR(tkey_i2c->pinctrl_irq)) {
		input_err(true, dev, "%s: Failed to get pinctrl\n", __func__);
		goto err_pinctrl_get;
	}
	for (i = 0; i < 2; ++i) {
		tkey_i2c->pin_state[i] = pinctrl_lookup_state(tkey_i2c->pinctrl_irq, str_states[i]);
		if (IS_ERR(tkey_i2c->pin_state[i])) {
			input_err(true, dev, "%s: Failed to get pinctrl state\n", __func__);
			goto err_pinctrl_get_state;
		}
	}

	dev = tkey_i2c->client->dev.parent->parent;
	input_err(true, dev, "%s: use dev's parent\n", __func__);

	tkey_i2c->pinctrl_i2c = devm_pinctrl_get(dev);
	if (IS_ERR(tkey_i2c->pinctrl_i2c)) {
		input_err(true, dev, "%s: Failed to get pinctrl\n", __func__);
		goto err_pinctrl_get_i2c;
	}
	for (i = 2; i < 4; ++i) {
		tkey_i2c->pin_state[i] = pinctrl_lookup_state(tkey_i2c->pinctrl_i2c, str_states[i]);
		if (IS_ERR(tkey_i2c->pin_state[i])) {
			input_err(true, dev, "%s: Failed to get pinctrl state\n", __func__);
			goto err_pinctrl_get_state_i2c;
		}
	}

	return 0;

err_pinctrl_get_state_i2c:
	devm_pinctrl_put(tkey_i2c->pinctrl_i2c);
err_pinctrl_get_i2c:
err_pinctrl_get_state:
	devm_pinctrl_put(tkey_i2c->pinctrl_irq);
err_pinctrl_get:
	return -ENODEV;
}

int touchkey_pinctrl(struct touchkey_i2c *tkey_i2c, int state)
{
	struct pinctrl *pinctrl = tkey_i2c->pinctrl_irq;
	int ret = 0;

	if (state >= I_STATE_ON_I2C)
		pinctrl = tkey_i2c->pinctrl_i2c;

	ret = pinctrl_select_state(pinctrl, tkey_i2c->pin_state[state]);
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev,
				"%s: Failed to configure irq pin\n", __func__);
		return ret;
	}

	return 0;
}

static void cypress_config_gpio_i2c(struct touchkey_i2c *tkey_i2c, int onoff)
{
	int ret;
	int state = onoff ? I_STATE_ON_I2C : I_STATE_OFF_I2C;

	ret = touchkey_pinctrl(tkey_i2c, state);
	if (ret < 0)
		input_err(true, &tkey_i2c->client->dev,
				"%s: Failed to configure i2c pin\n", __func__);
}

#ifdef TK_INFORM_CHARGER
static int touchkey_ta_setting(struct touchkey_i2c *tkey_i2c)
{
	u8 data[6] = { 0, };
	int count = 0;
	int ret = 0;
	unsigned short retry = 0;

	if (tkey_i2c->charging_mode) {
		data[1] = TK_BIT_CMD_TA_ON;
		data[2] = TK_BIT_WRITE_CONFIRM;
	} else {
		data[1] = TK_BIT_CMD_REGULAR;
		data[2] = TK_BIT_WRITE_CONFIRM;
	}

	count = i2c_touchkey_write(tkey_i2c->client, data, 3);

	while (retry < 3) {
		msleep(30);

		ret = i2c_touchkey_read(tkey_i2c->client, TK_STATUS_FLAG, data, 1);

		if (tkey_i2c->charging_mode) {
			if (data[0] & TK_BIT_TA_ON) {
				input_dbg(true, &tkey_i2c->client->dev,
						"%s: TA mode is Enabled\n", __func__);
				break;
			} else {
				input_err(true, &tkey_i2c->client->dev,
						"%s: Error to enable TA mode, retry %d\n",
						__func__, retry);
			}
		} else {
			if (!(data[0] & TK_BIT_TA_ON)) {
				input_dbg(true, &tkey_i2c->client->dev,
						"%s: TA mode is Disabled\n", __func__);
				break;
			} else {
				input_err(true, &tkey_i2c->client->dev,
						"%s: Error to disable TA mode, retry %d\n",
						__func__, retry);
			}
		}
		retry = retry + 1;
	}

	if (retry == 3)
		input_err(true, &tkey_i2c->client->dev, "%s: Failed to set the TA mode\n", __func__);

	return count;

}

static void touchkey_ta_cb(struct touchkey_callbacks *cb, bool ta_status)
{
	struct touchkey_i2c *tkey_i2c =
		container_of(cb, struct touchkey_i2c, callbacks);
	struct i2c_client *client = tkey_i2c->client;

	tkey_i2c->charging_mode = ta_status;

	if (tkey_i2c->enabled)
		touchkey_ta_setting(tkey_i2c);
}
#endif

static int touchkey_enable_status_update(struct touchkey_i2c *tkey_i2c)
{
	u8 data[4] = { 0, };
	int ret = 0;

	ret = i2c_touchkey_read(tkey_i2c->client, BASE_REG, data, 4);
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev, "%s: Failed to read Keycode_reg\n",
				__func__);
		return ret;
	}

	input_dbg(true, &tkey_i2c->client->dev,
			"data[0]=%x, data[1]=%x\n",
			data[0], data[1]);

	data[1] = TK_BIT_CMD_INSPECTION;
	data[2] = TK_BIT_WRITE_CONFIRM;

	ret = i2c_touchkey_write(tkey_i2c->client, data, 3);
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev, "%s, err(%d)\n", __func__, ret);
		tkey_i2c->status_update = false;
		return ret;
	}

	tkey_i2c->status_update = true;
	input_dbg(true, &tkey_i2c->client->dev, "%s\n", __func__);

	msleep(20);

	return 0;
}

static u8 touchkey_get_read_size(u8 cmd)
{
	switch (cmd) {
	case TK_CMD_READ_RAW:
	case TK_CMD_READ_DIFF:
	case TK_BASELINE_DATA:
		return 2;
	case TK_CMD_READ_IDAC:
	case TK_CMD_COMP_IDAC:
	case TK_CMD_READ_THRESHOLD:
		return 1;
		break;
	default:
		break;
	}
	return 0;
}

static int touchkey_fac_read_data(struct device *dev,
		char *buf, struct FAC_CMD *cmd)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int ret;
	u8 size;
	u8 base_index;
	u8 data[26] = { 0, };
	int i, j = 0;
	u16 max_val = 0;

	if (unlikely(!tkey_i2c->status_update)) {
		ret = touchkey_enable_status_update(tkey_i2c);
		if (ret < 0)
			goto out_fac_read_data;
	}

	size = touchkey_get_read_size(cmd->cmd);
	if (size == 0) {
		input_err(true, &tkey_i2c->client->dev, "wrong size %d\n", size);
		goto out_fac_read_data;
	}

	if (cmd->opt1 > 4) {
		input_err(true, &tkey_i2c->client->dev, "wrong opt1 %d\n", cmd->opt1);
		goto out_fac_read_data;
	}

	base_index = fac_reg_index[cmd->cmd] + size * cmd->opt1;
	if (base_index > 46) {
		input_err(true, &tkey_i2c->client->dev, "wrong index %d, cmd %d, size %d, opt1 %d\n",
				base_index, cmd->cmd, size, cmd->opt1);
		goto out_fac_read_data;
	}

	ret = i2c_touchkey_read(tkey_i2c->client, base_index, data, size);
	if (ret <  0) {
		input_err(true, &tkey_i2c->client->dev, "i2c read failed\n");
		goto out_fac_read_data;
	}

	/* make value */
	cmd->result = 0;
	for (i = size - 1; i >= 0; --i) {
		cmd->result = cmd->result | (data[j++] << (8 * i));
		max_val |= 0xff << (8 * i);
	}

	/* garbage check */
	if (unlikely(cmd->result == max_val)) {
		input_err(true, &tkey_i2c->client->dev, "cmd %d opt1 %d, max value\n",
				cmd->cmd, cmd->opt1);
		cmd->result = 0;
	}

out_fac_read_data:
	return sprintf(buf, "%d\n", cmd->result);
}

static ssize_t touchkey_raw_data0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_RAW, 0, 0}; /* recent outer*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_raw_data1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_RAW, 1, 0}; /* recent inner*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_diff_data0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_DIFF, 0, 0}; /* recent outer*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_diff_data1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_DIFF, 1, 0}; /* recent inner*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_idac0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_IDAC, 0, 0}; /* recent outer*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_idac1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_IDAC, 1, 0}; /* recent inner*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_threshold0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_THRESHOLD, 0, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t get_chip_vendor(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "CYPRESS\n");
}

/* To check firmware compatibility */
int get_module_class(u8 ver)
{
	static int size = ARRAY_SIZE(module_divider);
	int i;

	if (size == 2)
		return 0;

	for (i = size - 1; i > 0; --i) {
		if (ver < module_divider[i] &&
				ver >= module_divider[i-1])
			return i;
	}

	return 0;
}

bool is_same_module_class(struct touchkey_i2c *tkey_i2c)
{
	int class_ic, class_file;

	if (tkey_i2c->src_md_ver == tkey_i2c->md_ver_ic)
		return true;

	class_file = get_module_class(tkey_i2c->src_md_ver);
	class_ic = get_module_class(tkey_i2c->md_ver_ic);

	input_info(true, &tkey_i2c->client->dev, "module class, IC %d, File %d\n",
			class_ic, class_file);

	if (class_file == class_ic)
		return true;

	return false;
}

int tkey_load_fw_built_in(struct touchkey_i2c *tkey_i2c)
{
	int retry = 3;
	int ret;

	while (retry--) {
		ret = request_firmware(&tkey_i2c->firm_data, tkey_i2c->dtdata->fw_path,
					&tkey_i2c->client->dev);
		if (ret < 0) {
			input_err(true, &tkey_i2c->client->dev,
					"Unable to open firmware. ret %d retry %d\n",
					ret, retry);
			continue;
		}
		break;
	}

	if (ret >= 0) {
		input_info(true, &tkey_i2c->client->dev,
				"%s: fw path loaded %s\n", __func__, tkey_i2c->dtdata->fw_path);
		tkey_i2c->fw_img = (struct fw_image *)tkey_i2c->firm_data->data;
		tkey_i2c->src_fw_ver = tkey_i2c->fw_img->first_fw_ver;
		tkey_i2c->src_md_ver = tkey_i2c->fw_img->second_fw_ver;
	}

	return ret;
}

int tkey_load_fw_ffu(struct touchkey_i2c *tkey_i2c)
{
	int retry = 3;
	int ret;

	while (retry--) {
		ret = request_firmware(&tkey_i2c->firm_data, TKEY_FW_FFU_PATH,
					&tkey_i2c->client->dev);
		if (ret < 0) {
			input_err(true, &tkey_i2c->client->dev,
					"Unable to open firmware. ret %d retry %d\n",
					ret, retry);
			continue;
		}
		break;
	}

	if (ret >= 0) {
		input_info(true, &tkey_i2c->client->dev,
				"%s: fw path loaded %s\n", __func__, tkey_i2c->dtdata->fw_path);
		tkey_i2c->fw_img = (struct fw_image *)tkey_i2c->firm_data->data;
		tkey_i2c->src_fw_ver = tkey_i2c->fw_img->first_fw_ver;
		tkey_i2c->src_md_ver = tkey_i2c->fw_img->second_fw_ver;
	}

	return ret;
}

int tkey_load_fw_sdcard(struct touchkey_i2c *tkey_i2c)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(TKEY_FW_PATH, O_RDONLY, S_IRUSR);

	if (IS_ERR(fp)) {
		input_err(true, &tkey_i2c->client->dev, "failed to open %s.\n", TKEY_FW_PATH);
		ret = -ENOENT;
		set_fs(old_fs);
		return ret;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	input_dbg(true, &tkey_i2c->client->dev,
			"start, file path %s, size %ld Bytes\n",
			TKEY_FW_PATH, fsize);

	tkey_i2c->fw_img = kmalloc(fsize, GFP_KERNEL);
	if (!tkey_i2c->fw_img) {
		input_err(true, &tkey_i2c->client->dev,
				"%s, kmalloc failed\n", __func__);
		ret = -EFAULT;
		goto malloc_error;
	}

	nread = vfs_read(fp, (char __user *)tkey_i2c->fw_img,
			fsize, &fp->f_pos);
	input_dbg(true, &tkey_i2c->client->dev, "nread %ld Bytes\n", nread);
	if (nread != fsize) {
		input_err(true, &tkey_i2c->client->dev,
				"failed to read firmware file, nread %ld Bytes\n",
				nread);
		ret = -EIO;
		kfree(tkey_i2c->fw_img);
		goto read_err;
	}

	filp_close(fp, current->files);
	set_fs(old_fs);

	return 0;

read_err:
malloc_error:
	filp_close(fp, current->files);
	set_fs(old_fs);
	return ret;
}

int touchkey_load_fw(struct touchkey_i2c *tkey_i2c, u8 fw_path)
{
	int ret = 0;

	switch (fw_path) {
	case FW_BUILT_IN:
		ret = tkey_load_fw_built_in(tkey_i2c);
		break;
	case FW_IN_SDCARD:
		ret = tkey_load_fw_sdcard(tkey_i2c);
		break;
	case FW_FFU:
		ret = tkey_load_fw_ffu(tkey_i2c);
		break;
	default:
		input_err(true, &tkey_i2c->client->dev,
				"unknown path(%d)\n", fw_path);
		break;
	}

	return ret;
}

void touchkey_unload_fw(struct touchkey_i2c *tkey_i2c)
{
	switch (tkey_i2c->fw_path) {
	case FW_BUILT_IN:
		release_firmware(tkey_i2c->firm_data);
		tkey_i2c->firm_data = NULL;
		break;
	case FW_IN_SDCARD:
		kfree(tkey_i2c->fw_img);
		tkey_i2c->fw_img = NULL;
		break;
	default:
		break;
	}
	tkey_i2c->fw_path = FW_NONE;
}

int check_update_condition(struct touchkey_i2c *tkey_i2c)
{
	int ret = 0;

	/* check update condition */
	ret = is_same_module_class(tkey_i2c);
	if (!ret) {
		input_err(true, &tkey_i2c->client->dev, "%s: md classes are different\n", __func__);
		return TK_EXIT_UPDATE;
	}

	if (tkey_i2c->fw_ver_ic < tkey_i2c->src_fw_ver)
		return TK_RUN_UPDATE;

	/* if ic ver is higher than file, exit */
	if (tkey_i2c->fw_ver_ic > tkey_i2c->src_fw_ver)
		return TK_EXIT_UPDATE;

	return TK_RUN_CHK;
}

static int touchkey_i2c_update(struct touchkey_i2c *tkey_i2c)
{
	int ret;
	int retry = 3;

	disable_irq(tkey_i2c->irq);
	wake_lock(&tkey_i2c->fw_wakelock);

	input_dbg(true, &tkey_i2c->client->dev, "%s\n", __func__);
	tkey_i2c->update_status = TK_UPDATE_DOWN;

	//cypress_config_gpio_i2c(tkey_i2c, 0);

	while (retry--) {
		ret = cy8cmbr_swd_program(tkey_i2c);
		if (ret != 0) {
			msleep(50);
			input_err(true, &tkey_i2c->client->dev, "failed to update f/w. retry\n");
			continue;
		}

		input_info(true, &tkey_i2c->client->dev, "finish f/w update\n");
		tkey_i2c->update_status = TK_UPDATE_PASS;
		break;
	}

	if (retry <= 0) {
		tkey_i2c->power(tkey_i2c, false);
		tkey_i2c->update_status = TK_UPDATE_FAIL;
		input_err(true, &tkey_i2c->client->dev, "failed to update f/w\n");
		ret = TK_UPDATE_FAIL;
		goto err_fw_update;
	}

	//cypress_config_gpio_i2c(tkey_i2c, 1);

	msleep(tkey_i2c->dtdata->stabilizing_time);

	ret = touchkey_i2c_check(tkey_i2c);
	if (ret < 0)
		goto err_fw_update;

	input_info(true, &tkey_i2c->client->dev, "f/w ver = %#X, module ver = %#X\n",
			tkey_i2c->fw_ver_ic, tkey_i2c->md_ver_ic);

	enable_irq(tkey_i2c->irq);
err_fw_update:
	touchkey_unload_fw(tkey_i2c);
	wake_unlock(&tkey_i2c->fw_wakelock);
	return ret;
}

int touchkey_fw_update(struct touchkey_i2c *tkey_i2c, u8 fw_path, bool bforced)
{
	int ret = 0;

	if (tkey_i2c->device_ver == 0x10) {
		/* skip fw update(20075) */
		goto out;
	}

	ret = touchkey_load_fw(tkey_i2c, fw_path);
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev,
				"failed to load fw data\n");
		goto out;
	}
	tkey_i2c->fw_path = fw_path;

	/* f/w info */
	input_info(true, &tkey_i2c->client->dev, "fw ver %#x, new fw ver %#x\n",
			tkey_i2c->fw_ver_ic, tkey_i2c->src_fw_ver);
	input_info(true, &tkey_i2c->client->dev, "module ver %#x, new module ver %#x\n",
			tkey_i2c->md_ver_ic, tkey_i2c->src_md_ver);
#ifdef CRC_CHECK_INTERNAL
	input_info(true, &tkey_i2c->client->dev, "checkksum %#x, new checksum %#x\n",
			tkey_i2c->crc, tkey_i2c->fw_img->checksum);
#else
	input_info(true, &tkey_i2c->client->dev, "new checksum %#x\n",
			tkey_i2c->fw_img->checksum);
#endif

	if (unlikely(bforced))
		goto run_fw_update;

	ret = check_update_condition(tkey_i2c);

	if (ret == TK_EXIT_UPDATE) {
		input_info(true, &tkey_i2c->client->dev, "pass fw update\n");
		touchkey_unload_fw(tkey_i2c);
		goto out;
	}

	if (ret == TK_RUN_CHK) {
#ifdef CRC_CHECK_INTERNAL
		if (tkey_i2c->crc == tkey_i2c->fw_img->checksum) {
			input_info(true, &tkey_i2c->client->dev, "pass fw update\n");
			touchkey_unload_fw(tkey_i2c);
			goto out;
		}
#else
		tkey_i2c->do_checksum = true;
#endif
	}
	/* else do update */

run_fw_update:
#ifdef TK_USE_FWUPDATE_DWORK
	queue_work(tkey_i2c->fw_wq, &tkey_i2c->update_work);
#else
	ret = touchkey_i2c_update(tkey_i2c);
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev,
				"touchkey_i2c_update fail\n");
	}
#endif
out:
	return ret;
}

#ifdef TK_USE_FWUPDATE_DWORK
static void touchkey_i2c_update_work(struct work_struct *work)
{
	struct touchkey_i2c *tkey_i2c =
		container_of(work, struct touchkey_i2c, update_work);

	touchkey_i2c_update(tkey_i2c);
}
#endif

static irqreturn_t touchkey_interrupt(int irq, void *dev_id)
{
	struct touchkey_i2c *tkey_i2c = dev_id;
	u8 data[3];
	int ret;
	int i;
	int keycode_data[touchkey_count];

	if (unlikely(!touchkey_probe)) {
		input_err(true, &tkey_i2c->client->dev, "%s: Touchkey is not probed\n", __func__);
		return IRQ_HANDLED;
	}

	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 1);
	if (ret < 0)
		return IRQ_HANDLED;

	keycode_data[1] = data[0] & 0x3;
	keycode_data[2] = (data[0] >> 2) & 0x3;

	for (i = 1; i < touchkey_count; i++) {
		if (keycode_data[i]) {
			input_report_key(tkey_i2c->input_dev, touchkey_keycode[i], (keycode_data[i] % 2));
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
			input_err(true, &tkey_i2c->client->dev, "%s ver[%#x] mode[%d]\n",
					(keycode_data[i] % 2) ? "PRESS" : "RELEASE", tkey_i2c->fw_ver_ic,
					tkey_i2c->mc_data.cur_mode);
#else
			input_err(true, &tkey_i2c->client->dev, "%d %s ver[%#x] mode[%d]\n",
					touchkey_keycode[i], (keycode_data[i] % 2) ? "PRESS" : "RELEASE",
					tkey_i2c->fw_ver_ic, tkey_i2c->mc_data.cur_mode);
#endif
		}
	}

	input_sync(tkey_i2c->input_dev);

	return IRQ_HANDLED;
}

int touchkey_read_status(struct touchkey_i2c *tkey_i2c)
{
	char data[6] = {0, };
	int ret = 0;
	int retry = 3;

	while(retry--) {
		ret = i2c_touchkey_read(tkey_i2c->client, TK_STATUS_FLAG, data, 1);
		if (ret < 0) {
			input_err(true, &tkey_i2c->client->dev,
					"%s: Failed to read Keycode_reg %d times\n",
					__func__, retry);
			continue;
		}
	}
	if (ret < 0)
		return ret;

	if (data[0] & TK_BIT_GLOVE)
		tkey_i2c->ic_mode = MODE_GLOVE;
	else if (data[0] & TK_BIT_FLIP)
		tkey_i2c->ic_mode = MODE_FLIP;
	else
		tkey_i2c->ic_mode = MODE_NORMAL;

	input_info(true, &tkey_i2c->client->dev, "%s: ic mode is %c%d\n",
			__func__, "0ngf"[tkey_i2c->ic_mode], tkey_i2c->ic_mode);
	return ret;
}

bool tkey_is_enabled(struct touchkey_i2c *tkey_i2c)
{
	if (!touchkey_probe) {
		return false;
	}
	if (!tkey_i2c->enabled) {
		return false;
	}
	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		return false;
	}

	return true;
}

#ifdef CONFIG_KEYBOARD_THD_CHANGE_AS_FINGERPRINT
int touchkey_thd_change(struct touchkey_i2c *tkey_i2c)
{
	char data[3] = {0, };
	int ret = 0;

	data[1] = TK_BIT_CMD_REGULAR;
	if (tkey_i2c->thd_changed)
		data[2] = TK_BIT_LOW_SENSITIVITY;
	else
		data[2] = TK_BIT_NORMAL_SENSITIVITY;

	ret = i2c_touchkey_write(tkey_i2c->client, data, 3);
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev, "%s, err(%d)\n", __func__, ret);
		return ret;
	}

	input_info(true, &tkey_i2c->client->dev, "%s: %s\n",
			__func__, tkey_i2c->thd_changed ? "on" : "off");
	return ret;
}


int change_touchkey_thd_for_fingerprint(int on)
{
	int ret;

	if (!tkey_info)
		return -ENXIO;
	if (!tkey_info->client)
		return -ENXIO;

	input_info(true, &tkey_info->client->dev, "%s: %s\n", __func__, on ? "on" : "off");

	tkey_info->thd_changed = on;

	if (!tkey_is_enabled(tkey_info)) {
		input_err(true, &tkey_info->client->dev,
			"%s: touchkey is not enabled. save status\n", __func__);
		return 0;
	}

	if (tkey_info->tsk_enable_glove_mode) {
		input_err(true, &tkey_info->client->dev,
			"%s: tkey is in glove mode or keyboard cover mode\n", __func__);
		return 0;
	}

	ret = touchkey_thd_change(tkey_info);

	return ret;
}
EXPORT_SYMBOL(change_touchkey_thd_for_fingerprint);
#endif

static int touchkey_stop(struct touchkey_i2c *tkey_i2c)
{
	int i;

	mutex_lock(&tkey_i2c->lock);

	if (!tkey_i2c->enabled) {
		input_err(true, &tkey_i2c->client->dev, "Touch key already disabled\n");
		goto err_stop_out;
	}
	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		input_dbg(true, &tkey_i2c->client->dev, "wake_lock active\n");
		goto err_stop_out;
	}

	disable_irq(tkey_i2c->irq);

	/* release keys */
	for (i = 1; i < touchkey_count; ++i) {
		input_report_key(tkey_i2c->input_dev,
				touchkey_keycode[i], 0);
	}
	input_sync(tkey_i2c->input_dev);

	tkey_i2c->enabled = false;
	tkey_i2c->status_update = false;

	if (touchkey_led_status == TK_CMD_LED_ON)
		touchled_cmd_reversed = 1;

	cypress_config_gpio_i2c(tkey_i2c, 0);
	tkey_i2c->power(tkey_i2c, false);

err_stop_out:
	mutex_unlock(&tkey_i2c->lock);

	return 0;
}

static int touchkey_start(struct touchkey_i2c *tkey_i2c)
{
	mutex_lock(&tkey_i2c->lock);

	if (tkey_i2c->enabled) {
		input_err(true, &tkey_i2c->client->dev, "Touch key already enabled\n");
		goto err_start_out;
	}
	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		input_dbg(true, &tkey_i2c->client->dev, "wake_lock active\n");
		goto err_start_out;
	}

	cypress_config_gpio_i2c(tkey_i2c, 1);
	tkey_i2c->power(tkey_i2c, true);
	msleep(tkey_i2c->dtdata->stabilizing_time);

	tkey_i2c->enabled = true;

	if (touchled_cmd_reversed) {
		touchled_cmd_reversed = 0;
		i2c_touchkey_write(tkey_i2c->client,
				(u8 *) &touchkey_led_status, 1);
		input_err(true, &tkey_i2c->client->dev, "%s: Turning LED is reserved\n", __func__);
		msleep(30);
	}

#ifdef TK_INFORM_CHARGER
	touchkey_ta_setting(tkey_i2c);
#endif

	tkey_i2c->write_without_cal = true;
	if (tkey_i2c->tsk_enable_glove_mode)
		touchkey_mode_change(tkey_i2c, CMD_GLOVE_ON);
	touchkey_read_status(tkey_i2c);
	touchkey_mode_change(tkey_i2c, CMD_MODE_RESERVED);

#ifdef CONFIG_KEYBOARD_THD_CHANGE_AS_FINGERPRINT
	if (!tkey_i2c->tsk_enable_glove_mode && tkey_i2c->thd_changed)
		touchkey_thd_change(tkey_i2c);
#endif

	enable_irq(tkey_i2c->irq);
err_start_out:
	mutex_unlock(&tkey_i2c->lock);

	return 0;
}
#ifdef TK_USE_OPEN_DWORK
static void touchkey_open_work(struct work_struct *work)
{
	int retval;
	struct touchkey_i2c *tkey_i2c =
		container_of(work, struct touchkey_i2c,
				open_work.work);

	if (tkey_i2c->enabled) {
		input_err(true, &tkey_i2c->client->dev, "Touch key already enabled\n");
		return;
	}

	retval = touchkey_start(tkey_i2c);
	if (retval < 0)
		input_err(true, &tkey_i2c->client->dev,
				"%s: Failed to start device\n", __func__);
}
#endif

static int touchkey_input_open(struct input_dev *dev)
{
	struct touchkey_i2c *data = input_get_drvdata(dev);
	int ret;

	if (!touchkey_probe) {
		input_err(true, &data->client->dev, "%s: Touchkey is not probed\n", __func__);
		return 0;
	}

#ifdef TK_USE_OPEN_DWORK
	schedule_delayed_work(&data->open_work,
			msecs_to_jiffies(TK_OPEN_DWORK_TIME));
#else
	ret = touchkey_start(data);
	if (ret)
		goto err_open;
#endif

	input_info(true, &data->client->dev, "%s\n", __func__);

	return 0;

err_open:
	return ret;
}

static void touchkey_input_close(struct input_dev *dev)
{
	struct touchkey_i2c *data = input_get_drvdata(dev);

	if (!touchkey_probe) {
		input_err(true, &data->client->dev, "%s: Touchkey is not probed\n", __func__);
		return;
	}

#ifdef TK_USE_OPEN_DWORK
	cancel_delayed_work(&data->open_work);
#endif
	touchkey_stop(data);

	input_info(true, &data->client->dev, "%s\n", __func__);
}

static ssize_t touchkey_led_control(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data,on;
	int ret;
	static const int ledCmd[] = {TK_CMD_LED_OFF, TK_CMD_LED_ON};

	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		input_dbg(true, &tkey_i2c->client->dev,
				"%s, wakelock active\n", __func__);
		return size;
	}

	ret = sscanf(buf, "%d", &data);
	input_info(true, &tkey_i2c->client->dev,
			"%s %s\n", __func__, data ? "ON" : "OFF");

	if (ret != 1) {
		input_err(true, &tkey_i2c->client->dev, "%s, %d err\n",
				__func__, __LINE__);
		return size;
	}

	if (data != 0 && data != 1) {
		input_err(true, &tkey_i2c->client->dev, "%s wrong cmd %x\n",
				__func__, data);
		return size;
	}

	on = data; /* data back-up to control led by ldo */
	data = ledCmd[data];
	touchkey_led_status = data;

	if (!tkey_i2c->enabled) {
		touchled_cmd_reversed = 1;
		goto out_led_control;
	}

	ret = i2c_touchkey_write(tkey_i2c->client, (u8 *) &data, 1);
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev, "%s: Error turn on led %d\n",
				__func__, ret);
		touchled_cmd_reversed = 1;
		goto out_led_control;
	}
	msleep(30);

out_led_control:
	return size;
}

int tkey_cmd_to_mode(int cmd)
{
	switch (cmd) {
	case CMD_GLOVE_ON:
		return MODE_GLOVE;
	case CMD_FLIP_ON:
		return MODE_FLIP;
	default:
		return MODE_NORMAL;
	}
	return MODE_NORMAL;
}

int touchkey_mode_change(struct touchkey_i2c *tkey_i2c, int cmd)
{
	int mode;
	int final_mode;
	bool changed = false;
	struct cmd_mode_change *pTarget;
	int is_enabled;
	static bool skip_by_status = false;
	static bool g_reserved_by_f1;
	struct mode_change_data *mc_data = &tkey_i2c->mc_data;

	is_enabled = tkey_is_enabled(tkey_i2c);

	mutex_lock(&tkey_i2c->mc_data.mc_lock);
	mutex_lock(&tkey_i2c->mc_data.mcf_lock);

	/* etc cmd handling */
	if (cmd == CMD_GET_LAST_MODE) {
		int mode;
		if (mc_data->mtr.mode)
			mode = mc_data->mtr.mode;
		else if (mc_data->mtc.mode)
			mode = mc_data->mtc.mode;
		else
			mode = tkey_i2c->ic_mode;

		mutex_unlock(&tkey_i2c->mc_data.mcf_lock);
		mutex_unlock(&tkey_i2c->mc_data.mc_lock);
		return mode;
	}
	if (cmd == CMD_MODE_RESET) {
		mc_data->cur_mode = MODE_NORMAL;
		mc_data->mtc.mode = MODE_NONE;
		if (mc_data->busy)
			mc_data->mtr.cmd = CMD_MODE_RESET;
		else
			mc_data->mtr.mode = MODE_NONE;
		input_dbg(true, &tkey_i2c->client->dev, "reset mode change\n");
		goto out_mode_change;
	}
	if (cmd == CMD_MODE_RESERVED) {
		if (skip_by_status)
			goto out_mode_change;
		if (mc_data->mtc.mode)
			goto out_mode_change;
		if (tkey_i2c->ic_mode == mc_data->cur_mode)
			goto out_mode_change;

		input_dbg(true, &tkey_i2c->client->dev, "cmd reserved\n");

		if (mc_data->cur_mode == MODE_FLIP) {
			mc_data->cur_mode = MODE_NORMAL;
			cmd = CMD_FLIP_ON;
		} else if (mc_data->cur_mode == MODE_GLOVE) {
			mc_data->cur_mode = MODE_NORMAL;
			cmd = CMD_GLOVE_ON;
		} else
			goto out_mode_change;
	}

	input_dbg(true, &tkey_i2c->client->dev,
			"%s cur mode %d, cmd %d\n", __func__, mc_data->cur_mode, cmd);
	mode = tkey_cmd_to_mode(cmd);

	/* setup target pointer */
	if (!mc_data->busy && !skip_by_status) {
		pTarget = &mc_data->mtc;
		final_mode = mc_data->cur_mode;
	} else if (mc_data->busy || skip_by_status) {
		if (!mc_data->mtr.mode) {
			pTarget = &mc_data->mtr;
			final_mode = mc_data->mtc.mode;
		} else if (mc_data->mtr.mode) {
			pTarget = &mc_data->mtr;
			final_mode = mc_data->mtr.mode;
		}
	}

	/* check last command */
	if (final_mode == mode) {
		input_dbg(true, &tkey_i2c->client->dev,
				"final mode %d, cmd %d, same, skip\n", final_mode, mode);
		goto out_mode_change;
	}

	/* set cmd by priority flip > glove, normal */
	if (final_mode == MODE_FLIP) {
		if (cmd == CMD_FLIP_OFF) {
			if (g_reserved_by_f1) {
				g_reserved_by_f1 = false;
				pTarget->mode = MODE_GLOVE;
				pTarget->cmd = CMD_GLOVE_ON;
			} else {
				pTarget->mode = MODE_NORMAL;
				pTarget->cmd = cmd;
			}
			changed = true;
		} else if (cmd == CMD_GLOVE_ON) {
			g_reserved_by_f1 = true;
		} else {
			g_reserved_by_f1 = false;
			goto out_mode_change;
		}
	}  else if (final_mode == MODE_GLOVE) {
		if (cmd == CMD_FLIP_OFF)
			goto out_mode_change;
		pTarget->mode = mode;
		pTarget->cmd = cmd;
		changed = true;
	}  else {
		pTarget->mode = mode;
		pTarget->cmd = cmd;
		changed = true;
	}

	/* queue size control */
	if (mc_data->busy) {
		if (mc_data->mtc.mode == mc_data->mtr.mode) {
			input_dbg(true, &tkey_i2c->client->dev,
					"previous mode is same with this mode, %d %d\n",
					mc_data->mtc.mode, mc_data->mtr.mode);
			mc_data->mtr.mode = MODE_NONE;
			goto out_mode_change;
		}
	}

	/* execute mode change work */
	if (!mc_data->busy && changed) {
		mc_data->busy = true;
		/* execute by IC status */
		if (is_enabled) {
			/*cancel_work_sync(&tkey_i2c->mode_change_work);*/
			schedule_work(&tkey_i2c->mode_change_work);
		} else {
			skip_by_status = true;
			input_dbg(true, &tkey_i2c->client->dev, "skipped by flag\n");
		}
	}

out_mode_change:
	if (skip_by_status && is_enabled) {
		skip_by_status = false;
		if (mc_data->mtr.mode) {
			mc_data->mtc.mode = mc_data->mtr.mode;
			mc_data->mtr.mode = MODE_NONE;
		}
		/* do not need to check status of work ? */
		/*cancel_work_sync(&tkey_i2c->mode_change_work);*/
		schedule_work(&tkey_i2c->mode_change_work);
		input_dbg(true, &tkey_i2c->client->dev, "execute change work by skipped flag\n");
	}

	mutex_unlock(&tkey_i2c->mc_data.mcf_lock);
	mutex_unlock(&tkey_i2c->mc_data.mc_lock);
	return 0;
}

int touchkey_mode_control(struct touchkey_i2c *tkey_i2c, int mode)
{
	u8 command[] = {0, TK_BIT_CMD_REGULAR, TK_BIT_CMD_GLOVE, TK_BIT_CMD_FLIP};
	u8 data[6] = { 0, };
	unsigned short retry = 3;
	u8 status;
	int ret = 0;

	while (retry--) {
		data[1] = command[mode];
		if (data[1] == TK_BIT_CMD_GLOVE && tkey_i2c->write_without_cal) {
			data[2] = TK_BIT_WRITE_WITHOUT_CAL;
			tkey_i2c->write_without_cal = false;
		} else {
			data[2] = TK_BIT_WRITE_CONFIRM;
		}

		ret = i2c_touchkey_write(tkey_i2c->client, data, 3);
		if (ret < 0) {
			input_err(true, &tkey_i2c->client->dev,
					"%s: Failed to write command Keycode_reg %d times\n",
					__func__, retry);
			continue;
		}

		msleep(30);

		input_info(true, &tkey_i2c->client->dev,
				"%s: data[1]=%02X data[2]=%02X\n", __func__, data[1], data[2]);

		/* Check status */
		ret = i2c_touchkey_read(tkey_i2c->client, TK_STATUS_FLAG, data, 1);
		if (ret < 0) {
			input_err(true, &tkey_i2c->client->dev,
					"%s: Failed to read Keycode_reg %d times\n",
					__func__, retry);
			continue;
		}

		switch (mode) {
		case MODE_NORMAL:
			status = data[0] & TK_BIT_GLOVE;
			status |= data[0] & TK_BIT_FLIP;
			if (status) {
				input_err(true, &tkey_i2c->client->dev,
						"%s: error to set normal mode, bit g:%d, f:%d\n",
						__func__, !!(data[0] & TK_BIT_GLOVE),
						!!(data[0] & TK_BIT_FLIP));
				continue;
			}
			break;
		case MODE_GLOVE:
			status = !!(data[0] & TK_BIT_GLOVE);
			if (!status) {
				input_err(true, &tkey_i2c->client->dev,
						"%s: error to set glove mode, bit g:%d, f:%d\n",
						__func__, !!(data[0] & TK_BIT_GLOVE),
						!!(data[0] & TK_BIT_FLIP));
				continue;
			}
			break;
		case MODE_FLIP:
			status = !!(data[0] & TK_BIT_FLIP);
			if (!status) {
				input_err(true, &tkey_i2c->client->dev,
						"%s: error to set flip mode, bit g:%d, f:%d\n",
						__func__, !!(data[0] & TK_BIT_GLOVE),
						!!(data[0] & TK_BIT_FLIP));
				continue;
			}
			break;
		default:
			input_dbg(true, &tkey_i2c->client->dev, "%s, unknown mode %d\n", __func__, mode);
			break;
		}
		break;
	}

	if (retry == 3)
		input_err(true, &tkey_i2c->client->dev,
				"%s: Failed to change mode to %d\n", __func__, mode);

	return ret;
}

static void touchkey_i2c_mode_change_work(struct work_struct *work)
{
	struct touchkey_i2c *tkey_i2c =
		container_of(work, struct touchkey_i2c, mode_change_work);
	struct mode_change_data *mc_data = &tkey_i2c->mc_data;
	struct cmd_mode_change mode;
	int ret = 0;

	while (1) {
		/* dequeue */
		mutex_lock(&tkey_i2c->mc_data.mcf_lock);
		if (mc_data->mtc.mode == MODE_NONE)
			goto out_mode_change_work;
		mc_data->busy = true;
		mode.mode = mc_data->mtc.mode;
		mode.cmd = mc_data->mtc.cmd;
		mutex_unlock(&tkey_i2c->mc_data.mcf_lock);

		/* change mode */
		ret = touchkey_mode_control(tkey_i2c, mode.mode);
		if (ret < 0) {
			input_err(true, &tkey_i2c->client->dev, "%s: error to change mode to %d\n",
					__func__, mode.mode);
		}

		input_dbg(false, &tkey_i2c->client->dev,
				"change mode, %d %d\n", mc_data->cur_mode, mode.mode);

		mutex_lock(&tkey_i2c->mc_data.mcf_lock);
		mc_data->cur_mode = mode.mode;
		mutex_unlock(&tkey_i2c->mc_data.mcf_lock);

		input_dbg(true, &tkey_i2c->client->dev,
				"mcf:%c%d\n", "0ngf"[mc_data->cur_mode], mc_data->cur_mode);

		/* after job */
		mutex_lock(&tkey_i2c->mc_data.mcf_lock);
		if (mc_data->mtr.cmd == CMD_MODE_RESET) {
			mc_data->cur_mode = MODE_NORMAL;
			input_dbg(true, &tkey_i2c->client->dev, "%s, reset reserved, exit\n", __func__);
			goto out_mode_change_work;
		}
		if (mc_data->mtr.mode) {
			mc_data->mtc.mode = mc_data->mtr.mode;
			mc_data->mtc.cmd = mc_data->mtr.cmd;
			mc_data->mtr.mode = MODE_NONE;
		} else {
			mc_data->mtc.mode = MODE_NONE;
			goto out_mode_change_work;
		}
		mutex_unlock(&tkey_i2c->mc_data.mcf_lock);
	}

out_mode_change_work:
	mc_data->busy = false;
	mutex_unlock(&tkey_i2c->mc_data.mcf_lock);
	return ;
}

#ifdef CONFIG_GLOVE_TOUCH
static ssize_t glove_mode_enable(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data;
	int mode;

	sscanf(buf, "%d\n", &data);
	input_dbg(true, &tkey_i2c->client->dev, "%s %d\n", __func__, data);

	mode = touchkey_mode_change(tkey_i2c, CMD_GET_LAST_MODE);
	if (mode == MODE_FLIP && data == 0) {
		input_info(true, &client->dev, "%s, pass glove off by flip on\n", __func__);
		return size;
	}

	tkey_i2c->tsk_enable_glove_mode = data;
	touchkey_mode_change(tkey_i2c, (data == 1) ? CMD_GLOVE_ON : CMD_GLOVE_OFF);
	return size;
}
#endif

#ifdef TKEY_FLIP_MODE
static ssize_t flip_cover_mode_enable(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int flip_mode_on;

	sscanf(buf, "%d\n", &flip_mode_on);
	input_dbg(true, &tkey_i2c->client->dev, "%s %d\n", __func__, flip_mode_on);

	/* glove mode control */
	if (flip_mode_on) {
		touchkey_mode_change(tkey_i2c, CMD_FLIP_ON);
	} else {
		if (tkey_i2c->tsk_enable_glove_mode)
			touchkey_mode_change(tkey_i2c, CMD_GLOVE_ON);
		touchkey_mode_change(tkey_i2c, CMD_FLIP_OFF);
	}

	return size;
}
#endif

static ssize_t keyboard_cover_mode_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int keyboard_mode_on;
	int mode;

	sscanf(buf, "%d\n", &keyboard_mode_on);
	input_info(true, &tkey_i2c->client->dev, "%s %d\n", __func__, keyboard_mode_on);

	tkey_i2c->write_without_cal = false;

	mode = touchkey_mode_change(tkey_i2c, CMD_GET_LAST_MODE);
	if (mode == MODE_FLIP && keyboard_mode_on == 0) {
		input_err(true, &tkey_i2c->client->dev,
				"%s : pass glove(KBD) off by flip on\n", __func__);
		return size;
	}

	tkey_i2c->tsk_enable_glove_mode = keyboard_mode_on;
	touchkey_mode_change(tkey_i2c, keyboard_mode_on ? CMD_GLOVE_ON : CMD_GLOVE_OFF);

	return size;
}

static ssize_t touch_sensitivity_control(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int ret, enable;
	u8 data[4] = { 0, };

	sscanf(buf, "%d\n", &enable);
	input_info(true, &tkey_i2c->client->dev, "%s %d\n", __func__, enable);

	data[1] = TK_BIT_CMD_INSPECTION;
	if (enable)
		data[2] = TK_BIT_WRITE_CONFIRM;
	else
		data[2] = TK_BIT_EXIT_CONFIRM;

	tkey_i2c->status_update = enable;

	ret = i2c_touchkey_write(tkey_i2c->client, data, 3);
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev, "%s, err(%d)\n", __func__, ret);
		tkey_i2c->status_update = false;
		return ret;
	}

	if (!enable) {
		/* set to previous mode */
		if (tkey_i2c->tsk_enable_glove_mode)
			touchkey_mode_change(tkey_i2c, CMD_GLOVE_ON);
		touchkey_read_status(tkey_i2c);
		touchkey_mode_change(tkey_i2c, CMD_MODE_RESERVED);
	}

	ret = i2c_smbus_read_i2c_block_data(tkey_i2c->client,
			BASE_REG, 4, data);
	if (ret < 0) {
		input_info(true, &tkey_i2c->client->dev,
				"%s: fail to CYPRESS_DATA_UPDATE.\n", __func__);
		return ret;
	}
	if ((data[1] & 0x20))
		input_info(true, &tkey_i2c->client->dev,
				"%s: Control Enabled!!\n", __func__);
	else
		input_info(true, &tkey_i2c->client->dev,
				"%s: Control Disabled!!\n", __func__);

	return size;
}

static ssize_t set_touchkey_firm_version_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	input_dbg(true, &tkey_i2c->client->dev,
			"firm_ver_bin %0#4x\n", tkey_i2c->src_fw_ver);
	return sprintf(buf, "%0#4x\n", tkey_i2c->src_fw_ver);
}

static ssize_t set_touchkey_update_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	u8 fw_path;

	switch(*buf) {
	case 's':
	case 'S':
		fw_path = FW_BUILT_IN;
		break;
	case 'i':
	case 'I':
		fw_path = FW_IN_SDCARD;
		break;
	case 'f':
	case 'F':
		fw_path = FW_FFU;
		break;
	default:
		return size;
	}

	touchkey_fw_update(tkey_i2c, fw_path, true);

#ifdef TK_USE_FWUPDATE_DWORK
	cancel_work_sync(&tkey_i2c->update_work);
#endif

	return size;
}

static ssize_t set_touchkey_firm_version_read_show(struct device *dev,
		struct device_attribute
		*attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	char data[3] = { 0, };
	int count;

	i2c_touchkey_read(tkey_i2c->client, TK_FW_VER, data, 2);
	count = sprintf(buf, "0x%02x\n", data[0]);

	input_info(true, &tkey_i2c->client->dev, "Touch_version_read 0x%02x\n", data[0]);
	input_info(true, &tkey_i2c->client->dev, "Module_version_read 0x%02x\n", data[1]);
	return count;
}

static ssize_t set_touchkey_firm_status_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int count = 0;

	input_info(true, &tkey_i2c->client->dev, "Touch_update_read: update_status %d\n",
			tkey_i2c->update_status);

	if (tkey_i2c->update_status == TK_UPDATE_PASS)
		count = sprintf(buf, "PASS\n");
	else if (tkey_i2c->update_status == TK_UPDATE_DOWN)
		count = sprintf(buf, "Downloading\n");
	else if (tkey_i2c->update_status == TK_UPDATE_FAIL)
		count = sprintf(buf, "Fail\n");

	return count;
}

#ifdef CONFIG_TOUCHKEY_LIGHT_EFS
static ssize_t touchkey_light_version_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int count;
	int crc_cal, crc_efs;

	if (efs_read_light_table_version(tkey_i2c) < 0) {
		count = sprintf(buf, "NG");
		goto out;
	} else {
		if (tkey_i2c->light_version_efs == LIGHT_VERSION) {
			if (!check_light_table_crc(tkey_i2c)) {
				count = sprintf(buf, "NG_CS");
				goto out;
			}
		} else {
			crc_cal = efs_calculate_crc(tkey_i2c);
			crc_efs = efs_read_crc(tkey_i2c);
			input_info(true, &tkey_i2c->client->dev,
					"CRC compare: efs[%d], bin[%d]\n",
					crc_cal, crc_efs);
			if (crc_cal != crc_efs) {
				count = sprintf(buf, "NG_CS");
				goto out;
			}
		}
	}

	count = sprintf(buf, "%s,%s",
			tkey_i2c->light_version_full_efs,
			tkey_i2c->light_version_full_bin);
out:
	input_info(true, &tkey_i2c->client->dev, "%s: %s\n", __func__, buf);
	return count;
}

static ssize_t touchkey_light_update(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int ret;
	int vol_mv;
	int window_type = read_window_type();

	ret = efs_write(tkey_i2c);
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev, "%s: fail %d\n", __func__, ret);
		return -EIO;
	}

	vol_mv = efs_read_light_table_with_default(tkey_i2c, window_type);
	if (vol_mv >= LIGHT_VOLTAGE_MIN_VAL) {
		change_touch_key_led_voltage(&tkey_i2c->client->dev, vol_mv);
		input_info(true, &tkey_i2c->client->dev,
				"%s: read done for %d\n", __func__, window_type);
	} else {
		input_err(true, &tkey_i2c->client->dev,
				"%s: fail. voltage is %d\n", __func__, vol_mv);
	}

	return size;
}

static ssize_t touchkey_light_id_compare(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int count, ret;
	int window_type = read_window_type();

	if (window_type < 0) {
		input_err(true, &tkey_i2c->client->dev,
				"%s: window_type:%d, NG\n", __func__, window_type);
		return sprintf(buf, "NG");
	}

	ret = efs_read_light_table(tkey_i2c, window_type);
	if (ret < 0) {
		count = sprintf(buf, "NG");
	} else {
		count = sprintf(buf, "OK");
	}

	input_info(true, &tkey_i2c->client->dev,
			"%s: window_type:%d, %s\n", __func__, window_type, buf);
	return count;
}

static char* tokenizer(char* str, char delim)
{
	static char* str_addr;
	char* token = NULL;

	if (str != NULL)
		str_addr = str;
	else if (str_addr == NULL)
		return 0;

	token = str_addr;
	while (true) {
		if (!(*str_addr)) {
			break;
		} else if (*str_addr == delim) {
			*str_addr = '\0';
			str_addr = str_addr + 1;
			break;
		}
		str_addr++;
	}

	return token;
}

static ssize_t touchkey_light_table_write(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	struct light_info table[16];
	int ret;
	int vol_mv;
	int window_type;
	char *full_version;
	char data[150] = {0, };
	int i, crc, crc_cal;
	char *octa_id;
	int table_size = 0;

	snprintf(data, sizeof(data), buf);

	input_info(true, &tkey_i2c->client->dev, "%s: %s\n",
			__func__, data);

	full_version = tokenizer(data, ',');
	if (!full_version)
		return -EIO;

	table_size = (int)strlen(full_version) - 8;
	input_info(true, &tkey_i2c->client->dev, "%s: version:%s size:%d\n",
			__func__, full_version, table_size);
	if (table_size < 0 || table_size > 16) {
		input_err(true, &tkey_i2c->client->dev, "%s: table_size is unavailable\n", __func__);
		return -EIO;
	}

	if (kstrtoint(tokenizer(NULL, ','), 0, &crc))
		return -EIO;

	input_info(true, &tkey_i2c->client->dev, "%s: crc:%d\n",
			__func__, crc);
	if (!crc)
		return -EIO;

	for (i = 0; i < table_size; i++) {
		octa_id = tokenizer(NULL, '_');
		if (!octa_id)
			break;

		if (octa_id[0] >= 'A')
			table[i].octa_id = octa_id[0] - 'A' + 0x0A;
		else
			table[i].octa_id = octa_id[0] - '0';
		if (table[i].octa_id < 0 || table[i].octa_id > 0x0F)
			break;
		if (kstrtoint(tokenizer(NULL, ','), 0, &table[i].vol_mv))
			break;
	}

	if (!table_size) {
		input_err(true, &tkey_i2c->client->dev, "%s: no data in table\n", __func__);
		return -ENODATA;
	}

	for (i = 0; i < table_size; i++) {
		input_info(true, &tkey_i2c->client->dev, "%s: [%d] %X - %dmv\n",
				__func__, i, table[i].octa_id, table[i].vol_mv);
	}

	/* write efs */
	ret = efs_write_light_table_version(tkey_i2c, full_version);
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev,
				"%s: failed to write table ver %s. %d\n",
				__func__, full_version, ret);
		return ret;
	}

	tkey_i2c->light_version_efs = pick_light_table_version(full_version);

	for (i = 0; i < table_size; i++) {
		ret = efs_write_light_table(tkey_i2c, table[i]);
		if (ret < 0)
			break;
	}
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev,
				"%s: failed to write table%d. %d\n",
				__func__, i, ret);
		return ret;
	}

	ret = efs_write_light_table_crc(tkey_i2c, crc);
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev,
				"%s: failed to write table crc. %d\n",
				__func__, ret);
		return ret;
	}

	crc_cal = efs_calculate_crc(tkey_i2c);
	input_info(true, &tkey_i2c->client->dev,
			"%s: efs crc:%d, caldulated crc:%d\n",
			__func__, crc, crc_cal);
	if (crc_cal != crc)
		return -EIO;

	window_type = read_window_type();
	vol_mv = efs_read_light_table_with_default(tkey_i2c, window_type);
	if (vol_mv >= LIGHT_VOLTAGE_MIN_VAL) {
		change_touch_key_led_voltage(&tkey_i2c->client->dev, vol_mv);
		input_info(true, &tkey_i2c->client->dev,
				"%s: read done for %d\n", __func__, window_type);
	} else {
		input_err(true, &tkey_i2c->client->dev,
				"%s: fail. voltage is %d\n", __func__, vol_mv);
	}

	return size;
}
#endif

#ifdef CONFIG_KEYBOARD_THD_CHANGE_AS_FINGERPRINT
static ssize_t touchkey_fingerprint_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int enable, ret;

	if (!tkey_i2c)
		return -ENXIO;

	sscanf(buf, "%d\n", &enable);

	if (enable != 0 && enable != 1) {
		input_err(true, &tkey_i2c->client->dev,
			"%s: abnormal param %d\n", __func__, enable);
		return -EIO;
	}

	ret = change_touchkey_thd_for_fingerprint(enable);
	if (ret < 0)
		return ret;

	return size;
}
#endif

static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		touchkey_led_control);
static DEVICE_ATTR(touch_sensitivity, S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, touch_sensitivity_control);
static DEVICE_ATTR(touchkey_firm_update, S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, set_touchkey_update_store);
static DEVICE_ATTR(touchkey_firm_update_status, S_IRUGO | S_IWUSR | S_IWGRP,
		set_touchkey_firm_status_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO | S_IWUSR | S_IWGRP,
		set_touchkey_firm_version_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO | S_IWUSR | S_IWGRP,
		set_touchkey_firm_version_read_show, NULL);
static DEVICE_ATTR(touchkey_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, brightness_control);
static DEVICE_ATTR(touchkey_recent, S_IRUGO, touchkey_diff_data0_show, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, touchkey_diff_data1_show, NULL);
static DEVICE_ATTR(touchkey_recent_raw, S_IRUGO, touchkey_raw_data0_show, NULL);
static DEVICE_ATTR(touchkey_back_raw, S_IRUGO, touchkey_raw_data1_show, NULL);
static DEVICE_ATTR(touchkey_recent_idac, S_IRUGO, touchkey_idac0_show, NULL);
static DEVICE_ATTR(touchkey_back_idac, S_IRUGO, touchkey_idac1_show, NULL);
static DEVICE_ATTR(touchkey_threshold, S_IRUGO, touchkey_threshold0_show, NULL);
static DEVICE_ATTR(touchkey_get_chip_vendor, S_IRUGO, get_chip_vendor, NULL);
#ifdef CONFIG_GLOVE_TOUCH
static DEVICE_ATTR(glove_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		glove_mode_enable);
#endif
#ifdef TKEY_FLIP_MODE
static DEVICE_ATTR(flip_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		flip_cover_mode_enable);
#endif
static DEVICE_ATTR(keyboard_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		keyboard_cover_mode_enable);
#ifdef CONFIG_TOUCHKEY_LIGHT_EFS
static DEVICE_ATTR(touchkey_light_version, S_IRUGO, touchkey_light_version_read, NULL);
static DEVICE_ATTR(touchkey_light_update, S_IWUSR | S_IWGRP, NULL, touchkey_light_update);
static DEVICE_ATTR(touchkey_light_id_compare, S_IRUGO, touchkey_light_id_compare, NULL);
static DEVICE_ATTR(touchkey_light_table_write, S_IWUSR | S_IWGRP, NULL, touchkey_light_table_write);
#endif
#ifdef CONFIG_KEYBOARD_THD_CHANGE_AS_FINGERPRINT
static DEVICE_ATTR(touchkey_fingerprint_mode, S_IWUSR | S_IWGRP, NULL, touchkey_fingerprint_mode);
#endif
static struct attribute *touchkey_attributes[] = {
	&dev_attr_brightness.attr,
	&dev_attr_touch_sensitivity.attr,
	&dev_attr_touchkey_firm_update.attr,
	&dev_attr_touchkey_firm_update_status.attr,
	&dev_attr_touchkey_firm_version_phone.attr,
	&dev_attr_touchkey_firm_version_panel.attr,
	&dev_attr_touchkey_brightness.attr,
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_recent_raw.attr,
	&dev_attr_touchkey_back_raw.attr,
	&dev_attr_touchkey_recent_idac.attr,
	&dev_attr_touchkey_back_idac.attr,
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_touchkey_get_chip_vendor.attr,
#ifdef CONFIG_GLOVE_TOUCH
	&dev_attr_glove_mode.attr,
#endif
#ifdef TKEY_FLIP_MODE
	&dev_attr_flip_mode.attr,
#endif
	&dev_attr_keyboard_mode.attr,
#ifdef CONFIG_TOUCHKEY_LIGHT_EFS	
	&dev_attr_touchkey_light_version.attr,
	&dev_attr_touchkey_light_update.attr,
	&dev_attr_touchkey_light_id_compare.attr,
	&dev_attr_touchkey_light_table_write.attr,
#endif
#ifdef CONFIG_KEYBOARD_THD_CHANGE_AS_FINGERPRINT
	&dev_attr_touchkey_fingerprint_mode.attr,
#endif
	NULL,
};

static struct attribute_group touchkey_attr_group = {
	.attrs = touchkey_attributes,
};

static int touchkey_power(void *data, bool on)
{
	struct touchkey_i2c *tkey_i2c = (struct touchkey_i2c *)data;
	struct device *dev = &tkey_i2c->client->dev;
	struct regulator *regulator_core = NULL;
	struct regulator *regulator_io = NULL;
	static bool enabled;
	int ret = 0;
	int state = on ? I_STATE_ON_IRQ : I_STATE_OFF_IRQ;

	if (enabled == on) {
		input_err(true, dev,
				"%s : TK power already %s\n", __func__,(on)?"on":"off");
		return ret;
	}

	regulator_core = regulator_get(NULL, tkey_i2c->dtdata->regulator_avdd);
	if (IS_ERR_OR_NULL(regulator_core)) {
		input_err(true, dev,
				"%s : %s regulator_get failed\n", __func__, tkey_i2c->dtdata->regulator_avdd);
		ret = -EIO;
		goto out;
	}

	if (on) {
		ret = regulator_enable(regulator_core);
		if (ret) {
			input_err(true, dev,
					"%s: Failed to enable avdd: %d\n", __func__, ret);
			goto out;
		}
	} else {
		ret = regulator_disable(regulator_core);
		if (ret) {
			input_err(true, dev,
					"%s: Failed to disable avdd: %d\n", __func__, ret);
			goto out;
		}
	}
	input_info(true, dev, "%s %s: %s %d\n", __func__, on ? "on" : "off",
			TK_CORE_REGULATOR_NAME, regulator_is_enabled(regulator_core));

	if (!tkey_i2c->dtdata->ap_io_power) {
		regulator_io = regulator_get(NULL, TK_IO_REGULATOR_NAME);
		if (IS_ERR_OR_NULL(regulator_io)) {
			input_err(true, dev,
					"%s : %s regulator_get failed\n", __func__, TK_IO_REGULATOR_NAME);
			ret = -EIO;
			goto out;
		}
		if (on) {
			ret = regulator_enable(regulator_io);
			if (ret) {
				input_err(true, dev,
						"%s: Failed to enable dvdd: %d\n", __func__, ret);
				goto out;
			}
		} else {
			ret = regulator_disable(regulator_io);
			if (ret) {
				input_err(true, dev,
						"%s: Failed to disable dvdd: %d\n", __func__, ret);
				goto out;
			}
		}
		input_info(true, dev, "%s %s: %s %d\n", __func__, on ? "on" : "off",
				TK_IO_REGULATOR_NAME, regulator_is_enabled(regulator_io));
	}

	ret = touchkey_pinctrl(tkey_i2c, state);
	if (ret < 0)
		input_err(true, dev,
				"%s: Failed to configure irq pin\n", __func__);

	enabled = on;
out:
	regulator_put(regulator_core);
	if (!tkey_i2c->dtdata->ap_io_power)
		regulator_put(regulator_io);

	return ret;
}

#ifdef CONFIG_OF
static void cypress_request_gpio(struct i2c_client *client, struct touchkey_devicetree_data *dtdata)
{
	int ret;

	if (!dtdata->i2c_gpio) {
		ret = gpio_request(dtdata->gpio_scl, "touchkey_scl");
		if (ret) {
			input_err(true, &client->dev, "%s: unable to request touchkey_scl [%d]\n",
					__func__, dtdata->gpio_scl);
		}

		ret = gpio_request(dtdata->gpio_sda, "touchkey_sda");
		if (ret) {
			input_err(true, &client->dev, "%s: unable to request touchkey_sda [%d]\n",
					__func__, dtdata->gpio_sda);
		}
	}

	ret = gpio_request(dtdata->gpio_int, "touchkey_irq");
	if (ret) {
		input_err(true, &client->dev, "%s: unable to request touchkey_irq [%d]\n",
				__func__, dtdata->gpio_int);
	}
}

static struct touchkey_devicetree_data *cypress_parse_dt(struct i2c_client *client)
{

	struct device *dev = &client->dev;
	struct touchkey_devicetree_data *dtdata;
	struct device_node *np = dev->of_node;
	int ret;

	if (!np)
		return ERR_PTR(-ENOENT);

	dtdata = devm_kzalloc(dev, sizeof(*dtdata), GFP_KERNEL);
	if (!dtdata) {
		input_err(true, dev, "failed to allocate platform data\n");
		return ERR_PTR(-ENOMEM);
	}
	dev->platform_data = dtdata;

	/* reset, irq gpio info */
	dtdata->gpio_scl = of_get_named_gpio_flags(np, "cypress,scl-gpio",
			0, &dtdata->scl_gpio_flags);
	dtdata->gpio_sda = of_get_named_gpio_flags(np, "cypress,sda-gpio",
			0, &dtdata->sda_gpio_flags);
	dtdata->gpio_int = of_get_named_gpio_flags(np, "cypress,irq-gpio",
			0, &dtdata->irq_gpio_flags);
	dtdata->sub_det = of_get_named_gpio_flags(np, "cypress,sub-det",
			0, &dtdata->sub_det_flags);

	dtdata->i2c_gpio = of_property_read_bool(np, "cypress,i2c-gpio");
	dtdata->boot_on_ldo = of_property_read_bool(np, "cypress,boot-on-ldo");
	dtdata->ap_io_power = of_property_read_bool(np, "cypress,ap-io-power");

	ret = of_property_read_u32(np, "cypress,ic-stabilizing-time", &dtdata->stabilizing_time);
	if (ret) {
		input_err(true, &client->dev, "failed to ic-stabilizing-time %d\n", ret);
		dtdata->stabilizing_time = 150;
	}

	if (of_property_read_string(np, "cypress,regulator_avdd", &dtdata->regulator_avdd)) {
		input_err(true, &client->dev, "Failed to get regulator_avdd name property\n");
	}

	ret = of_property_read_string(np, "cypress,fw_path", (const char **)&dtdata->fw_path);
	if (ret) {
		input_err(true, &client->dev, "failed to read fw_path %d\n", ret);
	}

	input_info(true, &client->dev, "%s: scl=%d, sda=%d, int=%d, sub_det=%d, i2c_gpio=%d,"
			"boot_on_ldo=%d, stabilizing_time=%d, ap_io_power=%d, fw path=%s\n",
			__func__, dtdata->gpio_scl, dtdata->gpio_sda, dtdata->gpio_int,
			dtdata->sub_det, dtdata->i2c_gpio, dtdata->boot_on_ldo,
			dtdata->stabilizing_time, dtdata->ap_io_power, dtdata->fw_path);

	cypress_request_gpio(client, dtdata);

	return dtdata;
}
#endif

static int i2c_touchkey_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct touchkey_devicetree_data *dtdata = client->dev.platform_data;
	struct touchkey_i2c *tkey_i2c;
	bool bforced = false;
	struct input_dev *input_dev;
	int i;
	int ret = 0;
#ifdef CONFIG_TOUCHKEY_LIGHT_EFS
	char tmp[2] = {0, };
#endif

	input_info(true, &client->dev, "%s: start!\n", __func__);

	ret = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (!ret) {
		input_err(true, &client->dev, "No I2C functionality found\n");
		return -ENODEV;
	}

	tkey_i2c = kzalloc(sizeof(struct touchkey_i2c), GFP_KERNEL);
	if (tkey_i2c == NULL) {
		input_err(true, &client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	if (IS_ERR_OR_NULL(dtdata)) {
		dtdata = cypress_parse_dt(client);
		if (!dtdata) {
			input_err(true, &client->dev, "%s: no dtdata\n", __func__);
			ret = -EINVAL;
			goto err_allocate_input_device;
		}
	}

	if (gpio_is_valid(dtdata->sub_det)) {
		ret = gpio_get_value(dtdata->sub_det);
		if (!ret) {
			input_err(true, &client->dev, "Abov touchkey\n");
			ret = -ENODEV;
			goto err_allocate_input_device;
		}
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		input_err(true, &client->dev, "Failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_allocate_input_device;
	}

	client->irq = gpio_to_irq(dtdata->gpio_int);

	input_dev->name = "sec_touchkey";
	input_dev->phys = "sec_touchkey/input0";
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = &client->dev;
	input_dev->open = touchkey_input_open;
	input_dev->close = touchkey_input_close;

	tkey_i2c->dtdata = dtdata;
	tkey_i2c->input_dev = input_dev;
	tkey_i2c->client = client;
	tkey_i2c->irq = client->irq;
	tkey_i2c->name = "sec_touchkey";
	tkey_i2c->status_update = false;
	tkey_i2c->power = touchkey_power;
	tkey_i2c->mc_data.cur_mode = MODE_NORMAL;

	mutex_init(&tkey_i2c->lock);
	mutex_init(&tkey_i2c->i2c_lock);
	mutex_init(&tkey_i2c->mc_data.mc_lock);
	mutex_init(&tkey_i2c->mc_data.mcf_lock);
#ifdef TK_USE_OPEN_DWORK
	INIT_DELAYED_WORK(&tkey_i2c->open_work, touchkey_open_work);
#endif
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_LED, input_dev->evbit);
	set_bit(LED_MISC, input_dev->ledbit);
	set_bit(EV_KEY, input_dev->evbit);
#ifdef CONFIG_VT_TKEY_SKIP_MATCH
	set_bit(EV_TOUCHKEY, input_dev->evbit);
#endif
#ifdef TK_USE_FWUPDATE_DWORK
	INIT_WORK(&tkey_i2c->update_work, touchkey_i2c_update_work);
#endif
	INIT_WORK(&tkey_i2c->mode_change_work, touchkey_i2c_mode_change_work);
	wake_lock_init(&tkey_i2c->fw_wakelock, WAKE_LOCK_SUSPEND, "touchkey");

#ifdef CONFIG_TOUCHKEY_LIGHT_EFS
	INIT_DELAYED_WORK(&tkey_i2c->efs_open_work, touchkey_efs_open_work);
#endif
	for (i = 1; i < touchkey_count; i++)
		set_bit(touchkey_keycode[i], input_dev->keybit);

	input_set_drvdata(input_dev, tkey_i2c);
	i2c_set_clientdata(client, tkey_i2c);

	ret = input_register_device(input_dev);
	if (ret) {
		input_err(true, &client->dev, "Failed to register input device\n");
		goto err_register_device;
	}

	ret = touchkey_pinctrl_init(tkey_i2c);
	if (ret < 0) {
		input_err(true, &tkey_i2c->client->dev,
				"%s: Failed to init pinctrl: %d\n", __func__, ret);
		goto err_register_device;
	}

	tkey_i2c->power(tkey_i2c, true);

	if (!tkey_i2c->dtdata->boot_on_ldo)
		msleep(dtdata->stabilizing_time);

	tkey_i2c->enabled = true;

#ifdef TK_USE_FWUPDATE_DWORK
	tkey_i2c->fw_wq = create_singlethread_workqueue(client->name);
	if (!tkey_i2c->fw_wq) {
		input_err(true, &client->dev, "fail to create workqueue for fw_wq\n");
		ret = -ENOMEM;
		goto err_create_fw_wq;
	}
#endif

	cypress_config_gpio_i2c(tkey_i2c, 1);

	ret = touchkey_i2c_check(tkey_i2c);
	if (ret < 0) {
		input_err(true, &client->dev, "i2c_check failed\n");
		bforced = true;
	}

	ret = request_threaded_irq(tkey_i2c->irq, NULL, touchkey_interrupt,
			IRQF_DISABLED | IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT, tkey_i2c->name, tkey_i2c);
	if (ret < 0) {
		input_err(true, &client->dev, "Failed to request irq(%d) - %d\n",
				tkey_i2c->irq, ret);
		goto err_request_threaded_irq;
	}

#ifdef TK_HAS_FIRMWARE_UPDATE
	ret = touchkey_fw_update(tkey_i2c, FW_BUILT_IN, bforced);
	if (ret < 0) {
		input_err(true, &client->dev, "fw update fail\n");
		goto err_firmware_update;
	}
#endif

#ifdef TK_INFORM_CHARGER
	tkey_i2c->callbacks.inform_charger = touchkey_ta_cb;
	if (tkey_i2c->register_cb) {
		input_info(true, &client->dev, "Touchkey TA information\n");
		tkey_i2c->register_cb(&tkey_i2c->callbacks);
	}
#endif

	tkey_i2c->dev = sec_device_create(tkey_i2c, "sec_touchkey");

	ret = IS_ERR(tkey_i2c->dev);
	if (ret) {
		input_err(true, &client->dev, "Failed to create device(tkey_i2c->dev)!\n");
	} else {
		ret = sysfs_create_group(&tkey_i2c->dev->kobj,
				&touchkey_attr_group);
		if (ret)
			input_err(true, &client->dev, "Failed to create sysfs group\n");

		ret = sysfs_create_link(&tkey_i2c->dev->kobj,
				&tkey_i2c->input_dev->dev.kobj, "input");
		if (ret)
			input_err(true, &client->dev, "Failed to connect link\n");
	}

#ifdef CONFIG_TOUCHKEY_LIGHT_EFS
	tkey_i2c->light_table_crc = LIGHT_VERSION;
	sprintf(tkey_i2c->light_version_full_bin, "T%d.", LIGHT_VERSION);
	for (i = 0; i < LIGHT_TABLE_MAX; i++) {
		tkey_i2c->light_table_crc += tkey_light_voltage_table[i].octa_id;
		tkey_i2c->light_table_crc += tkey_light_voltage_table[i].vol_mv;
		snprintf(tmp, 2, "%X", tkey_light_voltage_table[i].octa_id);
		strncat(tkey_i2c->light_version_full_bin, tmp, 1);
	}
	input_info(true, &client->dev, "%s: light version of kernel : %s\n",
			__func__, tkey_i2c->light_version_full_bin);

	schedule_delayed_work(&tkey_i2c->efs_open_work, msecs_to_jiffies(2000));
#endif
#ifdef CONFIG_KEYBOARD_THD_CHANGE_AS_FINGERPRINT
	tkey_info = tkey_i2c;
#endif

	touchkey_probe = true;
	input_info(true, &client->dev, "%s: done\n", __func__);

	return 0;

#ifdef TK_HAS_FIRMWARE_UPDATE
err_firmware_update:
	disable_irq(tkey_i2c->irq);
	free_irq(tkey_i2c->irq, tkey_i2c);
#endif
err_request_threaded_irq:
#ifdef TK_USE_FWUPDATE_DWORK
	destroy_workqueue(tkey_i2c->fw_wq);
err_create_fw_wq:
#endif
	tkey_i2c->power(tkey_i2c, false);
	input_unregister_device(input_dev);
	input_dev = NULL;
err_register_device:
	wake_lock_destroy(&tkey_i2c->fw_wakelock);
	mutex_destroy(&tkey_i2c->mc_data.mcf_lock);
	mutex_destroy(&tkey_i2c->mc_data.mc_lock);
	mutex_destroy(&tkey_i2c->i2c_lock);
	mutex_destroy(&tkey_i2c->lock);
	input_free_device(input_dev);
err_allocate_input_device:
	kfree(tkey_i2c);
	return ret;
}

void touchkey_shutdown(struct i2c_client *client)
{
	struct touchkey_i2c *tkey_i2c = i2c_get_clientdata(client);

	if (!touchkey_probe)
		return;

	input_err(true, &tkey_i2c->client->dev, "%s\n", __func__);
#ifdef CONFIG_TOUCHKEY_LIGHT_EFS
	cancel_delayed_work(&tkey_i2c->efs_open_work);
#endif
	touchkey_stop(tkey_i2c);
	free_irq(tkey_i2c->irq, tkey_i2c);

	input_unregister_device(tkey_i2c->input_dev);
	tkey_i2c->input_dev = NULL;
	wake_lock_destroy(&tkey_i2c->fw_wakelock);
	mutex_destroy(&tkey_i2c->mc_data.mcf_lock);
	mutex_destroy(&tkey_i2c->mc_data.mc_lock);
	mutex_destroy(&tkey_i2c->i2c_lock);
	mutex_destroy(&tkey_i2c->lock);
	kfree(tkey_i2c);
#ifdef CONFIG_KEYBOARD_THD_CHANGE_AS_FINGERPRINT
	tkey_info = NULL;
#endif
}


static const struct i2c_device_id sec_touchkey_id[] = {
	{"sec_touchkey", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, sec_touchkey_id);

#ifdef CONFIG_OF
static struct of_device_id cypress_touchkey_dt_ids[] = {
	{ .compatible = "cypress,cypress_touchkey" },
	{ }
};
#endif

struct i2c_driver touchkey_i2c_driver = {
	.driver = {
		.name = "sec_touchkey_driver",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(cypress_touchkey_dt_ids),
#endif

	},
	.id_table = sec_touchkey_id,
	.probe = i2c_touchkey_probe,
	.shutdown = &touchkey_shutdown,
};

static int __init touchkey_init(void)
{
#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		pr_err("%s %s: LPM Charging Mode!!\n", SECLOG, __func__);
		return 0;
	}
#endif
	if (!lcdtype) {
		pr_err("%s %s: not connected!!\n", SECLOG, __func__);
		return 0;
	}

	return i2c_add_driver(&touchkey_i2c_driver);
}

static void __exit touchkey_exit(void)
{
	i2c_del_driver(&touchkey_i2c_driver);
	touchkey_probe = false;
}

module_init(touchkey_init);
module_exit(touchkey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("@@@");
MODULE_DESCRIPTION("Touchkey driver for Cypress touchkey controller ");
