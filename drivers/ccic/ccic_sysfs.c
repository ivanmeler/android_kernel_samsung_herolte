/*
 * ccic_sysfs.c
 *
 * Copyright (C) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ccic/ccic_sysfs.h>
#include <linux/ccic/s2mm005.h>
#include <linux/ccic/s2mm005_ext.h>
#include <linux/ccic/s2mm005_fw.h>
#include <linux/regulator/consumer.h>
#if defined(CONFIG_CCIC_ALTERNATE_MODE)
#include <linux/ccic/ccic_alternate.h>
#endif
static ssize_t s2mm005_cur_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mm005_version chip_swver;
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);

	if (!usbpd_data) {
		pr_err("usbpd_data is NULL\n");
		return -ENODEV;
	}

	s2mm005_get_chip_swversion(usbpd_data, &chip_swver);
	pr_err("%s CHIP SWversion %2x %2x %2x %2x\n", __func__,
	       chip_swver.main[2] , chip_swver.main[1], chip_swver.main[0], chip_swver.boot);

	usbpd_data->firm_ver[0] = chip_swver.main[2];
	usbpd_data->firm_ver[1] = chip_swver.main[1];
	usbpd_data->firm_ver[2] = chip_swver.main[0];
	usbpd_data->firm_ver[3] = chip_swver.boot;

	return sprintf(buf, "%02X %02X %02X %02X\n",
		       usbpd_data->firm_ver[0], usbpd_data->firm_ver[1],
		       usbpd_data->firm_ver[2], usbpd_data->firm_ver[3]);

}
static DEVICE_ATTR(cur_version, 0444, s2mm005_cur_ver_show, NULL);

static ssize_t s2mm005_src_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	struct s2mm005_version fw_swver;

	if (!usbpd_data) {
		pr_err("usbpd_data is NULL\n");
		return -ENODEV;
	}

	s2mm005_get_fw_version(usbpd_data->s2mm005_fw_product_id,
		&fw_swver, usbpd_data->firm_ver[3], usbpd_data->hw_rev);
	return sprintf(buf, "%02X %02X %02X %02X\n",
		fw_swver.main[2], fw_swver.main[1], fw_swver.main[0], fw_swver.boot);
}
static DEVICE_ATTR(src_version, 0444, s2mm005_src_ver_show, NULL);

static ssize_t s2mm005_show_manual_lpm_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);


    if (!usbpd_data) {
        pr_err("usbpd_data is NULL\n");
        return -ENODEV;
    }

    return sprintf(buf, "%d\n", usbpd_data->manual_lpm_mode);	
}
static ssize_t s2mm005_store_manual_lpm_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	int mode;

    if (!usbpd_data) {
        pr_err("usbpd_data is NULL\n");
        return -ENODEV;
    }

	sscanf(buf, "%d", &mode);
	pr_info("usb: %s mode=%d\n", __func__, mode);

	switch(mode){
	case 0:
		/* Disable Low Power Mode for App (SW JIGON Disable) */
		s2mm005_manual_JIGON(usbpd_data, 0);
		usbpd_data->manual_lpm_mode = 0;
		break;
	case 1:
		/* Enable Low Power Mode for App (SW JIGON Enable) */
		s2mm005_manual_JIGON(usbpd_data, 1);
		usbpd_data->manual_lpm_mode = 1;
		break;
	case 2:
		/* SW JIGON Enable */
		s2mm005_manual_JIGON(usbpd_data, 1);
//		s2mm005_manual_LPM(usbpd_data, 0x1);
		usbpd_data->manual_lpm_mode = 1;
		break;
	case 4:
		/* water lpm mode */
		s2mm005_manual_LPM(usbpd_data, 0x9);
		break;
	default:
		/* SW JIGON Disable */
		s2mm005_manual_JIGON(usbpd_data, 0);
		usbpd_data->manual_lpm_mode = 0;
		break;
	}

	return size;
}
static DEVICE_ATTR(lpm_mode, 0664,
	s2mm005_show_manual_lpm_mode, s2mm005_store_manual_lpm_mode);

