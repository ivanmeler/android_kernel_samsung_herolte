
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

#include "fts_ts.h"

#define WRITE_CHUNK_SIZE 64
#define FTS_DEFAULT_UMS_FW "/sdcard/stm.fw"
#define FTS_DEFAULT_FFU_FW "ffu_tsp.bin"
#define FTS64FILE_SIGNATURE 0xaaaa5555

enum {
	BUILT_IN = 0,
	UMS,
	NONE,
	FFU,
};

struct fts64_header {
	unsigned int signature;
	unsigned short fw_ver;
	unsigned char fw_id;
	unsigned char reserved1;
	unsigned char internal_ver[8];
	unsigned char released_ver[8];
	unsigned int reserved2;
	unsigned int checksum;
};

static int fts_fw_wait_for_flash_ready(struct fts_ts_info *info)
{
	unsigned char regAdd;
	unsigned char buf[3];
	int retry = 0;

	regAdd = FTS_CMD_READ_FLASH_STAT;

	while (info->fts_read_reg
		(info, &regAdd, 1, (unsigned char *)buf, 1)) {
		if ((buf[0] & 0x01) == 0)
			break;

		if (retry++ > FTS_RETRY_COUNT * 10) {
			tsp_debug_err(true, info->dev,
				 "%s: Time Over\n",
				 __func__);
			return -FTS_ERROR_TIMEOUT;
		}
		msleep(20);
	}

	return 0;
}

#define	FW_IMAGE_SIZE_D1	64 * 1024
#define	FW_IMAGE_SIZE_D2	128 * 1024
#define	SIGNEDKEY_SIZE	256

static int fts_fw_burn(struct fts_ts_info *info, unsigned char *fw_data)
{
	unsigned char regAdd[WRITE_CHUNK_SIZE + 3];
	int section;
	int fsize = FW_IMAGE_SIZE_D1;

	/* Check busy Flash */
	if (fts_fw_wait_for_flash_ready(info)<0)
		return -1;

	/* FTS_CMD_UNLOCK_FLASH */
	tsp_debug_info(true, info->dev, "%s: Unlock Flash\n", __func__);
	regAdd[0] = FTS_CMD_UNLOCK_FLASH;
	regAdd[1] = 0x74;
	regAdd[2] = 0x45;
	info->fts_write_reg(info, &regAdd[0], 3);
	msleep(500);

	/* Copy to PRAM */
	if (info->digital_rev == FTS_DIGITAL_REV_2)
		fsize = FW_IMAGE_SIZE_D2 + sizeof(struct fts64_header);

	tsp_debug_info(true, info->dev, "%s: Copy to PRAM [Size : %d]\n", __func__, fsize);

	for (section = 0; section < (fsize / WRITE_CHUNK_SIZE); section++) {
		regAdd[0] = FTS_CMD_WRITE_PRAM + (((section * WRITE_CHUNK_SIZE) >> 16) & 0x0f);
		regAdd[1] = ((section * WRITE_CHUNK_SIZE) >> 8) & 0xff;
		regAdd[2] = (section * WRITE_CHUNK_SIZE) & 0xff;
		memcpy(&regAdd[3],
			&fw_data[section * WRITE_CHUNK_SIZE +
				sizeof(struct fts64_header)],
			WRITE_CHUNK_SIZE);

		info->fts_write_reg(info, &regAdd[0], WRITE_CHUNK_SIZE + 3);
	}

	msleep(100);

	/* Erase Program Flash */
	tsp_debug_info(true, info->dev, "%s: Erase Program Flash\n", __func__);
	info->fts_command(info, FTS_CMD_ERASE_PROG_FLASH);
	msleep(100);

	/* Check busy Flash */
	if (fts_fw_wait_for_flash_ready(info)<0)
		return -1;

	/* Burn Program Flash */
	tsp_debug_info(true, info->dev, "%s: Burn Program Flash\n", __func__);
	info->fts_command(info, FTS_CMD_BURN_PROG_FLASH);
	msleep(100);

	/* Check busy Flash */
	if (fts_fw_wait_for_flash_ready(info)<0)
		return -1;

	/* Reset FTS */
	info->fts_systemreset(info);
	return 0;
}

