#include "poc.h"
#include "dpui.h" 
#include "../dsim.h"

#define POC_RD_OFS 18

#define POC_TEST_PATTERN_DEBUG

static u8 poc_wr_img[POC_IMG_SIZE] = { 0, };
static u8 poc_rd_img[POC_IMG_SIZE] = { 0, };
#ifdef POC_DEBUG
static u8 poc_txt_img[POC_IMG_SIZE * 6] = { 0, };
#endif

const char * const poc_op_string[] = {
	[POC_OP_NONE] = "POC_OP_NONE",
	[POC_OP_ERASE] = "POC_OP_ERASE",
	[POC_OP_WRITE] = "POC_OP_WRITE",
	[POC_OP_READ] = "POC_OP_READ",
	[POC_OP_CANCEL] = "POC_OP_CANCEL",
	[POC_OP_CHECKSUM] = "POC_OP_CHECKSUM",
	[POC_OP_CHECKPOC] = "POC_OP_CHECKPOC",
	[POC_OP_BACKUP] = "POC_OP_BACKUP",
	[POC_OP_SELF_TEST] = "POC_OP_SELF_TEST",
	[POC_OP_READ_TEST] = "POC_OP_READ_TEST",
	[POC_OP_WRITE_TEST] = "POC_OP_WRITE_TEST",
};