static ssize_t ccic_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);

	if(!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}

	return sprintf(buf, "%d\n", usbpd_data->pd_state);
}
static DEVICE_ATTR(state, 0444, ccic_state_show, NULL);

#if defined(CONFIG_SEC_FACTORY)
static ssize_t ccic_rid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);

	if(!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}

	return sprintf(buf, "%d\n", usbpd_data->cur_rid);
}
static DEVICE_ATTR(rid, 0444, ccic_rid_show, NULL);

static ssize_t s2mm005_store_control_option_command(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	int cmd;

	if(!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}

	sscanf(buf, "%d", &cmd);
	pr_info("usb: %s mode=%d\n", __func__, cmd);

	s2mm005_control_option_command(usbpd_data, cmd);

	return size;
}
static DEVICE_ATTR(ccic_control_option, 0220, NULL, s2mm005_store_control_option_command);

static ssize_t ccic_booting_dry_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);

	if(!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}
	pr_info("%s booting_run_dry=%d\n", __func__,
		usbpd_data->fac_booting_dry_check);

	return sprintf(buf, "%d\n", (usbpd_data->fac_booting_dry_check));
}
static DEVICE_ATTR(booting_dry, 0444, ccic_booting_dry_show, NULL);
#endif
static int ccic_firmware_update_built_in(struct device *dev)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	struct s2mm005_version chip_swver, fw_swver;

	s2mm005_get_chip_swversion(usbpd_data, &chip_swver);
	pr_err("%s CHIP SWversion %2x %2x %2x %2x - before\n", __func__,
	       chip_swver.main[2] , chip_swver.main[1], chip_swver.main[0], chip_swver.boot);
	s2mm005_get_fw_version(usbpd_data->s2mm005_fw_product_id,
		&fw_swver, chip_swver.boot, usbpd_data->hw_rev);
	pr_err("%s SRC SWversion:%2x,%2x,%2x,%2x\n",__func__,
		fw_swver.main[2], fw_swver.main[1], fw_swver.main[0], fw_swver.boot);

	pr_err("%s: FW UPDATE boot:%01d hw_rev:%02d\n", __func__, chip_swver.boot, usbpd_data->hw_rev);

	if(chip_swver.main[0] == fw_swver.main[0]) {
		pr_err("%s: FW version is same. Stop FW update. src:%2x chip:%2x\n", 
			__func__, chip_swver.main[0], fw_swver.main[0]);
		goto done;
	}

	if (chip_swver.boot == 4) {
		if (usbpd_data->hw_rev >= 9)
			s2mm005_flash_fw(usbpd_data, FLASH_WRITE);
		else
			s2mm005_flash_fw(usbpd_data, FLASH_WRITE4);
	} else if (chip_swver.boot == 5) {
			s2mm005_flash_fw(usbpd_data, FLASH_WRITE5);
	} else if (chip_swver.boot == 6) {
			s2mm005_flash_fw(usbpd_data, FLASH_WRITE6);
	} else {
		pr_err("%s: Didn't have same FW boot version. Stop FW update. src_boot:%2x chip_boot:%2x\n", 
			__func__, chip_swver.boot, fw_swver.boot);
		return -1;
	}
done:
	return 0;
}