static int fts_get_system_status(struct fts_ts_info *info, unsigned char *val1, unsigned char *val2)
{
	bool rc = -1;
	unsigned char regAdd1[4] = { 0xb2, 0x07, 0xfb, 0x04 };
	unsigned char regAdd2[4] = { 0xb2, 0x17, 0xfb, 0x04 };
	unsigned char data[FTS_EVENT_SIZE];
	int retry = 0;

	if (info->digital_rev == FTS_DIGITAL_REV_2)
		regAdd2[1] = 0x1f;

	info->fts_write_reg(info, &regAdd1[0], 4);
	info->fts_write_reg(info, &regAdd2[0], 4);

	memset(data, 0x0, FTS_EVENT_SIZE);
	regAdd1[0] = READ_ONE_EVENT;

	while (info->fts_read_reg(info, &regAdd1[0], 1, (unsigned char *)data,
				   FTS_EVENT_SIZE)) {
		if ((data[0] == 0x12) && (data[1] == regAdd1[1])
			&& (data[2] == regAdd1[2])) {
			rc = 0;
			*val1 = data[3];
			tsp_debug_info(true, info->dev,
				"%s: System Status 1 : 0x%02x\n",
			__func__, data[3]);
		}
		else if ((data[0] == 0x12) && (data[1] == regAdd2[1])
			&& (data[2] == regAdd2[2])) {
			rc = 0;
			*val2 = data[3];
			tsp_debug_info(true, info->dev,
				"%s: System Status 2 : 0x%02x\n",
			__func__, data[3]);
			break;
		}

		if (retry++ > FTS_RETRY_COUNT) {
			rc = -1;
			tsp_debug_err(true, info->dev,
				"%s: Time Over\n", __func__);
			break;
		}
	}
	return rc;
}

int fts_fw_wait_for_event(struct fts_ts_info *info, unsigned char eid)
{
	int rc;
	unsigned char regAdd;
	unsigned char data[FTS_EVENT_SIZE];
	int retry = 0;

	memset(data, 0x0, FTS_EVENT_SIZE);

	regAdd = READ_ONE_EVENT;
	rc = -1;
	while (info->fts_read_reg
	       (info, &regAdd, 1, (unsigned char *)data, FTS_EVENT_SIZE)) {
		if ((data[0] == EVENTID_STATUS_EVENT) &&
			(data[1] == eid)) {
			rc = 0;
			break;
		}

		if (retry++ > FTS_RETRY_COUNT * 15) {
			rc = -1;
			tsp_debug_info(true, info->dev, "%s: Time Over\n", __func__);
			break;
		}
		msleep(20);
	}

	return rc;
}

void fts_execute_autotune(struct fts_ts_info *info)
{
	info->fts_command(info, CX_TUNNING);
	msleep(300);
	fts_fw_wait_for_event(info, STATUS_EVENT_MUTUAL_AUTOTUNE_DONE);

#ifdef FTS_SUPPORT_WATER_MODE
	fts_fw_wait_for_event (info, STATUS_EVENT_WATER_SELF_AUTOTUNE_DONE);
	fts_fw_wait_for_event(info, STATUS_EVENT_SELF_AUTOTUNE_DONE);
#endif
#ifdef FTS_SUPPORT_SELF_MODE
	info->fts_command(info, SELF_AUTO_TUNE);
	msleep(300);
	fts_fw_wait_for_event(info, STATUS_EVENT_SELF_AUTOTUNE_DONE);
#endif

	info->fts_command(info, FTS_CMD_SAVE_CX_TUNING);
	msleep(230);
	fts_fw_wait_for_event(info, STATUS_EVENT_FLASH_WRITE_CXTUNE_VALUE);
}

