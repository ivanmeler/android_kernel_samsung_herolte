#ifdef SEC_TSP_FACTORY_TEST


#define TSP_FACTEST_RESULT_PASS		2
#define TSP_FACTEST_RESULT_FAIL		1
#define TSP_FACTEST_RESULT_NONE		0

#define BUFFER_MAX					(256 * 1024) - 16
#define READ_CHUNK_SIZE				128 // (2 * 1024) - 16

enum {
	TYPE_RAW_DATA = 0,
	TYPE_FILTERED_DATA = 2,
	TYPE_STRENGTH_DATA = 4,
	TYPE_BASELINE_DATA = 6
};

enum {
	BUILT_IN = 0,
	UMS,
};

enum CMD_STATUS {
	CMD_STATUS_WAITING = 0,
	CMD_STATUS_RUNNING,
	CMD_STATUS_OK,
	CMD_STATUS_FAIL,
	CMD_STATUS_NOT_APPLICABLE,
};

#ifdef FTS_SUPPORT_TOUCH_KEY
enum {
	TYPE_TOUCHKEY_RAW			= 0x34,
	TYPE_TOUCHKEY_STRENGTH	= 0x36,
	TYPE_TOUCHKEY_THRESHOLD	= 0x48,
};
#endif

static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_config_ver(void *device_data);
static void get_threshold(void *device_data);
static void module_off_master(void *device_data);
static void module_on_master(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_mis_cal_info(void *device_data);
static void get_wet_mode(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void get_checksum_data(void *device_data);
static void run_reference_read(void *device_data);
static void get_reference(void *device_data);
static void run_rawcap_read(void *device_data);
static void get_rawcap(void *device_data);
static void run_delta_read(void *device_data);
static void get_delta(void *device_data);
#ifdef FTS_SUPPORT_HOVER
static void run_abscap_read(void *device_data);
static void run_absdelta_read(void *device_data);
#endif
static void run_ix_data_read(void *device_data);
static void run_ix_data_read_all(void *device_data);
static void run_self_raw_read(void *device_data);
static void run_self_raw_read_all(void *device_data);
static void run_wtr_cx_data_read(void *device_data);
static void run_wtr_cx_data_read_all(void *device_data);
static void run_trx_short_test(void *device_data);
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
static void set_status_pureautotune(void *device_data);
static void get_status_pureautotune(void *device_data);
static void get_status_afe(void *device_data);
#endif
static void get_cx_data(void *device_data);
static void run_cx_data_read(void *device_data);
static void get_cx_all_data(void *device_data);
static void get_strength_all_data(void *device_data);
#ifdef FTS_SUPPORT_TOUCH_KEY
static void run_key_cx_data_read(void *device_data);
#endif
static void set_tsp_test_result(void *device_data);
static void get_tsp_test_result(void *device_data);
#ifdef FTS_SUPPORT_HOVER
static void hover_enable(void *device_data);
/* static void hover_no_sleep_enable(void *device_data); */
#endif
#ifdef CONFIG_GLOVE_TOUCH
static void glove_mode(void *device_data);
static void get_glove_sensitivity(void *device_data);
static void fast_glove_mode(void *device_data);
#endif
static void clear_cover_mode(void *device_data);
static void report_rate(void *device_data);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static void interrupt_control(void *device_data);
#endif

#if defined(CONFIG_INPUT_BOOSTER) || defined(TOUCH_BOOSTER_DVFS)
static void boost_level(void *device_data);
#endif
static void set_lpgw_mode(void *device_data);
static void set_lowpower_mode(void *device_data);
static void set_deepsleep_mode(void *device_data);
static void set_wirelesscharger_mode(void *device_data);
static void active_sleep_enable(void *device_data);
static void second_screen_enable(void *device_data);
static void set_longpress_enable(void *device_data);
static void set_sidescreen_x_length(void *device_data);
static void set_tunning_data(void *device_data);
static void set_dead_zone(void *device_data);
static void dead_zone_enable(void *device_data);
static void select_wakeful_edge(void *device_data);

#ifdef SMARTCOVER_COVER
static void smartcover_cmd(void *device_data);
#endif
#ifdef FTS_SUPPORT_STRINGLIB
static void scrub_enable(void *device_data);
static void quick_app_access_enable(void *device_data);
static void direct_indicator_enable(void *device_data);
static void spay_enable(void *device_data);
static void edge_swipe_enable(void *device_data);
static void aod_enable(void *device_data);
#endif
static void delay(void *device_data);
static void debug(void *device_data);
static void run_autotune_enable(void *device_data);
static void run_force_calibration(void *device_data);

static void set_mainscreen_disable(void *device_data);
static void set_rotation_status(void *device_data);

static void not_support_cmd(void *device_data);

static ssize_t store_cmd(struct device *dev, struct device_attribute *devattr,
			   const char *buf, size_t count);
static ssize_t show_cmd_status(struct device *dev,
				struct device_attribute *devattr, char *buf);
static ssize_t show_cmd_result(struct device *dev,
				struct device_attribute *devattr, char *buf);
static ssize_t cmd_list_show(struct device *dev,
				struct device_attribute *attr, char *buf);
static ssize_t fts_scrub_position(struct device *dev,
				struct device_attribute *attr, char *buf);
static ssize_t fts_edge_x_position(struct device *dev,
				struct device_attribute *attr, char *buf);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
static void tui_mode_cmd(struct fts_ts_info *info);
#endif

#ifdef FTS_SUPPROT_MULTIMEDIA
static void brush_enable(void *device_data);
static void velocity_enable(void *device_data);
#endif

static void lcd_orientation(void *device_data);

#define FT_CMD(name, func)	.cmd_name = name, .cmd_func = func
struct ft_cmd {
	struct list_head list;
	const char *cmd_name;
	void (*cmd_func) (void *device_data);
};
struct ft_cmd ft_commands[] = {
	{FT_CMD("fw_update", fw_update),},
	{FT_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{FT_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{FT_CMD("get_config_ver", get_config_ver),},
	{FT_CMD("get_threshold", get_threshold),},
	{FT_CMD("module_off_master", module_off_master),},
	{FT_CMD("module_on_master", module_on_master),},
	{FT_CMD("module_off_slave", not_support_cmd),},
	{FT_CMD("module_on_slave", not_support_cmd),},
	{FT_CMD("get_chip_vendor", get_chip_vendor),},
	{FT_CMD("get_chip_name", get_chip_name),},
	{FT_CMD("get_module_vendor", not_support_cmd),},
	{FT_CMD("get_mis_cal_info", get_mis_cal_info),},
	{FT_CMD("get_wet_mode", get_wet_mode),},
	{FT_CMD("get_x_num", get_x_num),},
	{FT_CMD("get_y_num", get_y_num),},
	{FT_CMD("get_checksum_data", get_checksum_data),},
	{FT_CMD("run_reference_read", run_reference_read),},
	{FT_CMD("get_reference", get_reference),},
	{FT_CMD("run_rawcap_read", run_rawcap_read),},
	{FT_CMD("get_rawcap", get_rawcap),},
	{FT_CMD("run_delta_read", run_delta_read),},
	{FT_CMD("get_delta", get_delta),},
#ifdef FTS_SUPPORT_HOVER
	{FT_CMD("run_abscap_read" , run_abscap_read),},
	{FT_CMD("run_absdelta_read", run_absdelta_read),},
#endif
	{FT_CMD("run_ix_data_read", run_ix_data_read),},
	{FT_CMD("run_ix_data_read_all", run_ix_data_read_all),},
	{FT_CMD("run_self_raw_read", run_self_raw_read),},
	{FT_CMD("run_self_raw_read_all", run_self_raw_read_all),},
	{FT_CMD("run_wtr_cx_data_read", run_wtr_cx_data_read),},
	{FT_CMD("run_wtr_cx_data_read_all", run_wtr_cx_data_read_all),},
	{FT_CMD("run_trx_short_test", run_trx_short_test),},
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
	{FT_CMD("set_status_pureautotune", set_status_pureautotune),},
	{FT_CMD("get_status_pureautotune", get_status_pureautotune),},
	{FT_CMD("get_status_afe", get_status_afe),},
#endif
	{FT_CMD("get_cx_data", get_cx_data),},
	{FT_CMD("run_cx_data_read", run_cx_data_read),},
	{FT_CMD("get_cx_all_data", get_cx_all_data),},
	{FT_CMD("get_strength_all_data", get_strength_all_data),},
#ifdef FTS_SUPPORT_TOUCH_KEY
	{FT_CMD("run_key_cx_data_read", run_key_cx_data_read),},
#endif
	{FT_CMD("set_tsp_test_result", set_tsp_test_result),},
	{FT_CMD("get_tsp_test_result", get_tsp_test_result),},
#ifdef FTS_SUPPORT_HOVER
	{FT_CMD("hover_enable", hover_enable),},
#endif
	{FT_CMD("hover_no_sleep_enable", not_support_cmd),},
#ifdef CONFIG_GLOVE_TOUCH
	{FT_CMD("glove_mode", glove_mode),},
	{FT_CMD("get_glove_sensitivity", get_glove_sensitivity),},
	{FT_CMD("fast_glove_mode", fast_glove_mode),},
#endif
	{FT_CMD("clear_cover_mode", clear_cover_mode),},
	{FT_CMD("report_rate", report_rate),},
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	{FT_CMD("interrupt_control", interrupt_control),},
#endif
#if defined(CONFIG_INPUT_BOOSTER)|| defined(TOUCH_BOOSTER_DVFS)
	{FT_CMD("boost_level", boost_level),},
#endif
	{FT_CMD("set_lpgw_mode", set_lpgw_mode),},
	{FT_CMD("set_lowpower_mode", set_lowpower_mode),},
	{FT_CMD("set_deepsleep_mode", set_deepsleep_mode),},
	{FT_CMD("set_wirelesscharger_mode", set_wirelesscharger_mode),},
	{FT_CMD("active_sleep_enable", active_sleep_enable),},
	{FT_CMD("second_screen_enable", second_screen_enable),},
	{FT_CMD("set_longpress_enable", set_longpress_enable),},
	{FT_CMD("set_sidescreen_x_length", set_sidescreen_x_length),},
	{FT_CMD("set_tunning_data", set_tunning_data),},
	{FT_CMD("set_dead_zone", set_dead_zone),},
	{FT_CMD("dead_zone_enable", dead_zone_enable),},
	{FT_CMD("select_wakeful_edge", select_wakeful_edge),},
#ifdef FTS_SUPPORT_STRINGLIB
	{FT_CMD("scrub_enable", scrub_enable),},
	{FT_CMD("quick_app_access_enable", quick_app_access_enable),},
	{FT_CMD("direct_indicator_enable", direct_indicator_enable),},
	{FT_CMD("spay_enable", spay_enable),},
	{FT_CMD("edge_swipe_enable", edge_swipe_enable),},
	{FT_CMD("aod_enable", aod_enable),},
#endif
#ifdef SMARTCOVER_COVER
	{FT_CMD("smartcover_cmd", smartcover_cmd),},
#endif
	{FT_CMD("delay", delay),},
	{FT_CMD("debug", debug),},
	{FT_CMD("run_autotune_enable", run_autotune_enable),},
	{FT_CMD("run_force_calibration", run_force_calibration),},
	{FT_CMD("set_mainscreen_disable", set_mainscreen_disable),},
	{FT_CMD("set_rotation_status", set_rotation_status),},
#ifdef FTS_SUPPROT_MULTIMEDIA
	{FT_CMD("brush_enable", brush_enable),},
	{FT_CMD("velocity_enable", velocity_enable),},
#endif
	{FT_CMD("lcd_orientation", lcd_orientation),},
	{FT_CMD("not_support_cmd", not_support_cmd),},
};

static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);
static DEVICE_ATTR(cmd_list, S_IRUGO, cmd_list_show, NULL);
static DEVICE_ATTR(scrub_pos, S_IRUGO, fts_scrub_position, NULL);
static DEVICE_ATTR(edge_pos, S_IRUGO, fts_edge_x_position, NULL);

static struct attribute *sec_touch_facotry_attributes[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	&dev_attr_cmd_list.attr,
	&dev_attr_scrub_pos.attr,
	&dev_attr_edge_pos.attr,
	NULL,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_facotry_attributes,
};

static int fts_check_index(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int node;
	if (info->cmd_param[0] < 0
	  || info->cmd_param[0] >= info->SenseChannelLength
	  || info->cmd_param[1] < 0
	  || info->cmd_param[1] >= info->ForceChannelLength) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		strncat(info->cmd_result, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_FAIL;
		tsp_debug_info(true, &info->client->dev, "%s: parameter error: %u,%u\n",
			   __func__, info->cmd_param[0], info->cmd_param[1]);
		node = -1;
		return node;
	}
	node = info->cmd_param[1] * info->SenseChannelLength + info->cmd_param[0];
	tsp_debug_info(true, &info->client->dev, "%s: node = %d\n", __func__, node);
	return node;
}

static ssize_t fts_scrub_position(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	char buff[CMD_STR_LEN] = { 0 };

	if (!info) {
		printk(KERN_ERR "%s: No platform data found\n",
				__func__);
		return -EINVAL;
	}

	if (!info->input_dev) {
		printk(KERN_ERR "%s: No input_dev data found\n",
				__func__);
		return -EINVAL;
	}

	tsp_debug_info(true, &info->client->dev, "%s: %d %d %d\n",
				__func__, info->scrub_id, info->scrub_x, info->scrub_y);
	snprintf(buff, sizeof(buff), "%d %d %d", info->scrub_id, info->scrub_x, info->scrub_y);

	info->scrub_id = 0;
	info->scrub_x = 0;
	info->scrub_y = 0;

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", buff);
}

static ssize_t fts_edge_x_position(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	char buff[CMD_STR_LEN] = { 0 };
	int edge_position_left, edge_position_right;

	if (!info) {
		printk(KERN_ERR "%s: No platform data found\n",
				__func__);
		return -EINVAL;
	}

	if (!info->input_dev) {
		printk(KERN_ERR "%s: No input_dev data found\n",
				__func__);
		return -EINVAL;
	}

	if ((lcdtype == S6E3HF2_WQXGA_ID1) || (lcdtype == S6E3HF2_WQXGA_ID2)) {
		edge_position_left = -1;
		edge_position_right = info->board->max_x + 1 - info->board->grip_area;
	} else {
		edge_position_left = info->board->grip_area;
		edge_position_right = info->board->max_x + 1 - info->board->grip_area;
	}

	tsp_debug_info(true, &info->client->dev, "%s: %d,%d\n", __func__, edge_position_left, edge_position_right);
	snprintf(buff, sizeof(buff), "%d,%d", edge_position_left, edge_position_right);

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", buff);
}

static void clear_cover_cmd_work(struct work_struct *work)
{
	struct fts_ts_info *info = container_of(work, struct fts_ts_info,
						cover_cmd_work.work);

	if (info->cmd_is_running) {
		schedule_delayed_work(&info->cover_cmd_work, msecs_to_jiffies(5));
	} else {
		/* check lock   */
		mutex_lock(&info->cmd_lock);
		info->cmd_is_running = true;
		mutex_unlock(&info->cmd_lock);

		info->cmd_state = CMD_STATUS_RUNNING;
		tsp_debug_err(true, &info->client->dev,
			"%s param = %d, %d\n", __func__,
			info->delayed_cmd_param[0], info->delayed_cmd_param[1]);

		info->cmd_param[0] = info->delayed_cmd_param[0];
		if (info->delayed_cmd_param[0] > 1)
			info->cmd_param[1] = info->delayed_cmd_param[1];
		strcpy(info->cmd, "clear_cover_mode");
		clear_cover_mode(info);
	}
}

static ssize_t store_cmd(struct device *dev, struct device_attribute *devattr,
			   const char *buf, size_t count)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	char *cur, *start, *end;
	char buff[CMD_STR_LEN] = { 0 };
	int len, i;
	struct ft_cmd *ft_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;

	if (!info) {
		printk(KERN_ERR "%s: No platform data found\n",
				__func__);
		return -EINVAL;
	}

	if (!info->input_dev) {
		printk(KERN_ERR "%s: No input_dev data found\n",
				__func__);
		return -EINVAL;
	}

	if (count > CMD_STR_LEN) {
		printk(KERN_ERR "%s: overflow command length\n",
				__func__);
		return -EINVAL;
	}

	if (info->cmd_is_running == true) {
		tsp_debug_err(true, &info->client->dev, "ft_cmd: other cmd is running.\n");
		if (strncmp("clear_cover_mode", buf, 16) == 0) {
			cancel_delayed_work(&info->cover_cmd_work);
			tsp_debug_err(true, &info->client->dev,
				"[cmd is delayed] %d, param = %d, %d\n", __LINE__, buf[17]-'0', buf[19]-'0');
			info->delayed_cmd_param[0] = buf[17]-'0';
			if (info->delayed_cmd_param[0] > 1)
				info->delayed_cmd_param[1] = buf[19]-'0';

			schedule_delayed_work(&info->cover_cmd_work, msecs_to_jiffies(10));
		}
		return -EBUSY;
	}
	else if (info->reinit_done == false) {
		tsp_debug_err(true, &info->client->dev, "ft_cmd: reinit is working\n");
		if (strncmp("clear_cover_mode", buf, 16) == 0) {
			cancel_delayed_work(&info->cover_cmd_work);
			tsp_debug_err(true, &info->client->dev,
				"[cmd is delayed] %d, param = %d, %d\n", __LINE__, buf[17]-'0', buf[19]-'0');
			info->delayed_cmd_param[0] = buf[17]-'0';
			if (info->delayed_cmd_param[0] > 1)
				info->delayed_cmd_param[1] = buf[19]-'0';

			if(info->delayed_cmd_param[0] == 0) schedule_delayed_work(&info->cover_cmd_work, msecs_to_jiffies(300));
		}
	}

	/* check lock   */
	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = true;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = 1;
	memset(info->cmd_param, 0x00, ARRAY_SIZE(info->cmd_param));

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(info->cmd, 0x00, ARRAY_SIZE(info->cmd));
	memcpy(info->cmd, buf, len);
	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);

	else
		memcpy(buff, buf, len);
	tsp_debug_info(true, &info->client->dev, "COMMAND : %s\n", buff);

	/* find command */
	list_for_each_entry(ft_cmd_ptr, &info->cmd_list_head, list) {
		if(ft_cmd_ptr->cmd_name == NULL){
			tsp_debug_info(true, &info->client->dev, "COMMAND : %s, ft_cmd_ptr->cmd_name is NULL!\n", buff);
			break;
		}
		if (!strncmp(buff, ft_cmd_ptr->cmd_name, CMD_STR_LEN)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(ft_cmd_ptr, &info->cmd_list_head, list) {
			if (ft_cmd_ptr->cmd_name != NULL && !strncmp("not_support_cmd", ft_cmd_ptr->cmd_name, sizeof("not_support_cmd")))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));

		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strnlen(buff, ARRAY_SIZE(buff))) =
				'\0';
				if (kstrtoint
				 (buff, 10,
				  info->cmd_param + param_cnt) < 0)
					goto err_out;
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while ((cur - buf <= len) && (param_cnt < CMD_PARAM_NUM));
	}
	tsp_debug_info(true, &info->client->dev, "cmd = %s\n", ft_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		tsp_debug_info(true, &info->client->dev, "cmd param %d= %d\n", i,
			  info->cmd_param[i]);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode())
		tui_mode_cmd(info);
	else
#endif
	ft_cmd_ptr->cmd_func(info);

err_out:
	return count;
}

