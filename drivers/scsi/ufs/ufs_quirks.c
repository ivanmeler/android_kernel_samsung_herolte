/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ufshcd.h"
#include "ufs_quirks.h"

#define SERIAL_NUM_SIZE 6
#define TOSHIBA_SERIAL_NUM_SIZE 10
static struct ufs_card_fix ufs_fixups[] = {
	/* UFS cards deviations table */
	UFS_FIX(UFS_VENDOR_ID_SAMSUNG, UFS_ANY_MODEL, UFS_DEVICE_QUIRK_BROKEN_LINEREST),
	END_FIX
};

/*UN policy
*  16 digits : mandate + serial number(6byte, hex raw data)
*  18 digits : manid + mandate + serial number(sec, hynix : 6byte hex,
*                                                                toshiba : 10byte + 00, ascii)
*/
void ufs_set_sec_unique_number(struct ufs_hba *hba, u8 *str_desc_buf, u8 *desc_buf)
{
	u8 manid;
	u8 snum_buf[UFS_UN_MAX_DIGITS];

	manid = hba->manufacturer_id & 0xFF;
	memset(hba->unique_number, 0, sizeof(hba->unique_number));
	memset(snum_buf, 0, sizeof(snum_buf));

#if defined(CONFIG_UFS_UN_18DIGITS)

	memcpy(snum_buf, str_desc_buf + QUERY_DESC_HDR_SIZE, SERIAL_NUM_SIZE);

	sprintf(hba->unique_number, "%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		manid,
		desc_buf[DEVICE_DESC_PARAM_MANF_DATE], desc_buf[DEVICE_DESC_PARAM_MANF_DATE+1],
		snum_buf[0], snum_buf[1], snum_buf[2], snum_buf[3], snum_buf[4], snum_buf[5]);

	/* Null terminate the unique number string */
	hba->unique_number[UFS_UN_18_DIGITS] = '\0';

#else
	/*default is 16 DIGITS UN*/
	memcpy(snum_buf, str_desc_buf + QUERY_DESC_HDR_SIZE, SERIAL_NUM_SIZE);

	sprintf(hba->unique_number, "%02x%02x%02x%02x%02x%02x%02x%02x",
		desc_buf[DEVICE_DESC_PARAM_MANF_DATE], desc_buf[DEVICE_DESC_PARAM_MANF_DATE+1],
		snum_buf[0], snum_buf[1], snum_buf[2], snum_buf[3], snum_buf[4], snum_buf[5]);
	
	/* Null terminate the unique number string */
	hba->unique_number[UFS_UN_16_DIGITS] = '\0';
#endif
}


