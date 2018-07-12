/* linux/drivers/video/mdnie.c
 *
 * Register interface file for Samsung mDNIe driver
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>

#include "mdnie.h"

#define MDNIE_SYSFS_PREFIX		"/sdcard/mdnie/"

#define IS_DMB(idx)				(idx == DMB_NORMAL_MODE)
#define IS_SCENARIO(idx)		(idx < SCENARIO_MAX && !(idx > VIDEO_NORMAL_MODE && idx < CAMERA_MODE))
#define IS_ACCESSIBILITY(idx)	(idx && idx < ACCESSIBILITY_MAX)
#define IS_HBM(idx)				(idx && idx < HBM_MAX)
#define IS_HMT(idx)				(idx && idx < HMT_MDNIE_MAX)

#define SCENARIO_IS_VALID(idx)	(IS_DMB(idx) || IS_SCENARIO(idx))

/* Split 16 bit as 8bit x 2 */
#define GET_MSB_8BIT(x)		((x >> 8) & (BIT(8) - 1))
#define GET_LSB_8BIT(x)		((x >> 0) & (BIT(8) - 1))

static struct class *mdnie_class;

/* Do not call mdnie write directly */
static int mdnie_write(struct mdnie_info *mdnie, struct mdnie_table *table, unsigned int num)
{
	int ret = 0;

	if (mdnie->enable)
		ret = mdnie->ops.write(mdnie->data, table->seq, num);

	return ret;
}

static int mdnie_write_table(struct mdnie_info *mdnie, struct mdnie_table *table)
{
	int i, ret = 0;
	struct mdnie_table *buf = NULL;

	for (i = 0; table->seq[i].len; i++) {
		if (IS_ERR_OR_NULL(table->seq[i].cmd)) {
			dev_info(mdnie->dev, "mdnie sequence %s %dth is null\n", table->name, i);
			return -EPERM;
		}
	}

	mutex_lock(&mdnie->dev_lock);

	buf = table;

	ret = mdnie_write(mdnie, buf, i);

	mutex_unlock(&mdnie->dev_lock);

	return ret;
}

static struct mdnie_table *mdnie_find_table(struct mdnie_info *mdnie)
{
	struct mdnie_table *table = NULL;

	mutex_lock(&mdnie->lock);

	if (IS_ACCESSIBILITY(mdnie->accessibility)) {
		table = mdnie->tune->accessibility_table ? &mdnie->tune->accessibility_table[mdnie->accessibility] : NULL;
		goto exit;
	} else if (IS_HMT(mdnie->hmt_mode)) {
		table = mdnie->tune->hmt_table ? &mdnie->tune->hmt_table[mdnie->hmt_mode] : NULL;
		goto exit;
	} else if (IS_HBM(mdnie->hbm)) {
		table = mdnie->tune->hbm_table ? &mdnie->tune->hbm_table[mdnie->hbm] : NULL;
		goto exit;
	} else if (IS_DMB(mdnie->scenario)) {
		table = mdnie->tune->dmb_table ? &mdnie->tune->dmb_table[mdnie->mode] : NULL;
		goto exit;
	} else if (IS_SCENARIO(mdnie->scenario)) {
		table = mdnie->tune->main_table ? &mdnie->tune->main_table[mdnie->scenario][mdnie->mode] : NULL;
		goto exit;
	}

exit:
	mutex_unlock(&mdnie->lock);

	return table;
}

static void mdnie_update_sequence(struct mdnie_info *mdnie, struct mdnie_table *table)
{
	mdnie_write_table(mdnie, table);
}

static void mdnie_update(struct mdnie_info *mdnie)
{
	struct mdnie_table *table = NULL;
	struct mdnie_scr_info *scr_info = mdnie->tune->scr_info;

	if (!mdnie->enable) {
		dev_err(mdnie->dev, "mdnie state is off\n");
		return;
	}

	table = mdnie_find_table(mdnie);
	if (!IS_ERR_OR_NULL(table) && !IS_ERR_OR_NULL(table->name)) {
		mdnie_update_sequence(mdnie, table);
		dev_info(mdnie->dev, "%s\n", table->name);

		mdnie->wrgb_current.r = table->seq[scr_info->index].cmd[scr_info->wr];
		mdnie->wrgb_current.g = table->seq[scr_info->index].cmd[scr_info->wg];
		mdnie->wrgb_current.b = table->seq[scr_info->index].cmd[scr_info->wb];
	}
}