static ssize_t show_cmd_status(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	char buff[16] = { 0 };

	if (!info) {
		printk(KERN_ERR "%s: No platform data found\n",
				__func__);
		return -EINVAL;
	}

	if (!info->input_dev) {
		printk(KERN_ERR "%s: No input_dev data found\n",
				__func__);
		return -EINVAL;
	}

	tsp_debug_info(true, &info->client->dev, "tsp cmd: status:%d\n", info->cmd_state);
	if (info->cmd_state == CMD_STATUS_WAITING)
		snprintf(buff, sizeof(buff), "WAITING");

	else if (info->cmd_state == CMD_STATUS_RUNNING)
		snprintf(buff, sizeof(buff), "RUNNING");

	else if (info->cmd_state == CMD_STATUS_OK)
		snprintf(buff, sizeof(buff), "OK");

	else if (info->cmd_state == CMD_STATUS_FAIL)
		snprintf(buff, sizeof(buff), "FAIL");

	else if (info->cmd_state == CMD_STATUS_NOT_APPLICABLE)
		snprintf(buff, sizeof(buff), "NOT_APPLICABLE");
	return snprintf(buf, TSP_BUF_SIZE, "%s\n", buff);
}

static ssize_t show_cmd_result(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	if (!info) {
		printk(KERN_ERR "%s: No platform data found\n",
				__func__);
		return -EINVAL;
	}

	if (!info->input_dev) {
		printk(KERN_ERR "%s: No input_dev data found\n",
				__func__);
		return -EINVAL;
	}

	tsp_debug_info(true, &info->client->dev, "tsp cmd: result: %s\n",
		   info->cmd_result);
	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = 0;
	return snprintf(buf, TSP_BUF_SIZE, "%s\n", info->cmd_result);
}

static ssize_t cmd_list_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int ii = 0;
	char buffer[info->cmd_buffer_size+CMD_STR_LEN];
	char buffer_name[CMD_STR_LEN];

	snprintf(buffer, CMD_STR_LEN, "++factory command list++\n");
	while (strncmp(ft_commands[ii].cmd_name, "not_support_cmd", 16) != 0) {
		snprintf(buffer_name, CMD_STR_LEN, "%s\n", ft_commands[ii].cmd_name);
		strcat(buffer, buffer_name);
		ii++;
	}

	tsp_debug_info(true, &info->client->dev,
		"%s: length : %u / %d\n", __func__,
		(unsigned int)strlen(buffer), info->cmd_buffer_size+CMD_STR_LEN);

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", buffer);
}

static void set_default_result(struct fts_ts_info *info)
{
	char delim = ':';
	memset(info->cmd_result, 0x00, ARRAY_SIZE(info->cmd_result));
	memcpy(info->cmd_result, info->cmd, strnlen(info->cmd, CMD_STR_LEN));
	strncat(info->cmd_result, &delim, 1);
}

static void set_cmd_result(struct fts_ts_info *info, char *buff, int len)
{
	strncat(info->cmd_result, buff, len);
}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
static void tui_mode_cmd(struct fts_ts_info *info)
{
	char buff[16] = "TUImode:FAIL";
	set_default_result(info);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
#endif

static void not_support_cmd(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[16] = { 0 };
	set_default_result(info);
	snprintf(buff, sizeof(buff), "%s", "NA");
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
	tsp_debug_info(true, &info->client->dev, "%s: \"%s\"\n", __func__, buff);
}

static void fw_update(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[64] = { 0 };
	int retval = 0;

	set_default_result(info);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	retval = fts_fw_update_on_hidden_menu(info, info->cmd_param[0]);

	if (retval < 0) {
		sprintf(buff, "%s", "NA");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_FAIL;
		tsp_debug_info(true, &info->client->dev, "%s: failed [%d]\n", __func__, retval);
	} else {
		sprintf(buff, "%s", "OK");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_OK;
		tsp_debug_info(true, &info->client->dev, "%s: success [%d]\n", __func__, retval);
	}

	return;
}

static int fts_get_channel_info(struct fts_ts_info *info)
{
	int rc = -1;
	unsigned char cmd[4] =
		{ 0xB2, 0x00, 0x14, 0x02 };
	unsigned char data[FTS_EVENT_SIZE];
	int retry = 0;

	memset(data, 0x0, FTS_EVENT_SIZE);

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, FLUSHBUFFER);

	fts_write_reg(info, &cmd[0], 4);
	cmd[0]=READ_ONE_EVENT;
	while (fts_read_reg
	       (info, &cmd[0], 1, (unsigned char *)data, FTS_EVENT_SIZE)) {

		if (data[0] == EVENTID_RESULT_READ_REGISTER) {
			if ((data[1] == cmd[1]) && (data[2] == cmd[2]))
			{
				info->SenseChannelLength = data[3];
				info->ForceChannelLength = data[4];

				rc = 0;
				break;
			}
		}

		if (retry++ > 30) {
			rc = -1;
			tsp_debug_info(true, &info->client->dev, "Time over - wait for channel info\n");
			break;
		}
		mdelay(5);
	}
	fts_interrupt_set(info, INT_ENABLE);

	return rc;
}

static void procedure_cmd_event(struct fts_ts_info *info, unsigned char *data)
{
	char buff[16] = { 0 };

	if ((data[1] == 0x01) && (data[2] == 0x8D))
	{
		snprintf(buff, sizeof(buff), "%d",
					*(unsigned short *)&data[3]);
		tsp_debug_info(true, &info->client->dev, "%s: %s\n", "get_threshold", buff);
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_OK;

	}
#ifdef CONFIG_GLOVE_TOUCH
	else if ((data[1] == 0x01) && (data[2] == 0xC6))
	{
		snprintf(buff, sizeof(buff), "%d",
					*(unsigned short *)&data[3]);
		tsp_debug_info(true, &info->client->dev, "%s: %s\n", "get_glove_sensitivity", buff);
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_OK;

	}
#endif
	else if ((data[1] == 0x07) && (data[2] == 0xE7))
	{
		if (data[3] <= TSP_FACTEST_RESULT_PASS) {
			sprintf(buff, "%s",
					data[3] == TSP_FACTEST_RESULT_PASS ? "PASS" :
					data[3] == TSP_FACTEST_RESULT_FAIL ? "FAIL" : "NONE");
			tsp_debug_info(true, &info->client->dev, "%s: success [%s][%d]", "get_tsp_test_result",
                                        data[3] == TSP_FACTEST_RESULT_PASS ? "PASS" :
                                        data[3] == TSP_FACTEST_RESULT_FAIL ? "FAIL" :
                                        "NONE", data[3]);
			set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
			info->cmd_state = CMD_STATUS_OK;
		}
		else
		{
			snprintf(buff, sizeof(buff), "%s", "NG");
			set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
			info->cmd_state = CMD_STATUS_FAIL;
			tsp_debug_info(true, &info->client->dev, "%s: %s\n",
							"get_tsp_test_result",
							buff);
		}

	}
}

void fts_print_frame(struct fts_ts_info *info, short *min, short *max)
{
	int i = 0;
	int j = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };

	pStr = kzalloc(6 * (info->SenseChannelLength + 1), GFP_KERNEL);
	if (pStr == NULL) {
		tsp_debug_info(true, &info->client->dev, "FTS pStr kzalloc failed\n");
		return;
	}

	memset(pStr, 0x0, 6 * (info->SenseChannelLength + 1));
	snprintf(pTmp, sizeof(pTmp), "    ");
	strncat(pStr, pTmp, 6 * info->SenseChannelLength);

	for (i = 0; i < info->SenseChannelLength; i++) {
		snprintf(pTmp, sizeof(pTmp), "Rx%02d  ", i);
		strncat(pStr, pTmp, 6 * info->SenseChannelLength);
	}

	tsp_debug_info(true, &info->client->dev, "FTS %s\n", pStr);
	memset(pStr, 0x0, 6 * (info->SenseChannelLength + 1));
	snprintf(pTmp, sizeof(pTmp), " +");
	strncat(pStr, pTmp, 6 * info->SenseChannelLength);

	for (i = 0; i < info->SenseChannelLength; i++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strncat(pStr, pTmp, 6 * info->SenseChannelLength);
	}

	tsp_debug_info(true, &info->client->dev, "FTS %s\n", pStr);

	for (i = 0; i < info->ForceChannelLength; i++) {
		memset(pStr, 0x0, 6 * (info->SenseChannelLength + 1));
		snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", i);
		strncat(pStr, pTmp, 6 * info->SenseChannelLength);

		for (j = 0; j < info->SenseChannelLength; j++) {
			snprintf(pTmp, sizeof(pTmp), "%5d ", info->pFrame[(i * info->SenseChannelLength) + j]);

			if (i > 0) {
				if (info->pFrame[(i * info->SenseChannelLength) + j] < *min)
					*min = info->pFrame[(i * info->SenseChannelLength) + j];

				if (info->pFrame[(i * info->SenseChannelLength) + j] > *max)
					*max = info->pFrame[(i * info->SenseChannelLength) + j];
			}
			strncat(pStr, pTmp, 6 * info->SenseChannelLength);
		}
		tsp_debug_info(true, &info->client->dev, "FTS %s\n", pStr);
	}

	kfree(pStr);
}

int fts_read_frame(struct fts_ts_info *info, unsigned char type, short *min,
		 short *max)
{
	unsigned char pFrameAddress[8] =
	{ 0xD0, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00 };
	unsigned int FrameAddress = 0;
	unsigned int writeAddr = 0;
	unsigned int start_addr = 0;
	unsigned int end_addr = 0;
	unsigned int totalbytes = 0;
	unsigned int remained = 0;
	unsigned int readbytes = 0xFF;
	unsigned int dataposition = 0;
	unsigned char *pRead = NULL;
	int rc = 0;
	int ret = 0;
	int i = 0;
	pRead = kzalloc(BUFFER_MAX, GFP_KERNEL);

	if (pRead == NULL) {
		tsp_debug_info(true, &info->client->dev, "FTS pRead kzalloc failed\n");
		rc = 1;
		goto ErrorExit;
	}

	pFrameAddress[2] = type;
	totalbytes = info->SenseChannelLength * info->ForceChannelLength * 2;
	ret = fts_read_reg(info, &pFrameAddress[0], 3, pRead, pFrameAddress[3]);

	if (ret >= 0) {
		if (info->digital_rev == FTS_DIGITAL_REV_1)
			FrameAddress = pRead[0] + (pRead[1] << 8);
		else if (info->digital_rev == FTS_DIGITAL_REV_2)
			FrameAddress = pRead[1] + (pRead[2] << 8);
		else if (info->digital_rev == FTS_DIGITAL_REV_3)
			FrameAddress = pRead[1] + (pRead[2] << 8);

		start_addr = FrameAddress+info->SenseChannelLength * 2;
		end_addr = start_addr + totalbytes;
	} else {
		tsp_debug_info(true, &info->client->dev, "FTS read failed rc = %d \n", ret);
		rc = 2;
		goto ErrorExit;
	}

#ifdef DEBUG_MSG
	tsp_debug_info(true, &info->client->dev, "FTS FrameAddress = %X \n", FrameAddress);
	tsp_debug_info(true, &info->client->dev, "FTS start_addr = %X, end_addr = %X \n", start_addr, end_addr);
#endif

	remained = totalbytes;
	for (writeAddr = start_addr; writeAddr < end_addr; writeAddr += READ_CHUNK_SIZE) {
		pFrameAddress[1] = (writeAddr >> 8) & 0xFF;
		pFrameAddress[2] = writeAddr & 0xFF;

		if (remained >= READ_CHUNK_SIZE)
			readbytes = READ_CHUNK_SIZE;
		else
			readbytes = remained;

		memset(pRead, 0x0, readbytes);

#ifdef DEBUG_MSG
		tsp_debug_info(true, &info->client->dev, "FTS %02X%02X%02X readbytes=%d\n",
			   pFrameAddress[0], pFrameAddress[1],
			   pFrameAddress[2], readbytes);

#endif
		if (info->digital_rev == FTS_DIGITAL_REV_1) {
			fts_read_reg(info, &pFrameAddress[0], 3, pRead, readbytes);
			remained -= readbytes;

			for (i = 0; i < readbytes; i += 2) {
				info->pFrame[dataposition++] =
				pRead[i] + (pRead[i + 1] << 8);
			}
		} else if ( (info->digital_rev == FTS_DIGITAL_REV_2) || (info->digital_rev == FTS_DIGITAL_REV_3) ) {
			fts_read_reg(info, &pFrameAddress[0], 3, pRead, readbytes + 1);
			remained -= readbytes;

			for (i = 1; i < (readbytes+1); i += 2) {
				info->pFrame[dataposition++] =
				pRead[i] + (pRead[i + 1] << 8);
			}
		}
	}
	kfree(pRead);

#ifdef DEBUG_MSG
	tsp_debug_info(true, &info->client->dev,
		   "FTS writeAddr = %X, start_addr = %X, end_addr = %X \n",
		   writeAddr, start_addr, end_addr);
#endif

	switch (type) {
	case TYPE_RAW_DATA:
		tsp_debug_info(true, &info->client->dev, "FTS [Raw Data : 0x%X%X] \n", pFrameAddress[0],
			FrameAddress);
		break;
	case TYPE_FILTERED_DATA:
		tsp_debug_info(true, &info->client->dev, "FTS [Filtered Data : 0x%X%X] \n",
			pFrameAddress[0], FrameAddress);
		break;
	case TYPE_STRENGTH_DATA:
		tsp_debug_info(true, &info->client->dev, "FTS [Strength Data : 0x%X%X] \n",
			pFrameAddress[0], FrameAddress);
		break;
	case TYPE_BASELINE_DATA:
		tsp_debug_info(true, &info->client->dev, "FTS [Baseline Data : 0x%X%X] \n",
			pFrameAddress[0], FrameAddress);
		break;
	}
	fts_print_frame(info, min, max);

ErrorExit:
	return rc;
}

static int fts_panel_ito_test(struct fts_ts_info *info)
{
	unsigned char cmd = READ_ONE_EVENT;
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char regAdd[4] = {0xB0, 0x03, 0x60, 0xFB};
	int retry = 0;
	int result = -1;

	fts_systemreset(info);
	fts_wait_for_ready(info);

	if (info->stm_ver != STM_VER7)
	{
		fts_command(info, SLEEPOUT);
		fts_delay(20);
	}

	disable_irq(info->irq);
	fts_interrupt_set(info, INT_DISABLE);

	if (info->stm_ver != STM_VER7)
	{
	fts_write_reg(info, &regAdd[0], 4);
	}

	fts_command(info, FLUSHBUFFER);

	fts_command(info, 0xA7);    // ITO test command
	fts_delay(200);
	memset(data, 0x0, FTS_EVENT_SIZE);
	while (fts_read_reg
			(info, &cmd, 1, (unsigned char *)data, FTS_EVENT_SIZE)) {

		if ((data[0] == 0x0F) && (data[1] == 0x05)) {
			switch (data[2]) {
			case 0x00 :
				result = 0;
				break;
			case 0x01 :
				tsp_debug_info(true, &info->client->dev, "[FTS] ITO Test result : Force channel [%d] open.\n",
					data[3]);
				break;
			case 0x02 :
				tsp_debug_info(true, &info->client->dev, "[FTS] ITO Test result : Sense channel [%d] open.\n",
					data[3]);
				break;
			case 0x03 :
				tsp_debug_info(true, &info->client->dev, "[FTS] ITO Test result : Force channel [%d] short to GND.\n",
					data[3]);
				break;
			case 0x04 :
				tsp_debug_info(true, &info->client->dev, "[FTS] ITO Test result : Sense channel [%d] short to GND.\n",
					data[3]);
				break;
			case 0x07 :
				tsp_debug_info(true, &info->client->dev, "[FTS] ITO Test result : Force channel [%d] short to force.\n",
					data[3]);
				break;
			case 0x08 :
				tsp_debug_info(true, &info->client->dev, "[FTS] ITO Test result : Sennse channel [%d] short to sense.\n",
					data[3]);
				break;
			case 0x0E :
				tsp_debug_info(true, &info->client->dev, "[FTS] ITO Test result : Sennse channel [%d] short to sense.\n",
					data[3]);
				break;
			default:
				break;
			}

			break;
		}

		if (retry++ > 30) {
			tsp_debug_info(true, &info->client->dev, "Time over - wait for result of ITO test\n");
			break;
		}
		fts_delay(10);
	}

	fts_systemreset(info);

	/* wait for ready event */
	fts_wait_for_ready(info);

#ifdef FTS_SUPPORT_NOISE_PARAM
	fts_set_noise_param(info);
#endif

	if (info->stm_ver != STM_VER7)
	{
		fts_command(info, SLEEPOUT);
	}

	fts_command(info, FLUSHBUFFER);
	fts_delay(10);
	fts_command(info, SENSEON);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif
#ifdef FTS_SUPPORT_HOVER
	if (info->hover_enabled)
		fts_command(info, FTS_CMD_HOVER_ON);
#endif
	if (info->flip_enable) {
		fts_set_cover_type(info, true);
	} 
#ifdef CONFIG_GLOVE_TOUCH
	else {
		if (info->mshover_enabled)
			fts_command(info, FTS_CMD_MSHOVER_ON);
	}
#endif
#ifdef FTS_SUPPORT_TA_MODE
	if (info->TA_Pluged)
		fts_command(info, FTS_CMD_CHARGER_PLUGGED);
#endif

	info->touch_count = 0;

	fts_interrupt_set(info, INT_ENABLE);
	enable_irq(info->irq);

	return result;
}

