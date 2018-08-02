#ifdef SEC_TSP_FACTORY_TEST

#define BUFFER_MAX			((256 * 1024) - 16)
#define READ_CHUNK_SIZE			128 // (2 * 1024) - 16

#ifdef PAT_CONTROL
/*---------------------------------------
	<<< apply to server >>>
	0x00 : no action
	0x01 : clear nv  
	0x02 : pat magic
	0x03 : rfu

	<<< use for temp bin >>>
	0x05 : forced clear nv & f/w update  before pat magic, eventhough same f/w
	0x06 : rfu
  ---------------------------------------*/
#define PAT_CONTROL_NONE  		0x00
#define PAT_CONTROL_CLEAR_NV 		0x01
#define PAT_CONTROL_PAT_MAGIC 		0x02
#define PAT_CONTROL_FORCE_UPDATE	0x05

#define PAT_COUNT_ZERO			0x00
#define PAT_MAX_LCIA			0x80
#define PAT_MAX_MAGIC			0xF5
#define PAT_MAGIC_NUMBER		0x83
#endif

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

#ifdef FTS_SUPPORT_TOUCH_KEY
enum {
	TYPE_TOUCHKEY_RAW	= 0x34,
	TYPE_TOUCHKEY_STRENGTH	= 0x36,
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
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void get_checksum_data(void *device_data);
static void run_reference_read(void *device_data);
static void get_reference(void *device_data);
static void run_rawcap_read(void *device_data);
static void get_rawcap(void *device_data);
static void run_delta_read(void *device_data);
static void get_delta(void *device_data);
static void get_pat_information(void *device_data);
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

static void set_status_pureautotune(void *device_data);
static void get_status_pureautotune(void *device_data);
static void get_status_afe(void *device_data);

static void get_cx_data(void *device_data);
static void run_cx_data_read(void *device_data);
static void get_cx_all_data(void *device_data);
static void get_strength_all_data(void *device_data);
#ifdef FTS_SUPPORT_TOUCH_KEY
static void run_key_cx_data_read(void *device_data);
#endif
static void set_tsp_test_result(void *device_data);
static void get_tsp_test_result(void *device_data);
static void increase_disassemble_count(void *device_data);
static void get_disassemble_count(void *device_data);
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

static void set_lpgw_mode(void *device_data);
static void set_lowpower_mode(void *device_data);
static void set_deepsleep_mode(void *device_data);
static void set_wirelesscharger_mode(void *device_data);
static void active_sleep_enable(void *device_data);
static void second_screen_enable(void *device_data);
static void set_tunning_data(void *device_data);
static void set_dead_zone(void *device_data);
static void dead_zone_enable(void *device_data);
static void select_wakeful_edge(void *device_data);

static void spay_enable(void *device_data);
static void aod_enable(void *device_data);

static void delay(void *device_data);
static void debug(void *device_data);

static void run_force_calibration(void *device_data);

static void set_mainscreen_disable(void *device_data);
static void set_rotation_status(void *device_data);

static void not_support_cmd(void *device_data);

static ssize_t fts_scrub_position(struct device *dev,
				struct device_attribute *attr, char *buf);
static ssize_t fts_edge_x_position(struct device *dev,
				struct device_attribute *attr, char *buf);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
//static void tui_mode_cmd(struct fts_ts_info *info);
#endif

static void lcd_orientation(void *device_data);

struct sec_cmd ft_commands[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_config_ver", get_config_ver),},
	{SEC_CMD("get_threshold", get_threshold),},
	{SEC_CMD("module_off_master", module_off_master),},
	{SEC_CMD("module_on_master", module_on_master),},
	{SEC_CMD("module_off_slave", not_support_cmd),},
	{SEC_CMD("module_on_slave", not_support_cmd),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_module_vendor", not_support_cmd),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("get_checksum_data", get_checksum_data),},
	{SEC_CMD("run_reference_read", run_reference_read),},
	{SEC_CMD("get_reference", get_reference),},
	{SEC_CMD("run_rawcap_read", run_rawcap_read),},
	{SEC_CMD("get_rawcap", get_rawcap),},
	{SEC_CMD("run_delta_read", run_delta_read),},
	{SEC_CMD("get_delta", get_delta),},
	{SEC_CMD("get_pat_information", get_pat_information),},
#ifdef FTS_SUPPORT_HOVER
	{SEC_CMD("run_abscap_read" , run_abscap_read),},
	{SEC_CMD("run_absdelta_read", run_absdelta_read),},
#endif
	{SEC_CMD("run_ix_data_read", run_ix_data_read),},
	{SEC_CMD("run_ix_data_read_all", run_ix_data_read_all),},
	{SEC_CMD("run_self_raw_read", run_self_raw_read),},
	{SEC_CMD("run_self_raw_read_all", run_self_raw_read_all),},
	{SEC_CMD("run_wtr_cx_data_read", run_wtr_cx_data_read),},
	{SEC_CMD("run_wtr_cx_data_read_all", run_wtr_cx_data_read_all),},
	{SEC_CMD("run_trx_short_test", run_trx_short_test),},
	{SEC_CMD("set_status_pureautotune", set_status_pureautotune),},
	{SEC_CMD("get_status_pureautotune", get_status_pureautotune),},
	{SEC_CMD("get_status_afe", get_status_afe),},
	{SEC_CMD("get_cx_data", get_cx_data),},
	{SEC_CMD("run_cx_data_read", run_cx_data_read),},
	{SEC_CMD("get_cx_all_data", get_cx_all_data),},
	{SEC_CMD("get_strength_all_data", get_strength_all_data),},
#ifdef FTS_SUPPORT_TOUCH_KEY
	{SEC_CMD("run_key_cx_data_read", run_key_cx_data_read),},
#endif
	{SEC_CMD("set_tsp_test_result", set_tsp_test_result),},
	{SEC_CMD("get_tsp_test_result", get_tsp_test_result),},
	{SEC_CMD("increase_disassemble_count", increase_disassemble_count),},
	{SEC_CMD("get_disassemble_count", get_disassemble_count),},
#ifdef FTS_SUPPORT_HOVER
	{SEC_CMD("hover_enable", hover_enable),},
#endif
	{SEC_CMD("hover_no_sleep_enable", not_support_cmd),},
#ifdef CONFIG_GLOVE_TOUCH
	{SEC_CMD("glove_mode", glove_mode),},
	{SEC_CMD("get_glove_sensitivity", get_glove_sensitivity),},
	{SEC_CMD("fast_glove_mode", fast_glove_mode),},
#endif
	{SEC_CMD("clear_cover_mode", clear_cover_mode),},
	{SEC_CMD("report_rate", report_rate),},
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	{SEC_CMD("interrupt_control", interrupt_control),},
#endif
	{SEC_CMD("set_lpgw_mode", set_lpgw_mode),},
	{SEC_CMD("set_lowpower_mode", set_lowpower_mode),},
	{SEC_CMD("set_deepsleep_mode", set_deepsleep_mode),},
	{SEC_CMD("set_wirelesscharger_mode", set_wirelesscharger_mode),},
	{SEC_CMD("active_sleep_enable", active_sleep_enable),},
	{SEC_CMD("second_screen_enable", second_screen_enable),},
	{SEC_CMD("set_tunning_data", set_tunning_data),},
	{SEC_CMD("set_dead_zone", set_dead_zone),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD("select_wakeful_edge", select_wakeful_edge),},
	{SEC_CMD("spay_enable", spay_enable),},
	{SEC_CMD("aod_enable", aod_enable),},
	{SEC_CMD("delay", delay),},
	{SEC_CMD("debug", debug),},
	{SEC_CMD("run_force_calibration", run_force_calibration),},
	{SEC_CMD("set_mainscreen_disable", set_mainscreen_disable),},
	{SEC_CMD("set_rotation_status", set_rotation_status),},
	{SEC_CMD("lcd_orientation", lcd_orientation),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static DEVICE_ATTR(scrub_pos, S_IRUGO, fts_scrub_position, NULL);
static DEVICE_ATTR(edge_pos, S_IRUGO, fts_edge_x_position, NULL);

static struct attribute *sec_touch_facotry_attributes[] = {
	&dev_attr_scrub_pos.attr,
	&dev_attr_edge_pos.attr,
	NULL,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_facotry_attributes,
};

static int fts_check_index(struct fts_ts_info *info)
{
	struct sec_cmd_data *sec = &info->sec;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int node;

	if (sec->cmd_param[0] < 0
		|| sec->cmd_param[0] >= info->SenseChannelLength
		|| sec->cmd_param[1] < 0
		|| sec->cmd_param[1] >= info->ForceChannelLength) {

		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, &info->client->dev, "%s: parameter error: %u,%u\n",
			   __func__, sec->cmd_param[0], sec->cmd_param[1]);
		node = -1;
		return node;
	}
	node = sec->cmd_param[1] * info->SenseChannelLength + sec->cmd_param[0];
	input_info(true, &info->client->dev, "%s: node = %d\n", __func__, node);
	return node;
}

static ssize_t fts_scrub_position(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

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

	input_info(true, &info->client->dev, "%s: %d %d %d\n",
				__func__, info->scrub_id, info->scrub_x, info->scrub_y);
	snprintf(buff, sizeof(buff), "%d %d %d", info->scrub_id, info->scrub_x, info->scrub_y);

	info->scrub_id = 0;
	info->scrub_x = 0;
	info->scrub_y = 0;

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buff);
}

static ssize_t fts_edge_x_position(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
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

	edge_position_left = info->board->grip_area;
	edge_position_right = info->board->max_x + 1 - info->board->grip_area;

	input_info(true, &info->client->dev, "%s: %d,%d\n", __func__, edge_position_left, edge_position_right);
	snprintf(buff, sizeof(buff), "%d,%d", edge_position_left, edge_position_right);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buff);
}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
/*
static void tui_mode_cmd(struct fts_ts_info *info)
{
	struct sec_cmd_data *sec = &info->sec;
	char buff[16] = "TUImode:FAIL";

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
*/
#endif

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: \"%s\"\n", __func__, buff);
}

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[64] = { 0 };
	int retval = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	retval = fts_fw_update_on_hidden_menu(info, sec->cmd_param[0]);

	if (retval < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, &info->client->dev, "%s: failed [%d]\n", __func__, retval);
	} else {
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &info->client->dev, "%s: success [%d]\n", __func__, retval);
	}

	return;
}

