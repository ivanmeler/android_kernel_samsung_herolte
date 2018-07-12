/*
 *  Copyright (C) 2016, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "light_colorid.h"

enum 
{
	COLOR_ID_UTYPE = 0,
	COLOR_ID_BLACK = 1,
	COLOR_ID_WHITE = 2,
	COLOR_ID_GOLD = 3,
	COLOR_ID_SILVER = 4,
	COLOR_ID_GREEN = 5,
	COLOR_ID_BLUE = 6,
	COLOR_ID_PINKGOLD = 7,
	COLOR_ID_DEFUALT,
} COLOR_ID_INDEX;

struct light_coef_predefine_item {
	int version;
	int color_id;
	int coef[7];	/* r_coef,g_coef,b_coef,c_coef,dgf,cct_coef,cct_offset */
	unsigned int thresh[2];	/* high,low*/
}; 

#if defined(CONFIG_SENSORS_SSP_HAECHI_880)
struct light_coef_predefine_item light_coef_predefine_table[COLOR_NUM+1] =
{
    {170920,     COLOR_ID_DEFUALT,   {-830, 1100, -1180, 1000, 814, 3521, 2095},      {55,40}},
    {170920,     COLOR_ID_UTYPE,      {-830, 1100, -1180, 1000, 814, 3521, 2095},     {55,40}},
    {170920,     COLOR_ID_BLACK,     {-830, 1100, -1180, 1000, 814, 3521, 2095},      {55,40}},
};
#else
struct light_coef_predefine_item light_coef_predefine_table[COLOR_NUM+1] =
{
	{160616,     COLOR_ID_DEFUALT,   {-138,-899,-128,1000,3336,1861,2161},      {470,310}},
    {160616,     COLOR_ID_UTYPE,     {-138,-899,-128,1000,3336,1861,2161},      {470,310}},
    {160616,     COLOR_ID_BLACK,     {-138,-899,-128,1000,3336,1861,2161},      {470,310}},
    {160616,     COLOR_ID_GOLD,      {-899,-19,427,1000,3314,14639,1689},       {360,240}},
    {160616,     COLOR_ID_SILVER,    {-831,544,-972,1000,2172,9329,1842},       {440,330}},
    {160616,     COLOR_ID_BLUE,      {-783,-484,-122,1000,3535,11334,1833},     {450,300}},
    {160809,     COLOR_ID_PINKGOLD,  {-872,112,-51,1000,2016,13838,1841},		{410,280}},
};
#endif
/* Color ID functions */

int light_corloid_read_colorid(void)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *colorid_filp = NULL;
	int color_id;
	char buf[8] = {0, };
	char temp;
	char *token, *str;

	//read current colorid
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	colorid_filp = filp_open(COLOR_ID_PATH, O_RDONLY, 0660);
	if (IS_ERR(colorid_filp)) {
		iRet = PTR_ERR(colorid_filp);
		if (iRet != -ENOENT)
			pr_err("[SSP] %s - Can't open colorid file\n",
				__func__);
		set_fs(old_fs);
		return iRet;
	}

	iRet = vfs_read(colorid_filp,
		(char *)buf, sizeof(char)*8, &colorid_filp->f_pos);
	if (iRet < 0) {
		pr_err("[SSP] %s - Can't read the colorid data\n", __func__);
		filp_close(colorid_filp, current->files);
		set_fs(old_fs);
		return iRet;
	}

	filp_close(colorid_filp, current->files);
	set_fs(old_fs);

	str = buf;
	token = strsep(&str, " ");
	iRet = kstrtou8(token, 16, &temp);
	if (iRet<0) {
		pr_err("[SSP] %s - colorid kstrtou8 error %d",__func__,iRet);
	}
	
	color_id = (int)(temp&0x0F);

	pr_info("[SSP] %s - colorid = %d\n", __func__, color_id);

	return color_id;
}