void fts_fw_init(struct fts_ts_info *info)
{
	info->fts_command(info, SLEEPOUT);
	msleep(50);

	if (info->digital_rev == FTS_DIGITAL_REV_2) {
		info->fts_command(info, FTS_CMD_TRIM_LOW_POWER_OSCILLATOR);
		msleep(300);
	}

	fts_execute_autotune(info);

	info->fts_command(info, SLEEPOUT);
	msleep(50);

	info->fts_command(info, SENSEON);

#ifdef FTS_SUPPORT_WATER_MODE
	fts_fw_wait_for_event(info, STATUS_EVENT_WATER_SELF_DONE);
#else
	fts_fw_wait_for_event (info, STATUS_EVENT_FORCE_CAL_DONE);
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey)
		info->fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif
}

const int fts_fw_updater(struct fts_ts_info *info, unsigned char *fw_data)
{
	const struct fts64_header *header;
	int retval;
	int retry;
	unsigned short fw_main_version;

	if (!fw_data) {
		tsp_debug_err(true, info->dev, "%s: Firmware data is NULL\n",
			__func__);
		return -ENODEV;
	}

	header = (struct fts64_header *)fw_data;
	fw_main_version = (header->released_ver[0] << 8) +
							(header->released_ver[1]);

	tsp_debug_info(true, info->dev,
		  "Starting firmware update : 0x%04X\n",
		  fw_main_version);

	retry = 0;
	while (1) {
		retval = fts_fw_burn(info, fw_data);
		if (retval >= 0) {
			info->fts_wait_for_ready(info);
			info->fts_get_version_info(info);

#ifdef FTS_SUPPORT_NOISE_PARAM
			info->fts_get_noise_param_address(info);
#endif

			if (fw_main_version == info->fw_main_version_of_ic) {
				tsp_debug_info(true, info->dev,
					  "%s: Success Firmware update\n",
					  __func__);
				fts_fw_init(info);
				retval = 0;
				break;
			}
		}

		if (retry++ > 3) {
			tsp_debug_err(true, info->dev, "%s: Fail Firmware update\n",
				 __func__);
			retval = -1;
			break;
		}
	}

	return retval;
}
EXPORT_SYMBOL(fts_fw_updater);

#define FW_IMAGE_NAME_D2_TB_INTEG			"tsp_stm/stm_tb_integ.fw"
#define FW_IMAGE_NAME_D2_Z2A			"tsp_stm/stm_z2a.fw"
#define FW_IMAGE_NAME_D2_Z2I			"tsp_stm/stm_z2i.fw"
#define CONFIG_ID_D1_S				0x2C
#define CONFIG_ID_D2_TR				0x2E
#define CONFIG_ID_D2_TB				0x30
#define CONFIG_OFFSET_BIN_D1			0xf822
#define CONFIG_OFFSET_BIN_D2			0x1E822
#define RX_OFFSET_BIN_D2				0x1E834
#define TX_OFFSET_BIN_D2				0x1E835

static bool fts_skip_firmware_update(struct fts_ts_info *info, unsigned char *fw_data)
{
	int config_id, num_rx, num_tx;

	config_id = fw_data[CONFIG_OFFSET_BIN_D2];
	num_rx = fw_data[RX_OFFSET_BIN_D2];
	num_tx = fw_data[TX_OFFSET_BIN_D2];

	if (strncmp(info->board->project_name, "TB", 2) == 0) {
		if (config_id != CONFIG_ID_D2_TB) {
			tsp_debug_info(true, info->dev, "%s: Skip update because config ID(%0x2X) is mismatched.\n",
				__func__,config_id);
			// return true; // tempory blocking to update f/w for TB, it will be remove later.. Xtopher
		}

		if ((num_rx != info->board->SenseChannelLength)
			|| (num_tx != info->board->ForceChannelLength)) {
			tsp_debug_info(true, info->dev,
				"%s: Skip update because revision is mismatched. rx[%d] tx[%d]\n",
				__func__, num_rx, num_tx);
			//return true;  // tempory blocking to update f/w for TB, it will be remove later.. Xtopher
		}
	} else if (strncmp(info->board->project_name, "TR", 2) == 0) {
		if (config_id != CONFIG_ID_D2_TR) {
			tsp_debug_info(true, info->dev,
				"%s: Skip update because config ID is mismatched. config_id[%d]\n",
				__func__, config_id);
			return true;
		}
	} else
		goto out;

out:
	return false;
}