static void update_color_position(struct mdnie_info *mdnie, unsigned int idx)
{
	u8 mode, scenario;
	mdnie_t *wbuf;
	struct mdnie_scr_info *scr_info = mdnie->tune->scr_info;

	dev_info(mdnie->dev, "%s: idx=%d\n", __func__, idx);

	mutex_lock(&mdnie->lock);

	for (mode = 0; mode < MODE_MAX; mode++) {
		for (scenario = 0; scenario <= EMAIL_MODE; scenario++) {
			wbuf = mdnie->tune->main_table[scenario][mode].seq[scr_info->index].cmd;
			if (IS_ERR_OR_NULL(wbuf))
				continue;
			if (scenario != EBOOK_MODE) {
				wbuf[scr_info->wr] = mdnie->tune->coordinate_table[mode][idx * 3 + 0];
				wbuf[scr_info->wg] = mdnie->tune->coordinate_table[mode][idx * 3 + 1];
				wbuf[scr_info->wb] = mdnie->tune->coordinate_table[mode][idx * 3 + 2];
			}
		}
	}

	mutex_unlock(&mdnie->lock);
}

static int mdnie_calibration(int *r)
{
	int ret = 0;

	if (r[1] > 0) {
		if (r[3] > 0)
			ret = 3;
		else
			ret = (r[4] < 0) ? 1 : 2;
	} else {
		if (r[2] < 0) {
			if (r[3] > 0)
				ret = 9;
			else
				ret = (r[4] < 0) ? 7 : 8;
		} else {
			if (r[3] > 0)
				ret = 6;
			else
				ret = (r[4] < 0) ? 4 : 5;
		}
	}

	pr_info("%d, %d, %d, %d, tune%d\n", r[1], r[2], r[3], r[4], ret);

	return ret;
}

static int get_panel_coordinate(struct mdnie_info *mdnie, int *result)
{
	int ret = 0;
	unsigned short x, y;

	x = mdnie->coordinate[0];
	y = mdnie->coordinate[1];

	if (!(x || y)) {
		dev_info(mdnie->dev, "%s: %d, %d\n", __func__, x, y);
		ret = -EINVAL;
		goto skip_color_correction;
	}

	result[COLOR_OFFSET_FUNC_F1] = mdnie->tune->color_offset[COLOR_OFFSET_FUNC_F1](x, y);
	result[COLOR_OFFSET_FUNC_F2] = mdnie->tune->color_offset[COLOR_OFFSET_FUNC_F2](x, y);
	result[COLOR_OFFSET_FUNC_F3] = mdnie->tune->color_offset[COLOR_OFFSET_FUNC_F3](x, y);
	result[COLOR_OFFSET_FUNC_F4] = mdnie->tune->color_offset[COLOR_OFFSET_FUNC_F4](x, y);

	ret = mdnie_calibration(result);
	dev_info(mdnie->dev, "%s: %d, %d, %d\n", __func__, x, y, ret);

skip_color_correction:
	mdnie->color_correction = 1;

	return ret;
}

static ssize_t mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->mode);
}

static ssize_t mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value = 0;
	int ret, idx, result[COLOR_OFFSET_FUNC_MAX] = {0,};

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d\n", __func__, value);

	if (value >= MODE_MAX)
		return -EINVAL;

	mutex_lock(&mdnie->lock);
	mdnie->mode = value;
	mutex_unlock(&mdnie->lock);

	if (!mdnie->color_correction) {
		idx = get_panel_coordinate(mdnie, result);
		if (idx > 0)
			update_color_position(mdnie, idx);
	}

	mdnie_update(mdnie);

	return count;
}