int light_colorid_read_efs_version(char buf[], int buf_length)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *version_filp = NULL;

	memset(buf,0,sizeof(char)*buf_length);
	
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	version_filp = filp_open(VERSION_FILE_NAME, O_RDONLY, 0660);
	
	if (IS_ERR(version_filp)) {
		iRet = PTR_ERR(version_filp);
		pr_err("[SSP] %s - Can't open version file %d\n", __func__, iRet);
		set_fs(old_fs);
		return iRet;
	}

	iRet = vfs_read(version_filp,
		(char *)buf, buf_length * sizeof(char), &version_filp->f_pos);
	
	
	if (iRet < 0) {
		pr_err("[SSP] %s - Can't read the version data %d\n", __func__,iRet);
	}

	filp_close(version_filp, current->files);
	set_fs(old_fs);

	pr_info("[SSP] %s - %s\n",__func__, buf);

	return iRet;
}

int light_colorid_write_efs_version(int version, char* color_ids)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *version_filp = NULL;
	char file_contents[COLOR_ID_VERSION_FILE_LENGTH] = {0, };
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	snprintf(file_contents, sizeof(file_contents), "%d.%s",version,color_ids);

	version_filp = filp_open(VERSION_FILE_NAME,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0660);

	if (IS_ERR(version_filp)) {
		pr_err("[SSP] %s - Can't open version file \n", __func__);
		set_fs(old_fs);
		iRet = PTR_ERR(version_filp);
		return iRet;
	}

	iRet = vfs_write(version_filp, (char *)&file_contents,
		sizeof(char)*COLOR_ID_VERSION_FILE_LENGTH, &version_filp->f_pos);
	if (iRet < 0) {
		pr_err("[SSP] %s - Can't write the version data to file\n", __func__);
		iRet = -EIO;
	}

	filp_close(version_filp, current->files);
	set_fs(old_fs);

	return iRet;	
}

bool light_colorid_check_efs_version(void)
{
	int iRet = 0, i;
	int efs_version;
	char str_efs_version[COLOR_ID_VERSION_FILE_LENGTH];

	pr_info("[SSP] %s ",__func__);

	iRet = light_colorid_read_efs_version(str_efs_version, COLOR_ID_VERSION_FILE_LENGTH);
	if(iRet < 0)
	{
		pr_err("[SSP] %s - version read failed %d \n", __func__, iRet);
		return false;
	}

	/* version parsing */
	{
		char* token;
		char* str = str_efs_version;
		token = strsep(&str, ".");
		iRet = kstrtos32(token, 10, &efs_version);
		if (iRet<0) {
			pr_err("[SSP] %s - version kstros32 error %d\n",__func__,iRet);
		}

		pr_info("[SSP] %s - version %d\n",__func__,efs_version);
	}

	for( i = 1 ; i <= COLOR_NUM ; i++ )
	{
		if(efs_version < light_coef_predefine_table[i].version)
		{
			pr_err("[SSP] %s - %d false\n",__func__,i);	
			return false;
		}
	}

	pr_info("[SSP] %s - success \n", __func__);

	return true;
}

int light_colorid_write_predefined_file(int color_id, int coef[], int thresh[])
{
	int iRet = 0, crc;
	mm_segment_t old_fs;
	struct file *coef_filp = NULL;
	char file_name[COLOR_ID_PREDEFINED_FILENAME_LENTH] = {0, };
	char file_contents[COLOR_ID_PREDEFINED_FILE_LENGTH] = {0, };

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	snprintf(file_name, sizeof(file_name), "%s%d",PREDEFINED_FILE_NAME,color_id);

	crc = coef[0] + coef[1] + coef[2] + coef[3] + coef[4] + coef[5] + coef[6] + thresh[0] + thresh[1];
	sprintf(file_contents,"%d,%d,%d,%d,%d,%d,%d,%u,%u,%d", 
		coef[0], coef[1], coef[2], coef[3], coef[4], coef[5], coef[6], thresh[0], thresh[1], crc);

	coef_filp = filp_open(file_name,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0660);

	if (IS_ERR(coef_filp)) {
		pr_err("[SSP] %s - Can't open coef file %d \n", __func__, color_id);
		set_fs(old_fs);
		iRet = PTR_ERR(coef_filp);
		return iRet;
	}

	iRet = vfs_write(coef_filp, (char *)file_contents, sizeof(char)*COLOR_ID_PREDEFINED_FILE_LENGTH, &coef_filp->f_pos);
	if (iRet < 0) {
		pr_err("[SSP] %s - Can't write the coef data to file\n", __func__);
		iRet = -EIO;
	}

	filp_close(coef_filp, current->files);
	set_fs(old_fs);

	return iRet;
}