static int fts_get_channel_info(struct fts_ts_info *info)   // Need to change function for sysinfo
{
	int rc = -1;
	unsigned char data[FTS_EVENT_SIZE] = { 0 };

	memset(data, 0x0, FTS_EVENT_SIZE);

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, FLUSHBUFFER);

	// Read Sense Channel
	rc = info->fts_get_sysinfo_data(info, FTS_SI_SENSE_CH_LENGTH, 3, data);
	if (rc <= 0) {
		info->SenseChannelLength = 0;
		input_info(true, info->dev, "%s: Get channel info Read Fail!! [Data : %2X]\n", __func__, data[0]);
		return rc;
	}
	info->SenseChannelLength = data[0];

	// Read Force Channel
	rc = info->fts_get_sysinfo_data(info, FTS_SI_FORCE_CH_LENGTH, 3, data);
	if (rc <= 0) {
		info->ForceChannelLength = 0;
		input_info(true, info->dev, "%s: Get channel info Read Fail!! [Data : %2X]\n", __func__, data[0]);
		return rc;
	}
	info->ForceChannelLength = data[0];

	fts_interrupt_set(info, INT_ENABLE);

	return rc;
}

void fts_print_frame(struct fts_ts_info *info, short *min, short *max)
{
	int i = 0;
	int j = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };

	pStr = kzalloc(6 * (info->SenseChannelLength + 1), GFP_KERNEL);
	if (pStr == NULL) {
		input_info(true, &info->client->dev, "FTS pStr kzalloc failed\n");
		return;
	}

	memset(pStr, 0x0, 6 * (info->SenseChannelLength + 1));
	snprintf(pTmp, sizeof(pTmp), "    ");
	strncat(pStr, pTmp, 6 * info->SenseChannelLength);

	for (i = 0; i < info->SenseChannelLength; i++) {
		snprintf(pTmp, sizeof(pTmp), "Rx%02d  ", i);
		strncat(pStr, pTmp, 6 * info->SenseChannelLength);
	}

	input_info(true, &info->client->dev, "FTS %s\n", pStr);
	memset(pStr, 0x0, 6 * (info->SenseChannelLength + 1));
	snprintf(pTmp, sizeof(pTmp), " +");
	strncat(pStr, pTmp, 6 * info->SenseChannelLength);

	for (i = 0; i < info->SenseChannelLength; i++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strncat(pStr, pTmp, 6 * info->SenseChannelLength);
	}

	input_info(true, &info->client->dev, "FTS %s\n", pStr);

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
		input_info(true, &info->client->dev, "FTS %s\n", pStr);
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
		input_info(true, &info->client->dev, "FTS pRead kzalloc failed\n");
		rc = 1;
		goto ErrorExit;
	}

	pFrameAddress[2] = type;
	totalbytes = info->SenseChannelLength * info->ForceChannelLength * 2;
	ret = fts_read_reg(info, &pFrameAddress[0], 3, pRead, pFrameAddress[3]);

	if (ret >= 0) {
		FrameAddress = pRead[1] + (pRead[2] << 8);

		start_addr = FrameAddress+info->SenseChannelLength * 2;
		end_addr = start_addr + totalbytes;
	} else {
		input_info(true, &info->client->dev, "FTS read failed rc = %d \n", ret);
		rc = 2;
		goto ErrorExit;
	}

#ifdef DEBUG_MSG
	input_info(true, &info->client->dev, "FTS FrameAddress = %X \n", FrameAddress);
	input_info(true, &info->client->dev, "FTS start_addr = %X, end_addr = %X \n", start_addr, end_addr);
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
		input_info(true, &info->client->dev, "FTS %02X%02X%02X readbytes=%d\n",
			   pFrameAddress[0], pFrameAddress[1],
			   pFrameAddress[2], readbytes);

#endif

		fts_read_reg(info, &pFrameAddress[0], 3, pRead, readbytes + 1);
		remained -= readbytes;

		for (i = 1; i < (readbytes+1); i += 2) {
			info->pFrame[dataposition++] =
			pRead[i] + (pRead[i + 1] << 8);
		}

	}
	kfree(pRead);

#ifdef DEBUG_MSG
	input_info(true, &info->client->dev,
		   "FTS writeAddr = %X, start_addr = %X, end_addr = %X \n",
		   writeAddr, start_addr, end_addr);
