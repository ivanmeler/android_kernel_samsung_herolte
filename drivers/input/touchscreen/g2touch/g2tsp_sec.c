#ifdef SEC_TSP_FACTORY_TEST
#define CHIP_INFO		"G2"
#define G2_DEVICE_NAME  "g2touch"
#define FW_RELEASE_DATE "1028"

static u16 tsp_dac[1024];
static u16 tsp_adc[1024];
static u16 tsp_area[1024];

#if 1	// NEW
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

// G2touch
static void run_dac_read(void *device_data);
static void run_adc_read(void *device_data);
static void run_area_read(void *device_data);
static void get_dac(void *device_data);
static void get_adc(void *device_data);
static void get_area(void *device_data);
static void get_diff_dac(void *device_data);
static void get_refDac(void *device_data);

static void run_factory_cal(void *device_data);

static void tsp_reset(void *device_data);
static void regb_read(void *device_data);
static void regb_write(void *device_data);

static void not_support_cmd(void *device_data);


struct sec_cmd g2tsp_commands[] = {
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
#if 0
	{SEC_CMD("run_reference_read", not_support_cmd),},
	{SEC_CMD("get_reference", not_support_cmd),},
	{SEC_CMD("run_rawcap_read", not_support_cmd),},
	{SEC_CMD("get_rawcap", not_support_cmd),},
	{SEC_CMD("run_delta_read", not_support_cmd),},
	{SEC_CMD("get_delta", not_support_cmd),},
	{SEC_CMD("get_pat_information", not_support_cmd),},

	{SEC_CMD("set_dead_zone", not_support_cmd),},
	{SEC_CMD("dead_zone_enable", not_support_cmd),},
	{SEC_CMD("run_force_calibration", not_support_cmd),},
#endif
	// G2touch 
	
	{SEC_CMD("run_dac_read", run_dac_read),},
	{SEC_CMD("run_adc_read", run_adc_read),},
	{SEC_CMD("run_area_read", run_area_read),},
	{SEC_CMD("get_dac", get_dac),},
	{SEC_CMD("get_adc", get_adc),},
	{SEC_CMD("get_area", get_area),},
	{SEC_CMD("get_diff_dac", get_diff_dac),},
	{SEC_CMD("get_refDac", get_refDac),},
	
	{SEC_CMD("run_factory_cal", run_factory_cal),},
	
	{SEC_CMD("tsp_reset", tsp_reset),},
	{SEC_CMD("regb_read", regb_read),},
	{SEC_CMD("regb_write", regb_write),},

	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, info->pdev, "%s: \"%s\"\n", __func__, buff);
}


static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[64] = { 0 };
	int retval = 0;

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
#if 0
	switch (info->cmd_param[0]) 
	{
		case BUILT_IN:
			info->firmware_download=1;
			info->factory_cal = 0;
			schedule_delayed_work(&info->fw_work, FW_RECOVER_DELAY);
			break;
		case UMS:
			input_info(true, info->pdev, "%s Storage Firmware update! \r\n", __func__);
			g2tsp_firmware_from_storage(info);
			break;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

#else
	if (info->power_status == G2TSP_TS_STATE_POWER_OFF) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	retval = g2tsp_ts_firmware_update_on_hidden_menu(info, sec->cmd_param[0]);
	if (retval < 0) {
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, &info->client->dev, "%s: failed [%d]\n", __func__, retval);
	} else {
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &info->client->dev, "%s: success [%d]\n", __func__, retval);
	}

#endif

	input_info(true, info->pdev, "%s: success [%d]\n", __func__, retval);

	return;
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
    snprintf(buff, sizeof(buff), "%s%06x",  CHIP_INFO, info->platform_data->fw_version[1]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	snprintf(buff, sizeof(buff), "%s%06x", CHIP_INFO, info->fw_ver_ic[1]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, info->pdev, "%s: %s\n", __func__, buff);

}

static void get_config_ver(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[20] = { 0 };

	// TO DO !!!!
	snprintf(buff, sizeof(buff), "%s_%s_%s", G2_DEVICE_NAME, CHIP_INFO, FW_RELEASE_DATE);

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, info->pdev, "%s: %s\n", __func__, buff);

}

