/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sec_ext.h>

#ifdef CONFIG_SEC_PARAM

#define SEC_PARAM_NAME "/dev/block/param"
#define STR_LENGTH 1024
struct sec_param_data_s {
	struct work_struct sec_param_work;
	unsigned long offset;
	char val;
};

struct sec_param_data_s_str {
	struct work_struct sec_param_work_str;
	unsigned long offset;
	char str[STR_LENGTH];
};

static struct sec_param_data_s sec_param_data;
static struct sec_param_data_s_str sec_param_data_str;
static DEFINE_MUTEX(sec_param_mutex);
static DEFINE_MUTEX(sec_param_mutex_str);

static void sec_param_update(struct work_struct *work)
{
	int ret = -1;
	struct file *fp;
	struct sec_param_data_s *param_data =
		container_of(work, struct sec_param_data_s, sec_param_work);

	fp = filp_open(SEC_PARAM_NAME, O_WRONLY | O_SYNC, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: filp_open error %ld\n", __func__, PTR_ERR(fp));
		return;
	}
	pr_info("%s: set param %c at %lu\n", __func__,
				param_data->val, param_data->offset);
	ret = fp->f_op->llseek(fp, param_data->offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s: llseek error %d!\n", __func__, ret);
		goto close_fp_out;
	}

	ret = vfs_write(fp, &param_data->val, 1, &(fp->f_pos));
	if (ret < 0)
		pr_err("%s: write error! %d\n", __func__, ret);

close_fp_out:
	if (fp)
		filp_close(fp, NULL);
	pr_info("%s: exit %d\n", __func__, ret);
}

static void sec_param_update_str(struct work_struct *work)
{
	int ret = -1;
	struct file *fp;
	struct sec_param_data_s_str *param_data_str =
		container_of(work, struct sec_param_data_s_str, sec_param_work_str);

	fp = filp_open(SEC_PARAM_NAME, O_WRONLY | O_SYNC, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: filp_open error %ld\n", __func__, PTR_ERR(fp));
		return;
	}
	pr_info("%s: set param %s at %lu\n", __func__,
				param_data_str->str, param_data_str->offset);
	ret = fp->f_op->llseek(fp, param_data_str->offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s: llseek error %d!\n", __func__, ret);
		goto close_fp_out;
	}

	ret = vfs_write(fp, (char __user *)param_data_str->str, strlen(param_data_str->str), &(fp->f_pos));
	if (ret < 0)
		pr_err("%s: write error! %d\n", __func__, ret);

close_fp_out:
	if (fp)
		filp_close(fp, NULL);
	pr_info("%s: exit %d\n", __func__, ret);
}

/*
  success : ret >= 0
  fail : ret < 0
 */
int sec_set_param(unsigned long offset, char val)
{
	int ret = -1;

	mutex_lock(&sec_param_mutex);

	if ((offset < CM_OFFSET) || (offset > CM_OFFSET + CM_OFFSET_LIMIT))
		goto unlock_out;

	switch (val) {
	case PARAM_OFF:
	case PARAM_ON:
		goto set_param;
	default:
		if (val >= PARAM_TEST0 && val < PARAM_MAX)
			goto set_param;
		goto unlock_out;
	}

set_param:
	sec_param_data.offset = offset;
	sec_param_data.val = val;

	schedule_work(&sec_param_data.sec_param_work);

	/* how to determine to return success or fail ? */

	ret = 0;
unlock_out:
	mutex_unlock(&sec_param_mutex);
	return ret;
}

/*
  success : ret >= 0
  fail : ret < 0
 */
int sec_set_param_str(unsigned long offset, const char *str, int size)
{
	int ret = -1;

	mutex_lock(&sec_param_mutex_str);

	if (!strcmp(str, ""))
		goto unlock_out_str;

	memset(sec_param_data_str.str, 0, sizeof(sec_param_data_str.str));
	sec_param_data_str.offset = offset;
	strncpy(sec_param_data_str.str, str, size);

	schedule_work(&sec_param_data_str.sec_param_work_str);

	/* how to determine to return success or fail ? */

	ret = 0;

unlock_out_str:
	mutex_unlock(&sec_param_mutex_str);
	return ret;
}

static int __init sec_param_work_init(void)
{
	pr_info("%s: start\n", __func__);

	sec_param_data.offset = 0;
	sec_param_data.val = '0';

	sec_param_data_str.offset = 0;
	memset(sec_param_data_str.str, 0, sizeof(sec_param_data_str.str));

	INIT_WORK(&sec_param_data.sec_param_work, sec_param_update);
	INIT_WORK(&sec_param_data_str.sec_param_work_str, sec_param_update_str);

	return 0;
}

static void __exit sec_param_work_exit(void)
{
	cancel_work_sync(&sec_param_data.sec_param_work);
	pr_info("%s: exit\n", __func__);
}
module_init(sec_param_work_init);
module_exit(sec_param_work_exit);

#endif /* CONFIG_SEC_PARAM */