#endif

	switch (type) {
	case TYPE_RAW_DATA:
		input_info(true, &info->client->dev, "FTS [Raw Data : 0x%X%X] \n", pFrameAddress[0],
			FrameAddress);
		break;
	case TYPE_FILTERED_DATA:
		input_info(true, &info->client->dev, "FTS [Filtered Data : 0x%X%X] \n",
			pFrameAddress[0], FrameAddress);
		break;
	case TYPE_STRENGTH_DATA:
		input_info(true, &info->client->dev, "FTS [Strength Data : 0x%X%X] \n",
			pFrameAddress[0], FrameAddress);
		break;
	case TYPE_BASELINE_DATA:
		input_info(true, &info->client->dev, "FTS [Baseline Data : 0x%X%X] \n",
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
	int retry = 0;
	int result = -1;

	fts_systemreset(info);
	fts_wait_for_ready(info);

	disable_irq(info->irq);
	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, FLUSHBUFFER);

	fts_command(info, 0xA7);    // ITO test command
	fts_delay(200);
	memset(data, 0x0, FTS_EVENT_SIZE);
	while (fts_read_reg(info, &cmd, 1, (unsigned char *)data, FTS_EVENT_SIZE)) {
		if ((data[0] == 0x0F) && (data[1] == 0x05)) {
			switch (data[2]) {
			case 0x00 :
				result = 0;
				break;
			case 0x01 :
				input_info(true, &info->client->dev, "[FTS] ITO Test result : Force channel [%d] open.\n",
					data[3]);
				break;
			case 0x02 :
				input_info(true, &info->client->dev, "[FTS] ITO Test result : Sense channel [%d] open.\n",
					data[3]);
				break;
			case 0x03 :
				input_info(true, &info->client->dev, "[FTS] ITO Test result : Force channel [%d] short to GND.\n",
					data[3]);
				break;
			case 0x04 :
				input_info(true, &info->client->dev, "[FTS] ITO Test result : Sense channel [%d] short to GND.\n",
					data[3]);
				break;
			case 0x05 :
				input_info(true, &info->client->dev, "[FTS] ITO Test result : Force channel [%d] short to VDD.\n",
					data[3]);
				break;
			case 0x06 :
				input_info(true, &info->client->dev, "[FTS] ITO Test result : Sense channel [%d] short to VDD.\n",
					data[3]);
				break;
			case 0x07 :
				input_info(true, &info->client->dev, "[FTS] ITO Test result : Force channel [%d] short to force.\n",
					data[3]);
				break;
			case 0x08 :
				input_info(true, &info->client->dev, "[FTS] ITO Test result : Sennse channel [%d] short to sense.\n",
					data[3]);
				break;
			default:
				break;
			}

			break;
		}

		if (retry++ > 30) {
			input_info(true, &info->client->dev, "Time over - wait for result of ITO test\n");
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
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	sprintf(buff, "ST%02X%04X",
			info->panel_revision,
			info->fw_main_version_of_bin);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	sprintf(buff, "ST%02X%04X",
			info->panel_revision,
			info->fw_main_version_of_ic);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_config_ver(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[20] = { 0 };

	snprintf(buff, sizeof(buff), "%s_ST_%04X",
		info->board->model_name ?: info->board->project_name ?: "STM",
		info->config_version_of_ic);

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_threshold(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	unsigned char buff[16] = { 0 };
	unsigned char data[5] = { 0 };
	unsigned short finger_threshold = 0;
	int rc;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	sec->cmd_state = SEC_CMD_STATUS_RUNNING;
	rc = info->fts_get_sysinfo_data(info, FTS_SI_FINGER_THRESHOLD, 4, data);
	if (rc <= 0) {
		input_info(true, info->dev, "%s: Get threshold Read Fail!! [Data : %2X%2X]\n", __func__, data[0], data[1]);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	finger_threshold = (unsigned short)(data[0] + (data[1] << 8));

	snprintf(buff, sizeof(buff), "%d", finger_threshold);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", "get_threshold", buff);
}

static void module_off_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[3] = { 0 };
	int ret = 0;

	if (info->tsp_enabled) {
		disable_irq(info->irq);
		info->tsp_enabled = false;
	}

	if (info->board->power)
		info->board->power(info, false);
	else
		ret = 1;

	if (ret == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else
		snprintf(buff, sizeof(buff), "%s", "NG");

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (strncmp(buff, "OK", 2) == 0)
		sec->cmd_state = SEC_CMD_STATUS_OK;
	else
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void module_on_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[3] = { 0 };
	int ret = 0;

	if (!info->tsp_enabled) {
		enable_irq(info->irq);
		info->tsp_enabled = true;
	}

	if (info->board->power)
		info->board->power(info, true);
	else
		ret = 1;

	if (ret == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else
		snprintf(buff, sizeof(buff), "%s", "NG");

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (strncmp(buff, "OK", 2) == 0)
		sec->cmd_state = SEC_CMD_STATUS_OK;
	else
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };
	strncpy(buff, "STM", sizeof(buff));
	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };

	memcpy(buff, info->firmware_name + 7, 9);

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", info->SenseChannelLength);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", info->ForceChannelLength);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_checksum_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };
	int rc;
	unsigned char data[6] = { 0 };

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_interrupt_set(info, INT_DISABLE);

	rc = info->fts_get_sysinfo_data(info, FTS_SI_CONFIG_CHECKSUM, 5, data);
	if (rc <= 0) {
		input_info(true, info->dev, "%s: Get checksum data Read Fail!! [Data : %2X%2X%2X%2X]\n", __func__, data[1], data[0], data[3], data[2]);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	fts_interrupt_set(info, INT_ENABLE);

	snprintf(buff, sizeof(buff), "%02X%02X%02X%02X", data[1], data[0], data[3], data[2]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_reference_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_read_frame(info, TYPE_BASELINE_DATA, &min, &max);
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_reference(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = fts_check_index(info);
	if (node < 0)
		return;

	val = info->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_rawcap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_read_frame(info, TYPE_FILTERED_DATA, &min, &max);
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_rawcap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = fts_check_index(info);
	if (node < 0)
		return;

	val = info->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_delta_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_read_frame(info, TYPE_STRENGTH_DATA, &min, &max);
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_strength_all_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;
	char all_strbuff[(info->ForceChannelLength)*(info->SenseChannelLength)*5];
	int i, j;

	memset(all_strbuff,0,sizeof(char)*((info->ForceChannelLength)*(info->SenseChannelLength)*5));	//size 5  ex(1125,)

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_read_frame(info, TYPE_STRENGTH_DATA, &min, &max);


	for (i = 0; i < info->ForceChannelLength; i++) {
		for (j = 0; j < info->SenseChannelLength; j++) {

			sprintf(buff, "%d,", info->pFrame[(i * info->SenseChannelLength) + j]);
			strcat(all_strbuff, buff);
		}
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, all_strbuff, strnlen(all_strbuff, sizeof(all_strbuff)));
	input_info(true, &info->client->dev, "%ld (%ld)\n", strnlen(all_strbuff, sizeof(all_strbuff)),sizeof(all_strbuff));
}

static void get_delta(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = fts_check_index(info);
	if (node < 0)
		return;

	val = info->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_pat_information(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[22] = { 0 };
	sec_cmd_set_default_result(sec);

#ifdef PAT_CONTROL
	get_pat_data(info);
#endif
	/* fixed tune version will be saved at excute autotune */
	sprintf(buff, "P%02XT%04X",
		info->cal_count,info->tune_fix_ver);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
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
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (!info->hover_enabled) {
		input_info(true, &info->client->dev, "%s: [ERROR] Hover is disabled\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP Hover disabled");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	while (!info->hover_ready) {
		if (retry++ > 500) {
			input_info(true, &info->client->dev, "%s: [FTS] Timeout - Abs Raw Data Ready Event\n",
					  __func__);
			break;
		}
		fts_delay(10);
	}

	regAdd[1] = (oAddr >> 8) & 0xff;
	regAdd[2] = oAddr & 0xff;
	rc = info->fts_read_reg(info, &regAdd[0], 3, (unsigned char *)&buff[0], 5);
	if (rc <= 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	input_info(true, &info->client->dev, "%s: Force Address : %02x%02x\n",
			__func__, buff[2], buff[1]);
	input_info(true, &info->client->dev, "%s: Sense Address : %02x%02x\n",
			__func__, buff[4], buff[3]);
	regAdd[1] = buff[4];
	regAdd[2] = buff[3];
	regAdd[4] = buff[2];
	regAdd[5] = buff[1];

	rc = info->fts_read_reg(info, &regAdd[0], 3,
							(unsigned char *)&buff[0],
							info->SenseChannelLength * 2 + 1);
	if (rc <= 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	data = (short *)&buff[1];

	memset(temp, 0x00, ARRAY_SIZE(temp));
	memset(temp2, 0x00, ARRAY_SIZE(temp2));

	for (i = 0; i < info->SenseChannelLength; i++) {
		input_info(true, &info->client->dev,
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
	if (rc <= 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	data = (short *)&buff[1];

	for (i = 0; i < info->ForceChannelLength; i++) {
		input_info(true, &info->client->dev,
				"%s: Tx [%d] = %d\n", __func__, i, *data);
		sprintf(temp, "%d,", *data);
		strncat(temp2, temp, 9);
		data++;
	}

	sec_cmd_set_cmd_result(sec, temp2, strnlen(temp2, sizeof(temp2)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void run_abscap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_self_frame(info, 0x000E);
}

static void run_absdelta_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_self_frame(info, 0x0012);
}
#endif

#define FTS_WATER_SELF_RAW_ADDR	0x1A

static void fts_read_ix_data(struct fts_ts_info *info, bool allnode)
{
	struct sec_cmd_data *sec = &info->sec;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	unsigned short max_tx_ix_sum = 0;
	unsigned short min_tx_ix_sum = 0xFFFF;

	unsigned short max_rx_ix_sum = 0;
	unsigned short min_rx_ix_sum = 0xFFFF;

	unsigned char tx_ix2[info->ForceChannelLength + 4];
	unsigned char rx_ix2[info->SenseChannelLength + 4];

	unsigned char regAdd[FTS_EVENT_SIZE];
	unsigned short tx_ix1 = 0, rx_ix1 = 0;

	unsigned short force_ix_data[info->ForceChannelLength * 2 + 1];
	unsigned short sense_ix_data[info->SenseChannelLength * 2 + 1];
	int buff_size,j;
	char *mbuff = NULL;
	int num,n,a,fzero;
	char cnum;
	int i = 0;
	int comp_header_addr, comp_start_tx_addr, comp_start_rx_addr;
	unsigned int rx_num, tx_num;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
		       __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	disable_irq(info->irq);
	fts_interrupt_set(info, INT_DISABLE);

	fts_command(info, SENSEOFF);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_OFF);
	}
#endif

	fts_command(info, FLUSHBUFFER);                 // Clear FIFO
	fts_delay(50);

	/* Request compensation data */
	regAdd[0] = 0xB8;
	regAdd[1] = 0x20;		// SELF IX
	regAdd[2] = 0x00;
	fts_write_reg(info, &regAdd[0], 3);
	fts_fw_wait_for_specific_event(info, EVENTID_STATUS_REQUEST_COMP, 0x20, 0x00);

	/* Read an address of compensation data */
	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = FTS_SI_COMPENSATION_OFFSET_ADDR;
	fts_read_reg(info, regAdd, 3, &buff[0], 4);
	comp_header_addr = buff[1] + (buff[2] << 8);

	/* Read header of compensation area */
	regAdd[0] = 0xD0;
	regAdd[1] = (comp_header_addr >> 8) & 0xFF;
	regAdd[2] = comp_header_addr & 0xFF;
	fts_read_reg(info, regAdd, 3, &buff[0], 16 + 1);
	tx_num = buff[5];
	rx_num = buff[6];

	tx_ix1 = (short) buff[10] * FTS_SEC_IX1_TX_MULTIPLIER;		// Self TX Ix1
	rx_ix1 = (short) buff[11] * FTS_SEC_IX1_RX_MULTIPLIER;		// Self RX Ix1

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

	input_info(true, &info->client->dev, "%s MIN_TX_IX_SUM : %d MAX_TX_IX_SUM : %d\n",
				__func__, min_tx_ix_sum, max_tx_ix_sum );
	input_info(true, &info->client->dev, "%s MIN_RX_IX_SUM : %d MAX_RX_IX_SUM : %d\n",
				__func__, min_rx_ix_sum, max_rx_ix_sum );

	fts_systemreset(info);
	fts_wait_for_ready(info);

	fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_WATER_MODE
	fts_fw_wait_for_event(info, STATUS_EVENT_WATER_SELF_DONE);
#else
	fts_fw_wait_for_event (info, STATUS_EVENT_FORCE_CAL_DONE_D3);
#endif
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

	enable_irq(info->irq);
	fts_interrupt_set(info, INT_ENABLE);

	if (allnode == true) {
		buff_size = (info->ForceChannelLength + info->SenseChannelLength + 2) * 5;
		mbuff = kzalloc(buff_size, GFP_KERNEL);
	}
	if (mbuff != NULL) {
		char *pBuf = mbuff;
		for (i = 0; i < info->ForceChannelLength; i++) {
			num =  force_ix_data[i];
			n = 100000;
			fzero = 0;
			for (j = 5; j > 0; j--) {
				n = n / 10;
				a = num / n;
				if (a)
					fzero = 1;
				cnum = a + '0';
				num  = num - a*n;
				if (fzero)
					*pBuf++ = cnum;
			}
			if (!fzero)
				*pBuf++ = '0';
			*pBuf++ = ',';
			input_info(true, &info->client->dev, "Force[%d] %d\n", i, force_ix_data[i]);
		}
		for (i = 0; i < info->SenseChannelLength; i++) {
			num =  sense_ix_data[i];
			n = 100000;
			fzero = 0;
			for (j = 5; j > 0; j--) {
				n = n / 10;
				a = num / n;
				if (a)
					fzero = 1;
				cnum = a + '0';
				num  = num - a * n;
				if (fzero)
					*pBuf++ = cnum;
			}
			if (!fzero)
				*pBuf++ = '0';
			if (i < (info->SenseChannelLength - 1))
				*pBuf++ = ',';
			input_info(true, &info->client->dev, "Sense[%d] %d\n", i, sense_ix_data[i]);
		}

		sec_cmd_set_cmd_result(sec, mbuff, buff_size);
		sec->cmd_state = SEC_CMD_STATUS_OK;
		kfree(mbuff);
	} else {
		if (allnode == true) {
		   snprintf(buff, sizeof(buff), "%s", "kzalloc failed");
		   sec->cmd_state = SEC_CMD_STATUS_FAIL;
		} else{
			snprintf(buff, sizeof(buff), "%d,%d,%d,%d", min_tx_ix_sum, max_tx_ix_sum, min_rx_ix_sum, max_rx_ix_sum);
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	}
}

static void run_ix_data_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_ix_data(info, false);
}

static void run_ix_data_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_ix_data(info, true);
}

static void fts_read_self_raw_frame(struct fts_ts_info *info, unsigned short oAddr, bool allnode)
{
	struct sec_cmd_data *sec = &info->sec;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char D0_offset = 1;
	unsigned char regAdd[3] = {0xD0, 0x00, 0x00};
	unsigned char ReadData[(info->ForceChannelLength + info->SenseChannelLength) * 2 + 1];
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
	unsigned char cmd[4] = {0xC7, 0x00, FTS_CFG_APWR, 0x00}; // Don't enter to IDLE

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_systemreset(info);
	fts_delay(50);
	fts_wait_for_ready(info);

	// Don't enter to IDLE mode
	fts_write_reg(info, &cmd[0], 4);
	fts_delay(20);

	fts_command(info, FLUSHBUFFER);                 // Clear FIFO
	fts_delay(20);

	// Auto-Tune
	fts_command(info, CX_TUNNING);
	msleep(300);
	fts_fw_wait_for_event_D3(info, STATUS_EVENT_MUTUAL_AUTOTUNE_DONE, 0x00);

	fts_command(info, SELF_AUTO_TUNE);
	msleep(300);
	fts_fw_wait_for_event_D3(info, STATUS_EVENT_SELF_AUTOTUNE_DONE_D3, 0x00);

	fts_command(info, SENSEON);
	fts_delay(100);

	fts_command(info, SENSEOFF);
	fts_delay(100);

	disable_irq(info->irq);
	fts_interrupt_set(info, INT_DISABLE);


#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_OFF);
	}
#endif

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

	input_info(true, &info->client->dev, "%s MIN_TX_SELF_RAW: %d MAX_TX_SELF_RAW : %d\n",
				__func__, min_tx_self_raw_data, max_tx_self_raw_data );
	input_info(true, &info->client->dev, "%s MIN_RX_SELF_RAW : %d MIN_RX_SELF_RAW : %d\n",
				__func__, min_rx_self_raw_data, max_rx_self_raw_data );


	fts_command(info, SENSEON);
	fts_delay(50);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

	enable_irq(info->irq);
	fts_interrupt_set(info, INT_ENABLE);

	if (allnode == true) {
		buff_size = (info->ForceChannelLength + info->SenseChannelLength + 2) * 7;
		mbuff = kzalloc(buff_size, GFP_KERNEL);
	}
	if (mbuff != NULL) {
		char *pBuf = mbuff;
		for (i = 0; i < info->ForceChannelLength; i++) {
			num =  self_force_raw_data[i];
			n = 100000;
			fzero = 0;
			for (j = 5; j > 0; j--) {
				n = n / 10;
				a = num / n;
				if (a)
					fzero = 1;
				cnum = a + '0';
				num  = num - a * n;
				if (fzero)
					*pBuf++ = cnum;
			}
			if (!fzero)
				*pBuf++ = '0';
			*pBuf++ = ',';
			input_info(true, &info->client->dev, "Force[%d] %d\n", i, self_force_raw_data[i]);
		}
		for (i = 0; i < info->SenseChannelLength; i++) {
			num =  self_sense_raw_data[i];
			n = 100000;
			fzero = 0;
			for (j = 5; j > 0; j--) {
				n = n / 10;
				a = num / n;
				if (a)
					fzero = 1;
				cnum = a + '0';
				num  = num - a * n;
				if (fzero)
					*pBuf++ = cnum;
			}
			if (!fzero)
				*pBuf++ = '0';
			if (i < (info->SenseChannelLength - 1))
				*pBuf++ = ',';
			input_info(true, &info->client->dev, "Sense[%d] %d\n", i, self_sense_raw_data[i]);
		}

		sec_cmd_set_cmd_result(sec, mbuff, buff_size);
		sec->cmd_state = SEC_CMD_STATUS_OK;
		kfree(mbuff);
	} else {
		if (allnode == true) {
		   snprintf(buff, sizeof(buff), "%s", "kzalloc failed");
		   sec->cmd_state = SEC_CMD_STATUS_FAIL;
		} else {
			snprintf(buff, sizeof(buff), "%d,%d,%d,%d", min_tx_self_raw_data, max_tx_self_raw_data, min_rx_self_raw_data, max_rx_self_raw_data);
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	}
}

static void run_self_raw_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_self_raw_frame(info, FTS_WATER_SELF_RAW_ADDR,false);
}

static void run_self_raw_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_self_raw_frame(info, FTS_WATER_SELF_RAW_ADDR,true);
}

static void run_trx_short_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	ret = fts_panel_ito_test(info);
	if (ret == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else
		snprintf(buff, sizeof(buff), "%s", "FAIL");

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_status_pureautotune(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int rc;
	unsigned char regAdd[2] = {0xC2, 0x0E};

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_command(info, SENSEOFF);
	fts_delay(50);

	disable_irq(info->irq);
	fts_interrupt_set(info, INT_DISABLE);
	fts_delay(50);

	if (sec->cmd_param[0] == 0)
		regAdd[0] = 0xC2;
	else
		regAdd[0] = 0xC1;

	input_info(true, &info->client->dev, "%s: CMD[%2X]\n",__func__,regAdd[0]);

	rc = fts_write_reg(info, regAdd, 2);
	msleep(20);

	info->fts_command(info, FTS_CMD_SAVE_CX_TUNING);
	msleep(230);
	fts_fw_wait_for_event(info, STATUS_EVENT_FLASH_WRITE_CXTUNE_VALUE);

	info->fts_systemreset(info);
	fts_wait_for_ready(info);

	info->fts_command(info, SENSEON);

	enable_irq(info->irq);
	fts_interrupt_set(info, INT_ENABLE);

	if (rc <= 0)
		snprintf(buff, sizeof(buff), "%s", "FAIL");
	else
		snprintf(buff, sizeof(buff), "%s", "OK");

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_status_pureautotune(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int rc;
	unsigned char data[5]= { 0 };
	int retVal = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	rc = info->fts_get_sysinfo_data(info, FTS_SI_PURE_AUTOTUNE_FLAG, 4, data);
	if (rc <= 0) {
		input_info(true, info->dev, "%s: PureAutotune Information Read Fail!! [Data : %2X%2X]\n", __func__, data[0], data[1]);
	} else {
	    	if((data[0] == PURE_AUTOTUNE_FLAG_ID0) && (data[1] == PURE_AUTOTUNE_FLAG_ID1))
	            retVal = 1;
	    	input_info(true, info->dev, "%s: PureAutotune Information !! [Data : %2X%2X]\n", __func__, data[0], data[1]);
	}

	snprintf(buff, sizeof(buff), "%d", retVal);
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_status_afe(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int rc = 0;
	unsigned char data[5]= { 0 };
	int retVal = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	rc = info->fts_get_sysinfo_data(info, FTS_SI_PURE_AUTOTUNE_CONFIG, 4, data);
	if (rc <= 0)
		input_info(true, info->dev,
			"%s: Final AFE [Data : %2X] Read Fail AFE Ver [Data : %2X]\n",
			__func__, data[0], data[1]);

	if (data[0])
		retVal = 1;

	input_info(true, info->dev,
		"%s: Final AFE [Data : %2X] AFE Ver [Data : %2X]\n",
		__func__, data[1], data[2]);

	snprintf(buff, sizeof(buff), "%d", retVal);
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#define FTS_MAX_TX_LENGTH		44
#define FTS_MAX_RX_LENGTH		64

#define FTS_CX2_READ_LENGTH		4
#define FTS_CX2_ADDR_OFFSET		3
#define FTS_CX2_TX_START		0

static void get_cx_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = fts_check_index(info);
	if (node < 0)
		return;

	val = info->cx_data[node];
	snprintf(buff, sizeof(buff), "%d", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);

}

static void run_cx_data_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char ReadData[info->ForceChannelLength][info->SenseChannelLength + FTS_CX2_READ_LENGTH];
	unsigned char regAdd[8];
	unsigned int addr, rx_num, tx_num;
	int i, j;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };

	int comp_header_addr, comp_start_addr;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	pStr = kzalloc(4 * (info->SenseChannelLength + 1), GFP_KERNEL);
	if (pStr == NULL) {
		input_info(true, &info->client->dev, "FTS pStr kzalloc failed\n");
		return;
	}

	input_info(true, &info->client->dev, "%s: start \n", __func__);

	disable_irq(info->irq);
	fts_interrupt_set(info, INT_DISABLE);

	fts_command(info, SENSEOFF);
	fts_delay(50);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_OFF); // Key Sensor OFF
	}
#endif
	fts_command(info, FLUSHBUFFER);
	fts_delay(50);

	/* Request compensation data */
	regAdd[0] = 0xB8;
	regAdd[1] = 0x04;		// MUTUAL CX (LPA)
	regAdd[2] = 0x00;
	fts_write_reg(info, &regAdd[0], 3);
	fts_fw_wait_for_specific_event(info, EVENTID_STATUS_REQUEST_COMP, 0x04, 0x00);

	/* Read an address of compensation data */
	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = FTS_SI_COMPENSATION_OFFSET_ADDR;
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
		for (i = 0; i < rx_num; i++) {
			snprintf(pTmp, sizeof(pTmp), "%3d", ReadData[j][i]);
			strncat(pStr, pTmp, 4 * rx_num);
		}
		input_info(true, &info->client->dev, "FTS %s\n", pStr);
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
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_cx_all_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char ReadData[info->ForceChannelLength][info->SenseChannelLength + FTS_CX2_READ_LENGTH];
	unsigned char regAdd[8];
	unsigned int addr, rx_num, tx_num;
	int i, j;
	unsigned char *pStr = NULL;
    	unsigned char pTmp[16] = { 0 };
	char all_strbuff[(info->ForceChannelLength)*(info->SenseChannelLength)*3];

    	int	comp_header_addr, comp_start_addr;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	input_info(true, &info->client->dev, "%s: start \n", __func__);

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

	pStr = kzalloc(4 * (info->SenseChannelLength + 1), GFP_KERNEL);
	if (pStr == NULL) {
		input_info(true, &info->client->dev, "FTS pStr kzalloc failed\n");
		return;
	}

	memset(all_strbuff, 0, sizeof(char) * (tx_num*rx_num*3));	//size 3  ex(45,)

	/* Request compensation data */
	regAdd[0] = 0xB8;
	regAdd[1] = 0x04;		// MUTUAL CX (LPA)
	regAdd[2] = 0x00;
	fts_write_reg(info, &regAdd[0], 3);
	fts_fw_wait_for_specific_event(info, EVENTID_STATUS_REQUEST_COMP, 0x04, 0x00);

	/* Read an address of compensation data */
	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = FTS_SI_COMPENSATION_OFFSET_ADDR;
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
		for (i = 0; i < rx_num; i++) {
			snprintf(pTmp, sizeof(pTmp), "%3d", ReadData[j][i]);
			strncat(pStr, pTmp, 4 * rx_num);
		}
		input_info(true, &info->client->dev, "FTS %s\n", pStr);
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

	kfree(pStr);

	enable_irq(info->irq);
	fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
	}
#endif
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, all_strbuff, strnlen(all_strbuff, sizeof(all_strbuff)));
	input_info(true, &info->client->dev, "%ld (%ld)\n", strnlen(all_strbuff, sizeof(all_strbuff)),sizeof(all_strbuff));
}

static void fts_read_wtr_cx_data(struct fts_ts_info *info, bool allnode)
{
	struct sec_cmd_data *sec = &info->sec;
	char buff[512] = { 0 };
	unsigned char regAdd[8];
	unsigned char *result_tx, *result_rx;
	char temp[8];
	unsigned int ii;
	unsigned int tx_num, rx_num;

        unsigned int comp_header_addr,comp_start_tx_addr,comp_start_rx_addr;
        int i;


	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	input_info(true, &info->client->dev, "%s: ++\n", __func__);

	disable_irq(info->irq);
	fts_interrupt_set(info, INT_DISABLE);

	fts_command(info, SENSEOFF);

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
		input_err(true, &info->client->dev, "%s: failed to alloc mem\n",
				__func__);

		snprintf(buff, sizeof(buff), "%s", "FAIL");
		goto err_alloc_mem;
	}

	/* Request compensation data */
	regAdd[0] = 0xB8;
	regAdd[1] = 0x20;		// SELF IX
	regAdd[2] = 0x00;
	fts_write_reg(info, &regAdd[0], 3);
	fts_fw_wait_for_specific_event(info, EVENTID_STATUS_REQUEST_COMP, 0x20, 0x00);

	/* Read an address of compensation data */
	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = FTS_SI_COMPENSATION_OFFSET_ADDR;
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

	for(i = 0; i < tx_num; i++) {
		result_tx[i] = buff[i + 1];
	}

	memset(buff, 0x0, 512);
	/* Read Self RX Ix2 */
	regAdd[0] = 0xD0;
	regAdd[1] = (comp_start_rx_addr >> 8) & 0xFF;
	regAdd[2] = comp_start_rx_addr & 0xFF;
	fts_read_reg(info, regAdd, 3, &buff[0], rx_num + 1);

	for(i = 0; i < rx_num; i++) {
		result_rx[i] = buff[i + 1];
	}

	if (allnode) {
		char *data = 0;

		data = result_tx;
		for (ii = 0; ii < info->ForceChannelLength; ii++) {
			input_info(true, &info->client->dev,
					"%s: Tx[%d] = %d\n", __func__, ii, *data);
			sprintf(temp, "%d,", *data);
			strncat(buff, temp, 9);
			data++;
		}

		data = result_rx;
		for (ii = 0; ii < info->SenseChannelLength; ii++) {
			input_info(true, &info->client->dev,
					"%s: Rx[%d] = %d\n", __func__, ii, *data);
			sprintf(temp, "%d,", *data);
			strncat(buff, temp, 9);
			data++;
		}
	} else {
		unsigned char min_tx = 255, max_tx = 0, min_rx = 255, max_rx = 0;

		for (ii = 0; ii < info->ForceChannelLength; ii++) {
			input_info(true, &info->client->dev,
					"%s: Tx[%d] = %d\n", __func__, ii, result_tx[ii]);

			min_tx = min(min_tx, result_tx[ii]);
			max_tx = max(max_tx, result_tx[ii]);
		}

		for (ii = 0; ii < info->SenseChannelLength; ii++) {
			input_info(true, &info->client->dev,
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
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);

}

static void run_wtr_cx_data_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_wtr_cx_data(info, false);
}

static void run_wtr_cx_data_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_wtr_cx_data(info, true);
}

#ifdef FTS_SUPPORT_PRESSURE_SENSOR
int fts_read_pressure_data(struct fts_ts_info *info)
{
	unsigned char pressure_reg[3] = {0, };
	unsigned char buf[32] = {0, };
	short value[10] = {0, };
	int rtn, i = 0;
	int pressure_sensor_value = 0;

	pressure_reg[0] = 0xD0;
	pressure_reg[1] = 0x00;
	pressure_reg[2] = FTS_SI_FORCE_TOUCH_STRENGTH_ADDR;

	rtn = fts_read_reg(info, pressure_reg, 3, &buf[0], 4);
	if (rtn <= 0) {
		input_err(true, &info->client->dev, "%s: Pressure read failed rc = %d\n", __func__, rtn);
		return rtn;
	} else {
		pressure_reg[1] = buf[2];
		pressure_reg[2] = buf[1];
	}

	rtn = fts_read_reg(info, pressure_reg, 3, &buf[0], PRESSURE_SENSOR_COUNT * 2 + 1);
	if (rtn <= 0) {
		input_err(true, &info->client->dev, "%s: Pressure read failed rc = %d\n", __func__, rtn);
		return rtn;
	} else {
		for (i = 0; i < PRESSURE_SENSOR_COUNT; i++) {
			value[i] = buf[i * 2 + 1] + (buf[i * 2 + 1 + 1] << 8);
			pressure_sensor_value += value[i];
		}
	}

	if (info->debug_string & 0x2)
		input_info(true, &info->client->dev, "%s: %d %d %d %d %d %d %d %d %d %d %d\n", __func__,
			value[0], value[1], value[2], value[3], value[4],
			value[5], value[6], value[7], value[8], value[9],
			pressure_sensor_value);

	return pressure_sensor_value;
}
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
#define USE_KEY_NUM 2
static void run_key_cx_data_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char key_cx2_data[2], key_cx1_data, total_cx_data[USE_KEY_NUM];
	unsigned char ReadData[USE_KEY_NUM * FTS_CX2_READ_LENGTH];
	unsigned char regAdd[8];
	unsigned int addr;
	int	tx_num, rx_num, DOFFSET = 1;
	int	comp_start_addr, comp_header_addr;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
		            __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	disable_irq(info->irq);

	/* Request compensation data */
	regAdd[0] = 0xB8;
	regAdd[1] = 0x10;   // For button
	regAdd[2] = 0x00;
	fts_write_reg(info, &regAdd[0], 3);
	fts_fw_wait_for_specific_event(info, EVENTID_STATUS_REQUEST_COMP, regAdd[1], 0x00);

	/* Read an address of compensation data */
	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = FTS_SI_COMPENSATION_OFFSET_ADDR;
	fts_read_reg(info, regAdd, 3, &buff[0], 4);
	comp_header_addr = buff[0 + DOFFSET] + (buff[1 + DOFFSET] << 8);

	/* Read header of compensation area */
	regAdd[0] = 0xD0;
	regAdd[1] = (comp_header_addr >> 8) & 0xFF;
	regAdd[2] = comp_header_addr & 0xFF;
	fts_read_reg(info, regAdd, 3, &buff[0], 16 + DOFFSET);
	tx_num = buff[4 + DOFFSET];
	rx_num = buff[5 + DOFFSET];
	key_cx1_data = buff[9 + DOFFSET];
	comp_start_addr = comp_header_addr + 0x10;

	memset(&ReadData[0], 0x0, rx_num);
	/* Read compensation data */
	addr = comp_start_addr;
	regAdd[0] = 0xD0;
	regAdd[1] = (addr >> 8) & 0xFF;
	regAdd[2] = addr & 0xFF;
	fts_read_reg(info, regAdd, 3, &ReadData[0], rx_num + DOFFSET);
	key_cx2_data[0] = ReadData[0 + DOFFSET];
	key_cx2_data[1] = ReadData[1 + DOFFSET];
	total_cx_data[0] = key_cx1_data * 2 + key_cx2_data[0];
	total_cx_data[1] = key_cx1_data * 2 + key_cx2_data[1];

	//snprintf(buff, sizeof(buff), "%s", "OK");
	snprintf(buff, sizeof(buff), "%d,%d,%d,%d", key_cx2_data[0], key_cx2_data[1], total_cx_data[0], total_cx_data[1]);

	enable_irq(info->irq);
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
#endif

int fts_get_tsp_test_result(struct fts_ts_info *info)
{
	unsigned char data[5] = { 0 };
	int ret;

	fts_systemreset(info);
	fts_wait_for_ready(info);

	ret = info->fts_get_sysinfo_data(info, FTS_SI_FACTORY_RESULT_FLAG, 4, data);
	if (ret < 0) {
		input_err(true, &info->client->dev,
			"%s failed. ret: %d\n", __func__, ret);
		ret = -EINVAL;
		goto out;
	}

	if (data[0] == 0xFF)
		data[0] = 0;
	if (data[1] == 0xFF)
		data[1] = 0;

	info->test_result.data[0] = data[0];
	info->disassemble_count = data[1];

out:
	fts_command(info, FLUSHBUFFER);
	fts_delay(10);
	fts_command(info, SENSEON);
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
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	ret = fts_get_tsp_test_result(info);
	if (ret < 0) {
		input_info(true, &info->client->dev, "%s: failed to get result\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "M:%s, M:%d, A:%s, A:%d",
				info->test_result.module_result == 0 ? "NONE" :
				info->test_result.module_result == 1 ? "FAIL" : "PASS",
				info->test_result.module_count,
				info->test_result.assy_result == 0 ? "NONE" :
				info->test_result.assy_result == 1 ? "FAIL" : "PASS",
				info->test_result.assy_count);

		sec_cmd_set_cmd_result(sec, buff, strlen(buff));
		sec->cmd_state = SEC_CMD_STATUS_OK;
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
	unsigned char regAdd[4] = {0xC7, 0x01, 0x00, 0x00};
	int ret;

	fts_systemreset(info);
	fts_wait_for_ready(info);

	input_info(true, &info->client->dev, "%s: [0x%X] M:%s, M:%d, A:%s, A:%d\n",
		__func__, info->test_result.data[0],
		info->test_result.module_result == 0 ? "NONE" :
		info->test_result.module_result == 1 ? "FAIL" : "PASS",
		info->test_result.module_count,
		info->test_result.assy_result == 0 ? "NONE" :
		info->test_result.assy_result == 1 ? "FAIL" : "PASS",
		info->test_result.assy_count);

	regAdd[2] = info->test_result.data[0];
	regAdd[3] = info->disassemble_count;

	ret = fts_write_reg(info, &regAdd[0], 4);
	if (ret < 0) {
		input_err(true, &info->client->dev,
			"%s failed. ret: %d\n", __func__, ret);
		return -EINVAL;
	}

	fts_command(info, FTS_CMD_SAVE_CX_TUNING);
	fts_delay(230);
	fts_fw_wait_for_event(info, STATUS_EVENT_FLASH_WRITE_CXTUNE_VALUE);

	return 0;
}

static void set_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	ret = fts_get_tsp_test_result(info);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: [ERROR] failed to get_tsp_test_result\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}
	sec->cmd_state = SEC_CMD_STATUS_RUNNING;

	if (info->test_result.data[0] == 0xFF) {
		input_info(true, &info->client->dev,
			"%s: clear factory_result as zero\n",
			__func__);
		info->test_result.data[0] = 0;
	}

	if (sec->cmd_param[0] == TEST_OCTA_ASSAY) {
		info->test_result.assy_result = sec->cmd_param[1];
		if (info->test_result.assy_count < 3)
			info->test_result.assy_count++;

	} else if (sec->cmd_param[0] == TEST_OCTA_MODULE) {
		info->test_result.module_result = sec->cmd_param[1];
		if (info->test_result.module_count < 3)
			info->test_result.module_count++;
	}

	ret = fts_set_tsp_test_result(info);
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void increase_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	ret = fts_get_tsp_test_result(info);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: [ERROR] failed to get_tsp_test_result\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}
	sec->cmd_state = SEC_CMD_STATUS_RUNNING;

	if (info->disassemble_count < 0xFE)
		info->disassemble_count++;

	ret = fts_set_tsp_test_result(info);
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);

}

static void get_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	ret = fts_get_tsp_test_result(info);
	if (ret < 0) {
		input_info(true, &info->client->dev, "%s: failed to get result\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {

		snprintf(buff, sizeof(buff), "%d\n", info->disassemble_count);

		sec_cmd_set_cmd_result(sec, buff, strlen(buff));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
}

int fts_get_tsp_flash_data(struct fts_ts_info *info, int offset, unsigned char cnt, unsigned char* buf)
{
	unsigned char regAdd[3] = {0xB8, 0x00, 0x08};
  	unsigned char data[255] = { 0 };
	unsigned short offset_addr;
	int ret;

	ret = fts_write_reg(info, &regAdd[0], 3);
	if (ret <= 0) {
		input_err(true, &info->client->dev,
			"%s failed. ret: %d\n", __func__, ret);
		return -EINVAL;
	}

	ret = info->fts_get_sysinfo_data(info, FTS_SI_COMPENSATION_OFFSET_ADDR, 4, data);
	if (ret < 0) {
		input_err(true, &info->client->dev,
			"%s failed. ret: %d\n", __func__, ret);
		ret = -EINVAL;
	}
	
	offset_addr = data[0] + (data[1] << 8);

    regAdd[0] = 0xD0;
    regAdd[1] = (offset_addr >> 8) & 0xFF;
    regAdd[2] = (offset_addr & 0xFF) + offset;

	ret = fts_read_reg(info, &regAdd[0], 3, (unsigned char *)buf, cnt + 1);
	if (ret <= 0) {
		input_err(true, &info->client->dev, "%s failed. ret: %d\n",
			__func__, ret);
		return ret;
	}
	return 0;
}

#ifdef PAT_CONTROL
void set_pat_cal_count(struct fts_ts_info *info, unsigned char count)
{
	unsigned char regAdd[6] = {0xC7, 0x02, 0x00, 0x00, 0x00, 0x00};
	int ret;

	regAdd[2] = count;
	regAdd[3] = (unsigned char)((info->tune_fix_ver & 0xFF00)>>8);
	regAdd[4] = (unsigned char)(info->tune_fix_ver & 0xFF);
	regAdd[5] = 0x00;//rfu

	ret = fts_write_reg(info, &regAdd[0], 6);
	if (ret < 0) {
		input_err(true, &info->client->dev,
			"%s failed. ret: %d\n", __func__, ret);
		return;
	}

	fts_command(info, FTS_CMD_SAVE_CX_TUNING);
	fts_delay(230);
	fts_fw_wait_for_event(info, STATUS_EVENT_FLASH_WRITE_CXTUNE_VALUE);

	return;
}

void set_pat_tune_ver(struct fts_ts_info *info, int ver)
{
	unsigned char regAdd[6] = {0xC7, 0x02, 0x00, 0x00, 0x00, 0x00};
	int ret;

	regAdd[2] = (unsigned char)info->cal_count;
	regAdd[3] = (unsigned char)((ver & 0xFF00)>>8);
	regAdd[4] = (unsigned char)(ver & 0xFF);
	regAdd[5] = 0x00;//rfu

	ret = fts_write_reg(info, &regAdd[0], 6);
	if (ret < 0) {
		input_err(true, &info->client->dev,
			"%s failed. ret: %d\n", __func__, ret);
		return;
	}

	fts_command(info, FTS_CMD_SAVE_CX_TUNING);
	fts_delay(230);
	fts_fw_wait_for_event(info, STATUS_EVENT_FLASH_WRITE_CXTUNE_VALUE);

	return;
}



void get_pat_data(struct fts_ts_info *info)
{
	unsigned char data[6] = { 0 };
	int ret;

	fts_systemreset(info);
	fts_wait_for_ready(info);

	ret = fts_get_tsp_flash_data(info, 18, 5, data);
	if (ret < 0) {
		input_err(true, &info->client->dev,
			"%s failed. ret: %d\n", __func__, ret);
		goto out;
	}	

	input_info(true, &info->client->dev, "PAT GET: [0x%X] [0x%X] [0x%X] [0x%X]\n",
		data[1],data[2],data[3],data[4]);

	info->cal_count = data[1];
	info->tune_fix_ver = data[2]<<8 | data[3];

	input_info(true, &info->client->dev, "cal_count = [0x%X], tune_fix_ver = [0x%4X]\n",info->cal_count, info->tune_fix_ver);

	
out:
	fts_command(info, FLUSHBUFFER);
	fts_delay(10);
	fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif
	fts_interrupt_set(info, INT_ENABLE);

	return;
}
#endif

#ifdef FTS_SUPPORT_HOVER
static void hover_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped || !(info->reinit_done) || (info->fts_power_state == FTS_POWER_STATE_LOWPOWER)) {
		input_info(true, &info->client->dev,
			"%s: [ERROR] Touch is stopped:%d, reinit_done:%d, power_state:%d\n",
			__func__, info->touch_stopped, info->reinit_done, info->fts_power_state);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;

		if(sec->cmd_param[0]==1){
			info->retry_hover_enable_after_wakeup = 1;
			input_info(true, &info->client->dev, "%s: retry_hover_on_after_wakeup \n", __func__);
		}

		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		int enables;
		enables = sec->cmd_param[0];
		if (enables == info->hover_enabled) {
			input_dbg(true, &info->client->dev,
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
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;

out:
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

/* static void hover_no_sleep_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	unsigned char regAdd[4] = {0xB0, 0x01, 0x18, 0x00};
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0]) {
			regAdd[3]=0x0F;
		} else {
			regAdd[3]=0x08;
		}
		fts_write_reg(info, &regAdd[0], 4);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
} */
#endif

#ifdef CONFIG_GLOVE_TOUCH
static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->mshover_enabled = sec->cmd_param[0];

		if (!info->touch_stopped && info->reinit_done) {
			if (info->mshover_enabled)
				fts_command(info, FTS_CMD_MSHOVER_ON);
			else
				fts_command(info, FTS_CMD_MSHOVER_OFF);
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_glove_sensitivity(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	unsigned char cmd[4] =
		{ 0xB2, 0x01, 0xC6, 0x02 };
	int timeout=0;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		char buff[SEC_CMD_STR_LEN] = { 0 };
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_write_reg(info, &cmd[0], 4);
	sec->cmd_state = SEC_CMD_STATUS_RUNNING;

	while (sec->cmd_state == SEC_CMD_STATUS_RUNNING) {
		if (timeout++>30) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			break;
		}
		msleep(10);
	}
}

static void fast_glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->fast_mshover_enabled = sec->cmd_param[0];

		if (!info->touch_stopped && info->reinit_done) {
			if (info->fast_mshover_enabled)
				fts_command(info, FTS_CMD_SET_FAST_GLOVE_MODE);
			else
				fts_command(info, FTS_CMD_SET_NOR_GLOVE_MODE);
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};
#endif

static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] > 1) {
			info->flip_enable = true;
			info->cover_type = sec->cmd_param[1];
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
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

static void report_rate(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 2) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] == REPORT_RATE_90HZ)
			fts_change_scan_rate(info, FTS_CMD_FAST_SCAN);
		else if (sec->cmd_param[0] == REPORT_RATE_60HZ)
			fts_change_scan_rate(info, FTS_CMD_SLOW_SCAN);
		else if (sec->cmd_param[0] == REPORT_RATE_30HZ)
			fts_change_scan_rate(info, FTS_CMD_USLOW_SCAN);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;

out:
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static void interrupt_control(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		int enables;
		enables = sec->cmd_param[0];
		if (enables)
			fts_irq_enable(info, true);
		else
			fts_irq_enable(info, false);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;

out:
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
#endif

bool check_lowpower_mode(struct fts_ts_info *info)
{
	bool ret = 0;
	unsigned char flag = info->lowpower_flag & 0xFF;

	if (flag)
		ret = 1;

	input_info(true, &info->client->dev,
		"%s: lowpower_mode flag : %d, ret:%d\n", __func__, flag, ret);

	if (flag & FTS_LOWP_FLAG_2ND_SCREEN)
		input_info(true, &info->client->dev, "%s: 2nd screen on\n", __func__);
	if (flag & FTS_LOWP_FLAG_BLACK_UI)
		input_info(true, &info->client->dev, "%s: swipe finger on\n", __func__);
	if (flag & FTS_LOWP_FLAG_SPAY)
		input_info(true, &info->client->dev, "%s: spay cmd on\n", __func__);
	if (flag & FTS_LOWP_FLAG_TEMP_CMD)
		input_info(true, &info->client->dev, "%s: known cmd on\n", __func__);
	if (flag & FTS_LOWP_FLAG_GESTURE_WAKEUP)
		input_info(true, &info->client->dev, "%s: 2finger gesture cmd on\n", __func__);

	return ret;

}

static void set_lpgw_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, &info->client->dev, "%s: Abnormal parameter:[%d]\n", __func__, sec->cmd_param[0]);
	} else {
		if ((sec->cmd_param[0] && (info->lowpower_flag & FTS_LOWP_FLAG_GESTURE_WAKEUP))
			|| (!sec->cmd_param[0] && !(info->lowpower_flag & FTS_LOWP_FLAG_GESTURE_WAKEUP))) {
			input_info(true, &info->client->dev, "%s: duplicate lpgw mode setting cmd:[0x%x], lowpower_flag:[0x%X]\n",
					__func__, sec->cmd_param[0], info->lowpower_flag & FTS_LOWP_FLAG_GESTURE_WAKEUP);
		} else {
			if (sec->cmd_param[0]) {
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
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_lowpower_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
	        snprintf(buff, sizeof(buff), "%s", "NG");
	        sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
#ifdef FTS_SUPPORT_SIDE_GESTURE
		if (info->board->support_sidegesture)
			info->lowpower_mode = sec->cmd_param[0];
#endif
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

static void set_deepsleep_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->deepsleep_mode = sec->cmd_param[0];

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

static void set_wirelesscharger_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char regAdd[2] = {0xC2, 0x10};
	int rc;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		info->wirelesscharger_mode = sec->cmd_param[0];

		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->wirelesscharger_mode = sec->cmd_param[0];

		if (info->wirelesscharger_mode == 0)
			regAdd[0] = 0xC2;
		else
			regAdd[0] = 0xC1;
		input_info(true, &info->client->dev, "%s: CMD[%2X]\n",__func__,regAdd[0]);

		rc = fts_write_reg(info, regAdd, 2);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;

out:
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

static void active_sleep_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
	/* To do here */
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};


static void second_screen_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
	        snprintf(buff, sizeof(buff), "%s", "NG");
	        sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if(sec->cmd_param[0])
			info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_2ND_SCREEN;
		else
			info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_2ND_SCREEN);

#ifdef FTS_SUPPORT_SIDE_GESTURE
		if (info->board->support_sidegesture)
			info->lowpower_mode = check_lowpower_mode(info);
#endif

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_tunning_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int i, ret;
	unsigned char regAdd[3] = {0xC6, 0x00, 0x00};

	sec_cmd_set_default_result(sec);
	memset(buff, 0, sizeof(buff));

	for (i = 0; i < 4; i++) {
		if (sec->cmd_param[i]) {
			if (i == 0)
				regAdd[1] = 0x02;
			else if (i == 1)
				regAdd[1] = 0x03;
			else if (i == 2)
				regAdd[1] = 0x05;
			else
				regAdd[1] = 0x07;

			regAdd[2] = sec->cmd_param[i];

			ret = fts_write_reg(info, regAdd, 3);
			if (ret < 0) {
				input_err(true, &info->client->dev, "%s: write tunning data failed!\n", __func__);
				snprintf(buff, sizeof(buff), "%s", "NG");
				sec->cmd_state = SEC_CMD_STATUS_FAIL;
				goto out;
			} else
				input_dbg(true, &info->client->dev, "%s: write tunning data:%d, %d, %d, ret:%d\n", __func__,
					regAdd[0], regAdd[1], regAdd[2], ret);

			fts_delay(5);
		}
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_dead_zone(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char regAdd[2] = {0xC4, 0x00};
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 6) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0]==1)
			regAdd[1] = 0x01;	/* side edge top */
		else if (sec->cmd_param[0]==2)
			regAdd[1] = 0x02;	/* side edge bottom */
		else if (sec->cmd_param[0]==3)
			regAdd[1] = 0x03;	/* side edge All On */
		else if (sec->cmd_param[0]==4)
			regAdd[1] = 0x04;	/* side edge Left Off */
		else if (sec->cmd_param[0]==5)
			regAdd[1] = 0x05;	/* side edge Right Off */
		else if (sec->cmd_param[0]==6)
			regAdd[1] = 0x06;	/* side edge All Off */
		else
			regAdd[1] = 0x0;	/* none	*/

		ret = fts_write_reg(info, regAdd, 2);

		if (ret < 0)
			input_err(true, &info->client->dev, "%s failed. ret: %d\n", __func__, ret);
		else
			input_info(true, &info->client->dev, "%s: reg:%d, ret: %d\n", __func__, sec->cmd_param[0], ret);

		fts_delay(1);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char regAdd[2] = {0xC2, 0x0C};
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0]==0) {
			regAdd[0] = 0xC1;	/* dead zone disable */
		} else {
			regAdd[0] = 0xC2;	/* dead zone enable */
		}

		ret = fts_write_reg(info, regAdd, 2);

		if (ret < 0)
			input_err(true, &info->client->dev, "%s failed. ret: %d\n", __func__, ret);
		else
			input_info(true, &info->client->dev, "%s: reg:%d, ret: %d\n", __func__, sec->cmd_param[0], ret);

		fts_delay(1);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void select_wakeful_edge(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char regAdd[2] = {0xC2, 0x0F};
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0]==0) {
			regAdd[0] = FTS_CMD_DISABLE_FEATURE;	/* right - C2, 0F */
			info->wakeful_edge_side = EDGEWAKE_RIGHT;
		} else {
			regAdd[0] = FTS_CMD_ENABLE_FEATURE;		/* left - C1, 0F */
			info->wakeful_edge_side = EDGEWAKE_LEFT;
		}
		ret = fts_write_reg(info, regAdd, 2);

		if (ret < 0)
			input_err(true, &info->client->dev, "%s failed. ret: %d\n", __func__, ret);
		else
			input_info(true, &info->client->dev, "%s: reg:%d, ret: %d\n", __func__, sec->cmd_param[0], ret);

		fts_delay(1);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_mainscreen_disable_cmd(struct fts_ts_info *info, bool on)
{
	struct sec_cmd_data *sec = &info->sec;
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
		input_err(true, &info->client->dev, "%s failed. ret: %d\n", __func__, ret);
	else
		input_info(true, &info->client->dev, "%s: reg:%d, ret: %d\n", __func__, sec->cmd_param[0], ret);
	fts_delay(1);
}