static ssize_t scenario_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->scenario);
}

static ssize_t scenario_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d\n", __func__, value);

	if (!SCENARIO_IS_VALID(value))
		value = UI_MODE;

	mutex_lock(&mdnie->lock);
	mdnie->scenario = value;
	mutex_unlock(&mdnie->lock);

	mdnie_update(mdnie);

	return count;
}

static ssize_t accessibility_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->accessibility);
}

static ssize_t accessibility_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value, s[9] = {0, }, i = 0;
	int ret;
	mdnie_t *wbuf;
	struct mdnie_scr_info *scr_info = mdnie->tune->scr_info;

	ret = sscanf(buf, "%8d %8x %8x %8x %8x %8x %8x %8x %8x %8x",
		&value, &s[0], &s[1], &s[2], &s[3],
		&s[4], &s[5], &s[6], &s[7], &s[8]);

	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d, %d\n", __func__, value, ret);

	if (value >= ACCESSIBILITY_MAX)
		return -EINVAL;

	mutex_lock(&mdnie->lock);
	mdnie->accessibility = value;
	if (value == COLOR_BLIND) {
		if (ret > ARRAY_SIZE(s) + 1) {
			mutex_unlock(&mdnie->lock);
			return -EINVAL;
		}
		wbuf = &mdnie->tune->accessibility_table[value].seq[scr_info->index].cmd[scr_info->cr];
		while (i < ret - 1) {
			wbuf[i * 2 + 0] = GET_LSB_8BIT(s[i]);
			wbuf[i * 2 + 1] = GET_MSB_8BIT(s[i]);
			i++;
		}

		dev_info(dev, "%s: %s\n", __func__, buf);
	}
	mutex_unlock(&mdnie->lock);

	mdnie_update(mdnie);

	return count;
}

static ssize_t color_correct_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	char *pos = buf;
	int i, idx, result[COLOR_OFFSET_FUNC_MAX] = {0,};

	if (!mdnie->color_correction)
		return -EINVAL;

	idx = get_panel_coordinate(mdnie, result);

	for (i = COLOR_OFFSET_FUNC_F1; i < COLOR_OFFSET_FUNC_MAX; i++)
		pos += sprintf(pos, "f%d: %d, ", i, result[i]);
	pos += sprintf(pos, "tune%d\n", idx);

	return pos - buf;
}

static ssize_t bypass_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->bypass);
}

static ssize_t bypass_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	struct mdnie_table *table = NULL;
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(dev, "%s: %d\n", __func__, value);

	if (value >= BYPASS_MAX)
		return -EINVAL;

	value = (value) ? BYPASS_ON : BYPASS_OFF;

	mutex_lock(&mdnie->lock);
	mdnie->bypass = value;
	mutex_unlock(&mdnie->lock);

	table = &mdnie->tune->bypass_table[value];
	if (!IS_ERR_OR_NULL(table)) {
		mdnie_write_table(mdnie, table);
		dev_info(mdnie->dev, "%s\n", table->name);
	}

	return count;
}

static ssize_t lux_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->hbm);
}

static ssize_t lux_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int hbm = 0, update = 0;
	int ret, value;

	ret = kstrtoint(buf, 0, &value);
	if (ret < 0)
		return ret;

	if (!mdnie->tune->get_hbm_index)
		return count;

	mutex_lock(&mdnie->lock);
	hbm = mdnie->tune->get_hbm_index(value);
	update = (mdnie->hbm != hbm) ? 1 : 0;
	mdnie->hbm = update ? hbm : mdnie->hbm;
	mutex_unlock(&mdnie->lock);

	if (update) {
		dev_info(dev, "%s: %d\n", __func__, value);
		mdnie_update(mdnie);
	}

	return count;
}

static ssize_t sensorRGB_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d %d %d\n", mdnie->wrgb_current.r, mdnie->wrgb_current.g, mdnie->wrgb_current.b);
}