static void get_fw_ver_bin(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[16] = { 0 };
	set_default_result(info);

	if (strncmp(info->board->model_name, "G928", 4) == 0) {
		info->tspid_val= 0;
		info->tspid2_val= gpio_get_value(info->board->tspid2);

		sprintf(buff, "ST%01X%01X%04X",
				info->tspid_val, info->tspid2_val,
				info->fw_main_version_of_bin);
	} else {
		sprintf(buff, "ST%02X%04X",
				info->panel_revision,
				info->fw_main_version_of_bin);
	}

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[16] = { 0 };
	set_default_result(info);

	if (strncmp(info->board->model_name, "G928", 4) == 0) {
		info->tspid_val= 0;
		info->tspid2_val= gpio_get_value(info->board->tspid2);

		sprintf(buff, "ST%01X%01X%04X",
				info->tspid_val, info->tspid2_val,
				info->fw_main_version_of_ic);
	} else {
		sprintf(buff, "ST%02X%04X",
				info->panel_revision,
				info->fw_main_version_of_ic);
	}

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_config_ver(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[20] = { 0 };

	snprintf(buff, sizeof(buff), "%s_ST_%04X",
		info->board->model_name ?: info->board->project_name ?: STM_DEVICE_NAME,
		info->config_version_of_ic);

	set_default_result(info);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_threshold(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	unsigned char cmd[4] =
		{ 0xB2, 0x00, 0x79, 0x02 }; // For Noble or Zero2
	int timeout=0;

	if (info->stm_ver == STM_VER7)  // For Hero2
	{
		cmd[1] = 0x01;
		cmd[2] = 0x8D;
	}

	set_default_result(info);

	if (info->touch_stopped) {
		char buff[CMD_STR_LEN] = { 0 };
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_write_reg(info, &cmd[0], 4);
	info->cmd_state = CMD_STATUS_RUNNING;

	while (info->cmd_state == CMD_STATUS_RUNNING) {
		if (timeout++>30) {
			info->cmd_state = CMD_STATUS_FAIL;
			break;
		}
		msleep(10);
	}
}

static void module_off_master(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[3] = { 0 };
	int ret = 0;

	mutex_lock(&info->lock);
	if (info->tsp_enabled) {
		disable_irq(info->irq);
		info->tsp_enabled = false;
	}
	mutex_unlock(&info->lock);

	if (info->board->power)
		info->board->power(info, false);
	else
		ret = 1;

	if (ret == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else
		snprintf(buff, sizeof(buff), "%s", "NG");

	set_default_result(info);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	if (strncmp(buff, "OK", 2) == 0)
		info->cmd_state = CMD_STATUS_OK;
	else
		info->cmd_state = CMD_STATUS_FAIL;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void module_on_master(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[3] = { 0 };
	int ret = 0;

	mutex_lock(&info->lock);
	if (!info->tsp_enabled) {
		enable_irq(info->irq);
		info->tsp_enabled = true;
	}
	mutex_unlock(&info->lock);

	if (info->board->power)
		info->board->power(info, true);
	else
		ret = 1;

	if (ret == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else
		snprintf(buff, sizeof(buff), "%s", "NG");

	set_default_result(info);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	if (strncmp(buff, "OK", 2) == 0)
		info->cmd_state = CMD_STATUS_OK;
	else
		info->cmd_state = CMD_STATUS_FAIL;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_vendor(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[16] = { 0 };
	strncpy(buff, "STM", sizeof(buff));
	set_default_result(info);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_name(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[16] = { 0 };

	if (info->digital_rev != FTS_DIGITAL_REV_3)
	{
		strncpy(buff, "FTS5AD56", sizeof(buff));
	}
	else if (info->digital_rev == FTS_DIGITAL_REV_3)
	{
		strncpy(buff, "FTS7BD50", sizeof(buff));
	}

	set_default_result(info);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_mis_cal_info(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[16] = { 0 };
	unsigned char regAdd[3] = { 0 };
	unsigned char data[2] = { 0 };
	int ret = 0;

	set_default_result(info);

	regAdd[0] = 0xC7;
	regAdd[1] = 0x05;

	ret = fts_write_reg(info, regAdd, 2);
	if (ret < 0)
		goto err_miscal;

	fts_delay(50);

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x67;

	ret = fts_read_reg(info, regAdd, 3, data, 2);
	if (ret < 0)
		goto err_miscal;

	tsp_debug_info(true, &info->client->dev, "%s: %02X, %02X\n", __func__, data[0], data[1]);

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	snprintf(buff, sizeof(buff), "%d", data[1]);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	return;

err_miscal:
	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	tsp_debug_err(true, &info->client->dev, "%s: failed.\n", __func__);
	snprintf(buff, sizeof(buff), "NG");
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_FAIL;
}

static void get_wet_mode(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[16] = { 0 };
	unsigned char regAdd[3] = { 0 };
	unsigned char data[2] = { 0 };
	int ret;

	set_default_result(info);

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x66;

	ret = fts_read_reg(info, regAdd, 3, data, 2);
	if (ret < 0)
		goto err_wetmode;

	tsp_debug_info(true, &info->client->dev, "%s: %02X, %02X\n", __func__, data[0], data[1]);

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	snprintf(buff, sizeof(buff), "%d", data[1]);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	return;

err_wetmode:
	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	tsp_debug_err(true, &info->client->dev, "%s: failed.\n", __func__);
	snprintf(buff, sizeof(buff), "NG");
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_FAIL;
}

static void get_x_num(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[16] = { 0 };

	set_default_result(info);
	snprintf(buff, sizeof(buff), "%d", info->SenseChannelLength);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_y_num(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[16] = { 0 };

	set_default_result(info);
	snprintf(buff, sizeof(buff), "%d", info->ForceChannelLength);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_checksum_data(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[16] = { 0 };
	int rc;
	int retry = 0;
	unsigned char regAdd[4];
	unsigned char cmd[1];
	unsigned char buf[5];
	unsigned char data[FTS_EVENT_SIZE];

	set_default_result(info);

	fts_interrupt_set(info, INT_DISABLE);

	if (info->stm_ver != STM_VER7)
	{
		regAdd[0] = 0xb3;
		regAdd[1] = 0x00;
		regAdd[2] = 0x01;

		info->fts_write_reg(info, regAdd, 3);
		fts_delay(1);

		regAdd[0] = 0xb1;
		regAdd[1] = 0xEF;
		regAdd[2] = 0xFC;

		rc = info->fts_read_reg(info, regAdd, 3, buf, 5);
	}
	else	// For STM_VER7
	{
		fts_command(info, FLUSHBUFFER);
		regAdd[0] = 0xb2;
		regAdd[1] = 0x07;
		regAdd[2] = 0xfc;
		regAdd[3] = 0x05;

		info->fts_write_reg(info, regAdd, 4);
		fts_delay(1);

		memset(buf, 0x0, 5);
		memset(data, 0x0, FTS_EVENT_SIZE);
		cmd[0] = READ_ONE_EVENT;

		while (info->fts_read_reg(info, &cmd[0], 1, (unsigned char *)data, FTS_EVENT_SIZE)) {
			if ((data[0] == 0x12) && (data[1] == regAdd[1])
				&& (data[2] == regAdd[2])) {
				rc = 0;
				buf[1] = data[3];
				buf[2] = data[4];
				buf[3] = data[5];
				buf[4] = data[6];
				break;
			}

			if (retry++ > FTS_RETRY_COUNT) {
				tsp_debug_err(true, info->dev,"%s: Time Over\n", __func__);
				break;
			}
		}

	}
	fts_interrupt_set(info, INT_ENABLE);

	snprintf(buff, sizeof(buff), "%02X%02X%02X%02X", buf[1], buf[2], buf[3], buf[4]);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_reference_read(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;

	set_default_result(info);
	if (info->touch_stopped) {
		char buff[CMD_STR_LEN] = { 0 };
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_read_frame(info, TYPE_BASELINE_DATA, &min, &max);
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_reference(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	set_default_result(info);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = fts_check_index(info);
	if (node < 0)
		return;

	val = info->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_rawcap_read(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
	//unsigned char regAdd[4] = {0xB0, 0x04, 0x49, 0x00}; // it's for Zero Prj
	//unsigned char regAdd[4] = {0xB0, 0x04, 0x48, 0x00}; // it's for Noble & Zero2 Prj
	unsigned char regAdd[4] = {0xB0, 0x03, 0x17, 0x00}; // it's for Hero2 Prj
#endif
	set_default_result(info);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

#if !defined(TSP_RUN_AUTOTUNE_DEFAULT)
	if (!info->run_autotune && ((info->digital_rev == FTS_DIGITAL_REV_2) || (info->digital_rev == FTS_DIGITAL_REV_3)))
		goto rawcap_read;
	else
		tsp_debug_info(true, &info->client->dev, "%s: set autotune\n\n", __func__);
#endif
	disable_irq(info->irq);

	if (info->digital_rev == FTS_DIGITAL_REV_1) {
		unsigned char data[FTS_EVENT_SIZE];
		unsigned char regAdd;
		int fail_retry = 0;

		fts_interrupt_set(info, INT_DISABLE);
		fts_command(info, CX_TUNNING);
		fts_delay(300);

		regAdd = READ_ONE_EVENT;
		while (fts_read_reg(info, &regAdd, 1, (unsigned char *)data, FTS_EVENT_SIZE)) {
			if ((data[0] == EVENTID_STATUS_EVENT) &&
				(data[1] == STATUS_EVENT_MUTUAL_AUTOTUNE_DONE)) {
				break;
			}

			if (fail_retry++ > FTS_RETRY_COUNT * 15) {
				tsp_debug_info(true, info->dev, "%s: Raw data read Time Over\n", __func__);
				break;
			}
			fts_delay(10);
		}

		fts_fw_wait_for_event(info, STATUS_EVENT_MUTUAL_AUTOTUNE_DONE);
		fts_fw_wait_for_event (info, STATUS_EVENT_WATER_SELF_AUTOTUNE_DONE);

		fts_interrupt_set(info, INT_ENABLE);
	} else if (((info->digital_rev == FTS_DIGITAL_REV_2) || (info->digital_rev == FTS_DIGITAL_REV_3))
#ifdef CONFIG_SEC_DEBUG_TSP_LOG
	&& !info->rawdata_read_lock
#endif
	) {
		fts_interrupt_set(info, INT_DISABLE);

		fts_command(info, SENSEOFF);
		fts_delay(50);
#ifdef FTS_SUPPORT_TOUCH_KEY
		if (info->board->support_mskey) {
			fts_command(info, FTS_CMD_KEY_SENSE_OFF); // Key Sensor OFF
		}
#endif
		fts_command(info, FLUSHBUFFER);

		fts_release_all_finger(info);
#ifdef FTS_SUPPORT_TOUCH_KEY
		if (info->board->support_mskey)
		fts_release_all_key(info);
#endif

#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
		info->o_afe_ver = info->afe_ver;
#endif

		fts_execute_autotune(info);

		// Added mutual auto tune
		fts_command(info, CX_TUNNING);
		fts_delay(300);
		fts_fw_wait_for_event_D3(info, STATUS_EVENT_MUTUAL_AUTOTUNE_DONE, 0x00);

#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
		//STMicro Auto-tune protection disable
		fts_write_reg(info, regAdd, 4);
		fts_delay(1);
#endif
		/* Command "SLEEPOUT" is not used from FTSD3. */
		if (info->stm_ver != STM_VER7)
		{
			fts_command(info, SLEEPOUT);
			fts_delay(1);
		}

		fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_WATER_MODE
		fts_fw_wait_for_event(info, STATUS_EVENT_WATER_SELF_DONE);
#else
		if( info->stm_ver == STM_VER7)
		{
			fts_fw_wait_for_event (info, STATUS_EVENT_FORCE_CAL_DONE_D3);
		}
		else
		{
			fts_fw_wait_for_event (info, STATUS_EVENT_FORCE_CAL_DONE);
		}
#endif
#ifdef FTS_SUPPORT_TOUCH_KEY
		if (info->board->support_mskey)
			fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

		fts_interrupt_set(info, INT_ENABLE);
	}
	enable_irq(info->irq);
#if !defined(TSP_RUN_AUTOTUNE_DEFAULT)
rawcap_read:
#endif
	fts_delay(50);
	fts_read_frame(info, TYPE_FILTERED_DATA, &min, &max);
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_rawcap(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	set_default_result(info);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = fts_check_index(info);
	if (node < 0)
		return;

	val = info->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_delta_read(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;

	set_default_result(info);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_read_frame(info, TYPE_STRENGTH_DATA, &min, &max);
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_strength_all_data(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;
	char all_strbuff[(info->ForceChannelLength)*(info->SenseChannelLength)*5];
	int i, j;

	memset(all_strbuff,0,sizeof(char)*((info->ForceChannelLength)*(info->SenseChannelLength)*5));	//size 5  ex(1125,)

	set_default_result(info);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_read_frame(info, TYPE_STRENGTH_DATA, &min, &max);


	for (i = 0; i < info->ForceChannelLength; i++) {
		for (j = 0; j < info->SenseChannelLength; j++) {

			sprintf(buff, "%d,", info->pFrame[(i * info->SenseChannelLength) + j]);
			strcat(all_strbuff, buff);
		}
	}

	info->cmd_state = CMD_STATUS_OK;

	set_cmd_result(info, all_strbuff, strnlen(all_strbuff, sizeof(all_strbuff)));
	tsp_debug_info(true, &info->client->dev, "%ld (%ld)\n", strnlen(all_strbuff, sizeof(all_strbuff)),sizeof(all_strbuff));
}

static void get_delta(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	set_default_result(info);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = fts_check_index(info);
	if (node < 0)
		return;

	val = info->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#ifdef FTS_SUPPORT_HOVER
void fts_read_self_frame(struct fts_ts_info *info, unsigned short oAddr)
{
	char buff[66] = {0, };
	short *data = 0;
	char temp[9] = {0, };
	char temp2[512] = {0, };
	int i;
	int rc;
	int retry=1;
	unsigned char regAdd[6] = {0xD0, 0x00, 0x00, 0xD0, 0x00, 0x00};

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (!info->hover_enabled) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Hover is disabled\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP Hover disabled");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	while (!info->hover_ready) {
		if (retry++ > 500) {
			tsp_debug_info(true, &info->client->dev, "%s: [FTS] Timeout - Abs Raw Data Ready Event\n",
					  __func__);
			break;
		}
		fts_delay(10);
	}

	regAdd[1] = (oAddr >> 8) & 0xff;
	regAdd[2] = oAddr & 0xff;
	rc = info->fts_read_reg(info, &regAdd[0], 3, (unsigned char *)&buff[0], 5);
	if (!rc) {
		info->cmd_state = CMD_STATUS_FAIL;
		return;
	}

	if (info->digital_rev == FTS_DIGITAL_REV_1) {
		tsp_debug_info(true, &info->client->dev, "%s: Force Address : %02x%02x\n",
				__func__, buff[1], buff[0]);
		tsp_debug_info(true, &info->client->dev, "%s: Sense Address : %02x%02x\n",
				__func__, buff[3], buff[2]);
		regAdd[1] = buff[3];
		regAdd[2] = buff[2];
		regAdd[4] = buff[1];
		regAdd[5] = buff[0];
	} else if ( (info->digital_rev == FTS_DIGITAL_REV_2) || (info->digital_rev == FTS_DIGITAL_REV_3) ) {
		tsp_debug_info(true, &info->client->dev, "%s: Force Address : %02x%02x\n",
				__func__, buff[2], buff[1]);
		tsp_debug_info(true, &info->client->dev, "%s: Sense Address : %02x%02x\n",
				__func__, buff[4], buff[3]);
		regAdd[1] = buff[4];
		regAdd[2] = buff[3];
		regAdd[4] = buff[2];
		regAdd[5] = buff[1];
	}

	rc = info->fts_read_reg(info, &regAdd[0], 3,
							(unsigned char *)&buff[0],
							info->SenseChannelLength * 2 + 1);
	if (!rc) {
		info->cmd_state = CMD_STATUS_FAIL;
		return;
	}

	if (info->digital_rev == FTS_DIGITAL_REV_1)
		data = (short *)&buff[0];
	else
		data = (short *)&buff[1];

	memset(temp, 0x00, ARRAY_SIZE(temp));
	memset(temp2, 0x00, ARRAY_SIZE(temp2));

	for (i = 0; i < info->SenseChannelLength; i++) {
		tsp_debug_info(true, &info->client->dev,
				"%s: Rx [%d] = %d\n", __func__,
				i,
				*data);
		sprintf(temp, "%d,", *data);
		strncat(temp2, temp, 9);
		data++;
	}

	rc = info->fts_read_reg(info, &regAdd[3], 3,
							(unsigned char *)&buff[0],
							info->ForceChannelLength * 2 + 1);
	if (!rc) {
		info->cmd_state = CMD_STATUS_FAIL;
		return;
	}

	if (info->digital_rev == FTS_DIGITAL_REV_1)
		data = (short *)&buff[0];
	else
		data = (short *)&buff[1];

	for (i = 0; i < info->ForceChannelLength; i++) {
		tsp_debug_info(true, &info->client->dev,
				"%s: Tx [%d] = %d\n", __func__, i, *data);
		sprintf(temp, "%d,", *data);
		strncat(temp2, temp, 9);
		data++;
	}

	set_cmd_result(info, temp2, strnlen(temp2, sizeof(temp2)));

	info->cmd_state = CMD_STATUS_OK;
}

static void run_abscap_read(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;

	set_default_result(info);
	fts_read_self_frame(info, 0x000E);
}

static void run_absdelta_read(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;

	set_default_result(info);
	fts_read_self_frame(info, 0x0012);
}
#endif

#define FTS_F_WIX1_ADDR	0x1FE7
#define FTS_S_WIX1_ADDR	0x1FE8
#define FTS_F_WIX2_ADDR	0x18FD
#define FTS_S_WIX2_ADDR	0x1929
#define FTS_WATER_SELF_RAW_ADDR	0x1A

static void fts_read_ix_data(struct fts_ts_info *info, bool allnode)
{
	char buff[CMD_STR_LEN] = { 0 };

	unsigned short max_tx_ix_sum = 0;
	unsigned short min_tx_ix_sum = 0xFFFF;

	unsigned short max_rx_ix_sum = 0;
	unsigned short min_rx_ix_sum = 0xFFFF;

	unsigned char tx_ix2[info->ForceChannelLength + 4];
	unsigned char rx_ix2[info->SenseChannelLength + 4];

	unsigned short ix1_addr = FTS_F_WIX1_ADDR;
	unsigned short ix2_tx_addr = FTS_F_WIX2_ADDR;
	unsigned short ix2_rx_addr = FTS_S_WIX2_ADDR;

	unsigned char regAdd[FTS_EVENT_SIZE];
	unsigned short tx_ix1 = 0, rx_ix1 = 0;
	unsigned char buf[FTS_EVENT_SIZE] = {0};
	unsigned char r_addr = READ_ONE_EVENT;

	unsigned short force_ix_data[info->ForceChannelLength * 2 + 1];
	unsigned short sense_ix_data[info->SenseChannelLength * 2 + 1];	
	int buff_size,j;
	char *mbuff = NULL;
	int num,n,a,fzero;
	char cnum;
	int retry = 0, i = 0;

	int	comp_header_addr, comp_start_tx_addr, comp_start_rx_addr;
	unsigned int rx_num, tx_num;

	set_default_result(info);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
		       __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

//	fts_command(info, SLEEPIN); // Sleep In for INT disable

	disable_irq(info->irq);
	fts_interrupt_set(info, INT_DISABLE);

	if (info->stm_ver == STM_VER7)
	{
		fts_command(info, SENSEOFF);
	}
	else
	{
		fts_systemreset(info);
		fts_delay(50);
		fts_wait_for_ready(info);

		fts_command(info, SLEEPOUT);
		fts_delay(50);
	}

	#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_OFF);
	}
	#endif

	fts_command(info, FLUSHBUFFER);                 // Clear FIFO
	fts_delay(50);

	if (info->stm_ver != STM_VER7)	{
		regAdd[0] = 0xB2;
		regAdd[1] = (ix1_addr >> 8)&0xff;
		regAdd[2] = (ix1_addr&0xff);
		regAdd[3] = 0x04;

		fts_write_reg(info, &regAdd[0], 4);
		fts_delay(1);

		retry = FTS_RETRY_COUNT * 3;
		do {
			if (retry < 0) {
				tsp_debug_err(true, &info->client->dev, "%s: failed to compare buf,[%x][%x][%x][%x] break1!\n",
								__func__, buf[1], buf[2], regAdd[1], regAdd[2]);
				break;
			}
			fts_delay(10);
			fts_read_reg(info, &r_addr, 1, &buf[0], FTS_EVENT_SIZE);
			retry--;
		} while (buf[1] != regAdd[1] || buf[2] != regAdd[2]);

		//read fifo
		tx_ix1 = (short)buf[3] * 4;
		rx_ix1 = (short)buf[4] * 4;

		tsp_debug_info(true, &info->client->dev, "%s: tx_ix1 : %d, rx_ix1 : %d (%d, %d)\n",
					__func__, tx_ix1, rx_ix1, buf[3], buf[4]);

		regAdd[0] = 0xB2;
		regAdd[1] = (ix2_tx_addr >>8)&0xff;
		regAdd[2] = (ix2_tx_addr & 0xff);

		for (i = 0; i < info->ForceChannelLength / 4 + 1; i++) {
			fts_write_reg(info, &regAdd[0], 4);
			fts_delay(1);

			retry = FTS_RETRY_COUNT * 3;
			do {
				if (retry < 0) {
					tsp_debug_err(true, &info->client->dev, "%s: failed to compare buf,[%x][%x][%x][%x] break2!\n",
								__func__, buf[1], buf[2], regAdd[1], regAdd[2]);
					break;
				}
				fts_delay(10);
				fts_read_reg(info, &r_addr, 1, &buf[0], FTS_EVENT_SIZE);
				retry--;
			} while (buf[1] != regAdd[1] || buf[2] != regAdd[2]);

			//read fifo
			tx_ix2[i*4] = buf[3];
			tx_ix2[i*4+1] = buf[4];
			tx_ix2[i*4+2] = buf[5];
			tx_ix2[i*4+3] = buf[6];

			ix2_tx_addr += 4;
			regAdd[0] = 0xB2;
			regAdd[1] = (ix2_tx_addr >>8)&0xff;
			regAdd[2] = (ix2_tx_addr & 0xff);
		}

		regAdd[0] = 0xB2;
		regAdd[1] = (ix2_rx_addr >>8)&0xff;
		regAdd[2] = (ix2_rx_addr & 0xff);

		for(i = 0; i < info->SenseChannelLength / 4 + 1;i++) {
			fts_write_reg(info, &regAdd[0], 4);
			fts_delay(1);

			retry = FTS_RETRY_COUNT * 3;
			do {
				if (retry < 0) {
					tsp_debug_err(true, &info->client->dev, "%s: failed to compare buf,[%x][%x][%x][%x] break3!\n",
								__func__, buf[1], buf[2], regAdd[1], regAdd[2]);

					break;
				}
				fts_delay(10);
				fts_read_reg(info, &r_addr, 1, &buf[0], FTS_EVENT_SIZE);
				retry--;
			} while (buf[1] != regAdd[1] || buf[2] != regAdd[2]);

			//read fifo
			rx_ix2[i*4] = buf[3];
			rx_ix2[i*4+1] = buf[4];
			rx_ix2[i*4+2] = buf[5];
			rx_ix2[i*4+3] = buf[6];

			ix2_rx_addr += 4;
			regAdd[0] = 0xB2;
			regAdd[1] = (ix2_rx_addr >>8)&0xff;
			regAdd[2] = (ix2_rx_addr & 0xff);
		}

		for(i = 0; i < info->ForceChannelLength; i++) {
			force_ix_data[i] = tx_ix1 + tx_ix2[i];
			if(max_tx_ix_sum < tx_ix1 + tx_ix2[i] )
				max_tx_ix_sum = tx_ix1 + tx_ix2[i];
			if(min_tx_ix_sum > tx_ix1 + tx_ix2[i] )
				min_tx_ix_sum = tx_ix1 + tx_ix2[i];
		}

		for(i = 0; i < info->SenseChannelLength; i++) {
			sense_ix_data[i] = rx_ix1 + rx_ix2[i];
			if(max_rx_ix_sum < rx_ix1 + rx_ix2[i] )
				max_rx_ix_sum = rx_ix1 + rx_ix2[i];
			if(min_rx_ix_sum > rx_ix1 + rx_ix2[i] )
				min_rx_ix_sum = rx_ix1 + rx_ix2[i];
		}
	}	else	{
		/* Request compensation data */
		regAdd[0] = 0xB8;
		regAdd[1] = 0x20;		// SELF IX
		regAdd[2] = 0x00;
		fts_write_reg(info, &regAdd[0], 3);
		fts_fw_wait_for_specific_event(info, EVENTID_STATUS_REQUEST_COMP, 0x20, 0x00);

		/* Read an address of compensation data */
		regAdd[0] = 0xD0;
		regAdd[1] = 0x00;
		regAdd[2] = 0x50;
		fts_read_reg(info, regAdd, 3, &buff[0], 4);
		comp_header_addr = buff[1] + (buff[2] << 8);

		/* Read header of compensation area */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_header_addr >> 8) & 0xFF;
		regAdd[2] = comp_header_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &buff[0], 16 + 1);
		tx_num = buff[5];
		rx_num = buff[6];

		tx_ix1 = (short) buff[10] * 4;		// Self TX Ix1
		rx_ix1 = (short) buff[11] * 2;		// Self RX Ix1

        comp_start_tx_addr = comp_header_addr + 0x10;
		comp_start_rx_addr = comp_start_tx_addr + tx_num;

		memset(tx_ix2, 0x0, tx_num);
		memset(rx_ix2, 0x0, rx_num);

		/* Read Self TX Ix2 */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_start_tx_addr >> 8) & 0xFF;
		regAdd[2] = comp_start_tx_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &tx_ix2[0], tx_num + 1);

		/* Read Self RX Ix2 */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_start_rx_addr >> 8) & 0xFF;
		regAdd[2] = comp_start_rx_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &rx_ix2[0], rx_num + 1);

		for(i = 0; i < info->ForceChannelLength; i++) {
			force_ix_data[i] = tx_ix1 + tx_ix2[i + 1];
			if(max_tx_ix_sum < force_ix_data[i])
				max_tx_ix_sum = force_ix_data[i];
			if(min_tx_ix_sum > force_ix_data[i])
				min_tx_ix_sum = force_ix_data[i];
		}

		for(i = 0; i < info->SenseChannelLength; i++) {
			sense_ix_data[i] = rx_ix1 + rx_ix2[i + 1];
			if(max_rx_ix_sum < sense_ix_data[i])
				max_rx_ix_sum = sense_ix_data[i];
			if(min_rx_ix_sum > sense_ix_data[i])
				min_rx_ix_sum = sense_ix_data[i];
		}
	}

	tsp_debug_info(true, &info->client->dev, "%s MIN_TX_IX_SUM : %d MAX_TX_IX_SUM : %d\n",
				__func__, min_tx_ix_sum, max_tx_ix_sum );
	tsp_debug_info(true, &info->client->dev, "%s MIN_RX_IX_SUM : %d MAX_RX_IX_SUM : %d\n",
				__func__, min_rx_ix_sum, max_rx_ix_sum );

	fts_systemreset(info);
	fts_wait_for_ready(info);

	if (info->stm_ver != STM_VER7)
	{
		fts_command(info, SLEEPOUT);
		fts_delay(1);
	}

	fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_WATER_MODE
	fts_fw_wait_for_event(info, STATUS_EVENT_WATER_SELF_DONE);
#else
	if( info->stm_ver == STM_VER7)
	{
		fts_fw_wait_for_event (info, STATUS_EVENT_FORCE_CAL_DONE_D3);
	}
	else
	{
		fts_fw_wait_for_event(info, STATUS_EVENT_FORCE_CAL_DONE);
	}
#endif
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

	enable_irq(info->irq);
	fts_interrupt_set(info, INT_ENABLE);

	if(allnode == true){
		buff_size = (info->ForceChannelLength + info->SenseChannelLength + 2)*5;
		mbuff = kzalloc(buff_size, GFP_KERNEL);
	}
	if (mbuff != NULL) {
		char *pBuf = mbuff;
		for(i = 0; i < info->ForceChannelLength; i++) {
			num =  force_ix_data[i];
			n = 100000;
			fzero = 0;
			for(j=5;j>0;j--){
				n = n/10;
				a = num/n;
				if(a) fzero = 1;
				cnum = a + '0';
				num  = num - a*n;
				if(fzero)*pBuf++ = cnum;
			}
			if(!fzero) *pBuf++ = '0';
			*pBuf++ = ',';
			tsp_debug_info(true, &info->client->dev, "%d ", force_ix_data[i]);
		}
		for(i = 0; i < info->SenseChannelLength; i++) {
			num =  sense_ix_data[i];
			n = 100000;
			fzero = 0;
			for(j=5;j>0;j--){
				n = n/10;
				a = num/n;
				if(a) fzero = 1;
				cnum = a + '0';
				num  = num - a*n;
				if(fzero)*pBuf++ = cnum;
			}
			if(!fzero) *pBuf++ = '0';
			if(i < (info->SenseChannelLength-1)) *pBuf++ = ',';
			tsp_debug_info(true, &info->client->dev, "%d ", sense_ix_data[i]);
		}

		set_cmd_result(info, mbuff, buff_size);
		info->cmd_state = CMD_STATUS_OK;
		kfree(mbuff);
	}
	else {
		if(allnode == true){	
		   snprintf(buff, sizeof(buff), "%s", "kzalloc failed");
		   info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		}
		else{
			snprintf(buff, sizeof(buff), "%d,%d,%d,%d", min_tx_ix_sum, max_tx_ix_sum, min_rx_ix_sum, max_rx_ix_sum);
			info->cmd_state = CMD_STATUS_OK;
		}
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	}
}

static void run_ix_data_read(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;

	set_default_result(info);
	fts_read_ix_data(info, false);
}

static void run_ix_data_read_all(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;

	set_default_result(info);
	fts_read_ix_data(info, true);
}

static void fts_read_self_raw_frame(struct fts_ts_info *info, unsigned short oAddr, bool allnode)
{
	char buff[CMD_STR_LEN*2] = { 0 };
	unsigned char D0_offset = 1;
	unsigned char regAdd[3] = {0xD0, 0x00, 0x00};
	unsigned char ReadData[info->SenseChannelLength * 2 + 1];
	unsigned short self_force_raw_data[info->ForceChannelLength * 2 + 1];
	unsigned short self_sense_raw_data[info->SenseChannelLength * 2 + 1];
	unsigned int FrameAddress = 0;
	unsigned char count=0;
	int buff_size,i,j;
	char *mbuff = NULL;
	int num,n,a,fzero;
	char cnum;
	unsigned short min_tx_self_raw_data = 0xFFFF;
	unsigned short max_tx_self_raw_data = 0;
	unsigned short min_rx_self_raw_data = 0xFFFF;
	unsigned short max_rx_self_raw_data = 0;
	unsigned char cmd[4] = { 0xB0, 0x01, 0x3D, 0x0C };	// Don't enter to IDLE for FTS7


	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_systemreset(info);
	fts_delay(50);
	fts_wait_for_ready(info);

	// Added self auto tune
	fts_command(info, SELF_AUTO_TUNE);
	fts_delay(300);
	fts_fw_wait_for_event_D3(info, STATUS_EVENT_SELF_AUTOTUNE_DONE_D3, 0x00);

	if (info->stm_ver != STM_VER7)
	{
		fts_command(info, SLEEPOUT);
		fts_delay(50);
	}
	else	// For STM7
	{
		fts_write_reg(info, &cmd[0], 4);
		fts_delay(50);
	}

	fts_command(info, SENSEON);
	fts_delay(50);

	disable_irq(info->irq);
	fts_interrupt_set(info, INT_DISABLE);
	//fts_command(info, SENSEOFF);


#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_OFF);
	}
#endif

	fts_command(info, FLUSHBUFFER);                 // Clear FIFO
	fts_delay(50);

	regAdd[1] = 0x00;
	regAdd[2] = oAddr;
	fts_read_reg(info, regAdd, 3, &ReadData[0], 4);

	FrameAddress = ReadData[D0_offset] + (ReadData[D0_offset + 1] << 8);           // D1 : DOFFSET = 0, D2 : DOFFSET : 1

	regAdd[1] = (FrameAddress >> 8) & 0xFF;
	regAdd[2] = FrameAddress & 0xFF;

	fts_read_reg(info, regAdd, 3, &ReadData[0], info->ForceChannelLength * 2 + 1);

	for(count = 0; count < info->ForceChannelLength; count++) {
		self_force_raw_data[count] = ReadData[count*2+D0_offset] + (ReadData[count*2+D0_offset+1]<<8);

		if(max_tx_self_raw_data < self_force_raw_data[count])
			max_tx_self_raw_data = self_force_raw_data[count];
		if(min_tx_self_raw_data > self_force_raw_data[count])
			min_tx_self_raw_data = self_force_raw_data[count];
	}

	regAdd[1] = 0x00;
	regAdd[2] = oAddr + 2;
	fts_read_reg(info, regAdd, 3, &ReadData[0], 4);

	FrameAddress = ReadData[D0_offset] + (ReadData[D0_offset + 1] << 8);           // D1 : DOFFSET = 0, D2 : DOFFSET : 1

	regAdd[1] = (FrameAddress >> 8) & 0xFF;
	regAdd[2] = FrameAddress & 0xFF;

	fts_read_reg(info, regAdd, 3, &ReadData[0], info->SenseChannelLength * 2 + 1);

	for(count = 0; count < info->SenseChannelLength; count++) {
		self_sense_raw_data[count] = ReadData[count*2+D0_offset] + (ReadData[count*2+D0_offset+1]<<8);

		if(max_rx_self_raw_data < self_sense_raw_data[count])
			max_rx_self_raw_data = self_sense_raw_data[count];
		if(min_rx_self_raw_data > self_sense_raw_data[count])
			min_rx_self_raw_data = self_sense_raw_data[count];
	}

	tsp_debug_info(true, &info->client->dev, "%s MIN_TX_SELF_RAW: %d MAX_TX_SELF_RAW : %d\n",
				__func__, min_tx_self_raw_data, max_tx_self_raw_data );
	tsp_debug_info(true, &info->client->dev, "%s MIN_RX_SELF_RAW : %d MIN_RX_SELF_RAW : %d\n",
				__func__, min_rx_self_raw_data, max_rx_self_raw_data );

	if (info->stm_ver != STM_VER7)
	{
		fts_command(info, SLEEPOUT);
		fts_delay(1);
	}

#ifdef FTS_SUPPORT_WATER_MODE
	fts_fw_wait_for_event(info, STATUS_EVENT_WATER_SELF_DONE);
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

	enable_irq(info->irq);
	fts_interrupt_set(info, INT_ENABLE);


	tsp_debug_info(true, &info->client->dev, "%s ForceChannelLength: %d    SenseChannelLength : %d\n",
				__func__, info->ForceChannelLength, info->SenseChannelLength );


	if(allnode == true){
		buff_size = (info->ForceChannelLength + info->SenseChannelLength + 2)*6;
		mbuff = kzalloc(buff_size, GFP_KERNEL);
	}
	if (mbuff != NULL) {
		char *pBuf = mbuff;
		for(i = 0; i < info->ForceChannelLength; i++) {
			num =  self_force_raw_data[i];
			n = 100000;
			fzero = 0;
			for(j=5;j>0;j--){
				n = n/10;
				a = num/n;
				if(a) fzero = 1;
				cnum = a + '0';
				num  = num - a*n;
				if(fzero)*pBuf++ = cnum;
			}
			if(!fzero) *pBuf++ = '0';
			*pBuf++ = ',';
			tsp_debug_info(true, &info->client->dev, "%d ", self_force_raw_data[i]);
		}
		for(i = 0; i < info->SenseChannelLength; i++) {
			num =  self_sense_raw_data[i];
			n = 100000;
			fzero = 0;
			for(j=5;j>0;j--){
				n = n/10;
				a = num/n;
				if(a) fzero = 1;
				cnum = a + '0';
				num  = num - a*n;
				if(fzero)*pBuf++ = cnum;
			}
			if(!fzero) *pBuf++ = '0';
			if(i < (info->SenseChannelLength-1)) *pBuf++ = ',';
			tsp_debug_info(true, &info->client->dev, "%d ", self_sense_raw_data[i]);
		}


		set_cmd_result(info, mbuff, buff_size);
		info->cmd_state = CMD_STATUS_OK;
		kfree(mbuff);
	}
	else {
		if(allnode == true){	
		   snprintf(buff, sizeof(buff), "%s", "kzalloc failed");
		   info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		}
		else{
			snprintf(buff, sizeof(buff), "%d,%d,%d,%d", min_tx_self_raw_data, max_tx_self_raw_data, min_rx_self_raw_data, max_rx_self_raw_data);
			info->cmd_state = CMD_STATUS_OK;
		}
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	}
}

static void run_self_raw_read(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;

	set_default_result(info);
	fts_read_self_raw_frame(info, FTS_WATER_SELF_RAW_ADDR,false);
}

static void run_self_raw_read_all(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;

	set_default_result(info);
	fts_read_self_raw_frame(info, FTS_WATER_SELF_RAW_ADDR,true);
}

static void run_trx_short_test(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int ret = 0;

	set_default_result(info);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	ret = fts_panel_ito_test(info);
	if (ret == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else
		snprintf(buff, sizeof(buff), "%s", "FAIL");

	info->cmd_state = CMD_STATUS_OK;
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
static void set_status_pureautotune(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int rc;
	unsigned char regAdd[2] = {0xC2, 0x0E};

	set_default_result(info);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_command(info, SENSEOFF);
	fts_delay(50);

	disable_irq(info->irq);
	fts_interrupt_set(info, INT_DISABLE);
	fts_delay(50);

	if (info->cmd_param[0] == 0)
		regAdd[0] = 0xC2;
	else
		regAdd[0] = 0xC1;

	tsp_debug_info(true, &info->client->dev, "%s: CMD[%2X]\n",__func__,regAdd[0]);

	rc = fts_write_reg(info, regAdd, 2);
	msleep(20);

	if (info->stm_ver != STM_VER7)
	{
		if (info->cmd_param[0] == 0)
			fts_fw_wait_for_event(info, STATUS_EVENT_PURE_AUTOTUNE_FLAG_ERASE_FINISH);
		else
			fts_fw_wait_for_event(info, STATUS_EVENT_PURE_AUTOTUNE_FLAG_WRITE_FINISH);
	}

	info->fts_command(info, FTS_CMD_SAVE_CX_TUNING);
	msleep(230);
	fts_fw_wait_for_event(info, STATUS_EVENT_FLASH_WRITE_CXTUNE_VALUE);

	info->fts_systemreset(info);
	fts_wait_for_ready(info);

	if (info->stm_ver != STM_VER7)
	{
		info->fts_command(info, SLEEPOUT);
		msleep(50);
	}
	info->fts_command(info, SENSEON);

	enable_irq(info->irq);
	fts_interrupt_set(info, INT_ENABLE);

	if (!rc)
		snprintf(buff, sizeof(buff), "%s", "FAIL");
	else
		snprintf(buff, sizeof(buff), "%s", "OK");

	info->cmd_state = CMD_STATUS_OK;
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_status_pureautotune(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int rc;
	unsigned char regAdd[3];
	unsigned char buf[5];
	int retVal = 0;

	set_default_result(info);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	regAdd[0] = 0xd0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x58;
	if (info->stm_ver == STM_VER7)
	{
		regAdd[2] = 0x4E;
	}

	rc = info->fts_read_reg(info, regAdd, 3, buf, 4);
	if (!rc)
	{
		tsp_debug_info(true, info->dev, "%s: PureAutotune Information Read Fail!! [Data : %2X%2X]\n", __func__, buf[1], buf[2]);
	}
	else
	{
	if((buf[1] == 0xA5) && (buf[2] == 0x96)) retVal = 1;
	tsp_debug_info(true, info->dev, "%s: PureAutotune Information !! [Data : %2X%2X]\n", __func__, buf[1], buf[2]);
	}
	snprintf(buff, sizeof(buff), "%d", retVal);
	info->cmd_state = CMD_STATUS_OK;
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_status_afe(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int rc = 0;
	unsigned char regAdd[3];
	unsigned char buf[5];
	int retVal = 0;

	set_default_result(info);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	regAdd[0] = 0xd0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x5A;
	if (info->stm_ver == STM_VER7)
	{
		regAdd[2] = 0x52;
	}

	rc = info->fts_read_reg(info, regAdd, 3, buf, 3);
	if (!rc)
	{
		tsp_debug_info(true, info->dev, "%s: Final AFE [Data : %2X] Read Fail AFE Ver [Data : %2X]\n", __func__, buf[1], buf[2]);
	}

	if( buf[1] ) retVal = 1;
	tsp_debug_info(true, info->dev, "%s: Final AFE [Data : %2X] AFE Ver [Data : %2X]\n", __func__, buf[1], buf[2]);

	snprintf(buff, sizeof(buff), "%d", retVal);
	info->cmd_state = CMD_STATUS_OK;
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
#endif


#define FTS_MAX_TX_LENGTH		44
#define FTS_MAX_RX_LENGTH		64

#define FTS_CX2_READ_LENGTH		4
#define FTS_CX2_ADDR_OFFSET		3
#define FTS_CX2_TX_START		0
#define FTS_CX2_BASE_ADDR		0x1000

#define FTS_WTR_TX_CX2_BASE_ADDR		0x1969
#define FTS_WTR_RX_CX2_BASE_ADDR		0x198A

static void get_cx_data(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	set_default_result(info);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = fts_check_index(info);
	if (node < 0)
		return;

	val = info->cx_data[node];
	snprintf(buff, sizeof(buff), "%d", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);

}

static void run_cx_data_read(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	unsigned char ReadData[info->ForceChannelLength][info->SenseChannelLength + FTS_CX2_READ_LENGTH];
	unsigned char regAdd[8];
	unsigned char buf[8];
	unsigned char r_addr = READ_ONE_EVENT;
	unsigned int addr, rx_num, tx_num;
	int i, j, cx_rx_length, max_tx_length, max_rx_length, address_offset = 0, start_tx_offset = 0, retry = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };

	int	comp_header_addr, comp_start_addr;

	set_default_result(info);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	pStr = kzalloc(4 * (info->SenseChannelLength + 1), GFP_KERNEL);
	if (pStr == NULL) {
		tsp_debug_info(true, &info->client->dev, "FTS pStr kzalloc failed\n");
		return;
	}

	tsp_debug_info(true, &info->client->dev, "%s: start \n", __func__);

	disable_irq(info->irq);
	fts_interrupt_set(info, INT_DISABLE);

	if (info->stm_ver == STM_VER7)
	{
		fts_command(info, SENSEOFF);
		fts_delay(50);
	}
	else
	{
		fts_systemreset(info);
		fts_delay(50);
		fts_wait_for_ready(info);

		fts_command(info, SLEEPOUT);
		fts_delay(50);
	}

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_OFF); // Key Sensor OFF
	}
#endif
	fts_command(info, FLUSHBUFFER);
	fts_delay(50);

	if (info->stm_ver != STM_VER7)
	{
		tx_num = info->ForceChannelLength;
		rx_num = info->SenseChannelLength;

		if (info->digital_rev == FTS_DIGITAL_REV_1) {
			max_tx_length = FTS_MAX_TX_LENGTH -4;
			max_rx_length = FTS_MAX_RX_LENGTH -4;
		} else {
			max_tx_length = FTS_MAX_TX_LENGTH;
			max_rx_length = FTS_MAX_RX_LENGTH;
		}

		start_tx_offset = FTS_CX2_TX_START * max_rx_length / FTS_CX2_READ_LENGTH * FTS_CX2_ADDR_OFFSET;
		address_offset = max_rx_length /FTS_CX2_READ_LENGTH;

		for(j = 0; j < tx_num; j++) {
			memset(pStr, 0x0, 4 * (rx_num + 1));
			snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", j);
			strncat(pStr, pTmp, 4 * rx_num);

			addr = FTS_CX2_BASE_ADDR + (j * address_offset * FTS_CX2_ADDR_OFFSET) + start_tx_offset;

			if(rx_num % FTS_CX2_READ_LENGTH != 0)
				cx_rx_length = rx_num / FTS_CX2_READ_LENGTH + 1;
			else
				cx_rx_length = rx_num / FTS_CX2_READ_LENGTH;

			for(i = 0; i < cx_rx_length; i++) {
				regAdd[0] = 0xB2;
				regAdd[1] = (addr >> 8) & 0xff;
				regAdd[2] = (addr & 0xff);
				regAdd[3] = 0x04;
				fts_write_reg(info, &regAdd[0], 4);

				retry = FTS_RETRY_COUNT * 3;
				do {
					if (retry < 0) {
						tsp_debug_err(true, &info->client->dev,
								"%s: failed to compare buf, break!\n", __func__);
						break;
					}

					fts_read_reg(info, &r_addr, 1, &buf[0], FTS_EVENT_SIZE);
					retry--;
				} while (buf[1] != regAdd[1] || buf[2] != regAdd[2]);

				ReadData[j][i * 4] = buf[3] & 0x3F;
				ReadData[j][i * 4 + 1] = (buf[3] & 0xC0) >> 6 | (buf[4] & 0x0F) << 2;
				ReadData[j][i * 4 + 2] = ((buf[4] & 0xF0)>> 4) | ((buf[5] & 0x03) << 4);
				ReadData[j][i * 4 + 3] = buf[5] >> 2;
				addr = addr + 3;

				snprintf(pTmp, sizeof(pTmp), "%3d%3d%3d%3d ",
			        ReadData[j][i*4], ReadData[j][i*4+1], ReadData[j][i*4+2], ReadData[j][i*4+3]);
				strncat(pStr, pTmp, 4 *rx_num);
			}

			tsp_debug_info(true, &info->client->dev, "FTS %s\n", pStr);
		}
	}	else	{
		/* Request compensation data */
		regAdd[0] = 0xB8;
		regAdd[1] = 0x04;		// MUTUAL CX (LPA)
		regAdd[2] = 0x00;
		fts_write_reg(info, &regAdd[0], 3);
		fts_fw_wait_for_specific_event(info, EVENTID_STATUS_REQUEST_COMP, 0x04, 0x00);

		/* Read an address of compensation data */
		regAdd[0] = 0xD0;
		regAdd[1] = 0x00;
		regAdd[2] = 0x50;
		fts_read_reg(info, regAdd, 3, &buff[0], 4);
		comp_header_addr = buff[1] + (buff[2] << 8);

		/* Read header of compensation area */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_header_addr >> 8) & 0xFF;
		regAdd[2] = comp_header_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &buff[0], 16 + 1);
		tx_num = buff[5];
		rx_num = buff[6];
		comp_start_addr = comp_header_addr + 0x10;

		/* Read compensation data */
		for (j = 0; j < tx_num; j++) {
			memset(&ReadData[j], 0x0, rx_num);
			memset(pStr, 0x0, 4 * (rx_num + 1));
			snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", j);
			strncat(pStr, pTmp, 4 * rx_num);

			addr = comp_start_addr + (rx_num * j);
			regAdd[0] = 0xD0;
			regAdd[1] = (addr >> 8) & 0xFF;
			regAdd[2] = addr & 0xFF;
			fts_read_reg(info, regAdd, 3, &ReadData[j][0], rx_num + 1);
			for (i = 0; i < rx_num; i++)
			{
				snprintf(pTmp, sizeof(pTmp), "%3d", ReadData[j][i]);
				strncat(pStr, pTmp, 4 * rx_num);
			}
			tsp_debug_info(true, &info->client->dev, "FTS %s\n", pStr);
		}
	}

	if (info->cx_data) {
		for (j = 0; j < tx_num; j++) {
			for(i = 0; i < rx_num; i++)
				info->cx_data[(j * rx_num) + i] = ReadData[j][i];
		}
	}

	kfree(pStr);

	snprintf(buff, sizeof(buff), "%s", "OK");
	enable_irq(info->irq);
	fts_interrupt_set(info, INT_ENABLE);
	fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
	}
#endif
	info->cmd_state = CMD_STATUS_OK;
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_cx_all_data(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	unsigned char ReadData[info->ForceChannelLength][info->SenseChannelLength + FTS_CX2_READ_LENGTH];
	unsigned char regAdd[8];
	unsigned char buf[8];
	unsigned char r_addr = READ_ONE_EVENT;
	unsigned int addr, rx_num, tx_num;
	int i, j, cx_rx_length, max_tx_length, max_rx_length, address_offset = 0, start_tx_offset = 0, retry = 0;
	char all_strbuff[(info->ForceChannelLength)*(info->SenseChannelLength)*3];

	set_default_result(info);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	tsp_debug_info(true, &info->client->dev, "%s: start \n", __func__);

	fts_command(info, SENSEOFF);
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_OFF); // Key Sensor OFF
	}
#endif
	disable_irq(info->irq);
	fts_command(info, FLUSHBUFFER);
	fts_delay(50);

	tx_num = info->ForceChannelLength;
	rx_num = info->SenseChannelLength;

	memset(all_strbuff,0,sizeof(char)*(tx_num*rx_num*3));	//size 3  ex(45,)

	if (info->digital_rev == FTS_DIGITAL_REV_1) {
		max_tx_length = FTS_MAX_TX_LENGTH -4;
		max_rx_length = FTS_MAX_RX_LENGTH -4;
	} else {
		max_tx_length = FTS_MAX_TX_LENGTH;
		max_rx_length = FTS_MAX_RX_LENGTH;
	}

	start_tx_offset = FTS_CX2_TX_START * max_rx_length / FTS_CX2_READ_LENGTH * FTS_CX2_ADDR_OFFSET;
	address_offset = max_rx_length /FTS_CX2_READ_LENGTH;

	for(j = 0; j < tx_num; j++) {
		addr = FTS_CX2_BASE_ADDR + (j * address_offset * FTS_CX2_ADDR_OFFSET) + start_tx_offset;

		if(rx_num % FTS_CX2_READ_LENGTH != 0)
			cx_rx_length = rx_num / FTS_CX2_READ_LENGTH + 1;
		else
			cx_rx_length = rx_num / FTS_CX2_READ_LENGTH;

		for(i = 0; i < cx_rx_length; i++) {
			regAdd[0] = 0xB2;
			regAdd[1] = (addr >> 8) & 0xff;
			regAdd[2] = (addr & 0xff);
			regAdd[3] = 0x04;
			fts_write_reg(info, &regAdd[0], 4);
			retry = FTS_RETRY_COUNT * 3;
			do {
				if (retry < 0) {
					tsp_debug_err(true, &info->client->dev,
							"%s: failed to compare buf, break!\n", __func__);
					break;
				}
				fts_read_reg(info, &r_addr, 1, &buf[0], FTS_EVENT_SIZE);
				retry--;
			} while (buf[1] != regAdd[1] || buf[2] != regAdd[2]);

			ReadData[j][i * 4] = buf[3] & 0x3F;
			ReadData[j][i * 4 + 1] = (buf[3] & 0xC0) >> 6 | (buf[4] & 0x0F) << 2;
			ReadData[j][i * 4 + 2] = ((buf[4] & 0xF0)>> 4) | ((buf[5] & 0x03) << 4);
			ReadData[j][i * 4 + 3] = buf[5] >> 2;
			addr = addr + 3;
		}
	}

	if (info->cx_data) {
		for (j = 0; j < tx_num; j++) {
			for(i = 0; i < rx_num; i++){
				info->cx_data[(j * rx_num) + i] = ReadData[j][i];
				sprintf(buff, "%d,", ReadData[j][i]);
				strcat(all_strbuff, buff);
			}
		}
	}

	enable_irq(info->irq);
	fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
	}
#endif
	info->cmd_state = CMD_STATUS_OK;
	set_cmd_result(info, all_strbuff, strnlen(all_strbuff, sizeof(all_strbuff)));
	tsp_debug_info(true, &info->client->dev, "%ld (%ld)\n", strnlen(all_strbuff, sizeof(all_strbuff)),sizeof(all_strbuff));
}

static void fts_read_wtr_cx_data(struct fts_ts_info *info, bool allnode)
{
	char buff[512] = { 0 };
	unsigned char regAdd[8];
	unsigned char *result_tx, *result_rx;
	char buf[8];
	unsigned char r_addr = READ_ONE_EVENT;
	unsigned int addr;
	int ii, kk = 0, retry;
	int tx_num, rx_num;

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	tsp_debug_info(true, &info->client->dev, "%s: ++\n", __func__);

	disable_irq(info->irq);
	fts_interrupt_set(info, INT_DISABLE);

	if (info->stm_ver == STM_VER7)
	{
		fts_command(info, SENSEOFF);
	}
	else
	{
		fts_systemreset(info);
		fts_delay(50);
		fts_wait_for_ready(info);

		fts_command(info, SLEEPOUT);
		fts_delay(50);
	}

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_OFF);
	}
#endif
	fts_command(info, FLUSHBUFFER);
	fts_delay(50);

	result_tx = kzalloc(info->ForceChannelLength + 10, GFP_KERNEL);
	result_rx = kzalloc(info->SenseChannelLength + 10, GFP_KERNEL);
	if (!result_tx || !result_rx) {
		tsp_debug_err(true, &info->client->dev, "%s: failed to alloc mem\n",
				__func__);

		snprintf(buff, sizeof(buff), "%s", "FAIL");
		goto err_alloc_mem;
	}

	if (info->stm_ver != STM_VER7)
	{
		if (info->ForceChannelLength % FTS_CX2_READ_LENGTH)
			tx_num = (info->ForceChannelLength / FTS_CX2_READ_LENGTH + 1);
		else
			tx_num = info->ForceChannelLength / FTS_CX2_READ_LENGTH;

		if (info->SenseChannelLength % FTS_CX2_READ_LENGTH)
			rx_num = (info->SenseChannelLength / FTS_CX2_READ_LENGTH + 1);
		else
			rx_num = info->SenseChannelLength / FTS_CX2_READ_LENGTH;

		for (ii = 0; ii < tx_num + rx_num; ii++) {

			if (ii < tx_num)
				addr = FTS_WTR_TX_CX2_BASE_ADDR + (ii * 3);
			else
				addr = FTS_WTR_RX_CX2_BASE_ADDR + ((ii - tx_num) * 3);

			if (ii == tx_num)
				kk = 0;

			regAdd[0] = 0xB2;
			regAdd[1] = (addr >> 8) & 0xFF;
			regAdd[2] = addr & 0xFF;
			regAdd[3] = 0x04;

			fts_write_reg(info, &regAdd[0], 4);

			retry = FTS_RETRY_COUNT * 3;
			do {
				if (retry < 0) {
					tsp_debug_err(true, &info->client->dev,
							"%s: failed to compare buf, break!\n", __func__);
					break;
				}

				memset(buf, 0x0, 8);

				fts_read_reg(info, &r_addr, 1, &buf[0], FTS_EVENT_SIZE);
				retry--;

			} while (buf[1] != regAdd[1] || buf[2] != regAdd[2]);

			tsp_debug_info(true, &info->client->dev,
					"%s: %d: [%04X] (%02X, %02X,) %02X, %02X, %02X, %02X, %02X, %02X\n",
					__func__, ii, addr, buf[0], buf[1], buf[2], buf[3],
					buf[4], buf[5], buf[6], buf[7]);

			if (ii < tx_num) {
				result_tx[kk++] = buf[3] & 0x3F;
				result_tx[kk++] = (buf[3] & 0xC0) >> 6 | (buf[4] & 0x0F) << 2;
				result_tx[kk++] = ((buf[4] & 0xF0)>> 4) | ((buf[5] & 0x03) << 4);
				result_tx[kk++] = buf[5] >> 2;
			} else {
				result_rx[kk++] = buf[3] & 0x3F;
				result_rx[kk++] = (buf[3] & 0xC0) >> 6 | (buf[4] & 0x0F) << 2;
				result_rx[kk++] = ((buf[4] & 0xF0)>> 4) | ((buf[5] & 0x03) << 4);
				result_rx[kk++] = buf[5] >> 2;
			}

		}
	}	else 	{
		int comp_header_addr, comp_start_tx_addr, comp_start_rx_addr;

		/* Request compensation data */
		regAdd[0] = 0xB8;
		regAdd[1] = 0x20;		// SELF IX
		regAdd[2] = 0x00;
		fts_write_reg(info, &regAdd[0], 3);
		fts_fw_wait_for_specific_event(info, EVENTID_STATUS_REQUEST_COMP, 0x20, 0x00);

		/* Read an address of compensation data */
		regAdd[0] = 0xD0;
		regAdd[1] = 0x00;
		regAdd[2] = 0x50;
		fts_read_reg(info, regAdd, 3, &buff[0], 4);
		comp_header_addr = buff[1] + (buff[2] << 8);

		/* Read header of compensation area */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_header_addr >> 8) & 0xFF;
		regAdd[2] = comp_header_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &buff[0], 16 + 1);
		tx_num = buff[5];
		rx_num = buff[6];

        comp_start_tx_addr = comp_header_addr + 0x10 + tx_num + rx_num;
		comp_start_rx_addr = comp_start_tx_addr + tx_num;

		memset(result_tx, 0x0, tx_num);
		memset(result_rx, 0x0, rx_num);

		memset(buff, 0x0, 512);
		/* Read Self TX Cx2 */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_start_tx_addr >> 8) & 0xFF;
		regAdd[2] = comp_start_tx_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &buff[0], tx_num + 1);

		for (ii = 0; ii < tx_num; ii++) {
			result_tx[ii] = buff[ii + 1];
		}

		memset(buff, 0x0, 512);
		/* Read Self RX Ix2 */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_start_rx_addr >> 8) & 0xFF;
		regAdd[2] = comp_start_rx_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &buff[0], rx_num + 1);

		for (ii = 0; ii < rx_num; ii++) {
			result_rx[ii] = buff[ii + 1];
		}
	}

	if (allnode) {
		char *data = 0; char temp[9];
		memset(buff, 0x0, 512);
		data = result_tx;
		for (ii = 0; ii < info->ForceChannelLength; ii++) {
			tsp_debug_info(true, &info->client->dev,
					"%s: Tx[%d] = %d\n", __func__, ii, *data);
			sprintf(temp, "%d,", *data);
			strncat(buff, temp, 9);
			data++;
		}

		data = result_rx;
		for (ii = 0; ii < info->SenseChannelLength; ii++) {
			tsp_debug_info(true, &info->client->dev,
					"%s: Rx[%d] = %d\n", __func__, ii, *data);
			sprintf(temp, "%d,", *data);
			strncat(buff, temp, 9);
			data++;
		}
	} else {
		unsigned char min_tx = 255, max_tx = 0, min_rx = 255, max_rx = 0;

		for (ii = 0; ii < info->ForceChannelLength; ii++) {
			tsp_debug_info(true, &info->client->dev,
					"%s: Tx[%d] = %d\n", __func__, ii, result_tx[ii]);

			min_tx = min(min_tx, result_tx[ii]);
			max_tx = max(max_tx, result_tx[ii]);
		}

		for (ii = 0; ii < info->SenseChannelLength; ii++) {
			tsp_debug_info(true, &info->client->dev,
					"%s: Rx[%d] = %d\n", __func__, ii, result_rx[ii]);
			min_rx = min(min_rx, result_rx[ii]);
			max_rx = max(max_rx, result_rx[ii]);
		}

		sprintf(buff, "%d,%d,%d,%d", min_tx, max_tx, min_rx, max_rx);

	}

	kfree(result_tx);
	kfree(result_rx);