extern int poc_erase(struct panel_private *panel)
{
	struct panel_poc_device *poc_dev = &panel->poc_dev;
	struct dsim_device *dsim = container_of(panel, struct dsim_device, priv);

	int ret = 0, i;

	pr_info("%s poc erase +++\n", __func__);
	mutex_lock(&panel->lock);

	ret = dsim_write_hl_data(dsim, SEQ_POC_F0_KEY_ENABLE, ARRAY_SIZE(SEQ_POC_F0_KEY_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F1_KEY_ENABLE, ARRAY_SIZE(SEQ_POC_F1_KEY_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_PGM_ENABLE, ARRAY_SIZE(SEQ_POC_PGM_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_WR_ENABLE, ARRAY_SIZE(SEQ_POC_WR_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_EXECUTE, ARRAY_SIZE(SEQ_POC_EXECUTE));
	if (unlikely(ret < 0)) {
		pr_err("%s, failed to poc-erase-enter-seq\n", __func__);
		goto out_poc_erase;
	}
	msleep(1);

	ret = dsim_write_hl_data(dsim, SEQ_POC_ERASE, ARRAY_SIZE(SEQ_POC_ERASE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_EXECUTE, ARRAY_SIZE(SEQ_POC_EXECUTE));

	if (unlikely(ret < 0)) {
		pr_err("%s, failed to poc-erase-seq\n", __func__);
		goto out_poc_erase;
	}

	for (i = 0; i < 41; i++) {
		msleep(100);
		if (atomic_read(&poc_dev->cancel)) {
			pr_err("%s, stopped by user at erase\n", __func__);
			goto cancel_poc_erase;
		}
	}

	ret = dsim_write_hl_data(dsim, SEQ_POC_PGM_ENABLE, ARRAY_SIZE(SEQ_POC_PGM_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F1_KEY_ENABLE, ARRAY_SIZE(SEQ_POC_F1_KEY_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F0_KEY_ENABLE, ARRAY_SIZE(SEQ_POC_F0_KEY_ENABLE));

	if (unlikely(ret < 0)) {
		pr_err("%s, failed to poc-erase-exit-seq\n", __func__);
		goto out_poc_erase;
	}
	mutex_unlock(&panel->lock);

	pr_info("%s poc erease ---\n", __func__);
	return 0;

cancel_poc_erase:
	ret = dsim_write_hl_data(dsim, SEQ_POC_PGM_DISABLE, ARRAY_SIZE(SEQ_POC_PGM_DISABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F1_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F1_KEY_DISABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F0_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F0_KEY_DISABLE));
	if (unlikely(ret < 0))
		pr_err("%s, failed to poc-erase-exit-seq\n", __func__);
	ret = -EIO;
	atomic_set(&poc_dev->cancel, 0);

out_poc_erase:
	mutex_unlock(&panel->lock);
	return ret;

}

extern int poc_read_data(struct panel_private *panel, u8 *buf, u32 addr, u32 len)
{
	struct panel_poc_device *poc_dev = &panel->poc_dev;
	struct dsim_device *dsim = container_of(panel, struct dsim_device, priv);
	int i, ret = 0;
	u32 poc_addr;
	u8 read_buf[POC_RD_OFS + 1];
	u8 READ_CMD[ARRAY_SIZE(SEQ_POC_RD_STT)] = { 0, };

	pr_info("%s poc read addr 0x%06X, %d(0x%X) bytes +++\n",
			__func__, addr, len, len);
	mutex_lock(&panel->lock);\

	ret = dsim_write_hl_data(dsim, SEQ_POC_F0_KEY_ENABLE, ARRAY_SIZE(SEQ_POC_F0_KEY_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F1_KEY_ENABLE, ARRAY_SIZE(SEQ_POC_F1_KEY_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_PGM_ENABLE, ARRAY_SIZE(SEQ_POC_PGM_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_WR_ENABLE, ARRAY_SIZE(SEQ_POC_WR_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_EXECUTE, ARRAY_SIZE(SEQ_POC_EXECUTE));
	udelay(2);
	ret |= dsim_write_hl_data(dsim, SEQ_POC_QD_ENABLE, ARRAY_SIZE(SEQ_POC_QD_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_EXECUTE, ARRAY_SIZE(SEQ_POC_EXECUTE));
	mdelay(10);
	if (unlikely(ret < 0)) {
		pr_err("%s, failed to read poc-rd-stt seq\n", __func__);
		goto out_poc_read;
	}

	memcpy(READ_CMD, SEQ_POC_RD_STT, sizeof(READ_CMD));
	for (i = 0; i < len; i++) {
		if (atomic_read(&poc_dev->cancel)) {
			pr_err("%s, stopped by user at %d bytes\n",
					__func__, i);
			goto cancel_poc_read;
		}

		poc_addr = addr + i;
		READ_CMD[6] = (poc_addr & 0xFF0000) >> 16;
		READ_CMD[7] = (poc_addr & 0x00FF00) >> 8;
		READ_CMD[8] = (poc_addr & 0x0000FF);

		ret = dsim_write_hl_data(dsim, READ_CMD, ARRAY_SIZE(READ_CMD));
		ret |= dsim_write_hl_data(dsim, SEQ_POC_EXECUTE, ARRAY_SIZE(SEQ_POC_EXECUTE));
		if (unlikely(ret < 0)) {
			pr_err("%s, failed to read poc-rd-dat seq\n", __func__);
			goto out_poc_read;
		}
		udelay(200);
		ret = dsim_read_hl_data(dsim, 0xEC, ARRAY_SIZE(read_buf), read_buf);
		if (ret != ARRAY_SIZE(read_buf)) {
			pr_err("%s, failed to read poc-read data\n", __func__);
			goto out_poc_read;
		}
		buf[i] = read_buf[POC_RD_OFS];
		if ((i % 4096) == 0)
			pr_info("%s [%04d] addr %06X %02X\n", __func__, i, poc_addr, buf[i]);
	}

	ret = dsim_write_hl_data(dsim, SEQ_POC_PGM_DISABLE, ARRAY_SIZE(SEQ_POC_PGM_DISABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F1_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F1_KEY_DISABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F0_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F0_KEY_DISABLE));

	if (unlikely(ret < 0)) {
		pr_err("%s, failed to read poc-rd-exit seq\n", __func__);
		goto out_poc_read;
	}
	mutex_unlock(&panel->lock);
	pr_info("%s poc read addr 0x%06X, %d(0x%X) bytes ---\n",
			__func__, addr, len, len);

	return 0;

cancel_poc_read:
	ret = dsim_write_hl_data(dsim, SEQ_POC_PGM_DISABLE, ARRAY_SIZE(SEQ_POC_PGM_DISABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F1_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F1_KEY_DISABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F0_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F0_KEY_DISABLE));

	if (unlikely(ret < 0))
		pr_err("%s, failed to read poc-rd-exit seq\n", __func__);
	ret = -EIO;
	atomic_set(&poc_dev->cancel, 0);

out_poc_read:
	mutex_unlock(&panel->lock);
	return ret;
}


int poc_write_data(struct panel_private *panel, u8 *data, u32 addr, u32 size)
{
	struct panel_poc_device *poc_dev = &panel->poc_dev;
	struct dsim_device *dsim = container_of(panel, struct dsim_device, priv);
	int i, ret = 0;
	u32 poc_addr;
	u8 WRITE_CMD[ARRAY_SIZE(SEQ_POC_WR_STT)] = { 0, };
	u8 POC_IMG_DATA[] = { 0xC1, 0x99 };

	mutex_lock(&panel->lock);

	ret = dsim_write_hl_data(dsim, SEQ_POC_F0_KEY_ENABLE, ARRAY_SIZE(SEQ_POC_F0_KEY_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F1_KEY_ENABLE, ARRAY_SIZE(SEQ_POC_F1_KEY_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_PGM_ENABLE, ARRAY_SIZE(SEQ_POC_PGM_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_WR_ENABLE, ARRAY_SIZE(SEQ_POC_WR_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_EXECUTE, ARRAY_SIZE(SEQ_POC_EXECUTE));
	udelay(2);
	ret |= dsim_write_hl_data(dsim, SEQ_POC_QD_ENABLE, ARRAY_SIZE(SEQ_POC_QD_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_EXECUTE, ARRAY_SIZE(SEQ_POC_EXECUTE));
	mdelay(10);

	if (unlikely(ret < 0)) {
		pr_err("%s, failed to read poc-wr-enter-seq\n", __func__);
		goto out_poc_write;
	}

	memcpy(WRITE_CMD, SEQ_POC_WR_STT, sizeof(WRITE_CMD));

	for (i = 0; i < size; i++) {
		if (atomic_read(&poc_dev->cancel)) {
			pr_err("%s, stopped by user at %d bytes\n",
					__func__, i);
			goto cancel_poc_write;
		}
		poc_addr = addr + i;
		if (i == 0 || (poc_addr & 0xFF) == 0) {
			poc_addr = addr + i;
			WRITE_CMD[6] = (poc_addr & 0xFF0000) >> 16;
			WRITE_CMD[7] = (poc_addr & 0x00FF00) >> 8;
			WRITE_CMD[8] = (poc_addr & 0x0000FF);
			ret = dsim_write_hl_data(dsim, SEQ_POC_WR_ENABLE, ARRAY_SIZE(SEQ_POC_WR_ENABLE));
			ret |= dsim_write_hl_data(dsim, SEQ_POC_EXECUTE, ARRAY_SIZE(SEQ_POC_EXECUTE));
			udelay(2);
			ret |= dsim_write_hl_data(dsim, WRITE_CMD, ARRAY_SIZE(WRITE_CMD));

			if (unlikely(ret < 0)) {
				pr_err("%s, failed to write poc-wr-stt seq\n", __func__);
				goto out_poc_write;
			}
		}
		POC_IMG_DATA[1] = data[i];
		ret = dsim_write_hl_data(dsim, POC_IMG_DATA, ARRAY_SIZE(POC_IMG_DATA));
		ret |= dsim_write_hl_data(dsim, SEQ_POC_EXECUTE, ARRAY_SIZE(SEQ_POC_EXECUTE));

		if (unlikely(ret < 0)) {
			pr_err("%s, failed to write poc-wr-img seq\n", __func__);
			goto out_poc_write;
		}
		udelay(2);

		if ((i % 4096) == 0)
			pr_info("%s addr %06X %02X\n", __func__, poc_addr, data[i]);

		if ((poc_addr & 0xFF) == 0xFF || i == (size - 1)) {
			ret = dsim_write_hl_data(dsim, SEQ_POC_WR_END, ARRAY_SIZE(SEQ_POC_WR_END));

			if (unlikely(ret < 0)) {
				pr_err("%s, failed to write poc-wr-exit seq\n", __func__);
				goto out_poc_write;
			}
			udelay(800);
		}
	}

	ret = dsim_write_hl_data(dsim, SEQ_POC_PGM_DISABLE, ARRAY_SIZE(SEQ_POC_PGM_DISABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F1_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F1_KEY_DISABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F0_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F0_KEY_DISABLE));

	if (unlikely(ret < 0)) {
		pr_err("%s, failed to write poc-wr-exit seq\n", __func__);
		goto out_poc_write;
	}
	mutex_unlock(&panel->lock);

	pr_info("%s poc write addr 0x%06X, %d(0x%X) bytes\n",
			__func__, addr, size, size);

	return 0;

cancel_poc_write:
	ret = dsim_write_hl_data(dsim, SEQ_POC_PGM_DISABLE, ARRAY_SIZE(SEQ_POC_PGM_DISABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F1_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F1_KEY_DISABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F0_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F0_KEY_DISABLE));

	if (unlikely(ret < 0))
		pr_err("%s, failed to read poc-wr-exit seq\n", __func__);
	ret = -EIO;
	atomic_set(&poc_dev->cancel, 0);

out_poc_write:
	mutex_unlock(&panel->lock);
	return ret;
}

static int poc_get_octa_poc(struct panel_private *panel)
{
	struct panel_poc_device *poc_dev = &panel->poc_dev;
	struct panel_poc_info *poc_info = &poc_dev->poc_info;

	poc_info->poc = panel->octa_id[2] & 0x0F;

	pr_info("%s poc %d\n", __func__, poc_info->poc);

	return 0;
}

static int poc_get_poc_chksum(struct panel_private *panel)
{
	struct panel_poc_device *poc_dev = &panel->poc_dev;
	struct panel_poc_info *poc_info = &poc_dev->poc_info;	
	struct dsim_device *dsim = container_of(panel, struct dsim_device, priv);
	int ret;

	mutex_lock(&panel->lock);
	ret = dsim_write_hl_data(dsim, SEQ_POC_F0_KEY_ENABLE, ARRAY_SIZE(SEQ_POC_F0_KEY_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F1_KEY_ENABLE, ARRAY_SIZE(SEQ_POC_F1_KEY_ENABLE));
	if (unlikely(ret < 0)) {
		pr_err("%s, failed to poc-checksum-enter-seq\n", __func__);
		ret = -EINVAL;
		goto out_poc_chksum;
	}
	if (panel->poc_allow_chechsum_read) {
		ret = dsim_read_hl_data(dsim, POC_CHKSUM_REG, POC_CHKSUM_LEN, poc_info->poc_chksum);
		if (ret != POC_CHKSUM_LEN) {
			pr_err("%s poc checksum length is invalid\n", __func__);
			ret = -EINVAL;
			goto out_poc_chksum;

		}
	}
	else {	
		pr_info("%s: skip read\n", __func__);
		memcpy(poc_info->poc_chksum, panel->poc_checksum_set, POC_CHKSUM_LEN);
	}

	pr_info("%s poc_chksum 0x%02X 0x%02X 0x%02X 0x%02X, result %d\n",
			__func__, poc_info->poc_chksum[0], poc_info->poc_chksum[1],
			poc_info->poc_chksum[2], poc_info->poc_chksum[3],
			poc_info->poc_chksum[4]);

out_poc_chksum:
	ret = dsim_write_hl_data(dsim, SEQ_POC_F1_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F1_KEY_DISABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F0_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F0_KEY_DISABLE));
	if (unlikely(ret < 0)) {
		pr_err("%s, failed to poc-checksum-exit-seq\n", __func__);
		ret = -EINVAL;
	}
	mutex_unlock(&panel->lock);

	return ret;
}

static int poc_get_poc_ctrl(struct panel_private *panel)
{
	struct panel_poc_device *poc_dev = &panel->poc_dev;
	struct panel_poc_info *poc_info = &poc_dev->poc_info;	
	struct dsim_device *dsim = container_of(panel, struct dsim_device, priv);
	int ret;

	mutex_lock(&panel->lock);
	ret = dsim_write_hl_data(dsim, SEQ_POC_F0_KEY_ENABLE, ARRAY_SIZE(SEQ_POC_F0_KEY_ENABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F1_KEY_ENABLE, ARRAY_SIZE(SEQ_POC_F1_KEY_ENABLE));
	if (unlikely(ret < 0)) {
		pr_err("%s, failed to poc-ctrl-enter-seq\n", __func__);
		ret = -EINVAL;
		goto out_poc_ctrl;

	}
	
	if (panel->poc_allow_chechsum_read) {
		ret = dsim_read_hl_data(dsim, POC_CTRL_REG, POC_CTRL_LEN, poc_info->poc_ctrl);
		if (ret != POC_CTRL_LEN) {
			pr_err("%s poc ctrl length is invalid\n", __func__);
			ret = -EINVAL;
			goto out_poc_ctrl;
		}
	}
	else {
		pr_info("%s: skip read\n", __func__);
		memcpy(poc_info->poc_ctrl, panel->poc_ctrl_set, POC_CTRL_LEN);
	}

	pr_info("%s poc_ctrl 0x%02X 0x%02X 0x%02X 0x%02X\n",
			__func__, poc_info->poc_ctrl[0], poc_info->poc_ctrl[1],
			poc_info->poc_ctrl[2], poc_info->poc_ctrl[3]);

	ret = dsim_write_hl_data(dsim, SEQ_POC_F1_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F1_KEY_DISABLE));
	ret |= dsim_write_hl_data(dsim, SEQ_POC_F0_KEY_DISABLE, ARRAY_SIZE(SEQ_POC_F0_KEY_DISABLE));
	if (unlikely(ret < 0)) {
		pr_err("%s, failed to poc-ctrl-exit-seq\n", __func__);
		ret = -EINVAL;
		goto out_poc_ctrl;

	}
out_poc_ctrl:
	mutex_unlock(&panel->lock);

	return ret;
}

int poc_test_pattern(u8 *buf, int size, int pattern)
{
	int i;

	if (pattern == 0) {
		for (i = 0; i < size; i++)
			buf[i] = (i % 256);
	} else {
		for (i = 0; i < size; i++)
			buf[i] = ((i % 2) == 0) ? 0x5A : 0xA5;
	}

	return 0;
}

int poc_test_verification(u8 *buf, int size, int pattern)
{
	int i, val, error_count = 0;

	for (i = 0; i < size; i++) {
		if (pattern == 0)
			val = (i % 256);
		else
			val = ((i % 2) == 0) ? 0x5A : 0xA5;

		if (buf[i] != val) {
			pr_warn("buf[%d] 0x%02x 0x%02x\n", i, buf[i], val);
			error_count++;
		}
	}

	pr_info("%s %s (error %d detected)\n", __func__,
			error_count ? "FAILED" : "SUCCESS", error_count);

	return 0;
}

int poc_verification(u8 *buf1, u8 *buf2, int size)
{
	int i, error_count = 0;

	for (i = 0; i < size; i++) {
		if (buf1[i] != buf2[i]) {
			pr_warn("buf[%d] 0x%02x 0x%02x\n", i, buf1[i], buf2[i]);
			error_count++;
		}
	}

	pr_info("%s %s (error %d detected)\n", __func__,
			error_count ? "FAILED" : "SUCCESS", error_count);

	return 0;
}

int poc_img_backup(struct panel_private *panel, int size)
{
	struct file *fp;
	mm_segment_t old_fs;
	int len = 0, pos = 0;
	ssize_t res;
	struct panel_poc_device *poc_dev = &panel->poc_dev;
	struct panel_poc_info *poc_info = &poc_dev->poc_info;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	pr_info("%s start\n", __func__);
	fp = filp_open(POC_IMG_PATH, O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0660);
	if (IS_ERR(fp)) {
		pr_err("%s, fail to open log file\n", __func__);
		goto open_err;
	}

	poc_info->rbuf = poc_rd_img;
	while (pos < size) {
		len = ((size - pos) < POC_IMG_WR_SIZE) ?
			(size - pos) : POC_IMG_WR_SIZE ;
		poc_info->rpos = pos;
		poc_info->rsize = len;
		res = poc_read_data(panel, &poc_info->rbuf[poc_info->rpos],
				POC_IMG_ADDR + poc_info->rpos, poc_info->rsize);
		if (res) {
			pr_err("%s failed to read poc\n", __func__);
			filp_close(fp, current->files);
			set_fs(old_fs);
			return -1;
		}
		vfs_write(fp, (u8 __user *)&poc_info->rbuf[pos], len, &fp->f_pos);
		pos += len;

		pr_info("%s write %d bytes\n", __func__, len);
	}

	pr_info("%s write %d bytes done!!\n", __func__, size);

	filp_close(fp, current->files);
	set_fs(old_fs);

	return 0;

open_err:
	set_fs(old_fs);
	return -1;
}

int set_panel_poc(struct panel_poc_device *poc_dev, u32 cmd)
{
	struct panel_poc_info *poc_info = &poc_dev->poc_info;
	struct panel_private *panel = container_of(poc_dev, struct panel_private, poc_dev);
	int ret= 0;

	if (cmd >= MAX_POC_OP) {
		pr_err("%s invalid poc_op %d\n", __func__, cmd);
		return -EINVAL;
	}

	pr_info("%s %s +\n", __func__, poc_op_string[cmd]);

	switch (cmd) {
		case POC_OP_ERASE:
			ret = poc_erase(panel);
			if (unlikely(ret < 0)) {
				pr_err("%s, failed to write poc-erase-seq\n", __func__);
				return ret;
			}
			poc_info->erased = true;
			break;
		case POC_OP_WRITE:
			ret = poc_write_data(panel, &poc_info->wbuf[poc_info->wpos],
					POC_IMG_ADDR + poc_info->wpos, poc_info->wsize);
			if (unlikely(ret < 0)) {
				pr_err("%s, failed to write poc-write-seq\n", __func__);
				return ret;
			}
			break;
		case POC_OP_READ:
			ret = poc_read_data(panel, &poc_info->rbuf[poc_info->rpos],
					POC_IMG_ADDR + poc_info->rpos, poc_info->rsize);
			if (unlikely(ret < 0)) {
				pr_err("%s, failed to write poc-read-seq\n", __func__);
				return ret;
			}
			break;
		case POC_OP_CHECKSUM:
			ret = poc_get_poc_chksum(panel);
			if (unlikely(ret < 0)) {
				pr_err("%s, failed to get poc checksum\n", __func__);
				return ret;
			}			
			// read once at 1-frame
			mdelay(17);
			ret = poc_get_poc_ctrl(panel);
			if (unlikely(ret < 0)) {
				pr_err("%s, failed to get poc ctrl\n", __func__);
				return ret;
			}
			break;
		case POC_OP_CHECKPOC:
			ret = poc_get_octa_poc(panel);
			if (unlikely(ret < 0)) {
				pr_err("%s, failed to get_octa_poc\n", __func__);
				return ret;
			}
			break;
		case POC_OP_BACKUP:
			ret = poc_img_backup(panel, POC_IMG_SIZE);
			if (unlikely(ret < 0)) {
				pr_err("%s, failed to backup poc-image\n", __func__);
				return ret;
			}
		case POC_OP_READ_TEST:
#ifdef POC_TEST_PATTERN_DEBUG
			pr_info("%s read pattern (%d bytes)\n", __func__, POC_IMG_SIZE);
			poc_info->rbuf = poc_rd_img;
			poc_info->rpos = 0;
			poc_info->rsize = POC_IMG_SIZE;
			ret = poc_read_data(panel, &poc_info->rbuf[poc_info->rpos],
					POC_IMG_ADDR + poc_info->rpos, poc_info->rsize);
			if (unlikely(ret < 0)) {
				pr_err("%s, failed to write poc-read-seq\n", __func__);
				return ret;
			}
			poc_test_verification(poc_rd_img, POC_IMG_SIZE, 0);
#else
			pr_info("%s unsupported READ_TEST\n", __func__);
#endif
			break;
		case POC_OP_WRITE_TEST:
#ifdef POC_TEST_PATTERN_DEBUG
			pr_info("%s erase pattern\n", __func__);
			ret = poc_erase(panel);
			if (unlikely(ret < 0)) {
				pr_err("%s, failed to write poc-erase-seq\n", __func__);
				return ret;
			}

			pr_info("%s write pattern (%d bytes)\n", __func__, POC_IMG_SIZE);
			poc_test_pattern(poc_wr_img, POC_IMG_SIZE, 0);
			poc_info->wbuf = poc_wr_img;
			poc_info->wpos = 0;
			poc_info->wsize = POC_IMG_SIZE;
			ret = poc_write_data(panel, &poc_info->wbuf[poc_info->wpos],
					POC_IMG_ADDR + poc_info->wpos, poc_info->wsize);
			if (unlikely(ret < 0)) {
				pr_err("%s, failed to write poc-write-seq\n", __func__);
				return ret;
			}
#else
			pr_info("%s unsupported WRITE_TEST\n", __func__);
#endif
			break;
		case POC_OP_SELF_TEST:
#ifdef POC_TEST_PATTERN_DEBUG
			pr_info("%s erase pattern\n", __func__);
			ret = poc_erase(panel);
			if (unlikely(ret < 0)) {
				pr_err("%s, failed to write poc-erase-seq\n", __func__);
				return ret;
			}

			pr_info("%s write pattern (%d bytes)\n", __func__, POC_TEST_PATTERN_SIZE);
			poc_test_pattern(poc_wr_img, POC_TEST_PATTERN_SIZE, 0);
			poc_info->wbuf = poc_wr_img;
			poc_info->wpos = 0;
			poc_info->wsize = POC_TEST_PATTERN_SIZE;
			ret = poc_write_data(panel, &poc_info->wbuf[poc_info->wpos],
					POC_IMG_ADDR + poc_info->wpos, poc_info->wsize);
			if (unlikely(ret < 0)) {
				pr_err("%s, failed to write poc-write-seq\n", __func__);
				return ret;
			}

			msleep(200);
			pr_info("%s read pattern (%d bytes)\n", __func__, POC_TEST_PATTERN_SIZE);
			poc_info->rbuf = poc_rd_img;
			poc_info->rpos = 0;
			poc_info->rsize = POC_TEST_PATTERN_SIZE;
			ret = poc_read_data(panel, &poc_info->rbuf[poc_info->rpos],
					POC_IMG_ADDR + poc_info->rpos, poc_info->rsize);
			if (unlikely(ret < 0)) {
				pr_err("%s, failed to write poc-read-seq\n", __func__);
				return ret;
			}
			poc_test_verification(poc_rd_img, POC_TEST_PATTERN_SIZE, 0);
#else
			pr_info("%s unsupported SELF_TEST\n", __func__);
#endif
			break;
		case POC_OP_NONE:
			pr_info("%s none operation\n", __func__);
			break;
		default:
			pr_err("%s invalid poc op\n", __func__);
			break;
	}

	pr_info("%s %s -\n", __func__, poc_op_string[cmd]);

	return 0;


	return 0;
}


static long panel_poc_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct panel_poc_device *poc_dev = file->private_data;
	struct panel_poc_info *poc_info = &poc_dev->poc_info;
	struct panel_private *panel = container_of(poc_dev, struct panel_private, poc_dev);
	int ret;

	if (unlikely(!poc_dev->opened)) {
		dsim_err("POC:ERR:%s: poc device not opened\n", __func__);
		return -EIO;
	}

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_err("POC:WARN:%s:panel is not active\n", __func__);
		return -EAGAIN;
	}

	switch (cmd) {
		case IOC_GET_POC_STATUS:
			if (copy_to_user((u32 __user *)arg, &poc_info->state,
						sizeof(poc_info->state))) {
				ret = -EFAULT;
				break;
			}
			break;
		case IOC_GET_POC_CHKSUM:
			ret = set_panel_poc(poc_dev, POC_OP_CHECKSUM);
			if (ret) {
				dsim_err("%s error set_panel_poc\n", __func__);
				ret = -EFAULT;
				break;
			}
			if (copy_to_user((u8 __user *)arg, &poc_info->poc_chksum[4],
						sizeof(poc_info->poc_chksum[4]))) {
				ret = -EFAULT;
				break;
			}
			break;
		case IOC_GET_POC_CSDATA:
			ret = set_panel_poc(poc_dev, POC_OP_CHECKSUM);
			if (ret) {
				dsim_err("%s error set_panel_poc\n", __func__);
				ret = -EFAULT;
				break;
			}
			if (copy_to_user((u8 __user *)arg, poc_info->poc_chksum,
						sizeof(poc_info->poc_chksum))) {
				ret = -EFAULT;
				break;
			}
			break;
		case IOC_GET_POC_ERASED:
			if (copy_to_user((u8 __user *)arg, &poc_info->erased,
						sizeof(poc_info->erased))) {
				ret = -EFAULT;
				break;
			}
			break;
		case IOC_GET_POC_FLASHED:
			ret = set_panel_poc(poc_dev, POC_OP_CHECKPOC);
			if (ret) {
				dsim_err("%s error set_panel_poc\n", __func__);
				ret = -EFAULT;
				break;
			}
			if (copy_to_user((u8 __user *)arg, &poc_info->poc,
						sizeof(poc_info->poc))) {
				ret = -EFAULT;
				break;
			}
			break;
		case IOC_SET_POC_ERASE:
			ret = set_panel_poc(poc_dev, POC_OP_ERASE);
			if (ret) {
				dsim_err("%s error set_panel_poc\n", __func__);
				ret = -EFAULT;
				break;
			}
			break;
		default:
			break;
	};

	return 0;
}

static int panel_poc_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct panel_poc_device *poc_dev = container_of(dev, struct panel_poc_device, dev);
	struct panel_poc_info *poc_info = &poc_dev->poc_info;
	struct panel_private *panel = container_of(poc_dev, struct panel_private, poc_dev);


	pr_info("%s was called\n", __func__);

	if (poc_dev->opened) {
		dsim_err("POC:ERR:%s: already opend\n", __func__);
		return -EBUSY;
	}

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_err("POC:WARN:%s:panel is not active %d\n", __func__, panel->state);
		return -EAGAIN;
	}

	poc_info->state = 0;
	memset(poc_info->poc_chksum, 0, sizeof(poc_info->poc_chksum));
	memset(poc_info->poc_ctrl, 0, sizeof(poc_info->poc_ctrl));

	poc_info->wbuf = poc_wr_img;
	poc_info->wpos = 0;
	poc_info->wsize = 0;

	poc_info->rbuf = poc_rd_img;
	poc_info->rpos = 0;
	poc_info->rsize = 0;

	file->private_data = poc_dev;
	poc_dev->opened = 1;
	atomic_set(&poc_dev->cancel, 0);

	return 0;
}

static int panel_poc_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct panel_poc_device *poc_dev = file->private_data;
	struct panel_poc_info *poc_info = &poc_dev->poc_info;

	pr_info("%s was called\n", __func__);

	poc_info->state = 0;
	memset(poc_info->poc_chksum, 0, sizeof(poc_info->poc_chksum));
	memset(poc_info->poc_ctrl, 0, sizeof(poc_info->poc_ctrl));

	poc_info->wbuf = NULL;
	poc_info->wpos = 0;
	poc_info->wsize = 0;

	poc_info->rbuf = NULL;
	poc_info->rpos = 0;
	poc_info->rsize = 0;

	poc_dev->opened = 0;
	atomic_set(&poc_dev->cancel, 0);

	return ret;
}

static ssize_t panel_poc_read(struct file *file, char __user *buf, size_t count,
		loff_t *ppos)
{
	struct panel_poc_device *poc_dev = file->private_data;
	struct panel_poc_info *poc_info = &poc_dev->poc_info;
	struct panel_private *panel = container_of(poc_dev, struct panel_private, poc_dev);
	ssize_t res;

	pr_info("%s : size : %d, ppos %d\n", __func__, (int)count, (int)*ppos);

	if (unlikely(!poc_dev->opened)) {
		dsim_err("POC:ERR:%s: poc device not opened\n", __func__);
		return -EIO;
	}

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_err("POC:WARN:%s:panel is not active\n", __func__);
		return -EAGAIN;
	}

	if (unlikely(!buf)) {
		dsim_err("POC:ERR:%s: invalid read buffer\n", __func__);
		return -EINVAL;
	}

	if (unlikely(*ppos >= POC_IMG_SIZE)) {
		dsim_err("POC:ERR:%s: invalid read pos %d\n",
				__func__, (int)*ppos);
		return -EINVAL;
	}

	poc_info->rbuf = poc_rd_img;
	poc_info->rpos = *ppos;
	if (*ppos + count > POC_IMG_SIZE) {
		dsim_err("POC:WARN:%s: adjust count %d\n", __func__, (int)count);
		poc_info->rsize = POC_IMG_SIZE - *ppos;
	} else {
		poc_info->rsize = count;
	}

	res = set_panel_poc(poc_dev, POC_OP_READ);
	if (res < 0)
		goto err_read;

#ifdef POC_DEBUG
	res = sprintf_hex_to_str(poc_txt_img, &poc_info->rbuf[poc_info->rpos], poc_info->rsize);
	res = simple_read_from_buffer(buf, count, ppos, poc_txt_img, POC_IMG_SIZE);
#else
	res = simple_read_from_buffer(buf, poc_info->rsize, ppos, poc_info->rbuf, POC_IMG_SIZE);
#endif

	pr_info("%s read %ld bytes (count %ld)\n", __func__, res, count);

err_read:
	return res;
}

static ssize_t panel_poc_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	struct panel_poc_device *poc_dev = file->private_data;
	struct panel_poc_info *poc_info = &poc_dev->poc_info;
	struct panel_private *panel = container_of(poc_dev, struct panel_private, poc_dev);
	ssize_t res;

	pr_info("%s : size : %d, ppos %d\n", __func__, (int)count, (int)*ppos);

	if (unlikely(!poc_dev->opened)) {
		dsim_err("POC:ERR:%s: poc device not opened\n", __func__);
		return -EIO;
	}

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_err("POC:WARN:%s:panel is not active\n", __func__);
		return -EAGAIN;
	}

	if (unlikely(!buf)) {
		dsim_err("POC:ERR:%s: invalid write buffer\n", __func__);
		return -EINVAL;
	}

	if (unlikely(*ppos + count > POC_IMG_SIZE)) {
		dsim_err("POC:ERR:%s: invalid write size pos %d, size %d\n",
				__func__, (int)*ppos, (int)count);
		return -EINVAL;
	}

	if (*ppos == 0) {
		res = set_panel_poc(poc_dev, POC_OP_ERASE);
		if (res)
			goto err_write;
	}

	poc_info->wbuf = poc_wr_img;
	poc_info->wpos = *ppos;
	poc_info->wsize = count;
	res = simple_write_to_buffer(poc_info->wbuf, POC_IMG_SIZE, ppos, buf, count);

	pr_info("%s write %ld bytes (count %ld)\n", __func__, res, count);

	res = set_panel_poc(poc_dev, POC_OP_WRITE);
	if (res < 0)
		goto err_write;

	return count;

err_write:
	mutex_unlock(&panel->lock);
	return res;
}

static const struct file_operations panel_poc_fops = {
	.owner = THIS_MODULE,
	.read = panel_poc_read,
	.write = panel_poc_write,
	.unlocked_ioctl = panel_poc_ioctl,
	.open = panel_poc_open,
	.release = panel_poc_release,
};


#ifdef CONFIG_DISPLAY_USE_INFO
enum {
	EPOCEFS_NOENT = 1,		/* No such file or directory */
	EPOCEFS_EMPTY = 2,		/* Empty file */
	EPOCEFS_READ = 3,		/* Read failed */
	MAX_EPOCEFS,
};

static int poc_get_efs_s32(char *filename, int *value)
{
	mm_segment_t old_fs;
	struct file *filp = NULL;
	int fsize = 0, nread, ret = 0;

	if (!filename || !value) {
		pr_err("%s invalid parameter\n", __func__);
		return -EINVAL;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(filename, O_RDONLY, 0440);
	if (IS_ERR(filp)) {
		ret = PTR_ERR(filp);
		if (ret == -ENOENT)
			pr_err("%s file(%s) not exist\n", __func__, filename);
		else
			pr_info("%s file(%s) open error(ret %d)\n",
					__func__, filename, ret);
		set_fs(old_fs);
		return -EPOCEFS_NOENT;
	}

	if (filp->f_path.dentry && filp->f_path.dentry->d_inode)
		fsize = filp->f_path.dentry->d_inode->i_size;

	if (fsize == 0) {
		pr_err("%s invalid file(%s) size %d\n",
				__func__, filename, fsize);
		ret = -EPOCEFS_EMPTY;
		goto exit;
	}

	nread = vfs_read(filp, (char __user *)value, 4, &filp->f_pos);
	if (nread != 4) {
		pr_err("%s failed to read (ret %d)\n", __func__, nread);
		ret = -EPOCEFS_READ;
		goto exit;
	}

	pr_info("%s %s(size %d) : %d\n", __func__, filename, fsize, *value);

exit:
	filp_close(filp, current->files);
	set_fs(old_fs);

	return ret;
}

static int poc_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct panel_poc_device *poc_dev;
	struct panel_poc_info *poc_info;
	struct dpui_info *dpui = data;
	char tbuf[MAX_DPUI_VAL_LEN];
	int size, ret;

	if (dpui == NULL) {
		pr_err("%s: dpui is null\n", __func__);
		return 0;
	}

	poc_dev = container_of(self, struct panel_poc_device, poc_notif);
	poc_info = &poc_dev->poc_info;

	ret = poc_get_efs_s32(POC_TOTAL_TRY_COUNT_FILE_PATH, &poc_info->total_trycount);
	if (ret < 0)
		poc_info->total_trycount = (ret > -MAX_EPOCEFS) ? ret : -1;
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", poc_info->total_trycount);
	set_dpui_field(DPUI_KEY_PNPOCT, tbuf, size);

	ret = poc_get_efs_s32(POC_TOTAL_FAIL_COUNT_FILE_PATH, &poc_info->total_failcount);
	if (ret < 0)
		poc_info->total_failcount = (ret > -MAX_EPOCEFS) ? ret : -1;
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", poc_info->total_failcount);
	set_dpui_field(DPUI_KEY_PNPOCF, tbuf, size);

	return 0;
}
#endif /* CONFIG_DISPLAY_USE_INFO */

int panel_poc_probe(struct panel_poc_device *poc_dev)
{
	struct panel_poc_info *poc_info;
	int ret;

	if (poc_dev == NULL) {
		dsim_err("POC:ERR:%s: invalid live clk \n", __func__);
		return -EINVAL;
	}

	poc_info = &poc_dev->poc_info;
	poc_dev->dev.minor = MISC_PANEL_POC_MINOR;
	poc_dev->dev.name = "poc";
	poc_dev->dev.fops = &panel_poc_fops;
	poc_dev->dev.parent = NULL;

	ret = misc_register(&poc_dev->dev);
	if (ret) {
		dsim_err("PANEL:ERR:%s: failed to register panel misc driver (ret %d)\n",
				__func__, ret);
		goto exit_probe;
	}

	poc_info->erased = false;
	poc_info->poc = 1;	/* default enabled */
	poc_dev->opened = 0;

#ifdef CONFIG_DISPLAY_USE_INFO
	poc_info->total_trycount = -1;
	poc_info->total_failcount = -1;

	poc_dev->poc_notif.notifier_call = poc_notifier_callback;
	ret = dpui_logging_register(&poc_dev->poc_notif, DPUI_TYPE_PANEL);
	if (ret) {
		dsim_err("ERR:PANEL:%s:failed to register dpui notifier callback\n", __func__);
		goto exit_probe;
	}
#endif
exit_probe:
	return ret;
}

