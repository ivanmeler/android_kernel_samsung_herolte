#include "../ssp.h"

#define	VENDOR		"AMS"

#if defined(CONFIG_SENSORS_SSP_TMD4903)
#define CHIP_ID 	"TMD4903"
#elif defined(CONFIG_SENSORS_SSP_TMD4904)
#define CHIP_ID 	"TMD4904"
#elif defined(CONFIG_SENSORS_SSP_TMD4905)
#define CHIP_ID 	"TMD4905"
#elif defined(CONFIG_SENSORS_SSP_TMD3725)
#define CHIP_ID 	"TMD3725"
#else
#define CHIP_ID 	"UNKNOWN"
#endif

#define PROX_ADC_BITS_NUM 		14

/*************************************************************************/
/* Functions                                                             */
/*************************************************************************/

static u16 get_proximity_rawdata(struct ssp_data *data)
{
	u16 uRowdata = 0;
	char chTempbuf[4] = { 0 };

	s32 dMsDelay = 20;
	memcpy(&chTempbuf[0], &dMsDelay, 4);

	if (data->bProximityRawEnabled == false) {
		send_instruction(data, ADD_SENSOR, PROXIMITY_RAW, chTempbuf, 4);
		msleep(200);
		uRowdata = data->buf[PROXIMITY_RAW].prox_raw[0];
		send_instruction(data, REMOVE_SENSOR, PROXIMITY_RAW,
			chTempbuf, 4);
	} else {
		uRowdata = data->buf[PROXIMITY_RAW].prox_raw[0];
	}

	return uRowdata;
}

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

static ssize_t proximity_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t proximity_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static ssize_t proximity_probe_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	bool probe_pass_fail = FAIL;

	if (data->uSensorState & (1 << PROXIMITY_SENSOR))
		probe_pass_fail = SUCCESS;
	else
		probe_pass_fail = FAIL;

	pr_info("[SSP]: %s - All sensor 0x%llx, prox_sensor %d\n",
		__func__, data->uSensorState, probe_pass_fail);
	
	return snprintf(buf, PAGE_SIZE, "%d\n", probe_pass_fail);
}

static ssize_t proximity_thresh_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: ProxThresh = hi - %u\n", data->uProxHiThresh);

	return sprintf(buf, "%u\n", data->uProxHiThresh);
}

static ssize_t proximity_thresh_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet, i=0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	
	while(i<PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while(i<16)
		prox_non_bits_mask += (1<<i++);

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, iRet);
	else {
		if(uNewThresh & prox_non_bits_mask)
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM,uNewThresh);
		else {
			uNewThresh &= prox_bits_mask;
			data->uProxHiThresh = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : hi - %u\n", __func__, data->uProxHiThresh);

	return size;
}

static ssize_t proximity_thresh_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: ProxThresh = lo - %u\n", data->uProxLoThresh);

	return sprintf(buf, "%u\n", data->uProxLoThresh);
}

static ssize_t proximity_thresh_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet, i=0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	
	while(i<PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while(i<16)
		prox_non_bits_mask += (1<<i++);

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, iRet);
	else {
		if(uNewThresh & prox_non_bits_mask)
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM,uNewThresh);
		else {
			uNewThresh &= prox_bits_mask;
			data->uProxLoThresh = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : lo - %u\n", __func__, data->uProxLoThresh);

	return size;
}

static ssize_t proximity_thresh_detect_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: ProxThresh = hidetect - %u\n", data->uProxHiThresh_detect);

	return sprintf(buf, "%u\n", data->uProxHiThresh_detect);
}

static ssize_t proximity_thresh_detect_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet, i=0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	
	while(i<PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while(i<16)
		prox_non_bits_mask += (1<<i++);

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, iRet);
	else {
		if(uNewThresh & prox_non_bits_mask)
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM,uNewThresh);
		else {
			uNewThresh &= prox_bits_mask;
			data->uProxHiThresh_detect = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : hidetect - %u\n", __func__, data->uProxHiThresh_detect);

	return size;
}

static ssize_t proximity_thresh_detect_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: ProxThresh = lodetect - %u\n", data->uProxLoThresh_detect);

	return sprintf(buf, "%u\n", data->uProxLoThresh_detect);
}