static void get_threshold(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	unsigned char buff[16] = { 0 };
	unsigned short finger_threshold = 0;

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	finger_threshold = 99;

	snprintf(buff, sizeof(buff), "%d", finger_threshold);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);

}

static void module_off_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[3] = { 0 };
	int ret = 0;

	// TO DO !!!!
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
	
	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

static void module_on_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[3] = { 0 };
	int ret = 0;

	// TO DO !!!!
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

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };

	// TO DO !!!!
	strncpy(buff, "G2TOUCH", sizeof(buff));

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };

	// TO DO !!!!
	snprintf(buff, sizeof(buff), "%s", "G7400");

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	snprintf(buff, sizeof(buff), "%d", 9);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	snprintf(buff, sizeof(buff), "%d", 9);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

static void get_checksum_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	snprintf(buff, sizeof(buff), "%02X%02X%02X%02X", 00, 00, 00, 00);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

// G2touch - Start
static int g2tsp_check_index(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);

	char buff[16] = { 0 };
	int node;

	// TO DO!
	if (sec->cmd_param[0] < 0
		|| sec->cmd_param[0] >= 15	//info->SenseChannelLength
		|| sec->cmd_param[1] < 0
		|| sec->cmd_param[1] >= 30) { //info->ForceChannelLength) {

		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		
		input_err(true, info->pdev, "%s: parameter error: %u,%u\n",
						__func__, sec->cmd_param[0], sec->cmd_param[1]);
		node = -1;
		return node;
	}
//	node = sec->cmd_param[1] * info->SenseChannelLength + sec->cmd_param[0];
	node = sec->cmd_param[1] * 15 + sec->cmd_param[0];
	input_info(true, info->pdev, "%s: node = %d\n", __func__, node);

	return node;
}

static int g2tsp_GetFrameData(struct g2tsp_info *ts, u8 type)
{
	int ret = -1;
	int time_out = 0;

	input_info(true, ts->pdev, "%s(%d) Enter+ \r\n", __func__, type);
	dbgMsgToCheese("%s(%d) Enter+ \r\n", __func__, type);

	ts->workMode		= eMode_FactoryTest;
	ts->calfinish		= 0;
	ts->factory_flag	= type & 0x7f;

	g2tsp_write_reg(ts, 0x00, 0x10);
	g2tsp_write_reg(ts, 0xce, 0x05);
	g2tsp_write_reg(ts, 0x30, 0x08);	

	if(type == TSP_FACTORY_FLAG){
		g2tsp_write_reg(ts, 0x30, 0x18);	
		g2tsp_write_reg(ts, 0xc6, 0x04);	
	}

	udelay(10);
	g2tsp_write_reg(ts, 0x00, 0x00);


	while (1)
	{
		msleep(10);
		if (ts->factory_flag & TSP_GET_FRAME_FLAG) {
			int i;

			input_info(true, ts->pdev, "Get Frame Data type(%d)!!",type);

			if(type == TSP_DAC_FLAG) {
				memcpy(tsp_dac, ts->dacBuf, sizeof(tsp_dac));
			} else if (type == TSP_ADC_FLAG) {
				memcpy(tsp_adc, ts->dacBuf, sizeof(tsp_dac));
			} else if (type == TSP_AREA_FLAG) {
				memcpy(tsp_area, ts->dacBuf, sizeof(tsp_dac));
			} else if (type == TSP_FACTORY_FLAG) {
				memcpy(tsp_area, ts->dacBuf, sizeof(tsp_dac));
			}

			for (i = 0 ; i < 390 ; i++) {
				printk("[%04d]", ts->dacBuf[i]);

				if ((i % 15) == 14 )
					printk("\r\n");
			}

			ret = 0;
			break;
		}

		if (time_out++ > 500) { //wait 5sec
			input_err(true, ts->pdev, "Get Frame Data Time Out type(%d)!! \r\n", type);
			dbgMsgToCheese("Get Frame Data Time Out type(%d)!! \r\n", type);
			break;
		}
	}

	ts->workMode		= eMode_Normal;
	g2tsp_write_reg(ts, 0x00, 0x10);
	g2tsp_write_reg(ts, 0xce, 0x00);
	g2tsp_write_reg(ts, 0xc6, 0x00);
	g2tsp_write_reg(ts, 0x30, 0x08);
	udelay(10);
	g2tsp_write_reg(ts, 0x00, 0x00);

	return ret;	
}