static void set_mainscreen_disable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 2) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0]==1){
			set_mainscreen_disable_cmd(info, 1);
		}else{
			set_mainscreen_disable_cmd(info, 0);
		}
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_rotation_status(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		int status = sec->cmd_param[0] % 2;

		if (status)
			fts_enable_feature(info, FTS_FEATURE_DUAL_SIDE_GUSTURE, true);
		else
			fts_enable_feature(info, FTS_FEATURE_DUAL_SIDE_GUSTURE, false);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
#ifdef FTS_SUPPORT_STRINGLIB
	unsigned short addr = FTS_CMD_STRING_ACCESS;
#endif
	int ret = 0;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);

		snprintf(buff, sizeof(buff), "%s", "TSP turned off");

		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0]) {
		info->fts_mode |= FTS_MODE_SPAY;
		info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_SPAY;
	} else {
		info->fts_mode &= ~FTS_MODE_SPAY;
		info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_SPAY);
	}

#ifdef FTS_SUPPORT_STRINGLIB
	ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
#endif
	if (ret < 0) {
		input_info(true, &info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

out:
	info->lowpower_mode = check_lowpower_mode(info);

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void aod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
#ifdef FTS_SUPPORT_STRINGLIB
	unsigned short addr = FTS_CMD_STRING_ACCESS;
#endif
	int ret = 0;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);

		snprintf(buff, sizeof(buff), "%s", "TSP turned off");

		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0]) {
		info->fts_mode |= FTS_MODE_AOD;
		info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_BLACK_UI;
	} else {
		info->fts_mode &= ~FTS_MODE_AOD;
		info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_BLACK_UI);
	}