int fts_fw_update_on_probe(struct fts_ts_info *info)
{
	int retval = 0;
	const struct firmware *fw_entry = NULL;
	unsigned char *fw_data = NULL;
	char fw_path[FTS_MAX_FW_PATH];
	const struct fts64_header *header;
	unsigned char SYS_STAT[2] = {0, };
	int tspid = 0;

	if (info->board->firmware_name)
		info->firmware_name = info->board->firmware_name;
	else
		return -1;

	snprintf(fw_path, FTS_MAX_FW_PATH, "%s", info->firmware_name);
	tsp_debug_info(true, info->dev, "%s: Load firmware : %s, Digital_rev : %d, TSP_ID : %d\n", __func__,
		  fw_path, info->digital_rev, tspid);
	retval = request_firmware(&fw_entry, fw_path, info->dev);
	if (retval) {
		tsp_debug_err(true, info->dev,
			"%s: Firmware image %s not available\n", __func__,
			fw_path);
		goto done;
	}

	if (info->digital_rev == FTS_DIGITAL_REV_1 && fw_entry->size!=(FW_IMAGE_SIZE_D1 + sizeof(struct fts64_header))) {
		tsp_debug_err(true, info->dev,
			"%s: Firmware image %s not available for FTS D1\n", __func__,
			fw_path);
		goto done;
	}

	if (info->digital_rev == FTS_DIGITAL_REV_2 && fw_entry->size!=(FW_IMAGE_SIZE_D2 + sizeof(struct fts64_header))) {
		tsp_debug_err(true, info->dev,
			"%s: Firmware image %s not available for FTS D2\n", __func__,
			fw_path);
		goto done;
	}

	fw_data = (unsigned char *)fw_entry->data;
	header = (struct fts64_header *)fw_data;

	info->fw_version_of_bin = (fw_data[5] << 8)+fw_data[4];
	info->fw_main_version_of_bin = (header->released_ver[0] << 8) +
								(header->released_ver[1]);
	if (info->digital_rev == FTS_DIGITAL_REV_1)
		info->config_version_of_bin = (fw_data[CONFIG_OFFSET_BIN_D1] << 8) + fw_data[CONFIG_OFFSET_BIN_D1 - 1];
	else
		info->config_version_of_bin = (fw_data[CONFIG_OFFSET_BIN_D2] << 8) + fw_data[CONFIG_OFFSET_BIN_D2 - 1];

	tsp_debug_info(true, info->dev,
					"Bin Firmware Version : 0x%04X "
					"Bin Config Version : 0x%04X "
					"Bin Main Version : 0x%04X\n",
					info->fw_version_of_bin,
					info->config_version_of_bin,
					info->fw_main_version_of_bin);

	if (fts_skip_firmware_update(info, fw_data))
		goto done;

	if ((info->fw_main_version_of_ic < info->fw_main_version_of_bin)
		|| ((info->config_version_of_ic < info->config_version_of_bin))
		|| ((info->fw_version_of_ic < info->fw_version_of_bin)))
		retval = fts_fw_updater(info, fw_data);
	else
		retval = FTS_NOT_ERROR;

	if (fts_get_system_status(info, &SYS_STAT[0], &SYS_STAT[1]) >= 0) {
		if (SYS_STAT[0] != SYS_STAT[1]) {
			info->fts_systemreset(info);
			msleep(20);
			info->fts_wait_for_ready(info);
			fts_fw_init(info);
		}
	}

done:
	if (fw_entry)
		release_firmware(fw_entry);

	return retval;
}
EXPORT_SYMBOL(fts_fw_update_on_probe);