static ssize_t sensorRGB_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	struct mdnie_table *table = NULL;
	unsigned int white_r, white_g, white_b;
	int ret;
	struct mdnie_scr_info *scr_info = mdnie->tune->scr_info;

	ret = sscanf(buf, "%8d %8d %8d", &white_r, &white_g, &white_b);
	if (ret < 0)
		return ret;

	if (mdnie->enable
		&& mdnie->accessibility == ACCESSIBILITY_OFF
		&& mdnie->mode == AUTO
		&& (mdnie->scenario == BROWSER_MODE || mdnie->scenario == EBOOK_MODE)) {
		dev_info(dev, "%s: %d, %d, %d\n", __func__, white_r, white_g, white_b);

		table = mdnie_find_table(mdnie);

		memcpy(&mdnie->table_buffer, table, sizeof(struct mdnie_table));
		memcpy(&mdnie->sequence_buffer, table->seq[scr_info->index].cmd, table->seq[scr_info->index].len);
		mdnie->table_buffer.seq[scr_info->index].cmd = mdnie->sequence_buffer;

		mdnie->table_buffer.seq[scr_info->index].cmd[scr_info->wr] = mdnie->wrgb_current.r = (unsigned char)white_r;
		mdnie->table_buffer.seq[scr_info->index].cmd[scr_info->wg] = mdnie->wrgb_current.g = (unsigned char)white_g;
		mdnie->table_buffer.seq[scr_info->index].cmd[scr_info->wb] = mdnie->wrgb_current.b = (unsigned char)white_b;

		mdnie_update_sequence(mdnie, &mdnie->table_buffer);
	}

	return count;
}

#ifdef CONFIG_LCD_HMT
static ssize_t hmtColorTemp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->hmt_mode);
}

static ssize_t hmtColorTemp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	if (value != mdnie->hmt_mode && value < HMT_MDNIE_MAX) {
		mutex_lock(&mdnie->lock);
		mdnie->hmt_mode = value;
		mutex_unlock(&mdnie->lock);
		mdnie_update(mdnie);
	}

	return count;
}
#endif

static DEVICE_ATTR(mode, 0664, mode_show, mode_store);
static DEVICE_ATTR(scenario, 0664, scenario_show, scenario_store);
static DEVICE_ATTR(accessibility, 0664, accessibility_show, accessibility_store);
static DEVICE_ATTR(color_correct, 0444, color_correct_show, NULL);
static DEVICE_ATTR(bypass, 0664, bypass_show, bypass_store);
static DEVICE_ATTR(lux, 0000, lux_show, lux_store);
static DEVICE_ATTR(sensorRGB, 0664, sensorRGB_show, sensorRGB_store);
#ifdef CONFIG_LCD_HMT
static DEVICE_ATTR(hmt_color_temperature, 0664, hmtColorTemp_show, hmtColorTemp_store);
#endif


static struct attribute *mdnie_attrs[] = {
	&dev_attr_mode.attr,
	&dev_attr_scenario.attr,
	&dev_attr_accessibility.attr,
	&dev_attr_color_correct.attr,
	&dev_attr_bypass.attr,
	&dev_attr_lux.attr,
	&dev_attr_sensorRGB.attr,
#ifdef CONFIG_LCD_HMT
	&dev_attr_hmt_color_temperature.attr,
#endif
	NULL,
};

ATTRIBUTE_GROUPS(mdnie);

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct mdnie_info *mdnie;
	struct fb_event *evdata = data;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	mdnie = container_of(self, struct mdnie_info, fb_notif);

	fb_blank = *(int *)evdata->data;

	dev_info(mdnie->dev, "%s: %d\n", __func__, fb_blank);

	if (evdata->info->node != 0)
		return NOTIFY_DONE;

	if (fb_blank == FB_BLANK_UNBLANK) {
		mutex_lock(&mdnie->lock);
		mdnie->enable = 1;
		mutex_unlock(&mdnie->lock);

		mdnie_update(mdnie);
	} else if (fb_blank == FB_BLANK_POWERDOWN) {
		mutex_lock(&mdnie->lock);
		mdnie->enable = 0;
		mutex_unlock(&mdnie->lock);
	}

	return NOTIFY_DONE;
}