static void run_dac_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	// TO DO !!!!

	ret = g2tsp_GetFrameData(info,TSP_DAC_FLAG);
		
	if (ret == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else {
		input_err(true, info->pdev, "%s: Error\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

static void run_adc_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	// TO DO !!!!

	ret = g2tsp_GetFrameData(info, TSP_ADC_FLAG);
		
	if (ret == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else {
		input_err(true, info->pdev, "%s: Error\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}


static void run_area_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	// TO DO !!!!

	ret = g2tsp_GetFrameData(info, TSP_AREA_FLAG);
		
	if (ret == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else {
		input_err(true, info->pdev, "%s: Error\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}


static void get_dac(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };
    unsigned int val;
    int node;

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	node = g2tsp_check_index(info);
	if (node < 0)
		return;
	
//    val = tsp_dac[(y*15)+x];
    val = tsp_dac[node];
    snprintf(buff, sizeof(buff), "%u", val);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

static void get_adc(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };
    unsigned int val;
    int node;

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	node = g2tsp_check_index(info);
	if (node < 0)
		return;
	
//    val = tsp_dac[(y*15)+x];
    val = tsp_adc[node];
    snprintf(buff, sizeof(buff), "%u", val);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

static void get_area(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };
    unsigned int val;
    int node;

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	node = g2tsp_check_index(info);
	if (node < 0)
		return;
	
//    val = tsp_dac[(y*15)+x];
    val = tsp_area[node];
    snprintf(buff, sizeof(buff), "%u", val);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}


static void get_diff_dac(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };
    int val;
    int node;

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	node = g2tsp_check_index(info);
	if (node < 0)
		return;
	
//    val = Refence_dac[(y*15)+x] - tsp_dac[(y*15)+x];
    val = Refence_dac[node] - tsp_dac[node];
    snprintf(buff, sizeof(buff), "%d", val);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

static void get_refDac(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };
    unsigned int val;
    int node;

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	node = g2tsp_check_index(info);
	if (node < 0)
		return;
	
    val = Refence_dac[node];
    snprintf(buff, sizeof(buff), "%u", val);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}


static void run_factory_cal(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	g2tsp_FactoryCal(info);

    snprintf(buff, sizeof(buff), "%s", "OK");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}


// G2touch Debugging - Start
static void tsp_reset(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	g2tsp_hw_power(0);
	g2tsp_hw_power(1);
//	g2tsp_reset(info);	// ?????

    g2tsp_reset_register(info);

    snprintf(buff, sizeof(buff), "%s", "OK");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

static void regb_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };
	u8 data;

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	g2tsp_read_reg(info, (u8)sec->cmd_param[0], &data);

    snprintf(buff, sizeof(buff), ",0x%x", (int)data);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}

static void regb_write(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct g2tsp_info *info = container_of(sec, struct g2tsp_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	// TO DO !!!!
	g2tsp_write_reg(info, (u8)sec->cmd_param[0], (u8)sec->cmd_param[1]);

    snprintf(buff, sizeof(buff), ", 0x%x 0x%x", sec->cmd_param[0], sec->cmd_param[1]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, info->pdev, "%s: %s\n", __func__, buff);
}
// G2touch Debugging - END


// G2touch - END

#else	// OLD
extern struct class *sec_class;
struct device *sec_touchscreen_dev;

#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func

struct tsp_cmd {
    struct list_head	list;
    const char	*cmd_name;
    void	(*cmd_func)(void *device_data);
};

static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_config_ver(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_threshold(void *device_data);
static void get_key_threshold(void *device_data);
static void get_reference(void *device_data);
static void get_normal(void *device_data);
static void get_delta(void *device_data);
static void run_reference_read(void *device_data);
static void run_normal_read(void *device_data);
static void run_delta_read(void *device_data);
static void not_support_cmd(void *device_data);

static void run_dac_read(void *device_data);
static void run_adc_read(void *device_data);
static void run_area_read(void *device_data);
static void get_dac(void *device_data);
static void get_adc(void *device_data);
static void get_area(void *device_data);

static void get_diff_dac(void *device_data);
static void get_refDac(void *device_data);


static void run_factory_cal(void *device_data);
static void tsp_reset(void *device_data);


static void regb_read(void *device_data);
static void regb_write(void *device_data);

int g2tsp_hw_power(int on);

struct tsp_cmd tsp_cmds[] = {
    {TSP_CMD("fw_update", fw_update),},
    {TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
    {TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
    {TSP_CMD("get_config_ver", get_config_ver),},
    {TSP_CMD("get_x_num", get_x_num),},
    {TSP_CMD("get_y_num", get_y_num),},
    {TSP_CMD("get_chip_vendor", get_chip_vendor),},
    {TSP_CMD("get_chip_name", get_chip_name),},	
    {TSP_CMD("get_threshold", get_threshold),},
    {TSP_CMD("get_key_threshold", get_key_threshold),},
    {TSP_CMD("module_off_master", not_support_cmd),},
    {TSP_CMD("module_on_master", not_support_cmd),},
    {TSP_CMD("module_off_slave", not_support_cmd),},
    {TSP_CMD("module_on_slave", not_support_cmd),},	
    {TSP_CMD("run_reference_read", run_reference_read),},
    {TSP_CMD("run_normal_read", run_normal_read),},
    {TSP_CMD("run_delta_read", run_delta_read),},	
    {TSP_CMD("get_reference", get_reference),},
    {TSP_CMD("get_normal", get_normal),},
    {TSP_CMD("get_delta", get_delta),},	
    {TSP_CMD("not_support_cmd", not_support_cmd),},

    /* add G2TSP */
    {TSP_CMD("run_factory_cal", run_factory_cal),},
    {TSP_CMD("run_dac_read", run_dac_read),},
    {TSP_CMD("run_area_read", run_area_read),},	// size of touch.
    {TSP_CMD("run_adc_read", run_adc_read),},
    {TSP_CMD("get_dac", get_dac),},
    {TSP_CMD("get_area", get_area),},
    {TSP_CMD("get_adc", get_adc),},
    {TSP_CMD("get_refdac", get_refDac),},
    {TSP_CMD("get_diff_dac", get_diff_dac),},
    {TSP_CMD("regb_read", regb_read),},
    {TSP_CMD("regb_write", regb_write),},
    {TSP_CMD("tsp_reset", tsp_reset),},
};


static void set_cmd_result(struct g2tsp_info *info, char *buff, int len)
{
    strncat(info->cmd_result, buff, len);
}

static void set_default_result(struct g2tsp_info *info)
{
    char delim = ':';
    memset(info->cmd_result, 0x00, ARRAY_SIZE(info->cmd_result));
    memcpy(info->cmd_result, info->cmd, strlen(info->cmd));
    strncat(info->cmd_result, &delim, 1);
}

static void not_support_cmd(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};

    set_default_result(info);
    sprintf(buff, "%s", "NA");
    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
    info->cmd_state = 4;
    input_info(true, info->pdev, "%s: \"%s(%d)\"\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));
}

static int g2tsp_GetFrameData(struct g2tsp_info *ts, u8 type)
{
    int ret=-1;
    int time_out = 0;

    input_info(true, ts->pdev, "%s(%d) Enter+ \r\n", __func__, type);
    dbgMsgToCheese("%s(%d) Enter+ \r\n", __func__, type);

    ts->workMode		= eMode_FactoryTest;
    ts->calfinish		= 0;
    ts->factory_flag	= type & 0x7f;

    g2tsp_write_reg(ts, 0x00, 0x10);
    g2tsp_write_reg(ts, 0xce, 0x05);
    g2tsp_write_reg(ts, 0x30, 0x08);	
    if(type == TSP_FACTORY_FLAG){
        g2tsp_write_reg(ts, 0x30, 0x18);	
        g2tsp_write_reg(ts, 0xc6, 0x04);	
    }

    udelay(10);
    g2tsp_write_reg(ts, 0x00, 0x00);


    while(1)
    {
        msleep(10);	//wait 10ms
        if(ts->factory_flag & TSP_GET_FRAME_FLAG) 
        {
            int i;

            input_info(true, ts->pdev, "Get Frame Data type(%d)!! \r\n",type);

            if(type == TSP_DAC_FLAG)
            {
                memcpy(tsp_dac, ts->dacBuf, sizeof(tsp_dac));
            } 
            else if (type == TSP_ADC_FLAG) 
            {
                memcpy(tsp_adc, ts->dacBuf, sizeof(tsp_dac));
            } 
            else if (type == TSP_AREA_FLAG) 
            {
                memcpy(tsp_area, ts->dacBuf, sizeof(tsp_dac));
            } 
            else if (type == TSP_FACTORY_FLAG) 
            {
                memcpy(tsp_area, ts->dacBuf, sizeof(tsp_dac));
            }

            for(i=0; i<390; i++) 
            {
                printk("[%04d]", ts->dacBuf[i]);
                if((i%15)== 14 )	printk("\r\n");
            }

            ret = 0;
            break;
        }

        if(time_out++ > 500) 
        { //wait 5sec
            input_info(true, ts->pdev, "Get Frame Data Time Out type(%d)!! \r\n", type);
            dbgMsgToCheese("Get Frame Data Time Out type(%d)!! \r\n", type);
            break;
        }

    }

    ts->workMode		= eMode_Normal;
    g2tsp_write_reg(ts, 0x00, 0x10);
    g2tsp_write_reg(ts, 0xce, 0x00);
    g2tsp_write_reg(ts, 0xc6, 0x00);
    g2tsp_write_reg(ts, 0x30, 0x08);
    udelay(10);
    g2tsp_write_reg(ts, 0x00, 0x00);

    return ret;	
}



static void get_fw_ver_bin(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};

    set_default_result(info);
    snprintf(buff, sizeof(buff), "%s%06x",  CHIP_INFO, info->platform_data->fw_version[1]);

    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
    info->cmd_state = 2;
    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));	
}

static void get_fw_ver_ic(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};	

    set_default_result(info);
    snprintf(buff, sizeof(buff), "%s%06x", CHIP_INFO, info->fw_ver_ic[1]);

    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
    info->cmd_state = 2;
    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));
}

static void get_config_ver(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[20] = {0};

    set_default_result(info);
    snprintf(buff, sizeof(buff), "%s_%s_%s", G2_DEVICE_NAME, CHIP_INFO, FW_RELEASE_DATE);
    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
    info->cmd_state = 2;
    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));

    return;
}

static void get_x_num(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};

    set_default_result(info);

    snprintf(buff, sizeof(buff), "%u", G2TSP_X_CH_NUM);
    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
    info->cmd_state = 2;
    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));
}