int light_colorid_read_predefined_file(int color_id, int coef[], unsigned int thresh[])
{
	int iRet = 0, crc, i;
	mm_segment_t old_fs;
	struct file *coef_filp = NULL;
	char file_name[COLOR_ID_PREDEFINED_FILENAME_LENTH] = {0, };
	char file_contents[COLOR_ID_PREDEFINED_FILE_LENGTH] = {0, };
	char *token, *str;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	
	snprintf(file_name, sizeof(file_name), "%s%d",PREDEFINED_FILE_NAME,color_id);

	coef_filp = filp_open(file_name, O_RDONLY, 0660);

	if (IS_ERR(coef_filp)) {
		pr_err("[SSP] %s - Can't open coef file %d \n", __func__, color_id);
		set_fs(old_fs);
		iRet = PTR_ERR(coef_filp);
		memcpy(coef,light_coef_predefine_table[0].coef,sizeof(int)*7);
		memcpy(thresh,light_coef_predefine_table[0].thresh,sizeof(unsigned int)*2);
		return iRet;
	}

	iRet = vfs_read(coef_filp,
		(char *)file_contents, sizeof(char)*COLOR_ID_PREDEFINED_FILE_LENGTH, &coef_filp->f_pos);
	
	if (iRet != sizeof(char)*COLOR_ID_PREDEFINED_FILE_LENGTH) {
		pr_err("[SSP] %s - Can't read the coef data to file\n", __func__);
		memcpy(coef,light_coef_predefine_table[0].coef,sizeof(int)*7);
		memcpy(thresh,light_coef_predefine_table[0].thresh,sizeof(unsigned int)*2);
		filp_close(coef_filp, current->files);
		set_fs(old_fs);
		return -EIO;
	}

	filp_close(coef_filp, current->files);
	set_fs(old_fs);

	//parsing
	str = file_contents;

	for(i=0 ; i<7; i++)
	{
		token = strsep(&str, ",\n");
		if(token == NULL)
		{
			pr_err("[SSP] %s - too few arguments %d(8 needed)\n",__func__,i);
			memcpy(coef,light_coef_predefine_table[0].coef,sizeof(int)*7);
			memcpy(thresh,light_coef_predefine_table[0].thresh,sizeof(unsigned int)*2);
			return -EINVAL;
		}

		iRet = kstrtos32(token, 10, &coef[i]);

		if (iRet<0) {
			pr_err("[SSP] %s - kstrtos32 error %d\n",__func__,iRet);
			memcpy(coef,light_coef_predefine_table[0].coef,sizeof(int)*7);
			memcpy(thresh,light_coef_predefine_table[0].thresh,sizeof(unsigned int)*2);
			return iRet;
		}
	}

	for(i=0 ; i<2; i++)
	{
		token = strsep(&str, ",\n");
		if(token == NULL)
		{
			pr_err("[SSP] %s - too few arguments %d(2 needed)\n",__func__,i);
			memcpy(coef,light_coef_predefine_table[0].coef,sizeof(int)*7);
			memcpy(thresh,light_coef_predefine_table[0].thresh,sizeof(unsigned int)*2);
			return -EINVAL;
		}

		iRet = kstrtou32(token, 10, &thresh[i]);
		
		if (iRet<0) {
			pr_err("[SSP] %s - kstrtou32 error %d\n",__func__,iRet);
			memcpy(coef,light_coef_predefine_table[0].coef,sizeof(int)*7);
			memcpy(thresh,light_coef_predefine_table[0].thresh,sizeof(unsigned int)*2);
			return iRet;
		}
	}

	token = strsep(&str, ",\n");
	if(token == NULL)
	{
		pr_err("[SSP] %s - too few arguments (1 needed)\n",__func__);
		memcpy(coef,light_coef_predefine_table[0].coef,sizeof(int)*7);
		memcpy(thresh,light_coef_predefine_table[0].thresh,sizeof(unsigned int)*2);
		return -EINVAL;
	}
	iRet = kstrtos32(token, 10, &crc);
		
	if(crc != (coef[0] + coef[1] + coef[2] + coef[3] + coef[4] + coef[5] + coef[6] + thresh[0] + thresh[1]))
	{
		pr_err("[SSP] %s - crc error %d(%d %d %d %d %d %d %d %u %u)\n", __func__,
			crc,coef[0],coef[1],coef[2],coef[3],coef[4],coef[5], coef[6], thresh[0], thresh[1]);
		//use default coef
		memcpy(coef,light_coef_predefine_table[0].coef,sizeof(int)*7);
		memcpy(thresh,light_coef_predefine_table[0].thresh,sizeof(unsigned int)*2);
		return -EINVAL;
	}
	
	return iRet;
}