#ifdef FTS_SUPPORT_STRINGLIB
	ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
#endif
	if (ret < 0) {
		input_info(true, &info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

out:
	info->lowpower_mode = check_lowpower_mode(info);

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#define	ORIENTATION_0	0
#define	ORIENTATION_90	1
#define	ORIENTATION_180	2
#define	ORIENTATION_270	3

static void lcd_orientation(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}else {
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	switch (sec->cmd_param[0]) {
	case ORIENTATION_0:
		input_info(true, &info->client->dev, "%s: Get 0 degree LCD orientation.\n", __func__);
		break;
	case ORIENTATION_90:
		input_info(true, &info->client->dev, "%s: Get 90 degree LCD orientation.\n", __func__);
		break;
	case ORIENTATION_180:
		input_info(true, &info->client->dev, "%s: Get 180 degree LCD orientation.\n", __func__);
		break;
	case ORIENTATION_270:
		input_info(true, &info->client->dev, "%s: Get 270 degree LCD orientation.\n", __func__);
		break;
	default:
		input_err(true, &info->client->dev, "%s: LCD orientation value error.\n", __func__);
		break;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void delay(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->delay_time = sec->cmd_param[0];

	input_info(true, &info->client->dev, "%s: delay time is %d\n", __func__, info->delay_time);
	snprintf(buff, sizeof(buff), "%d", info->delay_time);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void debug(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->debug_string = sec->cmd_param[0];

	input_info(true, &info->client->dev, "%s: command is %d\n", __func__, info->debug_string);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#ifdef PAT_CONTROL
void fts_set_pat_magic_number(struct fts_ts_info *info)
{
	input_info(true, &info->client->dev, "%s\n",__func__);
	set_pat_cal_count(info, PAT_MAGIC_NUMBER);
	input_info(true, &info->client->dev, "%s: write to nvm %X\n",__func__,PAT_MAGIC_NUMBER);
}
#endif

static void run_force_calibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	bool touch_on = false;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto autotune_fail;
	}

	if (info->rawdata_read_lock == 1){
		input_info(true, &info->client->dev, "%s: ramdump mode is runing, %d\n", __func__, info->rawdata_read_lock);
		goto autotune_fail;
	}

	if (info->touch_count > 0) {
		touch_on = true;
		input_info(true, info->dev, "%s: finger on touch(%d)\n", __func__, info->touch_count);
	}

	disable_irq(info->irq);

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

	fts_command(info,FTS_CMD_FORCE_AUTOTUNE);
	info->o_afe_ver = info->afe_ver;

	if (touch_on) {
		input_info(true, info->dev, "%s: finger! do not run autotune\n", __func__);
	} else {
		input_info(true, info->dev, "%s: run autotune\n", __func__);
#ifdef PAT_CONTROL
		fts_execute_autotune(info);

		/* count the number of calibration */
		info->cal_count++;

		/*change  from virtual pat to real pat by forced calibarion by LCIA */
		if(info->cal_count >= PAT_MAGIC_NUMBER) info->cal_count = 1;
		/* max value of forced calibration is limited */
		if(info->cal_count > PAT_MAX_LCIA) info->cal_count = PAT_MAX_LCIA;

		input_info(true, &info->client->dev, "%s: write to nvm %X\n",
					__func__, info->cal_count);

		set_pat_cal_count(info, info->cal_count);

		/* get ic information */
		info->fts_get_version_info(info);
		set_pat_tune_ver(info, info->fw_main_version_of_ic);
		get_pat_data(info);

		input_info(true, &info->client->dev, "%s: tune_fix_ver [0x%04X] \n", __func__, info->tune_fix_ver);
#else
		fts_execute_autotune(info);
#endif
	}

	fts_command(info, SENSEON);

#ifdef FTS_SUPPORT_WATER_MODE
	fts_fw_wait_for_event(info, STATUS_EVENT_WATER_SELF_DONE);
#else
	fts_fw_wait_for_event (info, STATUS_EVENT_FORCE_CAL_DONE_D3);
#endif
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

	fts_interrupt_set(info, INT_ENABLE);
	enable_irq(info->irq);

	if (touch_on) {
		snprintf(buff, sizeof(buff), "NG_FINGER_ON");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		sec->cmd_param[0] = 1;
		set_status_pureautotune(device_data);
		sec_cmd_set_default_result(sec);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;

autotune_fail:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
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
		pCMD[1] = buf[2];
		pCMD[2] = buf[1];
	} else
		return -1;

	ret = fts_read_reg(info, &pCMD[0], 3, buf, 9);
	if (ret < 0)
		return -2;

	for (i = 0 ; i < info->board->num_touchkey ; i++)
		if (info->board->touchkey[i].keycode == keycode) {
			return *(short *)&buf[(info->board->touchkey[i].value - 1) * 2 + 1];
		}

	return -3;
}

static ssize_t touchkey_recent_strength(struct device *dev,
				       struct device_attribute *attr, char *buf) {
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int value = 0;

	if (info->touch_stopped) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
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
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
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
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
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
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
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
	pCMD[2] = FTS_SI_SS_KEY_THRESHOLD;
	ret = fts_read_reg(info, &pCMD[0], 3, buf, 3);
	if (ret >= 0) {
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
	input_dbg(true, &info->client->dev, "%s, %d\n", __func__, data);

	if (ret != 1) {
		input_err(true, &info->client->dev, "%s, %d err\n",
			__func__, __LINE__);
		return size;
	}

	if (data != 0 && data != 1) {
		input_err(true, &info->client->dev, "%s wrong cmd %x\n",
			__func__, data);
		return size;
	}

	ret = info->board->led_power(info, (bool)data);
	if (ret) {
		input_err(true, &info->client->dev, "%s: Error turn on led %d\n",
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