static void get_y_num(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};

    set_default_result(info);

    snprintf(buff, sizeof(buff), "%u", G2TSP_Y_CH_NUM);
    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
    info->cmd_state = 2;
    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));
}

static void get_chip_vendor(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};

    set_default_result(info);

    snprintf(buff, sizeof(buff), "%s", CHIP_INFO);
    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
    info->cmd_state = 2;
    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));
}

static void get_chip_name(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};

    set_default_result(info);

    snprintf(buff, sizeof(buff), "%s", CHIP_INFO);
    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
    info->cmd_state = 2;
    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));
}

static void get_threshold(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};

    set_default_result(info);
    snprintf(buff, sizeof(buff), "%d", 80);

    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
    info->cmd_state = 2;
    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));
}

static void get_key_threshold(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};

    set_default_result(info);
    snprintf(buff, sizeof(buff), "%u", 20);

    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
    info->cmd_state = 2;
    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));
}

static void run_reference_read(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;

    set_default_result(info);
    info->cmd_state = 2;
}

static void run_dac_read(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;

    set_default_result(info);

    if(g2tsp_GetFrameData(info,TSP_DAC_FLAG)<0)
    return;

    info->cmd_state = 2;
}

static void run_area_read(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;

    set_default_result(info);

    if(g2tsp_GetFrameData(info,TSP_AREA_FLAG)<0)
    return;


    info->cmd_state = 2;
}