err_alloc_mem:

	enable_irq(info->irq);
	fts_interrupt_set(info, INT_ENABLE);
	fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
	}
#endif
	info->cmd_state = CMD_STATUS_OK;
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);

}

static void run_wtr_cx_data_read(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;

	set_default_result(info);

	fts_read_wtr_cx_data(info, false);

}

static void run_wtr_cx_data_read_all(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;

	set_default_result(info);

	fts_read_wtr_cx_data(info, true);
}

#ifdef FTS_SUPPORT_TOUCH_KEY
#define USE_KEY_NUM 2
static void run_key_cx_data_read(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	unsigned char key_cx2_data[2];
	unsigned char ReadData[USE_KEY_NUM * FTS_CX2_READ_LENGTH];
	unsigned char regAdd[8];
	unsigned char buf[8];
	unsigned char r_addr = READ_ONE_EVENT;
	unsigned int addr;
	int i = 0, retry = 0;

	set_default_result(info);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
		            __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	disable_irq(info->irq);

	addr = FTS_CX2_BASE_ADDR;
	            
	regAdd[0] = 0xB2;
	regAdd[1] = (addr >> 8) & 0xff;
	regAdd[2] = (addr & 0xff);
	regAdd[3] = 0x04;

	fts_write_reg(info, &regAdd[0], 4);
	fts_delay(1);

	retry = FTS_RETRY_COUNT * 10;
	do {
		if (retry < 0) {
			tsp_debug_info(true, &info->client->dev,"%s: failed to compare buf, break!\n", __func__);
			break;
		}

		fts_read_reg(info, &r_addr, 1, &buf[0], FTS_EVENT_SIZE);
		retry--;
	} while (buf[1] != regAdd[1] || buf[2] != regAdd[2]);


	ReadData[i * 4] = buf[3] & 0x3F;
	ReadData[i * 4 + 1] = (buf[3] & 0xC0) >> 6 | (buf[4] & 0x0F) << 2;
	ReadData[i * 4 + 2] = ((buf[4] & 0xF0)>> 4) | ((buf[5] & 0x03) << 4);
	ReadData[i * 4 + 3] = buf[5] >> 2;

	key_cx2_data[0] = ReadData[2]; key_cx2_data[1] = ReadData[3]; 

	tsp_debug_info(true, &info->client->dev, "%s: [Key 1:%d][Key 2:%d]\n", __func__,
	            key_cx2_data[0], key_cx2_data[1]);

	//snprintf(buff, sizeof(buff), "%s", "OK");
	snprintf(buff, sizeof(buff), "%d,%d", key_cx2_data[0], key_cx2_data[1]);
	enable_irq(info->irq);

	info->cmd_state = CMD_STATUS_OK;
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
#endif

int fts_get_tsp_test_result(struct fts_ts_info *info)
{
	unsigned char cmd[3] = {0xD0, 0x00, 0x56};
	unsigned char buff[4] = { 0 };
	int ret;

	fts_systemreset(info);
	fts_wait_for_ready(info);

	ret = info->fts_read_reg(info, cmd, 3, &buff[0], 2);
	if (ret < 0) {
		tsp_debug_err(true, &info->client->dev,
			"%s failed. ret: %d\n", __func__, ret);
		ret = -EINVAL;
		goto out;
	}
	info->test_result.data[0] = buff[1];

out:
	fts_command(info, FLUSHBUFFER);
	fts_delay(10);
	info->fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif
	fts_interrupt_set(info, INT_ENABLE);

	return ret;
}
EXPORT_SYMBOL(fts_get_tsp_test_result);

static void get_tsp_test_result(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int ret;

	set_default_result(info);

	if (info->stm_ver != STM_VER7) {
		tsp_debug_info(true, &info->client->dev, "%s: not_support_cmd\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	ret = fts_get_tsp_test_result(info);
	if (ret < 0) {
		tsp_debug_info(true, &info->client->dev, "%s: failed to get result\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
			info->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (info->test_result.data[0] == 0xFF) {
			tsp_debug_info(true, &info->client->dev,
				"%s: clear factory_result as zero\n",
				__func__);
			info->test_result.data[0] = 0;
		}

		snprintf(buff, sizeof(buff), "M:%s, M:%d, A:%s, A:%d",
				info->test_result.module_result == 0 ? "NONE" :
				info->test_result.module_result == 1 ? "FAIL" : "PASS",
				info->test_result.module_count,
				info->test_result.assy_result == 0 ? "NONE" :
				info->test_result.assy_result == 1 ? "FAIL" : "PASS",
				info->test_result.assy_count);

		set_cmd_result(info, buff, strlen(buff));
		info->cmd_state = CMD_STATUS_OK;
	}
}

/* FACTORY TEST RESULT SAVING FUNCTION
 * bit 3 ~ 0 : OCTA Assy
 * bit 7 ~ 4 : OCTA module
 * param[0] : OCTA module(1) / OCTA Assy(2)
 * param[1] : TEST NONE(0) / TEST FAIL(1) / TEST PASS(2) : 2 bit
 */
static int fts_set_tsp_test_result(struct fts_ts_info *info)
{
	unsigned char regAdd[3] = {0xC7, 0x01, 0x00};
	int ret;

	tsp_debug_info(true, &info->client->dev, "%s: [0x%X] M:%s, M:%d, A:%s, A:%d\n",
		__func__, info->test_result.data[0],
		info->test_result.module_result == 0 ? "NONE" :
		info->test_result.module_result == 1 ? "FAIL" : "PASS",
		info->test_result.module_count,
		info->test_result.assy_result == 0 ? "NONE" :
		info->test_result.assy_result == 1 ? "FAIL" : "PASS",
		info->test_result.assy_count);

	regAdd[2] = info->test_result.data[0];

	ret = fts_write_reg(info, &regAdd[0], 3);
	if (ret < 0) {
		tsp_debug_err(true, &info->client->dev,
			"%s failed. ret: %d\n", __func__, ret);
		return -EINVAL;
	}

//	fts_delay(100);
	fts_command(info, FTS_CMD_SAVE_CX_TUNING);

	fts_delay(230);
	fts_fw_wait_for_event(info, STATUS_EVENT_FLASH_WRITE_CXTUNE_VALUE);

	return 0;
}

static void set_tsp_test_result(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int ret;

	set_default_result(info);

	if (info->stm_ver != STM_VER7) {
		tsp_debug_info(true, &info->client->dev, "%s: not_support_cmd\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	ret = fts_get_tsp_test_result(info);
	if (ret < 0) {
		tsp_debug_err(true, &info->client->dev, "%s: [ERROR] failed to get_tsp_test_result\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_FAIL;
		return;
	}
	info->cmd_state = CMD_STATUS_RUNNING;

	if (info->test_result.data[0] == 0xFF) {
		tsp_debug_info(true, &info->client->dev,
			"%s: clear factory_result as zero\n",
			__func__);
		info->test_result.data[0] = 0;
	}

	if (info->cmd_param[0] == TEST_OCTA_ASSAY) {
		info->test_result.assy_result = info->cmd_param[1];
		if (info->test_result.assy_count < 3)
			info->test_result.assy_count++;

	} else if (info->cmd_param[0] == TEST_OCTA_MODULE) {
		info->test_result.module_result = info->cmd_param[1];
		if (info->test_result.module_count < 3)
			info->test_result.module_count++;
	}

	ret = fts_set_tsp_test_result(info);
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
	snprintf(buff, sizeof(buff), "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;
	}

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#ifdef FTS_SUPPORT_HOVER
static void hover_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->touch_stopped || !(info->reinit_done) || (info->fts_power_state == FTS_POWER_STATE_LOWPOWER)) {
		tsp_debug_info(true, &info->client->dev,
			"%s: [ERROR] Touch is stopped:%d, reinit_done:%d, power_state:%d\n",
			__func__, info->touch_stopped, info->reinit_done, info->fts_power_state);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;

		if(info->cmd_param[0]==1){
			info->retry_hover_enable_after_wakeup = 1;
			tsp_debug_info(true, &info->client->dev, "%s: retry_hover_on_after_wakeup \n", __func__);
		}

		goto out;
	}

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		int enables;
		enables = info->cmd_param[0];
		if (enables == info->hover_enabled) {
			tsp_debug_dbg(true, &info->client->dev,
				"%s: Skip duplicate command. Hover is already %s.\n",
				__func__, info->hover_enabled ? "enabled" : "disabled");
		} else {
			if (enables) {
				unsigned char regAdd[4] = {0xB0, 0x01, 0x29, 0x41};
				unsigned char Dly_regAdd[4] = {0xB0, 0x01, 0x72, 0x04};
				fts_write_reg(info, &Dly_regAdd[0], 4);
				fts_write_reg(info, &regAdd[0], 4);
				fts_command(info, FTS_CMD_HOVER_ON);
				info->hover_enabled = true;
				info->hover_ready = false;
			} else {
				unsigned char Dly_regAdd[4] = {0xB0, 0x01, 0x72, 0x08};
				fts_write_reg(info, &Dly_regAdd[0], 4);
				fts_command(info, FTS_CMD_HOVER_OFF);
				info->hover_enabled = false;
				info->hover_ready = false;
			}
		}
		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_WAITING;

out:
	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

/* static void hover_no_sleep_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	unsigned char regAdd[4] = {0xB0, 0x01, 0x18, 0x00};
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (info->cmd_param[0]) {
			regAdd[3]=0x0F;
		} else {
			regAdd[3]=0x08;
		}
		fts_write_reg(info, &regAdd[0], 4);

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
} */
#endif

#ifdef CONFIG_GLOVE_TOUCH
static void glove_mode(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		info->mshover_enabled = info->cmd_param[0];

		if (!info->touch_stopped && info->reinit_done) {
			if (info->mshover_enabled)
				fts_command(info, FTS_CMD_MSHOVER_ON);
			else
				fts_command(info, FTS_CMD_MSHOVER_OFF);
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_WAITING;

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_glove_sensitivity(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	unsigned char cmd[4] =
		{ 0xB2, 0x01, 0xC6, 0x02 };
	int timeout=0;

	set_default_result(info);

	if (info->touch_stopped) {
		char buff[CMD_STR_LEN] = { 0 };
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_write_reg(info, &cmd[0], 4);
	info->cmd_state = CMD_STATUS_RUNNING;

	while (info->cmd_state == CMD_STATUS_RUNNING) {
		if (timeout++>30) {
			info->cmd_state = CMD_STATUS_FAIL;
			break;
		}
		msleep(10);
	}
}

static void fast_glove_mode(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		info->fast_mshover_enabled = info->cmd_param[0];

		if (!info->touch_stopped && info->reinit_done) {
			if (info->fast_mshover_enabled)
				fts_command(info, FTS_CMD_SET_FAST_GLOVE_MODE);
			else
				fts_command(info, FTS_CMD_SET_NOR_GLOVE_MODE);
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_WAITING;

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};
#endif

static void clear_cover_mode(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (info->cmd_param[0] > 1) {
			info->flip_enable = true;
			info->cover_type = info->cmd_param[1];
		} else {
			info->flip_enable = false;
		}

		if (!info->touch_stopped && info->reinit_done) {
			if (info->flip_enable) {
#ifdef CONFIG_GLOVE_TOUCH
				if (info->mshover_enabled
					&& (strncmp(info->board->project_name, "TB", 2) != 0))
					fts_command(info, FTS_CMD_MSHOVER_OFF);
#endif
				fts_set_cover_type(info, true);
			} else {
				fts_set_cover_type(info, false);
#ifdef CONFIG_GLOVE_TOUCH
				if (info->fast_mshover_enabled)
					fts_command(info, FTS_CMD_SET_FAST_GLOVE_MODE);
				else if (info->mshover_enabled)
					fts_command(info, FTS_CMD_MSHOVER_ON);
#endif
			}
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_WAITING;

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

static void report_rate(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 2) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (info->cmd_param[0] == REPORT_RATE_90HZ)
			fts_change_scan_rate(info, FTS_CMD_FAST_SCAN);
		else if (info->cmd_param[0] == REPORT_RATE_60HZ)
			fts_change_scan_rate(info, FTS_CMD_SLOW_SCAN);
		else if (info->cmd_param[0] == REPORT_RATE_30HZ)
			fts_change_scan_rate(info, FTS_CMD_USLOW_SCAN);

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_WAITING;

out:
	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static void interrupt_control(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		int enables;
		enables = info->cmd_param[0];
		if (enables)
			fts_irq_enable(info, true);
		else
			fts_irq_enable(info, false);

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_WAITING;

out:
	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
#endif

#if defined(CONFIG_INPUT_BOOSTER)
static void boost_level(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	unsigned char max_level = 4;

#ifdef CONFIG_INPUT_BOOSTER
	max_level = BOOSTER_LEVEL_MAX;
#endif

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] >= max_level) {
		snprintf(buff, sizeof(buff), "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
#ifdef CONFIG_INPUT_BOOSTER
		change_booster_level_for_tsp(info->cmd_param[0]);
#endif
		tsp_debug_dbg(false, &info->client->dev,
						"%s %d\n",
						__func__, info->cmd_param[0]);

		snprintf(buff, sizeof(buff), "OK");
		info->cmd_state = CMD_STATUS_OK;
	}

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = CMD_STATUS_WAITING;

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	return;
}
#endif

bool check_lowpower_mode(struct fts_ts_info *info)
{
	bool ret = 0;
	unsigned char flag = info->lowpower_flag & 0xFF;

	if (flag)
		ret = 1;

	tsp_debug_info(true, &info->client->dev,
		"%s: lowpower_mode flag : %d, ret:%d\n", __func__, flag, ret);

	if (flag & FTS_LOWP_FLAG_2ND_SCREEN)
		tsp_debug_info(true, &info->client->dev, "%s: 2nd screen on\n", __func__);
	if (flag & FTS_LOWP_FLAG_BLACK_UI)
		tsp_debug_info(true, &info->client->dev, "%s: swipe finger on\n", __func__);
	if (flag & FTS_LOWP_FLAG_QUICK_APP_ACCESS)
		tsp_debug_info(true, &info->client->dev, "%s: quick app cmd on\n", __func__);
	if (flag & FTS_LOWP_FLAG_DIRECT_INDICATOR)
		tsp_debug_info(true, &info->client->dev, "%s: direct indicator cmd on\n", __func__);
	if (flag & FTS_LOWP_FLAG_SPAY)
		tsp_debug_info(true, &info->client->dev, "%s: spay cmd on\n", __func__);
	if (flag & FTS_LOWP_FLAG_TEMP_CMD)
		tsp_debug_info(true, &info->client->dev, "%s: known cmd on\n", __func__);
	if (flag & FTS_LOWP_FLAG_GESTURE_WAKEUP)
		tsp_debug_info(true, &info->client->dev, "%s: 2finger gesture cmd on\n", __func__);

	return ret;

}

static void set_lpgw_mode(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		tsp_debug_err(true, &info->client->dev, "%s: Abnormal parameter:[%d]\n", __func__, info->cmd_param[0]);
	} else {
		if ((info->cmd_param[0] && (info->lowpower_flag & FTS_LOWP_FLAG_GESTURE_WAKEUP))
			|| (!info->cmd_param[0] && !(info->lowpower_flag & FTS_LOWP_FLAG_GESTURE_WAKEUP))) {
			tsp_debug_info(true, &info->client->dev, "%s: duplicate lpgw mode setting cmd:[0x%x], lowpower_flag:[0x%X]\n",
					__func__, info->cmd_param[0], info->lowpower_flag & FTS_LOWP_FLAG_GESTURE_WAKEUP);
		} else {
			if (info->cmd_param[0]) {
				if (info->fts_power_state == FTS_POWER_STATE_POWERDOWN || 
						info->fts_power_state == FTS_POWER_STATE_LOWPOWER ) {
					info->start_device(info);
					info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_GESTURE_WAKEUP;
					info->lowpower_mode = check_lowpower_mode(info);
					info->stop_device(info);
				} else {
					info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_GESTURE_WAKEUP;
					info->lowpower_mode = check_lowpower_mode(info);
				}
			} else {
				if (info->fts_power_state == FTS_POWER_STATE_LOWPOWER) {
					info->start_device(info);
					info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_GESTURE_WAKEUP);
					info->lowpower_mode = check_lowpower_mode(info);
					info->stop_device(info);
				} else {
					info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_GESTURE_WAKEUP);
					info->lowpower_mode = check_lowpower_mode(info);
				}
			}
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_lowpower_mode(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
	        snprintf(buff, sizeof(buff), "%s", "NG");
	        info->cmd_state = CMD_STATUS_FAIL;
	} else {
#ifdef FTS_SUPPORT_SIDE_GESTURE
		if (info->board->support_sidegesture)
			info->lowpower_mode = info->cmd_param[0];
#endif
		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

static void set_deepsleep_mode(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		info->deepsleep_mode = info->cmd_param[0];

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

static void set_wirelesscharger_mode(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	unsigned char regAdd[2] = {0xC2, 0x10};
	int rc;

	set_default_result(info);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		info->wirelesscharger_mode = info->cmd_param[0];

		if (info->wirelesscharger_mode ==0) regAdd[0] = 0xC2;
		else regAdd[0] = 0xC1;
		tsp_debug_info(true, &info->client->dev, "%s: CMD[%2X]\n",__func__,regAdd[0]);

		rc = fts_write_reg(info, regAdd, 2);

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

static void active_sleep_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
	/* To do here */
		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};


static void second_screen_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
	        snprintf(buff, sizeof(buff), "%s", "NG");
	        info->cmd_state = CMD_STATUS_FAIL;
	} else {
		if(info->cmd_param[0])
			info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_2ND_SCREEN;
		else
			info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_2ND_SCREEN);

#ifdef FTS_SUPPORT_SIDE_GESTURE
		if (info->board->support_sidegesture)
			info->lowpower_mode = check_lowpower_mode(info);
#endif

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_longpress_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	unsigned char regAdd[4] = {0xB0, 0x07, 0x10, 0x03};
	int ret;
	int bflag = 0;

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (info->cmd_param[0])
			bflag = 1;
		else
			bflag = 0;

		if (bflag)
			regAdd[3] = 0x03;
		else
			regAdd[3] = 0x02;

		ret = fts_write_reg(info, regAdd, 4);

		if (ret < 0)
			tsp_debug_err(true, &info->client->dev, "%s failed. ret: %d\n", __func__, ret);
		else
			tsp_debug_info(true, &info->client->dev, "%s: on/off:%d, ret: %d\n", __func__, bflag, ret);

		fts_delay(1);

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_sidescreen_x_length(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	/* TB Side screen area length  */
	unsigned char regAdd[4] = {0xB0, 0x07, 0x1C, 0xA0}; //default Side screen x length setting
	int ret;

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 0xA0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		regAdd[3] = info->cmd_param[0]; // Change Side screen x length

		ret = fts_write_reg(info, regAdd, 4);
		if (ret < 0)
			tsp_debug_err(true, &info->client->dev, "%s failed. ret: %d\n", __func__, ret);
		else
			tsp_debug_info(true, &info->client->dev, "%s: x length:%d, ret: %d\n", __func__, regAdd[3], ret);
		fts_delay(1);

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_tunning_data(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int i, ret;
	unsigned char regAdd[3] = {0xC6, 0x00, 0x00};

	set_default_result(info);
	memset(buff, 0, sizeof(buff));

	for (i = 0; i < 4; i++) {
		if (info->cmd_param[i]) {
			if (i == 0)
				regAdd[1] = 0x02;
			else if (i == 1)
				regAdd[1] = 0x03;
			else if (i == 2)
				regAdd[1] = 0x05;
			else
				regAdd[1] = 0x07;

			regAdd[2] = info->cmd_param[i];

			ret = fts_write_reg(info, regAdd, 3);
			if (ret < 0) {
				tsp_debug_err(true, &info->client->dev, "%s: write tunning data failed!\n", __func__);
				snprintf(buff, sizeof(buff), "%s", "NG");
				info->cmd_state = CMD_STATUS_FAIL;
				goto out;
			} else
				tsp_debug_dbg(true, &info->client->dev, "%s: write tunning data:%d, %d, %d, ret:%d\n", __func__,
					regAdd[0], regAdd[1], regAdd[2], ret);

			fts_delay(5);
		}
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;
out:
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_dead_zone(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	unsigned char regAdd[2] = {0xC4, 0x00};
	int ret;

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 6) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (info->cmd_param[0]==1)
			regAdd[1] = 0x01;	/* side edge top */
		else if (info->cmd_param[0]==2)
			regAdd[1] = 0x02;	/* side edge bottom */
		else if (info->cmd_param[0]==3)
			regAdd[1] = 0x03;	/* side edge All On */
		else if (info->cmd_param[0]==4)
			regAdd[1] = 0x04;	/* side edge Left Off */
		else if (info->cmd_param[0]==5)
			regAdd[1] = 0x05;	/* side edge Right Off */
		else if (info->cmd_param[0]==6)
			regAdd[1] = 0x06;	/* side edge All Off */
		else
			regAdd[1] = 0x0;	/* none	*/

		ret = fts_write_reg(info, regAdd, 2);

		if (ret < 0)
			tsp_debug_err(true, &info->client->dev, "%s failed. ret: %d\n", __func__, ret);
		else
			tsp_debug_info(true, &info->client->dev, "%s: reg:%d, ret: %d\n", __func__, info->cmd_param[0], ret);

		fts_delay(1);

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void dead_zone_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	unsigned char regAdd[2] = {0xC2, 0x0C};
	int ret;

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (info->cmd_param[0]==0) {
			regAdd[0] = 0xC1;	/* dead zone enable */
		} else {
			regAdd[0] = 0xC2;	/* dead zone disable */
		}

		ret = fts_write_reg(info, regAdd, 2);

		if (ret < 0)
			tsp_debug_err(true, &info->client->dev, "%s failed. ret: %d\n", __func__, ret);
		else
			tsp_debug_info(true, &info->client->dev, "%s: reg:%d, ret: %d\n", __func__, info->cmd_param[0], ret);

		fts_delay(1);

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void select_wakeful_edge(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	unsigned char regAdd[2] = {0xC2, 0x0F};
	int ret;

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (info->cmd_param[0]==0) {
			regAdd[0] = FTS_CMS_DISABLE_FEATURE;	/* right - C2, 0F */
			info->wakeful_edge_side = EDGEWAKE_RIGHT;
		} else {
			regAdd[0] = FTS_CMS_ENABLE_FEATURE;		/* left - C1, 0F */
			info->wakeful_edge_side = EDGEWAKE_LEFT;
		}
		ret = fts_write_reg(info, regAdd, 2);

		if (ret < 0)
			tsp_debug_err(true, &info->client->dev, "%s failed. ret: %d\n", __func__, ret);
		else
			tsp_debug_info(true, &info->client->dev, "%s: reg:%d, ret: %d\n", __func__, info->cmd_param[0], ret);

		fts_delay(1);

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_mainscreen_disable_cmd(struct fts_ts_info *info, bool on)
{
	int ret;
	unsigned char regAdd[2] = {0xC2, 0x07};
	
	if (on){
		regAdd[0] = 0xC1;				// main screen disable			
		info->mainscr_disable = true;
	}else{ 
		regAdd[0] = 0xC2;				// enable like normal			
		info->mainscr_disable = false;
	}
	
	ret = fts_write_reg(info, regAdd, 2);
	
	if (ret < 0)
		tsp_debug_err(true, &info->client->dev, "%s failed. ret: %d\n", __func__, ret);
	else
		tsp_debug_info(true, &info->client->dev, "%s: reg:%d, ret: %d\n", __func__, info->cmd_param[0], ret);
	fts_delay(1);
}

static void set_mainscreen_disable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 2) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (info->cmd_param[0]==1){
			set_mainscreen_disable_cmd(info, 1);
		}else{ 
			set_mainscreen_disable_cmd(info, 0);
		}
		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;			
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#ifdef SMARTCOVER_COVER
void change_smartcover_table(struct fts_ts_info *info)
{
	u8 i, j, k, h, temp, temp_sum;

	for(i=0; i<MAX_BYTE; i++)
		for(j=0; j<MAX_TX; j++)
			info->changed_table[j][i] = info->smart_cover[i][j];

	#if 1 // debug
	tsp_debug_info(true, &info->client->dev, "%s smart_cover value\n", __func__);
	for(i=0; i<MAX_BYTE; i++){
		pr_cont("[fts] ");
		for(j=0; j<MAX_TX; j++)
			pr_cont("%d ",info->smart_cover[i][j]);
		pr_cont("\n");
	}

	tsp_debug_info(true, &info->client->dev, "%s changed_table value\n", __func__);
	for(j=0; j<MAX_TX; j++){
		pr_cont("[fts] ");
		for(i=0; i<MAX_BYTE; i++)
			pr_cont("%d ",info->changed_table[j][i]);
		pr_cont("\n");
	}
	#endif

	tsp_debug_info(true, &info->client->dev, "%s %d\n", __func__, __LINE__);

	for(i=0; i<MAX_TX; i++)
		for(j=0; j<4; j++)
			info->send_table[i][j] = 0;
		tsp_debug_info(true, &info->client->dev, "%s %d\n", __func__, __LINE__);

	for(i=0; i<MAX_TX; i++){
		temp = 0;
		for(j=0; j<MAX_BYTE; j++)
			temp += info->changed_table[i][j];
		if(temp == 0 ) continue;

		for(k=0; k<4; k++){
			temp_sum = 0;
			for(h=0; h<8; h++){
				temp_sum += ((u8)(info->changed_table[i][h+8*k])) << (7-h);
			}
			info->send_table[i][k] = temp_sum;
		}

		tsp_debug_info(true, &info->client->dev, "i:%2d, %2X %2X %2X %2X \n", \
			i,info->send_table[i][0],info->send_table[i][1],info->send_table[i][2],info->send_table[i][3]);
	}
	tsp_debug_info(true, &info->client->dev, "%s %d\n", __func__, __LINE__);


}
void set_smartcover_mode(struct fts_ts_info *info, bool on)
{
	int ret;
	unsigned char regMon[2] = {0xC1, 0x0A};
	unsigned char regMoff[2] = {0xC2, 0x0A};

	if(on ==1){
		ret = fts_write_reg(info, regMon, 2);
		if (ret < 0)
			tsp_debug_err(true, &info->client->dev, "%s mode on failed. ret: %d\n", __func__, ret);
	}else{
		ret = fts_write_reg(info, regMoff, 2);
		if (ret < 0)
			tsp_debug_err(true, &info->client->dev, "%s mode off failed. ret: %d\n", __func__, ret);
	}
}
void set_smartcover_clear(struct fts_ts_info *info)
{
	int ret;
	unsigned char regClr[6] = {0xC5, 0xFF, 0x00, 0x00, 0x00, 0x00};

	ret = fts_write_reg(info, regClr, 6);
	if (ret < 0)
		tsp_debug_err(true, &info->client->dev, "%s data clear failed. ret: %d\n", __func__, ret);
}


void set_smartcover_data(struct fts_ts_info *info)
{
	int ret;
	u8 i, j;
	u8 temp=0;
	unsigned char regData[6] = {0xC5, 0x00, 0x00, 0x00, 0x00, 0x00};


	for(i=0; i<MAX_TX; i++){
		temp = 0;
		for(j=0; j<4; j++)
			temp += info->send_table[i][j];
		if(temp == 0 ) continue;

		regData[1] = i;

		for(j=0; j<4; j++)
			regData[2+j] = info->send_table[i][j];

		tsp_debug_info(true, &info->client->dev, "i:%2d, %2X %2X %2X %2X \n", \
			regData[1],regData[2],regData[3],regData[4], regData[5]);

		// data write
		ret = fts_write_reg(info, regData, 6);
		if (ret < 0)
			tsp_debug_err(true, &info->client->dev, "%s data write[%d] failed. ret: %d\n", __func__,i, ret);


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
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	u8 i, j, t;

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 6) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {

		if(info->cmd_param[0]==0){			// off

			set_smartcover_mode(info, 0);
			tsp_debug_info(true, &info->client->dev, "%s mode off, normal\n", __func__);

		} else if(info->cmd_param[0]==1){	// off, globe mode

			set_smartcover_mode(info, 0);
			tsp_debug_info(true, &info->client->dev, "%s mode off, globe mode\n", __func__);
#ifdef CONFIG_GLOVE_TOUCH
			if (info->fast_mshover_enabled)
				fts_command(info, FTS_CMD_SET_FAST_GLOVE_MODE);
			else if (info->mshover_enabled)
				fts_command(info, FTS_CMD_MSHOVER_ON);
#endif
		} else if(info->cmd_param[0]==3){	// on

			set_smartcover_clear(info);
			set_smartcover_data(info);
			tsp_debug_info(true, &info->client->dev, "%s data send\n", __func__);
			set_smartcover_mode(info, 1);
			tsp_debug_info(true, &info->client->dev, "%s mode on\n", __func__);

		} else if(info->cmd_param[0]==4){	// clear

			for(i=0; i<MAX_BYTE; i++)
				for(j=0; j<MAX_TX; j++)
					info->smart_cover[i][j] = 0;
			tsp_debug_info(true, &info->client->dev, "%s data clear\n", __func__);

		} else if(info->cmd_param[0]==5){	// data write

			if(info->cmd_param[1]<0 ||  info->cmd_param[1]>= 32){
				tsp_debug_info(true, &info->client->dev, "%s data tx size is over[%d]\n", \
					__func__,info->cmd_param[1]);
				snprintf(buff, sizeof(buff), "%s", "NG");
				info->cmd_state = CMD_STATUS_FAIL;
				goto fail;
			}
			tsp_debug_info(true, &info->client->dev, "%s data %2X, %2X, %2X\n", __func__, \
				info->cmd_param[1],info->cmd_param[2],info->cmd_param[3] );

			t = info->cmd_param[1];
			info->smart_cover[t][0] = (info->cmd_param[2]&0x80)>>7;
			info->smart_cover[t][1] = (info->cmd_param[2]&0x40)>>6;
			info->smart_cover[t][2] = (info->cmd_param[2]&0x20)>>5;
			info->smart_cover[t][3] = (info->cmd_param[2]&0x10)>>4;
			info->smart_cover[t][4] = (info->cmd_param[2]&0x08)>>3;
			info->smart_cover[t][5] = (info->cmd_param[2]&0x04)>>2;
			info->smart_cover[t][6] = (info->cmd_param[2]&0x02)>>1;
			info->smart_cover[t][7] = (info->cmd_param[2]&0x01);
			info->smart_cover[t][8] = (info->cmd_param[3]&0x80)>>7;
			info->smart_cover[t][9] = (info->cmd_param[3]&0x40)>>6;
			info->smart_cover[t][10] = (info->cmd_param[3]&0x20)>>5;
			info->smart_cover[t][11] = (info->cmd_param[3]&0x10)>>4;
			info->smart_cover[t][12] = (info->cmd_param[3]&0x08)>>3;
			info->smart_cover[t][13] = (info->cmd_param[3]&0x04)>>2;
			info->smart_cover[t][14] = (info->cmd_param[3]&0x02)>>1;
			info->smart_cover[t][15] = (info->cmd_param[3]&0x01);

		} else if(info->cmd_param[0]==6){	// data change

			change_smartcover_table(info);
			tsp_debug_info(true, &info->client->dev, "%s data change\n", __func__);

		} else {

			tsp_debug_info(true, &info->client->dev, "%s cmd[%d] not use\n", __func__, info->cmd_param[0] );
			snprintf(buff, sizeof(buff), "%s", "NG");
			info->cmd_state = CMD_STATUS_FAIL;
			goto fail;

		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
fail:
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};
#endif

static void set_rotation_status(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		int status = info->cmd_param[0] % 2;

		if (status)
			fts_enable_feature(info, FTS_FEATURE_DUAL_SIDE_GUSTURE, true);
		else
			fts_enable_feature(info, FTS_FEATURE_DUAL_SIDE_GUSTURE, false);

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#ifdef FTS_SUPPORT_STRINGLIB
static void scrub_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	unsigned short addr = FTS_CMD_STRING_ACCESS;
	int ret;
	char buff[CMD_STR_LEN] = { 0 };

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);

		snprintf(buff, sizeof(buff), "%s", "TSP turned off");

		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	set_default_result(info);

	if (info->cmd_param[0]) {
		info->fts_mode |= FTS_MODE_SCRUB;
		info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_BLACK_UI;

	} else {
		info->fts_mode &= ~FTS_MODE_SCRUB;
		info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_BLACK_UI);
	}

	ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
	if (ret < 0) {
		dev_err(&info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;

		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

out:
	info->lowpower_mode = check_lowpower_mode(info);

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void quick_app_access_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	unsigned short addr = FTS_CMD_STRING_ACCESS;
	int ret;
	char buff[CMD_STR_LEN] = { 0 };

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);

		snprintf(buff, sizeof(buff), "%s", "TSP turned off");

		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	set_default_result(info);

	if (info->cmd_param[0]) {
		info->fts_mode |= FTS_MODE_QUICK_APP_ACCESS;
		info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_QUICK_APP_ACCESS;

	} else {
		info->fts_mode &= ~FTS_MODE_QUICK_APP_ACCESS;
		info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_QUICK_APP_ACCESS);
	}

	ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
	if (ret < 0) {
		dev_err(&info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;

		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

out:
	info->lowpower_mode = check_lowpower_mode(info);

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void direct_indicator_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	unsigned short addr = FTS_CMD_STRING_ACCESS;
	int ret;
	char buff[CMD_STR_LEN] = { 0 };

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);

		snprintf(buff, sizeof(buff), "%s", "TSP turned off");

		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	set_default_result(info);

	if (info->cmd_param[0]) {
		info->fts_mode |= FTS_MODE_DIRECT_INDICATOR;
		info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_DIRECT_INDICATOR;

	} else {
		info->fts_mode &= ~FTS_MODE_DIRECT_INDICATOR;
		info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_DIRECT_INDICATOR);
	}

	ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
	if (ret < 0) {
		dev_err(&info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;

		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

out:
	info->lowpower_mode = check_lowpower_mode(info);

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void spay_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	unsigned short addr = FTS_CMD_STRING_ACCESS;
	int ret;
	char buff[CMD_STR_LEN] = { 0 };

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);

		snprintf(buff, sizeof(buff), "%s", "TSP turned off");

		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	set_default_result(info);

	if (info->cmd_param[0]) {
		info->fts_mode |= FTS_MODE_SPAY;
		info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_SPAY;
	} else {
		info->fts_mode &= ~FTS_MODE_SPAY;
		info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_SPAY);
	}

	ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
	if (ret < 0) {
		tsp_debug_info(true, &info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;

		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

out:
	info->lowpower_mode = check_lowpower_mode(info);

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void edge_swipe_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	unsigned short addr = FTS_CMD_STRING_ACCESS;
	int ret;
	char buff[CMD_STR_LEN] = { 0 };

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);

		snprintf(buff, sizeof(buff), "%s", "TSP turned off");

		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	set_default_result(info);

	if (info->cmd_param[0]) {
		info->fts_mode |= FTS_MODE_EDGE_SWIPE;
		info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_BLACK_UI;
	} else {
		info->fts_mode &= ~FTS_MODE_EDGE_SWIPE;
		info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_BLACK_UI);
	}

	ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
	if (ret < 0) {
		tsp_debug_info(true, &info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;

		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

out:
	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void aod_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	unsigned short addr = FTS_CMD_STRING_ACCESS;
	int ret;
	char buff[CMD_STR_LEN] = { 0 };

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);

		snprintf(buff, sizeof(buff), "%s", "TSP turned off");

		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	set_default_result(info);

	if (info->cmd_param[0]) {
		info->fts_mode |= FTS_MODE_EDGE_SWIPE;
		info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_BLACK_UI;
	} else {
		info->fts_mode &= ~FTS_MODE_EDGE_SWIPE;
		info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_BLACK_UI);
	}

	ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
	if (ret < 0) {
		tsp_debug_info(true, &info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;

		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

out:
	info->lowpower_mode = check_lowpower_mode(info);

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}


#endif

#ifdef FTS_SUPPROT_MULTIMEDIA
static void brush_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	unsigned char value;

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 2) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	}else {

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}

	if (info->brush_enable != (info->cmd_param[0]? true : false)){
			info->brush_enable = info->cmd_param[0] ? true : false;
	}
	tsp_debug_info(true, &info->client->dev, "%s: brush_enable[%d] \n", __func__, info->brush_enable);

	value = info->cmd_param[0] ? 0x01 : 0x00;
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}


static void velocity_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	unsigned char value;

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 2) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	}else {
		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}

	if (info->velocity_enable != (info->cmd_param[0]? true : false)){
			info->velocity_enable = info->cmd_param[0] ? true : false;
	}
	tsp_debug_info(true, &info->client->dev, "%s: velocity_enable[%d] \n", __func__, info->velocity_enable);

	value = info->cmd_param[0] ? 0x01 : 0x00;
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
#endif

#define	ORIENTATION_0	0
#define	ORIENTATION_90	1
#define	ORIENTATION_180	2
#define	ORIENTATION_270	3

static void lcd_orientation(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	}else {
		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}

	switch (info->cmd_param[0]) {
	case ORIENTATION_0:
		tsp_debug_info(true, &info->client->dev, "%s: Get 0 degree LCD orientation.\n", __func__);
		break;
	case ORIENTATION_90:
		tsp_debug_info(true, &info->client->dev, "%s: Get 90 degree LCD orientation.\n", __func__);
		break;
	case ORIENTATION_180:
		tsp_debug_info(true, &info->client->dev, "%s: Get 180 degree LCD orientation.\n", __func__);
		break;
	case ORIENTATION_270:
		tsp_debug_info(true, &info->client->dev, "%s: Get 270 degree LCD orientation.\n", __func__);
		break;
	default:
		tsp_debug_err(true, &info->client->dev, "%s: LCD orientation value error.\n", __func__);
		break;
	}

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void delay(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	info->delay_time = info->cmd_param[0];

	tsp_debug_info(true, &info->client->dev, "%s: delay time is %d\n", __func__, info->delay_time);
	snprintf(buff, sizeof(buff), "%d", info->delay_time);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void debug(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	info->debug_string = info->cmd_param[0];

	tsp_debug_info(true, &info->client->dev, "%s: command is %d\n", __func__, info->debug_string);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_autotune_enable(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	set_default_result(info);

	info->run_autotune = info->cmd_param[0];

	tsp_debug_info(true, &info->client->dev, "%s: command is %s\n",
			__func__, info->run_autotune ? "ENABLE" : "DISABLE");

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_force_calibration(void *device_data)
{
	struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	bool touch_on = false;
	set_default_result(info);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
	if(info->rawdata_read_lock == 1){
		tsp_debug_info(true, &info->client->dev, "%s: ramdump mode is runing, %d\n", __func__, info->rawdata_read_lock);
		goto autotune_fail;
	}
#endif
	if (info->touch_count > 0) {
		touch_on = true;
		tsp_debug_info(true, info->dev, "%s: finger on touch(%d)\n", __func__, info->touch_count);
	}

#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
	info->cmd_param[0] = 0;
	set_status_pureautotune(device_data);
	set_default_result(info);
#endif

	disable_irq(info->irq);

	if ( (info->digital_rev == FTS_DIGITAL_REV_2) || (info->digital_rev == FTS_DIGITAL_REV_3) ) {
		fts_interrupt_set(info, INT_DISABLE);

		fts_command(info, SENSEOFF);
		fts_delay(50);

#ifdef FTS_SUPPORT_TOUCH_KEY
		if (info->board->support_mskey) {
			fts_command(info, FTS_CMD_KEY_SENSE_OFF);
		}
#endif
		fts_command(info, FLUSHBUFFER);
		fts_delay(10);

		fts_release_all_finger(info);
#ifdef FTS_SUPPORT_TOUCH_KEY
		if (info->board->support_mskey)
			fts_release_all_key(info);
#endif
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
		fts_command(info,FTS_CMD_FORCE_AUTOTUNE);
		info->o_afe_ver = info->afe_ver;
#endif

		if (touch_on) {
			tsp_debug_info(true, info->dev, "%s: finger! do not run autotune\n", __func__);
		} else {
			tsp_debug_info(true, info->dev, "%s: run autotune\n", __func__);
			fts_execute_autotune(info);
		}

		if (info->stm_ver != STM_VER7)
		{
			fts_command(info, SLEEPOUT);
			fts_delay(1);
		}

		fts_command(info, SENSEON);

#ifdef FTS_SUPPORT_WATER_MODE
		fts_fw_wait_for_event(info, STATUS_EVENT_WATER_SELF_DONE);
#else
		if( info->stm_ver == STM_VER7)
		{
			fts_fw_wait_for_event (info, STATUS_EVENT_FORCE_CAL_DONE_D3);
		}
		else
		{
			fts_fw_wait_for_event(info, STATUS_EVENT_FORCE_CAL_DONE);
		}
#endif
#ifdef FTS_SUPPORT_TOUCH_KEY
		if (info->board->support_mskey)
			fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

		fts_interrupt_set(info, INT_ENABLE);
	}else {
		enable_irq(info->irq);
		tsp_debug_info(true, &info->client->dev, "%s: digital_rev not matched, %d\n", __func__, info->digital_rev);
		goto autotune_fail;
	}

	enable_irq(info->irq);
	if (touch_on) {
		snprintf(buff, sizeof(buff), "NG_FINGER_ON");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
		info->cmd_param[0] = 1;
		set_status_pureautotune(device_data);
		set_default_result(info);
#endif
		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;

autotune_fail:
	snprintf(buff, sizeof(buff), "%s", "NG");
	info->cmd_state = CMD_STATUS_FAIL;
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;
}

#ifdef FTS_SUPPORT_TOUCH_KEY
int read_touchkey_data(struct fts_ts_info *info, unsigned char type, unsigned int keycode)
{
	unsigned char pCMD[3] = { 0xD0, 0x00, 0x00};
	unsigned char buf[9] = { 0 };
	int i;
	int ret = 0;

	pCMD[2] = type;

	ret = fts_read_reg(info, &pCMD[0], 3, buf, 3);
	if (ret >= 0) {
		if (info->digital_rev == FTS_DIGITAL_REV_1) {
			pCMD[1] = buf[1];
			pCMD[2] = buf[0];
		}
		else {
			pCMD[1] = buf[2];
			pCMD[2] = buf[1];
		}
	} else
		return -1;

	ret = fts_read_reg(info, &pCMD[0], 3, buf, 9);
	if (ret < 0)
		return -2;

	for (i = 0 ; i < info->board->num_touchkey ; i++)
		if (info->board->touchkey[i].keycode == keycode) {
			if (info->digital_rev == FTS_DIGITAL_REV_1)
				return *(short *)&buf[(info->board->touchkey[i].value - 1) * 2];
			else
				return *(short *)&buf[(info->board->touchkey[i].value - 1) * 2 + 1];
		}

	return -3;
}

static ssize_t touchkey_recent_strength(struct device *dev,
				       struct device_attribute *attr, char *buf) {
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int value = 0;

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		return sprintf(buf, "%d\n", value);
	}

	value = read_touchkey_data(info, TYPE_TOUCHKEY_STRENGTH, KEY_RECENT);

	return sprintf(buf, "%d\n", value);
}

static ssize_t touchkey_back_strength(struct device *dev,
				       struct device_attribute *attr, char *buf) {
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int value = 0;

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		return sprintf(buf, "%d\n", value);
	}

	value = read_touchkey_data(info, TYPE_TOUCHKEY_STRENGTH, KEY_BACK);

	return sprintf(buf, "%d\n", value);
}

static ssize_t touchkey_recent_raw(struct device *dev,
				       struct device_attribute *attr, char *buf) {
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int value = 0;

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		return sprintf(buf, "%d\n", value);
	}

	value = read_touchkey_data(info, TYPE_TOUCHKEY_RAW, KEY_RECENT);

	return sprintf(buf, "%d\n", value);
}

static ssize_t touchkey_back_raw(struct device *dev,
				       struct device_attribute *attr, char *buf) {
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int value = 0;

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		return sprintf(buf, "%d\n", value);
	}

	value = read_touchkey_data(info, TYPE_TOUCHKEY_RAW, KEY_BACK);

	return sprintf(buf, "%d\n", value);
}

static ssize_t touchkey_threshold(struct device *dev,
				       struct device_attribute *attr, char *buf) {
	struct fts_ts_info *info = dev_get_drvdata(dev);
	unsigned char pCMD[3] = { 0xD0, 0x00, 0x00};
	int value;
	int ret = 0;

	value = -1;
	pCMD[2] = TYPE_TOUCHKEY_THRESHOLD;
	ret = fts_read_reg(info, &pCMD[0], 3, buf, 3);
	if (ret >= 0) {
		if (info->digital_rev == FTS_DIGITAL_REV_1)
			value = *(unsigned short *)&buf[0];
		else
			value = *(unsigned short *)&buf[1];
	}

	info->touchkey_threshold = value;
	return sprintf(buf, "%d\n", info->touchkey_threshold);
}

static ssize_t fts_touchkey_led_control(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int data, ret;

	ret = sscanf(buf, "%d", &data);
	tsp_debug_dbg(true, &info->client->dev, "%s, %d\n", __func__, data);

	if (ret != 1) {
		tsp_debug_err(true, &info->client->dev, "%s, %d err\n",
			__func__, __LINE__);
		return size;
	}

	if (data != 0 && data != 1) {
		tsp_debug_err(true, &info->client->dev, "%s wrong cmd %x\n",
			__func__, data);
		return size;
	}

	ret = info->board->led_power(info, (bool)data);
	if (ret) {
		tsp_debug_err(true, &info->client->dev, "%s: Error turn on led %d\n",
			__func__, ret);

		goto out;
	}
	msleep(30);

out:
	return size;
}

static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP, NULL, fts_touchkey_led_control);
static DEVICE_ATTR(touchkey_recent, S_IRUGO, touchkey_recent_strength, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, touchkey_back_strength, NULL);
static DEVICE_ATTR(touchkey_recent_raw, S_IRUGO, touchkey_recent_raw, NULL);
static DEVICE_ATTR(touchkey_back_raw, S_IRUGO, touchkey_back_raw, NULL);
static DEVICE_ATTR(touchkey_threshold, S_IRUGO, touchkey_threshold, NULL);

static struct attribute *sec_touchkey_factory_attributes[] = {
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_recent_raw.attr,
	&dev_attr_touchkey_back_raw.attr,
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_brightness.attr,
	NULL,
};

static struct attribute_group sec_touchkey_factory_attr_group = {
	.attrs = sec_touchkey_factory_attributes,
};
#endif

#endif