static int ccic_firmware_update_ums(struct device *dev)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	unsigned char *fw_data;
	struct s2mm005_fw *fw_hd;
	struct file *fp;
	mm_segment_t old_fs;
	long fw_size, nread;
	int error = 0;

	if(!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(CCIC_DEFAULT_UMS_FW, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		pr_err("%s: failed to open %s.\n", __func__,
						CCIC_DEFAULT_UMS_FW);
		error = -ENOENT;
		goto open_err;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;

	if (0 < fw_size) {
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data, fw_size, &fp->f_pos);

		pr_info("%s: start, file path %s, size %ld Bytes\n",
					__func__, CCIC_DEFAULT_UMS_FW, fw_size);
		filp_close(fp, NULL);

		if (nread != fw_size) {
			pr_err("%s: failed to read firmware file, nread %ld Bytes\n",
					__func__, nread);
			error = -EIO;
		} else {
			fw_hd = (struct s2mm005_fw*)fw_data;
			pr_info("CCIC FW ver - cur:%02X %02X %02X %02X / bin:%02X %02X %02X %02X\n",
					usbpd_data->firm_ver[0], usbpd_data->firm_ver[1], usbpd_data->firm_ver[2], usbpd_data->firm_ver[3],
					fw_hd->boot, fw_hd->main[0], fw_hd->main[1], fw_hd->main[2]);

			if(fw_hd->boot == usbpd_data->firm_ver[3]) {
				if (s2mm005_flash_fw(usbpd_data, FLASH_WRITE_UMS) >= 0)
					goto done;
			} else {
				pr_err("error : Didn't match to CCIC FW firmware version\n");
				error = -EINVAL;
			}
		}
		if (error < 0)
			pr_err("%s: failed update firmware\n", __func__);
done:
		kfree(fw_data);
	}

open_err:
	set_fs(old_fs);
	return error;
}

static ssize_t ccic_store_firmware_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	u8 val = 0;

	if(!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}
	s2mm005_read_byte_flash(usbpd_data->i2c, FLASH_STATUS_0x24, &val, 1);
	pr_err("%s flash mode: %s\n", __func__, flashmode_to_string(val));

	return sprintf(buf, "%s\n", flashmode_to_string(val));
}
static DEVICE_ATTR(fw_update_status, 0444, ccic_store_firmware_status_show, NULL);

static ssize_t ccic_store_firmware_update(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	struct s2mm005_version version;
	int mode = 0,  ret = 1;

    if (!usbpd_data) {
        pr_err("usbpd_data is NULL\n");
        return -ENODEV;
    }

	sscanf(buf, "%d", &mode);
	pr_info("%s mode=%d\n", __func__, mode);

	s2mm005_get_chip_swversion(usbpd_data, &version);
	pr_err("%s CHIP SWversion %2x %2x %2x %2x - before\n", __func__,
	       version.main[2] , version.main[1], version.main[0], version.boot);

	/* Factory cmd for firmware update
 	* argument represent what is source of firmware like below.
 	*
 	* 0 : [BUILT_IN] Getting firmware from source.
 	* 1 : [UMS] Getting firmware from sd card.
 	*/

	switch (mode) {
	case BUILT_IN:
		ret = ccic_firmware_update_built_in(dev);
		break;
	case UMS:
		ret = ccic_firmware_update_ums(dev);
		break;
	default:
		pr_err("%s: Not support command[%d]\n",
			__func__, mode);
		break;
	}

	s2mm005_get_chip_swversion(usbpd_data, &version);
	pr_err("%s CHIP SWversion %2x %2x %2x %2x - after\n", __func__,
	       version.main[2] , version.main[1], version.main[0], version.boot);

	return size;
}
static DEVICE_ATTR(fw_update, 0220, NULL, ccic_store_firmware_update);