static void run_adc_read(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;

    set_default_result(info);

    if(g2tsp_GetFrameData(info,TSP_ADC_FLAG) < 0)
    return;

    info->cmd_state = 2;
}

static void get_diff_dac(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};
    int val;
    int x,y;

    set_default_result(info);
    x = info->cmd_param[0];
    y = info->cmd_param[1];

    val = Refence_dac[(y*15)+x] - tsp_dac[(y*15)+x];
    snprintf(buff, sizeof(buff), "%u", val);
    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

    info->cmd_state = 2;

    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));	
}

static void get_refDac(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};
    unsigned int val;
    int x,y;

    set_default_result(info);
    x = info->cmd_param[0];
    y = info->cmd_param[1];

    val = Refence_dac[(y*15)+x];
    snprintf(buff, sizeof(buff), "%u", val);
    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

    info->cmd_state = 2;
    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));	
}


static void get_dac(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};
    unsigned int val;
    int x,y;

    set_default_result(info);
    x = info->cmd_param[0];
    y = info->cmd_param[1];

    val = tsp_dac[(y*15)+x];
    snprintf(buff, sizeof(buff), "%u", val);
    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));


    info->cmd_state = 2;
    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));	
}

static void get_area(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};
    unsigned int val;
    int x,y;

    set_default_result(info);
    x = info->cmd_param[0];
    y = info->cmd_param[1];

    val = tsp_area[(y*15)+x];
    snprintf(buff, sizeof(buff), "%u", val);
    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

    info->cmd_state = 2;
    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));	
}