int light_colorid_write_all_predefined_data(void)
{
	int i, version = -1, iRet=0;
	char str_ids[COLOR_ID_IDS_LENGTH] = "";

	pr_info("[SSP] %s \n", __func__);
	
	for( i = 1 ; i <= COLOR_NUM ; i++)
	{
		char buf[5];

		iRet = light_colorid_write_predefined_file(
			light_coef_predefine_table[i].color_id,
			light_coef_predefine_table[i].coef,
			light_coef_predefine_table[i].thresh);
		if(iRet<0)
		{
			pr_err("[SSP] %s - %d write file err %d \n",__func__,i, iRet);
			break;
		}
		if(version < light_coef_predefine_table[i].version)
			version = light_coef_predefine_table[i].version;

		snprintf(buf, sizeof(buf), "%x",light_coef_predefine_table[i].color_id);
		strcat(str_ids, buf);
	}

	if(iRet >= 0)
	{
		iRet = light_colorid_write_efs_version(version, str_ids);
		if(iRet < 0)
			pr_err("[SSP] %s - write version file err %d \n",__func__,iRet);
	}

	return iRet;
}

int initialize_light_colorid(struct ssp_data *data)
{
	int color_id, ret;
	unsigned int thresh[2];

	if(!light_colorid_check_efs_version())
	{
		light_colorid_write_all_predefined_data();
	}

	color_id = light_corloid_read_colorid();

	ret = light_colorid_read_predefined_file(color_id, data->light_coef, thresh);
	if(ret < 0)
		pr_err("[SSP] %s - read predefined file failed : ret %d \n", __func__, ret);
		
	data->uProxHiThresh = thresh[0];
	data->uProxLoThresh = thresh[1];
	data->uProxHiThresh_detect = DEFUALT_DETECT_HIGH_THRESHOLD;
	data->uProxLoThresh_detect = DEFUALT_DETECT_LOW_THRESHOLD;

	set_light_coef(data);

	pr_info("[SSP] %s - finished id %d\n",__func__, color_id);

	return 0;
}


/*************************************************************************/
/* Color ID HiddenHole Sysfs                                             */
/*************************************************************************/
ssize_t light_hiddendhole_version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char efs_version[COLOR_ID_VERSION_FILE_LENGTH] = "";
	char str_ids[COLOR_ID_IDS_LENGTH] = "";
	char idsbuf[5];
	int i, version = -1, iRet = 0;
	char *token, *str, temp[2];
	int coef[7], err = 0, length;
	u8 colorid;
	unsigned int thresh[2];

	for( i = 1 ; i <= COLOR_NUM ; i++)
	{
		if(version < light_coef_predefine_table[i].version)
			version = light_coef_predefine_table[i].version;

		snprintf(idsbuf, sizeof(idsbuf), "%x",light_coef_predefine_table[i].color_id);
		strcat(str_ids, idsbuf);
	}

	iRet = light_colorid_read_efs_version(efs_version, COLOR_ID_VERSION_FILE_LENGTH);
	if(iRet < 0)
	{
		pr_err("[SSP]: %s - version read failed %d \n", __func__, iRet);
		return -EIO;
	}
	
	str = efs_version;
	token = strsep(&str, ".");
	length = strlen(str);

	for(i=0;i<length;i++)
	{
		temp[0] = str[i];
		temp[1] = '\0';
		iRet = kstrtou8(temp, 16, &colorid);
		if (iRet<0) {
			pr_err("[SSP] %s - colorid %c kstrtou8 error %d\n",__func__,str[i],iRet);
			err = 1;
			break;
		}
		
		iRet = light_colorid_read_predefined_file((int)colorid,coef,thresh);
		if(iRet < 0)
		{
			err = 1; 
			pr_err("[SSP] %s - read coef fail, err = %d\n",__func__, err);
			break;
		}
	}

	if(err == 1)
		return snprintf(buf, PAGE_SIZE, "%d,%d\n", 0,0);
	else
		return snprintf(buf, PAGE_SIZE, "P%s.%s,P%d.%s\n", efs_version,str,version,str_ids);
}