static ssize_t ccic_store_sink_pdo_update(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	uint32_t data = 0;
	uint16_t REG_ADD;
	uint8_t MSG_BUF[32] = {0,};
	SINK_VAR_SUPPLY_Typedef *pSINK_MSG;
	MSG_HEADER_Typedef *pMSG_HEADER;
	uint32_t * MSG_DATA;
	uint8_t cnt;

	if (!usbpd_data) {
		pr_err("usbpd_data is NULL\n");
		return -ENODEV;
	}

	sscanf(buf, "%x\n", &data);
	if (data == 0)
		data = 0x8F019032; // 5V~12V, 500mA
	pr_info("%s data=0x%x\n", __func__, data);

	/* update Sink PDO */
	REG_ADD = REG_TX_SINK_CAPA_MSG;
	s2mm005_read_byte(usbpd_data->i2c, REG_ADD, MSG_BUF, 32);

	MSG_DATA = (uint32_t *)&MSG_BUF[0];
	pr_err("--- Read Data on TX_SNK_CAPA_MSG(0x220)\n");
	for(cnt = 0; cnt < 8; cnt++) {
		pr_err("   0x%08X\n", MSG_DATA[cnt]);
	}

	pMSG_HEADER = (MSG_HEADER_Typedef *)&MSG_BUF[0];
	pMSG_HEADER->BITS.Number_of_obj += 1;
	pSINK_MSG = (SINK_VAR_SUPPLY_Typedef *)&MSG_BUF[8];
	pSINK_MSG->DATA = data;
	pr_err("--- Write DATA\n");
	for (cnt = 0; cnt < 8; cnt++) {
		pr_err("   0x%08X\n", MSG_DATA[cnt]);
	}

	s2mm005_write_byte(usbpd_data->i2c, REG_ADD, &MSG_BUF[0], 32);

	for (cnt = 0; cnt < 32; cnt++) {
		MSG_BUF[cnt] = 0;
	}

	for (cnt = 0; cnt < 8; cnt++) {
		pr_err("   0x%08X\n", MSG_DATA[cnt]);
	}
	s2mm005_read_byte(usbpd_data->i2c, REG_ADD, MSG_BUF, 32);

	pr_err("--- Read 2 new Data on TX_SNK_CAPA_MSG(0x220)\n");
	for(cnt = 0; cnt < 8; cnt++) {
		pr_err("   0x%08X\n", MSG_DATA[cnt]);
	}
	
	return size;
}
static DEVICE_ATTR(sink_pdo_update, 0220, NULL, ccic_store_sink_pdo_update);

#if defined(CONFIG_CCIC_ALTERNATE_MODE)
static ssize_t ccic_send_samsung_uVDM_message(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	int ret = 0;

	if (!usbpd_data) {
	    pr_err("usbpd_data is NULL\n");
	    return -ENODEV;
	}
	ret = send_samsung_unstructured_vdm_message(usbpd_data, buf, size);
	if( ret < 0 )
		return ret;
	else
		return size;
}
static DEVICE_ATTR(samsung_uvdm, 0220, NULL, ccic_send_samsung_uVDM_message);

static ssize_t ccic_send_uVDM_message(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	int cmd = 0;

    if (!usbpd_data) {
        pr_err("usbpd_data is NULL\n");
        return -ENODEV;
    }

	sscanf(buf, "%d", &cmd);
	pr_info("%s cmd=%d\n", __func__, cmd);

	send_unstructured_vdm_message(usbpd_data, cmd);

	return size;
}
static DEVICE_ATTR(uvdm, 0220, NULL, ccic_send_uVDM_message);

static ssize_t ccic_send_dna_audio_uVDM_message(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	int cmd = 0;

    if (!usbpd_data) {
        pr_err("usbpd_data is NULL\n");
        return -ENODEV;
    }

	sscanf(buf, "%d", &cmd);
	pr_info("%s cmd=%d\n", __func__, cmd);

	send_dna_audio_unstructured_vdm_message(usbpd_data, cmd);

	return size;
}
static DEVICE_ATTR(dna_audio_uvdm, 0220, NULL, ccic_send_dna_audio_uVDM_message);

static ssize_t ccic_send_dex_fan_uVDM_message(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	int cmd = 0;

    if (!usbpd_data) {
        pr_err("usbpd_data is NULL\n");
        return -ENODEV;
    }

	sscanf(buf, "%d", &cmd);
	pr_info("%s cmd=%d\n", __func__, cmd);

	send_dex_fan_unstructured_vdm_message(usbpd_data, cmd);

	return size;
}
static DEVICE_ATTR(dex_fan_uvdm, 0220, NULL, ccic_send_dex_fan_uVDM_message);