static void get_adc(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};
    unsigned int val;
    int x,y;

    set_default_result(info);
    x = info->cmd_param[0];
    y = info->cmd_param[1];

    val = tsp_adc[(y*15)+x];
    snprintf(buff, sizeof(buff), "%u", val);
    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

    info->cmd_state = 2;
    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));	
}


static void run_factory_cal(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;

    set_default_result(info);
    g2tsp_FactoryCal(info);
    info->cmd_state = 2;
}

static void run_normal_read(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;

    set_default_result(info);

    info->cmd_state = 2;
}

static void run_delta_read(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;

    set_default_result(info);

    info->cmd_state = 2;
}

static void get_reference(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};

    set_default_result(info);
    snprintf(buff, sizeof(buff), "%u", 1);
    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
    info->cmd_state = 2;

    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));
}
static void get_normal(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};
	
    set_default_result(info);
    snprintf(buff, sizeof(buff), "%u", 1);

    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
    info->cmd_state = 2;

    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));

}

static void get_delta(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char buff[16] = {0};

    set_default_result(info);
    snprintf(buff, sizeof(buff), "%u", 0);

    set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
    info->cmd_state = 2;

    input_info(true, info->pdev, "%s: %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));
}

int g2tsp_firmware_from_storage(struct g2tsp_info *ts)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fw_size, nread;
	int error = 0;
//	int i;
	const char* firmware_name = TSP_FW_DEFALT_PATH;//ts->platform_data->firmware_name;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (firmware_name) {
		input_info(true, ts->pdev, "%s Firmware Filename : %s\r\n", __func__, firmware_name);

		fp = filp_open(firmware_name, O_RDONLY, S_IRUSR);
		if (IS_ERR(fp)) {
			input_info(true, ts->pdev, "%s: failed to open %s.\n", __func__, firmware_name);
			error = -ENOENT;
			goto open_err;
		}

		fw_size = fp->f_path.dentry->d_inode->i_size;

		if (0 < fw_size) {
			unsigned char *fw_data;

			fw_data = kzalloc(fw_size, GFP_KERNEL);
			nread = vfs_read(fp, (char __user *)fw_data, fw_size, &fp->f_pos);

			input_info(true, ts->pdev, "%s: start, file path %s, size %ld Bytes\n",
								__func__, firmware_name, fw_size);
#if 0
			// for debugging
			for (i = 0 ; i < (nread / 16) ; i += 16) {
				
				input_info(true, ts->pdev,
					"FW Header %x [%02x] [%02x] [%02x] [%02x] [%02x] [%02x] [%02x] [%02x] "
					"[%02x] [%02x] [%02x] [%02x] [%02x] [%02x] [%02x] [%02x]\n",
					i, fw_data[i+0], fw_data[i+1], fw_data[i+2], fw_data[i+3],
					fw_data[i+4], fw_data[i+5], fw_data[i+6], fw_data[i+7],
					fw_data[i+8], fw_data[i+9], fw_data[i+10], fw_data[i+11],
					fw_data[i+12], fw_data[i+13], fw_data[i+14], fw_data[i+15]);
			}
#endif
			if (nread != fw_size) {
				input_info(true, ts->pdev, "%s: failed to read firmware file, nread %ld Bytes\n", __func__, nread);
				error = -EIO;
			} else {
				if ((ts->fw_ver_ic[0] == 0) && (ts->fw_ver_ic[1] == 0)) {
					input_info(true, ts->pdev, "TSP Reset !! \r\n");
					g2tsp_write_reg(ts, 0x0, 0x10);
					udelay(5);
					g2tsp_write_reg(ts, 0x0, 0x00);
					mdelay(150);
				}

				G7400_firmware_update(ts, fw_data, fw_size);

				g2tsp_version_update(ts);

				if(ts->factory_cal == 1) {
					g2tsp_FactoryCal(ts);
				}
			}
			kfree(fw_data);

		}

		filp_close(fp, NULL);
	}

	open_err:
	set_fs(old_fs);
	return error;
}