ssize_t light_hiddenhole_version_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	char temp;
	int version = -1, iRet=0;
	char str_ids[COLOR_ID_IDS_LENGTH] = "";
	char *token, *str;
		
	pr_info("[SSP] %s - %s\n", __func__,buf);

	sscanf(buf,"%c%6d",&temp, &version);

	str = (char*)buf;
	token = strsep(&str, ".");

	if(str == NULL)
	{
		pr_err("[SSP] %s - ids str is nullw\n",__func__);
		return -EINVAL;
	}
		
	if(strlen(str) < COLOR_ID_IDS_LENGTH)
		sscanf(str,"%20s",str_ids);
	else
	{
		pr_err("[SSP] %s - ids overflow\n",__func__);
		return -EINVAL;
	}

	iRet = light_colorid_write_efs_version(version, str_ids);
	if(iRet < 0)
		pr_err("[SSP] %s - write version file err %d \n",__func__,iRet);
	else
		pr_info("[SSP] %s - Success\n", __func__);

	return size;
}

ssize_t light_hiddendhole_hh_write_all_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0, err = 0;

	ret = light_colorid_write_all_predefined_data();

	if(ret >= 0)
	{
		int coef[7], i;
		unsigned int thresh[2];
		for( i = 1 ; i <= COLOR_NUM ; i++)
		{
			ret = light_colorid_read_predefined_file(light_coef_predefine_table[i].color_id,coef,thresh);
			if(ret < 0)
			{
				err = 1; 
				pr_err("[SSP] %s - read coef fail, err = %d\n",__func__, err);
				break;
			}
		}
	}
	else
	{
		err = 1;
		pr_err("[SSP] %s - write fail\n",__func__);
	}
	
	return snprintf(buf, PAGE_SIZE, "%s\n", (err==0)?"TRUE":"FALSE");
}