static ssize_t ccic_send_attention_message(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	int cmd = 0;

	if (!usbpd_data) {
		pr_err("usbpd_data is NULL\n");
		return -ENODEV;
	}

	sscanf(buf, "%d", &cmd);
	pr_info("%s cmd=%d\n", __func__, cmd);

	send_attention_message(usbpd_data, cmd);

	return size;
}
static DEVICE_ATTR(attention, 0220, NULL, ccic_send_attention_message);

static ssize_t ccic_send_role_swap_message(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	int cmd = 0;

    if (!usbpd_data) {
        pr_err("usbpd_data is NULL\n");
        return -ENODEV;
    }

	sscanf(buf, "%d", &cmd);
	pr_info("%s cmd=%d\n", __func__, cmd);

	send_role_swap_message(usbpd_data, cmd);

	return size;
}
static DEVICE_ATTR(role_swap, 0220, NULL, ccic_send_role_swap_message);

static ssize_t ccic_acc_device_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);

	if (!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}
	pr_info("%s 0x%04x\n", __func__, usbpd_data->Device_Version);

	return sprintf(buf, "%04x\n", usbpd_data->Device_Version);
}
static DEVICE_ATTR(acc_device_version, 0444, ccic_acc_device_version_show,NULL);
#endif

static ssize_t ccic_set_gpio(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	int mode;
	u8 W_DATA[2];
	u8 REG_ADD;
	u8 R_DATA;
	int i;
	struct device_node *np = NULL;
	const char *ss_vdd;
	int ret = 0;
	struct regulator *vdd085_usb;

	if (!usbpd_data) {
		pr_err("usbpd_data is NULL\n");
		return -ENODEV;
	}

	sscanf(buf, "%d", &mode);
	pr_info("usb: %s mode=%d\n", __func__, mode);

	/* VDD_USB_3P0_AP is on for DP SWITCH */
	np = of_find_compatible_node(NULL, NULL, "samsung,usb-notifier");
	if (!np) {
		pr_err("%s: failed to get the battery device node\n", __func__);
		return 0;
	} else {
		if(of_property_read_string(np, "hs-regulator", (char const **)&ss_vdd) < 0) {
			pr_err("%s - get ss_vdd error\n", __func__);
		}

		vdd085_usb = regulator_get(NULL, ss_vdd);
		if (IS_ERR(vdd085_usb) || vdd085_usb == NULL) {
			pr_err("%s - vdd085_usb regulator_get fail\n", __func__);
			return 0;
		}

	}
	/* for Wake up*/
	for(i=0; i<5; i++){
		R_DATA = 0x00;
		REG_ADD = 0x8;
		s2mm005_read_byte(usbpd_data->i2c, REG_ADD, &R_DATA, 1);   //dummy read
	}
	udelay(10);

	switch(mode){
	case 0:
		if (!regulator_is_enabled(vdd085_usb)) {
			ret = regulator_enable(vdd085_usb);
			if (ret) {
				pr_err("%s - enable vdd085_usb ldo enable failed, ret=%d\n",
				       __func__, ret);
				regulator_put(vdd085_usb);
				return 0;
			}
		}
		regulator_put(vdd085_usb);

		/* SBU1/SBU2 set as open-drain status*/
		// SBU1/2 Open command ON
		REG_ADD = 0x10;
		W_DATA[0] = 0x03;
		W_DATA[1] = 0x85;
		s2mm005_write_byte(usbpd_data->i2c, REG_ADD, &W_DATA[0], 2);

		break;
	case 1:
		/* SBU1/SBU2 set as default status */
		// SBU1/2 Open command OFF
		REG_ADD = 0x10;
		W_DATA[0] = 0x03;
		W_DATA[1] = 0x86;
		s2mm005_write_byte(usbpd_data->i2c, REG_ADD, &W_DATA[0], 2);

		if (regulator_is_enabled(vdd085_usb)) {
			ret = regulator_disable(vdd085_usb);
			if (ret) {
				pr_err("%s - enable vdd085_usb ldo enable failed, ret=%d\n",
				__func__, ret);
				regulator_put(vdd085_usb);
				return 0;
			}
		}
		regulator_put(vdd085_usb);
		break;
	default:
		break;
	}

	return size;
}