static void fw_update(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char result[16] = {0};

    input_info(true, info->pdev, "%s\n", __func__);

    switch (info->cmd_param[0]) 
    {
        case BUILT_IN:
            info->firmware_download=1;
            info->factory_cal = 0;
            schedule_delayed_work(&info->fw_work, FW_RECOVER_DELAY);
            break;
        case UMS:
            input_info(true, info->pdev, "%s Storage Firmware update! \r\n", __func__);
            g2tsp_firmware_from_storage(info);
            break;
    }
    info->cmd_state = 2;
    snprintf(result, sizeof(result) , "%s", "OK");
    set_cmd_result(info, result, strnlen(result, sizeof(result)));
    return;
}

static void regb_read(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char result[16] = {0};
    u8 data;

    g2tsp_read_reg(info, (u8)info->cmd_param[0],&data);

    snprintf(result, sizeof(result) , ", 0x%x", (int)data);
    set_cmd_result(info, result, strnlen(result, sizeof(result)));
    info->cmd_state = 2;

    return;
}

static void regb_write(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char result[16] = {0};

    g2tsp_write_reg(info, (u8)info->cmd_param[0],(u8)info->cmd_param[1]);
    snprintf(result, sizeof(result) , ", 0x%x 0x%x", info->cmd_param[0], info->cmd_param[1]);
    set_cmd_result(info, result, strnlen(result, sizeof(result)));

    info->cmd_state = 2;

    return;
}

static void tsp_reset(void *device_data)
{
    struct g2tsp_info *info = (struct g2tsp_info *)device_data;
    char result[16] = {0};

    g2tsp_hw_power(0);
    g2tsp_hw_power(1);
    g2tsp_reset_register(info);

    snprintf(result, sizeof(result) , "tsp reset");
    set_cmd_result(info, result, strnlen(result, sizeof(result)));

    info->cmd_state = 2;

    return;
}