static ssize_t proximity_thresh_detect_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet, i=0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	
	while(i<PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while(i<16)
		prox_non_bits_mask += (1<<i++);

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, iRet);
	else {
		if(uNewThresh & prox_non_bits_mask)
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM,uNewThresh);
		else {
			uNewThresh &= prox_bits_mask;
			data->uProxLoThresh_detect = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : lodetect - %u\n", __func__, data->uProxLoThresh_detect);

	return size;
}


static ssize_t proximity_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", get_proximity_rawdata(data));
}

static ssize_t proximity_default_trim_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	struct ssp_msg *msg;
	u8 buffer[8] = {0,};
	int trim;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_PROX_GET_TRIM;
	msg->length = 2;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);
		return FAIL;
	}

	if (buffer[1] == 0xff)
		trim = (0xff - buffer[0]) * (-1);
	else
		trim = buffer[0];

	pr_info("[SSP] %s - %d, 0x%x, 0x%x\n", __func__, trim, buffer[1], buffer[0]);

	return snprintf(buf, PAGE_SIZE, "%d\n", trim);
}

static ssize_t proximity_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->buf[PROXIMITY_RAW].prox_raw[1],
		data->buf[PROXIMITY_RAW].prox_raw[2],
		data->buf[PROXIMITY_RAW].prox_raw[3]);
}

/*
	Proximity Raw Sensor Register/Unregister
*/
static ssize_t proximity_avg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	char chTempbuf[4] = { 0 };
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	s32 dMsDelay = 20;
	memcpy(&chTempbuf[0], &dMsDelay, 4);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	if (dEnable) {
		send_instruction(data, ADD_SENSOR, PROXIMITY_RAW, chTempbuf, 4);
		data->bProximityRawEnabled = true;
	} else {
		send_instruction(data, REMOVE_SENSOR, PROXIMITY_RAW,
			chTempbuf, 4);
		data->bProximityRawEnabled = false;
	}

	return size;
}

static ssize_t barcode_emul_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->bBarcodeEnabled);
}

static ssize_t barcode_emul_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	if (dEnable)
		set_proximity_barcode_enable(data, true);
	else
		set_proximity_barcode_enable(data, false);

	return size;
}

#ifdef CONFIG_SENSORS_SSP_PROX_SETTING
static ssize_t proximity_setting_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 1);
}

static ssize_t proximity_setting_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet;
	u8 val[2];
	char* token;
	char* str;
	struct ssp_msg *msg;
	struct ssp_data *data = dev_get_drvdata(dev);

	pr_info("[SSP] %s - %s\n", __func__,buf);

	//parsing
	str = (char *)buf;
	token = strsep(&str, "\n");
	if (token == NULL) {
		pr_err("[SSP] %s : too few arguments (2 needed)", __func__);
			return -EINVAL;
	}

	iRet = kstrtou8(token, 10, &val[0]);
	if (iRet<0) {
		pr_err("[SSP] %s : kstrtou8 error %d",__func__,iRet);
		return iRet;
	}

	token = strsep(&str, "\n");
	if (token == NULL) {
		pr_err("[SSP] %s : too few arguments (2 needed)", __func__);
			return -EINVAL;
	}
	
	iRet = kstrtou8(token, 16, &val[1]);
	if (iRet<0) {
		pr_err("[SSP] %s : kstrtou8 error %d",__func__,iRet);
		return iRet;
	}

	pr_info("[SSP] %s - index = %d value = 0x%x\n", __func__, val[0],val[1]);

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SET_PROX_SETTING;
	msg->length = 2;
	msg->options = AP2HUB_WRITE;
	msg->buffer = (char*) kzalloc(2, GFP_KERNEL);

	msg->free_buffer = 1;
	memcpy(msg->buffer, val, 2);

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	return size;
}
#endif

static ssize_t proximity_alert_thresh_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->uProxAlertHiThresh);
}