static ssize_t ccic_get_gpio(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	u8 W_DATA[4];
	u8 REG_ADD;
	u8 R_DATA;
	int i;

	if (!usbpd_data) {
		pr_err("usbpd_data is NULL\n");
		return -ENODEV;
	}

	/* for Wake up*/
	for(i=0; i<5; i++){
		R_DATA = 0x00;
		REG_ADD = 0x8;
		s2mm005_read_byte(usbpd_data->i2c, REG_ADD, &R_DATA, 1);   //dummy read
	}
	udelay(10);

	W_DATA[0] =0x2;
	W_DATA[1] =0x10;

	W_DATA[2] =0x84;
	W_DATA[3] =0x10;

	s2mm005_write_byte(usbpd_data->i2c, 0x10, &W_DATA[0], 4);
	s2mm005_read_byte(usbpd_data->i2c, 0x14, &R_DATA, 1);

	pr_err("%s SBU1 status = %2x , SBU2 status = %2x \n", __func__,
		(R_DATA & 0x10) >> 4,(R_DATA & 0x20) >> 5);

	return sprintf(buf, "%d %d\n", (R_DATA & 0x10) >> 4,(R_DATA & 0x20) >> 5);

}
static DEVICE_ATTR(control_gpio, 0664, ccic_get_gpio, ccic_set_gpio);

static ssize_t ccic_water_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);

	if(!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}
	pr_info("%s water=%d, run_dry=%d\n", __func__,
		usbpd_data->water_det, usbpd_data->run_dry);

	return sprintf(buf, "%d\n", (usbpd_data->water_det | !usbpd_data->run_dry));
}
static DEVICE_ATTR(water, 0444, ccic_water_show, NULL);

static ssize_t ccic_usbpd_ids_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	int retval = 0;

	if (!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}
	retval = sprintf(buf, "%04x:%04x\n",
		le16_to_cpu(usbpd_data->Vendor_ID),
		le16_to_cpu(usbpd_data->Product_ID));
	pr_info("usb: %s : %s",
		__func__, buf);

	return retval;
}
static DEVICE_ATTR(usbpd_ids, 0444, ccic_usbpd_ids_show, NULL);

static ssize_t ccic_usbpd_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mm005_data *usbpd_data = dev_get_drvdata(dev);
	int retval = 0;

	if (!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}
	retval = sprintf(buf, "%d\n", usbpd_data->acc_type);
	pr_info("usb: %s : %d",
		__func__, usbpd_data->acc_type);

	return retval;
}

static DEVICE_ATTR(usbpd_type, 0444, ccic_usbpd_type_show, NULL);

static struct attribute *ccic_attributes[] = {
	&dev_attr_cur_version.attr,
	&dev_attr_src_version.attr,
	&dev_attr_lpm_mode.attr,
	&dev_attr_state.attr,
#if defined(CONFIG_SEC_FACTORY)
	&dev_attr_rid.attr,
	&dev_attr_ccic_control_option.attr,
	&dev_attr_booting_dry.attr,
#endif
	&dev_attr_fw_update.attr,
	&dev_attr_fw_update_status.attr,
	&dev_attr_sink_pdo_update.attr,
#if defined(CONFIG_CCIC_ALTERNATE_MODE)
	&dev_attr_uvdm.attr,
	&dev_attr_attention.attr,
	&dev_attr_role_swap.attr,
	&dev_attr_samsung_uvdm.attr,
	&dev_attr_dna_audio_uvdm.attr,
	&dev_attr_dex_fan_uvdm.attr,
	&dev_attr_acc_device_version.attr,
	&dev_attr_usbpd_ids.attr,
	&dev_attr_usbpd_type.attr,
#endif
	&dev_attr_control_gpio.attr,
	&dev_attr_water.attr,
	NULL
};

const struct attribute_group ccic_sysfs_group = {
	.attrs = ccic_attributes,
};