static ssize_t store_cmd(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
    struct g2tsp_info *info = current_ts;

    char *cur, *start, *end;
    char buff[TSP_CMD_STR_LEN] = {0};
    int len, i;
    struct tsp_cmd *tsp_cmd_ptr = NULL;
    char delim = ',';
    bool cmd_found = false;
    int param_cnt = 0;
    int ret;

    if (info->cmd_is_running == true) 
    {
        input_info(true, info->pdev, "tsp_cmd: other cmd is running.\n");
        goto err_out;
    }

    /* check lock  */
    mutex_lock(&info->cmd_lock);
    info->cmd_is_running = true;
    mutex_unlock(&info->cmd_lock);

    info->cmd_state = 1;

    for (i = 0; i < ARRAY_SIZE(info->cmd_param); i++)
    info->cmd_param[i] = 0;

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

    /* find command */
    list_for_each_entry(tsp_cmd_ptr, &info->cmd_list_head, list) 
    {
        if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) 
        {
            cmd_found = true;
            break;
        }
    }

    /* set not_support_cmd */
    if (!cmd_found) 
    {
        list_for_each_entry(tsp_cmd_ptr, &info->cmd_list_head, list) 
        {
            if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
            break;
        }
    }

    /* parsing parameters */
    if (cur && cmd_found) 
    {
        cur++;
        start = cur;
        memset(buff, 0x00, ARRAY_SIZE(buff));
        do 
        {
            if (*cur == delim || cur - buf == len) 
            {
                end = cur;
                memcpy(buff, start, end - start);
                *(buff + strlen(buff)) = '\0';
                ret = kstrtoint(buff, 10,\
                info->cmd_param + param_cnt);
                start = cur + 1;
                memset(buff, 0x00, ARRAY_SIZE(buff));
                param_cnt++;
            }
            cur++;
        } while (cur - buf <= len);
    }

    input_info(true, info->pdev, "%s  cmd = %s\n", __func__, tsp_cmd_ptr->cmd_name);
    for (i = 0; i < param_cnt; i++)
    input_info(true, info->pdev, "cmd param %d= %d\n", i, info->cmd_param[i]);

    tsp_cmd_ptr->cmd_func(info);

    err_out:
    return count;
}

static ssize_t show_cmd_status(struct device *dev, struct device_attribute *devattr, char *buf)
{
    struct g2tsp_info *info = current_ts;
    //struct g2tsp_info *info = dev_get_drvdata(dev);
    char buff[16] = {0};

    input_info(true, info->pdev, "tsp cmd: status:%d\n",
    info->cmd_state);

    if (info->cmd_state == 0)
    snprintf(buff, sizeof(buff), "WAITING");

    else if (info->cmd_state == 1)
    snprintf(buff, sizeof(buff), "RUNNING");

    else if (info->cmd_state == 2)
    snprintf(buff, sizeof(buff), "OK");

    else if (info->cmd_state == 3)
    snprintf(buff, sizeof(buff), "FAIL");

    else if (info->cmd_state == 4)
    snprintf(buff, sizeof(buff), "NOT_APPLICABLE");

    return snprintf(buf, TSP_BUF_SIZE, "%s\n", buff);
}

static ssize_t show_cmd_result(struct device *dev, struct device_attribute* devattr, char *buf)
{
    struct g2tsp_info *info = current_ts;
    //struct g2tsp_info *info = dev_get_drvdata(dev);

    input_info(true, info->pdev, "tsp cmd: result: %s\n", info->cmd_result);

    mutex_lock(&info->cmd_lock);
    info->cmd_is_running = false;
    mutex_unlock(&info->cmd_lock);

    info->cmd_state = 0;

    return snprintf(buf, TSP_BUF_SIZE, "%s\n", info->cmd_result);
}


static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);

static struct attribute *sec_touch_attributes[] = 
{
    &dev_attr_cmd.attr,
    &dev_attr_cmd_status.attr,
    &dev_attr_cmd_result.attr,
    NULL,
};

static struct attribute_group sec_touch_attributes_group = 
{
    .attrs = sec_touch_attributes,
};
#endif

#endif