ssize_t light_hiddenhole_write_all_data_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int color_id = 0;
	int coef[7] = {0, };	/* r_coef,g_coef,b_coef,c_coef,dgf,cct_coef,cct_offset */
	unsigned int thresh[2] = {0, };	/* high,low*/
	int crc = 0;
	int iRet = 0;
	int temp_ret = 0;
	mm_segment_t old_fs;
	struct file *coef_filp = NULL;
	char file_name[COLOR_ID_PREDEFINED_FILENAME_LENTH] = {0, };
	char file_contents[COLOR_ID_PREDEFINED_FILE_LENGTH] = {0, };

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pr_info("[SSP] %s - %s\n", __func__,buf);

	temp_ret = sscanf(buf, "%2d,%6d,%6d,%6d,%6d,%6d,%6d,%6d,%6d,%6d,%7d",
		&color_id, &coef[4], &coef[0], &coef[1],
		&coef[2], &coef[3], &coef[5], &coef[6],
		&thresh[0], &thresh[1], &crc);

	if((coef[0]+coef[1]+coef[2]+coef[3]+coef[4]+coef[5]+coef[6]+thresh[0]+thresh[1] != crc) ||
		(temp_ret != WRITE_ALL_DATA_NUMBER))
	{
		set_fs(old_fs);
		pr_info("[SSP] %s - crc is wrong\n", __func__);
		return -EINVAL;
	}

	pr_info("[SSP] %s - %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",__func__,
		color_id, coef[4], coef[0], coef[1], 
		coef[2], coef[3], coef[5], coef[6],
		thresh[0], thresh[1], crc);
	
	snprintf(file_name, sizeof(file_name), "%s%d",PREDEFINED_FILE_NAME,color_id);
	snprintf(file_contents, sizeof(file_contents), "%d,%d,%d,%d,%d,%d,%d,%u,%u,%d", 
		coef[0], coef[1], coef[2], coef[3], coef[4], coef[5], coef[6], thresh[0], thresh[1], crc);

	coef_filp = filp_open(file_name,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0660);

	if (IS_ERR(coef_filp)) {
		pr_err("[SSP] %s - Can't open coef file %d \n", __func__, color_id);
		set_fs(old_fs);
		iRet = PTR_ERR(coef_filp);
		return iRet;
	}

	iRet = vfs_write(coef_filp, (char *)file_contents,
		sizeof(char)*COLOR_ID_PREDEFINED_FILE_LENGTH, &coef_filp->f_pos);
	if (iRet < 0) {
		pr_err("[SSP] %s - Can't write the coef data to file\n", __func__);
		filp_close(coef_filp, current->files);		
		set_fs(old_fs);
		return -EIO;
	}

	filp_close(coef_filp, current->files);
	set_fs(old_fs);

	pr_info("[SSP] %s - Success %d\n", __func__, iRet);
	return size;
}

ssize_t  light_hiddendhole_is_exist_efs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int is_exist = 1, colorid;
	mm_segment_t old_fs;
	struct file *coef_filp = NULL;
	char file_name[COLOR_ID_PREDEFINED_FILENAME_LENTH];

	pr_info("[SSP] %s ",__func__);

	colorid = light_corloid_read_colorid();
	if(colorid < 0)
	{
		pr_err("[SSP] %s - read colorid failed - %d\n",__func__, colorid);
		is_exist = 0;
	}
	else
	{
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		snprintf(file_name, sizeof(file_name), "%s%d",PREDEFINED_FILE_NAME,colorid);

		coef_filp = filp_open(file_name, O_RDONLY, 0660);

		if (IS_ERR(coef_filp)) {
			pr_err("[SSP] %s - Can't open coef file %d \n", __func__, colorid);
			set_fs(old_fs);
			is_exist = 0;
		}
		else
		{
			filp_close(coef_filp, current->files);
			set_fs(old_fs);
		}
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", (is_exist==1)?"TRUE":"FALSE");
}

ssize_t light_hiddendhole_check_coef_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d", 1);
}
ssize_t light_hiddendhole_check_coef_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	pr_info("[SSP] %s ",__func__);

	if(initialize_light_colorid(data) <0)
		pr_err("[SSP]: %s - initialize light colorid failed\n", __func__);

	return size;
}

static DEVICE_ATTR(hh_ver, S_IRUGO | S_IWUSR | S_IWGRP,
	light_hiddendhole_version_show, light_hiddenhole_version_store);
static DEVICE_ATTR(hh_write_all_data, S_IRUGO | S_IWUSR | S_IWGRP,
	light_hiddendhole_hh_write_all_data_show, light_hiddenhole_write_all_data_store);
static DEVICE_ATTR(hh_is_exist_efs, S_IRUGO, light_hiddendhole_is_exist_efs_show, NULL);
static DEVICE_ATTR(hh_check_coef, S_IRUGO | S_IWUSR | S_IWGRP, light_hiddendhole_check_coef_show, light_hiddendhole_check_coef_store);

static struct device_attribute *hiddenhole_attrs[] = {
	&dev_attr_hh_ver,
	&dev_attr_hh_write_all_data,
	&dev_attr_hh_is_exist_efs,
	&dev_attr_hh_check_coef,
	NULL,
};

void initialize_hiddenhole_factorytest(struct ssp_data *data)
{
	sensors_register(data->hiddenhole_device, data, hiddenhole_attrs, "hidden_hole");
}

void remove_hiddenhole_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->hiddenhole_device, hiddenhole_attrs);
}