static int fts_load_fw_from_kernel(struct fts_ts_info *info,
				 const char *fw_path)
{
	int retval;
	const struct firmware *fw_entry = NULL;
	unsigned char *fw_data = NULL;

	if (!fw_path) {
		tsp_debug_err(true, info->dev, "%s: Firmware name is not defined\n",
			__func__);
		return -EINVAL;
	}

	tsp_debug_info(true, info->dev, "%s: Load firmware : %s\n", __func__,
		 fw_path);

	retval = request_firmware(&fw_entry, fw_path, info->dev);

	if (retval) {
		tsp_debug_err(true, info->dev,
			"%s: Firmware image %s not available\n", __func__,
			fw_path);
		goto done;
	}

	fw_data = (unsigned char *)fw_entry->data;

	if (fts_skip_firmware_update(info, fw_data))
		goto done;

	if (info->irq)
		disable_irq(info->irq);
	else
		hrtimer_cancel(&info->timer);

	info->fts_systemreset(info);
	info->fts_wait_for_ready(info);

	retval = fts_fw_updater(info, fw_data);
	if (retval)
		tsp_debug_err(true, info->dev, "%s: failed update firmware\n",
			__func__);

	if (info->irq)
		enable_irq(info->irq);
	else
		hrtimer_start(&info->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
 done:
	if (fw_entry)
		release_firmware(fw_entry);

	return retval;
}

static int fts_load_fw_from_ums(struct fts_ts_info *info)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fw_size, nread;
	int error = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(FTS_DEFAULT_UMS_FW, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		tsp_debug_err(true, info->dev, "%s: failed to open %s.\n", __func__,
			FTS_DEFAULT_UMS_FW);
		error = -ENOENT;
		goto open_err;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;

	if (0 < fw_size) {
		unsigned char *fw_data;
		const struct fts64_header *header;
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data,
				 fw_size, &fp->f_pos);

		tsp_debug_info(true, info->dev,
			 "%s: start, file path %s, size %ld Bytes\n",
			 __func__, FTS_DEFAULT_UMS_FW, fw_size);

		if (nread != fw_size) {
			tsp_debug_err(true, info->dev,
				"%s: failed to read firmware file, nread %ld Bytes\n",
				__func__, nread);
			error = -EIO;
		} else {
			if (fts_skip_firmware_update(info, fw_data))
				goto done;

			header = (struct fts64_header *)fw_data;
			if (header->signature == FTS64FILE_SIGNATURE) {
				if (info->irq)
					disable_irq(info->irq);
				else
					hrtimer_cancel(&info->timer);

				info->fts_systemreset(info);
				info->fts_wait_for_ready(info);

				tsp_debug_info(true, info->dev,
					"[UMS] Firmware Version : 0x%04X "
					"[UMS] Main Version : 0x%04X\n",
					(fw_data[5] << 8)+fw_data[4],
					(header->released_ver[0] << 8) +
							(header->released_ver[1]));

				error = fts_fw_updater(info, fw_data);

				if (info->irq)
					enable_irq(info->irq);
				else
					hrtimer_start(&info->timer,
						  ktime_set(1, 0),
						  HRTIMER_MODE_REL);
			} else {
				error = -1;
				tsp_debug_err(true, info->dev,
					 "%s: File type is not match with FTS64 file. [%8x]\n",
					 __func__, header->signature);
			}
		}

		if (error < 0)
			tsp_debug_err(true, info->dev, "%s: failed update firmware\n",
				__func__);

done:
		kfree(fw_data);
	}

	filp_close(fp, NULL);

 open_err:
	set_fs(old_fs);
	return error;
}