int ufs_get_device_info(struct ufs_hba *hba, struct ufs_card_info *card_data)
{
	int err;
	u8 model_index;
	u8 serial_num_index;
	u8 str_desc_buf[QUERY_DESC_STRING_MAX_SIZE + 1];
	u8 desc_buf[QUERY_DESC_DEVICE_MAX_SIZE];
	u8 health_buf[QUERY_DESC_HEALTH_MAX_SIZE];
#ifdef CONFIG_JOURNAL_DATA_TAG
	u8 vendor_specific_buf[QUERY_DESC_VENDOR_SPECIFIC_SIZE];
#endif
	bool ascii_type;

	err = ufshcd_read_device_desc(hba, desc_buf,
					QUERY_DESC_DEVICE_MAX_SIZE);
	if (err)
		goto out;

	err = ufshcd_read_health_desc(hba, health_buf,
					QUERY_DESC_HEALTH_MAX_SIZE);
	if (err)
		printk("%s: DEVICE_HEALTH desc read fail, err  = %d\n", __FUNCTION__, err);

	/* getting Life Time at Device Health DESC*/
	card_data->lifetime = health_buf[HEALTH_DEVICE_DESC_PARAM_LIFETIMEA];

	/*
	 * getting vendor (manufacturerID) and Bank Index in big endian
	 * format
	 */
	card_data->wmanufacturerid = desc_buf[DEVICE_DESC_PARAM_MANF_ID] << 8 |
				     desc_buf[DEVICE_DESC_PARAM_MANF_ID + 1];

	hba->manufacturer_id = card_data->wmanufacturerid;
	hba->lifetime = card_data->lifetime;

#ifdef CONFIG_JOURNAL_DATA_TAG
	if (hba->manufacturer_id == UFS_VENDOR_ID_SAMSUNG &&
			hba->host->journal_tag == JOURNAL_TAG_UNKNOWN) {
		vendor_specific_buf[4] = 0;
		err = ufshcd_read_vendor_specific_desc(hba,
				QUERY_DESC_IDN_VENDOR, 0, vendor_specific_buf,
				QUERY_DESC_VENDOR_SPECIFIC_SIZE);
		if (!err && hba->host && (vendor_specific_buf[4] & 0x1)) {
			printk("%s: vendor_desc[4]=0x%x. setting UFS journal "
					"tag support to 1.\n",
					__func__, vendor_specific_buf[4]);
			hba->host->journal_tag = JOURNAL_TAG_ON;
		} else {
			printk("%s: UFS does not support journal tag "
					"- vendor_desc[4]=0x%x.(err %d).\n",
					__func__, vendor_specific_buf[4], err);
			hba->host->journal_tag = JOURNAL_TAG_OFF;
		}
	}
#endif

	/*product name*/
	model_index = desc_buf[DEVICE_DESC_PARAM_PRDCT_NAME];

	memset(str_desc_buf, 0, QUERY_DESC_STRING_MAX_SIZE);
	err = ufshcd_read_string_desc(hba, model_index, str_desc_buf,
					QUERY_DESC_STRING_MAX_SIZE, ASCII_STD);
	if (err)
		goto out;

	str_desc_buf[QUERY_DESC_STRING_MAX_SIZE] = '\0';
	strlcpy(card_data->model, (str_desc_buf + QUERY_DESC_HDR_SIZE),
		min_t(u8, str_desc_buf[QUERY_DESC_LENGTH_OFFSET],
		      MAX_MODEL_LEN));

	card_data->model[MAX_MODEL_LEN] = '\0';

	/*serial number*/
	serial_num_index = desc_buf[DEVICE_DESC_PARAM_SN];
	memset(str_desc_buf, 0, QUERY_DESC_STRING_MAX_SIZE);

	/*spec is unicode but sec use hex data*/
	ascii_type = UTF16_STD;

	err = ufshcd_read_string_desc(hba, serial_num_index, str_desc_buf,
		 QUERY_DESC_STRING_MAX_SIZE, ascii_type);

	if (err)
		goto out;
	str_desc_buf[QUERY_DESC_STRING_MAX_SIZE] = '\0';

	ufs_set_sec_unique_number(hba, str_desc_buf, desc_buf);

	printk("%s: UFS = %s , LT: 0x%02x \n",
		__FUNCTION__, hba->unique_number, health_buf[3]<<4|health_buf[4]);

out:
	return err;
}

void ufs_advertise_fixup_device(struct ufs_hba *hba)
{
	int err;
	struct ufs_card_fix *f;
	struct ufs_card_info card_data;

	card_data.wmanufacturerid = 0;
	card_data.model = kmalloc(MAX_MODEL_LEN + 1, GFP_KERNEL);
	if (!card_data.model)
		goto out;

	/* get device data*/
	err = ufs_get_device_info(hba, &card_data);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting device info\n", __func__);
		goto out;
	}

	for (f = ufs_fixups; f->quirk; f++) {
		/* if same wmanufacturerid */
		if (((f->card.wmanufacturerid == card_data.wmanufacturerid) ||
		     (f->card.wmanufacturerid == UFS_ANY_VENDOR)) &&
		    /* and same model */
		    (STR_PRFX_EQUAL(f->card.model, card_data.model) ||
		     !strcmp(f->card.model, UFS_ANY_MODEL)))
			/* update quirks */
			hba->dev_quirks |= f->quirk;
	}
out:
	kfree(card_data.model);
}
