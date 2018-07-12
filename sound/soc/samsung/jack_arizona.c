#include "../codecs/florida.h"
#include "../codecs/clearwater.h"
#include "../codecs/arizona.h"

/* To support PBA function test */
static struct class *jack_class;
static struct device *jack_dev;
static struct device *codec_dev;

static ssize_t earjack_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct arizona_extcon_info *info = dev_get_drvdata(dev);
	int status = info->edev.state;
	int report = 0;

	if ((status & BIT_HEADSET) ||
		(status & BIT_HEADSET_NO_MIC)) {
		report = 1;
	}

	return sprintf(buf, "%d\n", report);
}

static ssize_t earjack_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_key_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct arizona_extcon_info *info = dev_get_drvdata(dev);
	struct arizona *arizona = info->arizona;
	unsigned int val, lvl;
	int report = 0;
	int ret, key;


	ret = regmap_read(arizona->regmap, ARIZONA_MIC_DETECT_3, &val);
	if (ret != 0)
		dev_err(arizona->dev, "Failed to read MICDET: %d\n", ret);

	dev_err(arizona->dev, "MICDET: %x\n", val);

	if (val & MICD_LVL_0_TO_7) {
		if (info->mic) {
			lvl = val & ARIZONA_MICD_LVL_MASK;
			lvl >>= ARIZONA_MICD_LVL_SHIFT;

			WARN_ON(!lvl);
			WARN_ON(ffs(lvl) - 1 >= info->num_micd_ranges);
			if (lvl && ffs(lvl) - 1 < info->num_micd_ranges) {
				key = info->micd_ranges[ffs(lvl) - 1].key;

				if (key == KEY_MEDIA)
					report = true;
			}
		}
	}

	return sprintf(buf, "%d\n", report);
}

static ssize_t earjack_key_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_select_jack_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);

	return 0;
}

static ssize_t earjack_select_jack_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct arizona_extcon_info *info = dev_get_drvdata(dev);

	if ((!size) || (buf[0] != '1')) {
		switch_set_state(&info->edev, 0);
		pr_info("Forced remove microphone\n");
	} else {
		switch_set_state(&info->edev, 1);
		pr_info("Forced detect microphone\n");
	}

	return size;
}

static ssize_t earjack_mic_adc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct arizona_extcon_info *info = dev_get_drvdata(dev);
	int adc;

	adc = arizona_extcon_take_manual_mic_reading(info);

	return sprintf(buf, "%d\n", adc);
}

static ssize_t earjack_mic_adc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t check_codec_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct arizona_extcon_info *info = dev_get_drvdata(dev);
	struct arizona *arizona = info->arizona;
	unsigned int reg;
	int ret, retval;

	pm_runtime_get_sync(arizona->dev);

	/* Verify that this is a chip we know about */
	ret = regmap_read(arizona->regmap, ARIZONA_SOFTWARE_RESET, &reg);
	if (ret != 0) {
		/* Failed to read ID register */
		retval = 0;
		goto exit;
	}


	switch (reg) {
	case 0x5102:
	case 0x5110:
	case 0x6349:
	case 0x6363:
	case 0x8997:
	case 0x6338:
	case 0x6360:
	case 0x6364:
		retval = 1;
		break;
	default:
		/* Unknown device ID */
		retval = 0;
	}

exit:
	pm_runtime_put_sync(arizona->dev);

	return sprintf(buf, "%d\n", retval);
}

static ssize_t check_codec_id_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t water_detected_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct arizona_extcon_info *info = dev_get_drvdata(dev);
	struct arizona *arizona = info->arizona;
	struct snd_soc_codec *codec = arizona->dapm->component->codec;
	int retval;

	retval = arizona_get_moisture_state(codec);

	return sprintf(buf, "%d\n", retval);
}

static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_select_jack_show, earjack_select_jack_store);

static DEVICE_ATTR(key_state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_key_state_show, earjack_key_state_store);

static DEVICE_ATTR(state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_state_show, earjack_state_store);

static DEVICE_ATTR(mic_adc, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_mic_adc_show, earjack_mic_adc_store);

static DEVICE_ATTR(water_detected, S_IRUSR | S_IRGRP,
		   water_detected_show, NULL);

static DEVICE_ATTR(check_codec_id, S_IRUGO | S_IWUSR | S_IWGRP,
		   check_codec_id_show, check_codec_id_store);

static void create_jack_devices(struct arizona_extcon_info *info)
{
#if defined(CONFIG_SEC_FACTORY)
	struct arizona *arizona = info->arizona;
	struct snd_soc_codec *codec = arizona->dapm->component->codec;

	/* To disable antenna jack feature on factory binary */
	arizona_set_custom_jd(codec, &arizona_hpdet_moisture);
#endif
	/* To support PBA function test */
	jack_class = class_create(THIS_MODULE, "audio");

	if (IS_ERR(jack_class))
		pr_err("Failed to create class\n");

	/* Create jack sysfs node */
	jack_dev = device_create(jack_class, NULL, 0, info, "earjack");

	if (device_create_file(jack_dev, &dev_attr_select_jack) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_select_jack.attr.name);

	if (device_create_file(jack_dev, &dev_attr_key_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_key_state.attr.name);

	if (device_create_file(jack_dev, &dev_attr_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_state.attr.name);

	if (device_create_file(jack_dev, &dev_attr_mic_adc) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_mic_adc.attr.name);

	if (device_create_file(jack_dev, &dev_attr_water_detected) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_water_detected.attr.name);

	/* Create CODEC ID check sysfs node */
	codec_dev = device_create(jack_class, NULL, 0, info, "codec");

	if (device_create_file(codec_dev, &dev_attr_check_codec_id) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_check_codec_id.attr.name);
}