static int fts_load_fw_from_ffu(struct fts_ts_info *info)
{
	int retval;
	const struct firmware *fw_entry = NULL;
	unsigned char *fw_data = NULL;
	const char *fw_path = FTS_DEFAULT_FFU_FW;
	const struct fts64_header *header;

	if (!fw_path) {
		tsp_debug_err(true, info->dev, "%s: Firmware name is not defined\n",
			__func__);
		return -EINVAL;
	}

	tsp_debug_info(true, info->dev, "%s: Load firmware : %s\n", __func__,
		 fw_path);

	retval = request_firmware(&fw_entry, fw_path, info->dev);

	if (retval) {
		tsp_debug_err(true, info->dev,
			"%s: Firmware image %s not available\n", __func__,
			fw_path);
		goto done;
	}

	if ((info->digital_rev == FTS_DIGITAL_REV_2) &&
		(fw_entry->size != (FW_IMAGE_SIZE_D2 + sizeof(struct fts64_header) + SIGNEDKEY_SIZE))) {
		tsp_debug_err(true, info->dev,
			"%s: Unsigned firmware %s is not available, %ld\n", __func__,
			fw_path, fw_entry->size);
		retval = -EPERM;
		goto done;
	}

	fw_data = (unsigned char *)fw_entry->data;
	header = (struct fts64_header *)fw_data;

	info->fw_version_of_bin = (fw_data[5] << 8)+fw_data[4];
	info->fw_main_version_of_bin = (header->released_ver[0] << 8) +
								(header->released_ver[1]);
	if (info->digital_rev == FTS_DIGITAL_REV_1)
		info->config_version_of_bin = (fw_data[CONFIG_OFFSET_BIN_D1] << 8) + fw_data[CONFIG_OFFSET_BIN_D1 - 1];
	else
		info->config_version_of_bin = (fw_data[CONFIG_OFFSET_BIN_D2] << 8) + fw_data[CONFIG_OFFSET_BIN_D2 - 1];

	tsp_debug_info(true, info->dev,
					"FFU Firmware Version : 0x%04X "
					"FFU Config Version : 0x%04X "
					"FFU Main Version : 0x%04X\n",
					info->fw_version_of_bin,
					info->config_version_of_bin,
					info->fw_main_version_of_bin);

	/* TODO : if you need to check firmware version between IC and Binary,
	 * add it this position.
	 */

	if (info->irq)
		disable_irq(info->irq);
	else
		hrtimer_cancel(&info->timer);

	info->fts_systemreset(info);
	info->fts_wait_for_ready(info);

	retval = fts_fw_updater(info, fw_data);
	if (retval)
		tsp_debug_err(true, info->dev, "%s: failed update firmware\n",
			__func__);

	if (info->irq)
		enable_irq(info->irq);
	else
		hrtimer_start(&info->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
 done:
	if (fw_entry)
		release_firmware(fw_entry);

	return retval;
}

int fts_fw_update_on_hidden_menu(struct fts_ts_info *info, int update_type)
{
	int retval = 0;

	/* Factory cmd for firmware update
	 * argument represent what is source of firmware like below.
	 *
	 * 0 : [BUILT_IN] Getting firmware which is for user.
	 * 1 : [UMS] Getting firmware from sd card.
	 * 2 : none
	 * 3 : [FFU] Getting firmware from air.
	 */
	switch (update_type) {
	case BUILT_IN:
		retval = fts_load_fw_from_kernel(info, info->firmware_name);
		break;

	case UMS:
		retval = fts_load_fw_from_ums(info);
		break;

	case FFU:
		retval = fts_load_fw_from_ffu(info);
		break;

	default:
		tsp_debug_err(true, info->dev, "%s: Not support command[%d]\n",
			__func__, update_type);
		break;
	}

	return retval;
}
EXPORT_SYMBOL(fts_fw_update_on_hidden_menu);