static int mdnie_register_fb(struct mdnie_info *mdnie)
{
	memset(&mdnie->fb_notif, 0, sizeof(mdnie->fb_notif));
	mdnie->fb_notif.notifier_call = fb_notifier_callback;
	return fb_register_client(&mdnie->fb_notif);
}

int mdnie_register(struct device *p, void *data, mdnie_w w, mdnie_r r, unsigned int *coordinate, struct mdnie_tune *tune)
{
	int ret = 0;
	struct mdnie_info *mdnie;
	static unsigned int mdnie_no;

	if (IS_ERR_OR_NULL(mdnie_class)) {
		mdnie_class = class_create(THIS_MODULE, "mdnie");
		if (IS_ERR_OR_NULL(mdnie_class)) {
			pr_err("failed to create mdnie class\n");
			ret = -EINVAL;
			goto error0;
		}

		mdnie_class->dev_groups = mdnie_groups;
	}

	mdnie = kzalloc(sizeof(struct mdnie_info), GFP_KERNEL);
	if (!mdnie) {
		pr_err("failed to allocate mdnie\n");
		ret = -ENOMEM;
		goto error1;
	}

	mdnie->dev = device_create(mdnie_class, p, 0, &mdnie, !mdnie_no ? "mdnie" : "mdnie%d", mdnie_no);
	if (IS_ERR_OR_NULL(mdnie->dev)) {
		pr_err("failed to create mdnie device\n");
		ret = -EINVAL;
		goto error2;
	}

	mdnie_no++;
	mdnie->scenario = UI_MODE;
	mdnie->mode = STANDARD;
	mdnie->enable = 0;
	mdnie->tuning = 0;
	mdnie->accessibility = ACCESSIBILITY_OFF;
	mdnie->bypass = BYPASS_OFF;

	mdnie->data = data;
	mdnie->ops.write = w;
	mdnie->ops.read = r;

	mdnie->coordinate[0] = coordinate[0];
	mdnie->coordinate[1] = coordinate[1];
	mdnie->tune = tune;

	mutex_init(&mdnie->lock);
	mutex_init(&mdnie->dev_lock);

	dev_set_drvdata(mdnie->dev, mdnie);

	mdnie_register_fb(mdnie);

	mdnie->enable = 1;
	mdnie_update(mdnie);

	dev_info(mdnie->dev, "registered successfully\n");

	return 0;

error2:
	kfree(mdnie);
error1:
	class_destroy(mdnie_class);
error0:
	return ret;
}


static int attr_find_and_store(struct device *dev,
	const char *name, const char *buf, size_t size)
{
	struct device_attribute *dev_attr;
	struct kernfs_node *kn;
	struct attribute *attr;

	kn = kernfs_find_and_get(dev->kobj.sd, name);
	if (!kn) {
		dev_info(dev, "%s: not found: %s\n", __func__, name);
		return 0;
	}

	attr = kn->priv;
	dev_attr = container_of(attr, struct device_attribute, attr);

	if (dev_attr && dev_attr->store)
		dev_attr->store(dev, dev_attr, buf, size);

	kernfs_put(kn);

	return 0;
}

ssize_t attr_store_for_each(struct class *cls,
	const char *name, const char *buf, size_t size)
{
	struct class_dev_iter iter;
	struct device *dev;
	int error = 0;
	struct class *class = cls;

	if (!class)
		return -EINVAL;
	if (!class->p) {
		WARN(1, "%s called for class '%s' before it was initialized",
		     __func__, class->name);
		return -EINVAL;
	}

	class_dev_iter_init(&iter, class, NULL, NULL);
	while ((dev = class_dev_iter_next(&iter))) {
		error = attr_find_and_store(dev, name, buf, size);
		if (error)
			break;
	}
	class_dev_iter_exit(&iter);

	return error;
}

struct class *get_mdnie_class(void)
{
	return mdnie_class;
}