static ssize_t prox_light_get_dhr_sensor_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	struct ssp_msg *msg;
	u8 buffer[18] = {0, };
	u8 chipId = 0;
	u16 threshold_high = 0;
	u16 threshold_low = 0;
	u16 threshold_detect_high = 0;
	u16 threshold_detect_low = 0;
	u8 p_drive_current = 0;
	u8 persistant_time = 0;
	u8 p_pulse = 0;
	u8 p_gain = 0;
	u8 p_time = 0;
	u8 p_pulse_length = 0;
	u8 l_atime = 0;
	int offset = 0;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_GET_PROXIMITY_LIGHT_DHR_SENSOR_INFO;
	msg->length = 18;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);
		return FAIL;
	}

	chipId = buffer[0];
	threshold_high = ((((u16) buffer[2]) << 8 ) & 0xff00 ) | ((((u16) buffer[1])) & 0x00ff );
	threshold_low = ((((u16) buffer[4]) << 8 ) & 0xff00 ) | ((((u16) buffer[3])) & 0x00ff );
	threshold_detect_high = ((((u16) buffer[6]) << 8 ) & 0xff00 ) | ((((u16) buffer[5])) & 0x00ff );
	threshold_detect_low = ((((u16) buffer[8]) << 8 ) & 0xff00 ) | ((((u16) buffer[7])) & 0x00ff );
	p_drive_current = buffer[9];
	persistant_time = buffer[10];
	p_pulse = buffer[11];
	p_gain = buffer[12];
	p_time = buffer[13];
	p_pulse_length = buffer[14];
	l_atime = buffer[15];

	if (buffer[17] == 0xff)
		offset = (0xff - buffer[16]) * (-1);
	else
		offset = buffer[16];

	pr_info("[SSP] %s - %02x, %d, %d, %d, %d, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %d", __func__,
		chipId, threshold_high, threshold_low, threshold_detect_high, threshold_detect_low,
		p_drive_current, persistant_time, p_pulse, p_gain, p_time, p_pulse_length, l_atime, offset);
	
	return snprintf(buf, PAGE_SIZE, "\"THD\":\"%d %d %d %d\","\
		"\"PDRIVE_CURRENT\":\"%02x\","\
		"\"PERSIST_TIME\":\"%02x\","\
		"\"PPULSE\":\"%02x\","\
		"\"PGAIN\":\"%02x\","\
		"\"PTIME\":\"%02x\","\
		"\"PPLUSE_LEN\":\"%02x\","\
		"\"ATIME\":\"%02x\","\
		"\"POFFSET\":\"%d\"\n",
		threshold_high, threshold_low, threshold_detect_high, threshold_detect_low,
		p_drive_current, persistant_time, p_pulse, p_gain, p_time, p_pulse_length, l_atime, offset);
}


static DEVICE_ATTR(vendor, 0440, proximity_vendor_show, NULL);
static DEVICE_ATTR(name, 0440, proximity_name_show, NULL);
static DEVICE_ATTR(prox_probe, 0440, proximity_probe_show, NULL);
static DEVICE_ATTR(thresh_high, 0660, proximity_thresh_high_show, proximity_thresh_high_store);
static DEVICE_ATTR(thresh_low, 0660, proximity_thresh_low_show, proximity_thresh_low_store);
static DEVICE_ATTR(thresh_detect_high, 0660, proximity_thresh_detect_high_show, proximity_thresh_detect_high_store);
static DEVICE_ATTR(thresh_detect_low, 0660, proximity_thresh_detect_low_show, proximity_thresh_detect_low_store);
static DEVICE_ATTR(prox_trim, 0440, proximity_default_trim_show, NULL);
static DEVICE_ATTR(raw_data, 0440, proximity_raw_data_show, NULL);
static DEVICE_ATTR(prox_avg, 0660, proximity_avg_show, proximity_avg_store);

static DEVICE_ATTR(barcode_emul_en, 0660, barcode_emul_enable_show, barcode_emul_enable_store);

#ifdef CONFIG_SENSORS_SSP_PROX_SETTING
static DEVICE_ATTR(setting, 0660, proximity_setting_show, proximity_setting_store);
#endif

static DEVICE_ATTR(prox_alert_thresh, 0440, proximity_alert_thresh_show, NULL);
static DEVICE_ATTR(dhr_sensor_info, 0440, prox_light_get_dhr_sensor_info_show, NULL);

static struct device_attribute *prox_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_prox_probe,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_thresh_detect_high,
	&dev_attr_thresh_detect_low,
	&dev_attr_prox_trim,
	&dev_attr_raw_data,
	&dev_attr_prox_avg,
	&dev_attr_barcode_emul_en,
#ifdef CONFIG_SENSORS_SSP_PROX_SETTING
	&dev_attr_setting,
#endif
	&dev_attr_prox_alert_thresh,
	&dev_attr_dhr_sensor_info,
	NULL,
};

void initialize_prox_factorytest(struct ssp_data *data)
{
	sensors_register(data->prox_device, data,
		prox_attrs, "proximity_sensor");
}

void remove_prox_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->prox_device, prox_attrs);
}


