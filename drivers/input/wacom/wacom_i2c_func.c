/*
 *  wacom_i2c_func.c - Wacom G5 Digitizer Controller (I2C bus)
 *
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "wacom.h"
#include "wacom_i2c_func.h"
#include "wacom_i2c_firm.h"

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
#define CONFIG_SAMSUNG_KERNEL_DEBUG_USER
#endif
int coordc;

void forced_release(struct wacom_i2c *wac_i2c)
{
#if defined(CONFIG_SAMSUNG_KERNEL_DEBUG_USER)
	input_info(true, &wac_i2c->client->dev, "%s\n", __func__);
#endif
	input_report_abs(wac_i2c->input_dev, ABS_X, 0);
	input_report_abs(wac_i2c->input_dev, ABS_Y, 0);
	input_report_abs(wac_i2c->input_dev, ABS_PRESSURE, 0);
	input_report_abs(wac_i2c->input_dev, ABS_DISTANCE, 0);
	input_report_key(wac_i2c->input_dev, BTN_STYLUS, 0);
	input_report_key(wac_i2c->input_dev, BTN_TOUCH, 0);
	input_report_key(wac_i2c->input_dev, BTN_TOOL_RUBBER, 0);
	input_report_key(wac_i2c->input_dev, BTN_TOOL_PEN, 0);
	input_sync(wac_i2c->input_dev);
#ifdef CONFIG_INPUT_BOOSTER
	input_booster_send_event(BOOSTER_DEVICE_PEN, BOOSTER_MODE_FORCE_OFF);
#endif

	wac_i2c->pen_prox = 0;
	wac_i2c->pen_pressed = 0;
	wac_i2c->side_pressed = 0;
	/*wac_i2c->pen_pdct = PDCT_NOSIGNAL;*/
}

int wacom_i2c_send(struct wacom_i2c *wac_i2c,
			  const char *buf, int count, bool mode)
{
	struct i2c_client *client = mode ?
		wac_i2c->client_boot : wac_i2c->client;

	if (wac_i2c->boot_mode && !mode) {
		input_err(true, &client->dev, "failed to send\n");
		return 0;
	}

	return i2c_master_send(client, buf, count);
}

int wacom_i2c_recv(struct wacom_i2c *wac_i2c,
			char *buf, int count, bool mode)
{
	struct i2c_client *client = mode ?
		wac_i2c->client_boot : wac_i2c->client;

	if (wac_i2c->boot_mode && !mode) {
		input_err(true, &client->dev, "failed to send\n");
		return 0;
	}

	return i2c_master_recv(client, buf, count);
}

int wacom_i2c_test(struct wacom_i2c *wac_i2c)
{
	int ret, i;
	char buf, test[10];
	buf = COM_QUERY;

	ret = wacom_i2c_send(wac_i2c, &buf, sizeof(buf), false);
	if (ret > 0)
		input_err(true, &wac_i2c->client->dev, "buf:%d, sent:%d\n", buf, ret);
	else {
		input_err(true, &wac_i2c->client->dev, "Digitizer is not active\n");
		return -1;
	}

	ret = wacom_i2c_recv(wac_i2c, test, sizeof(test), false);
	if (ret >= 0) {
		for (i = 0; i < 8; i++)
			input_info(true, &wac_i2c->client->dev, "%d\n", test[i]);
	} else {
		input_err(true, &wac_i2c->client->dev, "Digitizer does not reply\n");
		return -1;
	}

	return 0;
}

int wacom_checksum(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;
	int ret = 0, retry = 10;
	int i = 0;
	u8 buf[5] = {0, };

	buf[0] = COM_CHECKSUM;

	while (retry--) {
		ret = wacom_i2c_send(wac_i2c, &buf[0], 1, false);
		if (ret < 0) {
			input_err(true, &client->dev,
			       "i2c fail, retry, %d\n",
			       __LINE__);
			continue;
		}

		msleep(200);
		ret = wacom_i2c_recv(wac_i2c, buf, 5, false);
		if (ret < 0) {
			input_err(true, &client->dev,
			       "i2c fail, retry, %d\n",
			       __LINE__);
			continue;
		} else if (buf[0] == 0x1f)
			break;
		input_info(true, &client->dev, "checksum retry\n");
	}

	if (ret >= 0) {
		input_info(true, &client->dev, 
		       "received checksum %x, %x, %x, %x, %x\n",
		       buf[0], buf[1], buf[2], buf[3], buf[4]);
	}

	for (i = 0; i < 5; ++i) {
		if (buf[i] != fw_chksum[i]) {
			input_info(true, &client->dev,
			       "checksum fail %dth %x %x\n", i,
			       buf[i], fw_chksum[i]);
			break;
		}
	}

	wac_i2c->checksum_result = (5 == i);

	return wac_i2c->checksum_result;
}

int wacom_i2c_query(struct wacom_i2c *wac_i2c)
{
	struct wacom_g5_platform_data *pdata = wac_i2c->pdata;
	struct wacom_features *wac_feature = wac_i2c->wac_feature;
	struct i2c_client *client = wac_i2c->client;
	u8 data[COM_QUERY_BUFFER] = {0, };
	u8 *query = data + COM_QUERY_POS;
	int read_size = COM_QUERY_BUFFER;
	u8 buf = COM_QUERY;
	int ret;
	int i;
	int max_x, max_y, pressure;

	for (i = 0; i < COM_QUERY_RETRY; i++) {
		if (unlikely(pdata->use_query_cmd)) {
			ret = wacom_i2c_send(wac_i2c, &buf, 1, false);
			if (ret < 0) {
				input_err(true, &client->dev, "I2C send failed(%d)\n", ret);
				msleep(50);
				continue;
			}
			read_size = COM_QUERY_NUM;
			query = data;
			msleep(50);
		}

		ret = wacom_i2c_recv(wac_i2c, data, read_size, false);
		if (ret < 0) {
			input_err(true, &client->dev, "I2C recv failed(%d)\n", ret);
			continue;
		}

		input_info(true, &client->dev, "%s: %dth ret of wacom query=%d\n",
		       __func__, i, ret);

		if (read_size != ret) {
			input_err(true, &client->dev, "read size error %d of %d\n", ret, read_size);
			continue;
		}

		if (0x0f == query[EPEN_REG_HEADER]) {
			wac_feature->fw_version =
				((u16) query[EPEN_REG_FWVER1] << 8) + (u16) query[EPEN_REG_FWVER2];
			break;
		}

		input_err(true, &client->dev,
				"%X, %X, %X, %X, %X, %X, %X, fw=0x%x\n",
				query[0], query[1], query[2], query[3],
				query[4], query[5], query[6],
				wac_feature->fw_version);
	}

	input_info(true, &client->dev,
		"%X, %X, %X, %X, %X, %X, %X, %X, %X, %X, %X, %X, %X, %X\n", query[0],
		query[1], query[2], query[3], query[4], query[5], query[6],
		query[7], query[8], query[9], query[10], query[11], query[12], query[13]);

	if (i == COM_QUERY_RETRY || ret < 0) {
		input_err(true, &client->dev, "%s:failed to read query\n", __func__);
		wac_feature->fw_version = 0;
		wac_i2c->query_status = false;
		return ret;
	}
	wac_i2c->query_status = true;

	max_x = ((u16) query[EPEN_REG_X1] << 8) + (u16) query[EPEN_REG_X2];
	max_y = ((u16) query[EPEN_REG_Y1] << 8) + (u16) query[EPEN_REG_Y2];
	pressure = ((u16) query[EPEN_REG_PRESSURE1] << 8) + (u16) query[EPEN_REG_PRESSURE2];

	input_info(true, &client->dev,"q max_x=%d max_y=%d, max_pressure=%d\n",
		max_x, max_y, pressure);
	input_info(true, &client->dev,"p max_x=%d max_y=%d, max_pressure=%d\n",
		pdata->max_x, pdata->max_y, pdata->max_pressure);
	input_info(true, &client->dev,"fw_version=0x%X (d7:0x%X,d8:0x%X)\n",
	       wac_feature->fw_version, query[EPEN_REG_FWVER1], query[EPEN_REG_FWVER2]);
	input_info(true, &client->dev,"mpu %#x, bl %#x, tx %d, ty %d, h %d\n",
		query[EPEN_REG_MPUVER], query[EPEN_REG_BLVER],
		query[EPEN_REG_TILT_X], query[EPEN_REG_TILT_Y],
		query[EPEN_REG_HEIGHT]);

	return ret;
}

int wacom_i2c_modecheck(struct wacom_i2c *wac_i2c)
{
	u8 buf = COM_QUERY;
	int ret;
	int mode = WACOM_I2C_MODE_NORMAL;
	u8 data[COM_QUERY_BUFFER] = {0, };
	int read_size = COM_QUERY_NUM;

	ret = wacom_i2c_send(wac_i2c, &buf, 1, false);
	if (ret < 0) {
		mode = WACOM_I2C_MODE_BOOT;
	}
	else{
		mode = WACOM_I2C_MODE_NORMAL;
	}

	ret = wacom_i2c_recv(wac_i2c, data, read_size, false);
	ret = wacom_i2c_recv(wac_i2c, data, read_size, false);
	input_info(true, &wac_i2c->client->dev, "I2C send at usermode(%d) querys(%d)\n", mode, ret);

	return mode;
}

int wacom_i2c_set_sense_mode(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;
	int retval;
	char data[4] = {0, 0, 0, 0};

	input_info(true, &wac_i2c->client->dev, "%s cmd mod(%d)\n", __func__, wac_i2c->wcharging_mode);

	if (wac_i2c->wcharging_mode == 1)
		data[0] = COM_LOW_SENSE_MODE;
	else if(wac_i2c->wcharging_mode == 3)
		data[0] = COM_LOW_SENSE_MODE2;
	else{
		/* it must be 0 */
		data[0] = COM_NORMAL_SENSE_MODE;
	}

	retval = wacom_i2c_send(wac_i2c, &data[0], 1, WACOM_NORMAL_MODE);
	if (retval != 1) {
		input_err(true, &client->dev, "%s: failed to send wacom i2c mode, %d\n", __func__, retval);
		return retval;
	}

	msleep(60);

	data[0] = COM_SAMPLERATE_STOP;
	retval = wacom_i2c_send(wac_i2c, &data[0], 1, WACOM_NORMAL_MODE);
	if (retval != 1) {
		input_err(true, &client->dev, "%s: failed to read wacom i2c send1, %d\n", __func__, retval);
		return retval;
	}

	msleep(60);

#if 0 // temp block not to receive gabage irq by cmd
	data[1] = COM_REQUEST_SENSE_MODE;
	retval = wacom_i2c_send(wac_i2c, &data[1], 1, WACOM_NORMAL_MODE);
	if (retval != 1) {
		input_err(true, &client->dev, "%s: failed to read wacom i2c send2, %d\n", __func__, retval);
		return retval;
	}

	msleep(60);

	retval = wacom_i2c_recv(wac_i2c, &data[2], 2, WACOM_NORMAL_MODE);
	if (retval != 2) {
		input_err(true, &client->dev, "%s: failed to read wacom i2c recv, %d\n", __func__, retval);
		return retval;
	}

	input_info(true, &client->dev, "%s: mode:%X, %X\n", __func__, data[2], data[3]);

	data[0] = COM_SAMPLERATE_STOP;
	retval = wacom_i2c_send(wac_i2c, &data[0], 1, WACOM_NORMAL_MODE);
	if (retval != 1) {
		input_err(true, &client->dev, "%s: failed to read wacom i2c send3, %d\n", __func__, retval);
		return retval;
	}

	msleep(60);
#endif
	data[0] = COM_SAMPLERATE_START;
	retval = wacom_i2c_send(wac_i2c, &data[0], 1, WACOM_NORMAL_MODE);
	if (retval != 1) {
		input_err(true, &client->dev, "%s: failed to read wacom i2c send4, %d\n", __func__, retval);
		return retval;
	}

	msleep(60);

	return data[3];
}

#ifdef WACOM_USE_SURVEY_MODE
int wacom_i2c_set_survey_mode(struct wacom_i2c *wac_i2c, int mode)
{
	struct i2c_client *client = wac_i2c->client;
	int retval;
	char data[4] = {0, 0, 0, 0};

	input_info(true, &client->dev, "%s cmd & ps %s mode : %s\n",
					__func__, wac_i2c->battery_saving_mode ? "on" : "off",
					mode ? "garage only" : "garage & aop");


	data[0] = mode ? COM_SURVEY_TARGET_GARAGEONLY : COM_SURVEYSCAN;
	retval = wacom_i2c_send(wac_i2c, &data[0], 1, WACOM_NORMAL_MODE);
	if (retval != 1) {
		input_err(true, &client->dev, "%s: failed to read wacom i2c send survey, %d\n", __func__, retval);
		wac_i2c->reset_flag = true;
		return retval;
	}
	msleep(35);
	wac_i2c->survey_cmd_state = true;
	return retval;
}

int wacom_i2c_exit_survey_mode(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;
	int retval;
	char data[4] = {0, 0, 0, 0};

	input_info(true, &wac_i2c->client->dev, "%s cmd\n", __func__);

	data[0] = COM_SURVEYEXIT;
	retval = wacom_i2c_send(wac_i2c, &data[0], 1, WACOM_NORMAL_MODE);
	if (retval != 1) {
		input_err(true, &client->dev,  "%s: failed to read wacom i2c send (survey exit), %d\n", __func__, retval);
		wac_i2c->reset_flag = true;
		return retval;
	}
	wac_i2c->survey_cmd_state = false;
	return 0;
}
#endif

#ifdef WACOM_IMPORT_FW_ALGO
#ifdef WACOM_USE_OFFSET_TABLE
void wacom_i2c_coord_offset(u16 *coordX, u16 *coordY)
{
	u16 ix, iy;
	u16 dXx_0, dXy_0, dXx_1, dXy_1;
	int D0, D1, D2, D3, D;

	ix = (u16) (((*coordX)) / CAL_PITCH);
	iy = (u16) (((*coordY)) / CAL_PITCH);

	dXx_0 = *coordX - (ix * CAL_PITCH);
	dXx_1 = CAL_PITCH - dXx_0;

	dXy_0 = *coordY - (iy * CAL_PITCH);
	dXy_1 = CAL_PITCH - dXy_0;

	if (*coordX <= WACOM_MAX_COORD_X) {
		D0 = tableX[user_hand][screen_rotate][ix +
						      (iy * LATTICE_SIZE_X)] *
		    (dXx_1 + dXy_1);
		D1 = tableX[user_hand][screen_rotate][ix + 1 +
						      iy * LATTICE_SIZE_X] *
		    (dXx_0 + dXy_1);
		D2 = tableX[user_hand][screen_rotate][ix +
						      (iy +
						       1) * LATTICE_SIZE_X] *
		    (dXx_1 + dXy_0);
		D3 = tableX[user_hand][screen_rotate][ix + 1 +
						      (iy +
						       1) * LATTICE_SIZE_X] *
		    (dXx_0 + dXy_0);
		D = (D0 + D1 + D2 + D3) / (4 * CAL_PITCH);

		if (((int)*coordX + D) > 0)
			*coordX += D;
		else
			*coordX = 0;
	}

	if (*coordY <= WACOM_MAX_COORD_Y) {
		D0 = tableY[user_hand][screen_rotate][ix +
						      (iy * LATTICE_SIZE_X)] *
		    (dXy_1 + dXx_1);
		D1 = tableY[user_hand][screen_rotate][ix + 1 +
						      iy * LATTICE_SIZE_X] *
		    (dXy_1 + dXx_0);
		D2 = tableY[user_hand][screen_rotate][ix +
						      (iy +
						       1) * LATTICE_SIZE_X] *
		    (dXy_0 + dXx_1);
		D3 = tableY[user_hand][screen_rotate][ix + 1 +
						      (iy +
						       1) * LATTICE_SIZE_X] *
		    (dXy_0 + dXx_0);
		D = (D0 + D1 + D2 + D3) / (4 * CAL_PITCH);

		if (((int)*coordY + D) > 0)
			*coordY += D;
		else
			*coordY = 0;
	}
}
#endif

#ifdef WACOM_USE_AVERAGING
#define STEP 32
void wacom_i2c_coord_average(short *CoordX, short *CoordY,
			     int bFirstLscan, int aveStrength)
{
	unsigned char i;
	unsigned int work;
	unsigned char ave_step = 4, ave_shift = 2;
	static int Sum_X, Sum_Y;
	static int AveBuffX[STEP], AveBuffY[STEP];
	static unsigned char AvePtr;
	static unsigned char bResetted;
#ifdef WACOM_USE_AVE_TRANSITION
	static int tmpBuffX[STEP], tmpBuffY[STEP];
	static unsigned char last_step, last_shift;
	static bool transition;
	static int tras_counter;
#endif
	if (bFirstLscan == 0) {
		bResetted = 0;
#ifdef WACOM_USE_AVE_TRANSITION
		transition = false;
		tras_counter = 0;
		last_step = 4;
		last_shift = 2;
#endif
		return ;
	}
#ifdef WACOM_USE_AVE_TRANSITION
	if (bResetted) {
		if (transition) {
			ave_step = last_step;
			ave_shift = last_shift;
		} else {
			ave_step = 2 << (aveStrength-1);
			ave_shift = aveStrength;
		}

		if (!transition && ave_step != 0 && last_step != 0) {
			if (ave_step > last_step) {
				transition = true;
				tras_counter = ave_step;
				/*input_info(true, &wac_i2c->client->dev, 
					"Trans %d to %d\n",
					last_step, ave_step);*/

				memcpy(tmpBuffX, AveBuffX,
					sizeof(unsigned int) * last_step);
				memcpy(tmpBuffY, AveBuffY,
					sizeof(unsigned int) * last_step);
				for (i = 0 ; i < last_step; ++i) {
					AveBuffX[i] = tmpBuffX[AvePtr];
					AveBuffY[i] = tmpBuffY[AvePtr];
					if (++AvePtr >= last_step)
						AvePtr = 0;
				}
				for ( ; i < ave_step; ++i) {
					AveBuffX[i] = *CoordX;
					AveBuffY[i] = *CoordY;
					Sum_X += *CoordX;
					Sum_Y += *CoordY;
				}
				AvePtr = 0;

				*CoordX = Sum_X >> ave_shift;
				*CoordY = Sum_Y >> ave_shift;

				bResetted = 1;

				last_step = ave_step;
				last_shift = ave_shift;
				return ;
			} else if (ave_step < last_step) {
				transition = true;
				tras_counter = ave_step;
				/*input_info(true, &wac_i2c->client->dev, 
					"Trans %d to %d\n",
					last_step, ave_step);*/

				memcpy(tmpBuffX, AveBuffX,
					sizeof(unsigned int) * last_step);
				memcpy(tmpBuffY, AveBuffY,
					sizeof(unsigned int) * last_step);
				Sum_X = 0;
				Sum_Y = 0;
				for (i = 1 ; i <= ave_step; ++i) {
					if (AvePtr == 0)
						AvePtr = last_step - 1;
					else
						--AvePtr;
					AveBuffX[ave_step-i] = tmpBuffX[AvePtr];
					Sum_X = Sum_X + tmpBuffX[AvePtr];

					AveBuffY[ave_step-i] = tmpBuffY[AvePtr];
					Sum_Y = Sum_Y + tmpBuffY[AvePtr];
				}
				AvePtr = 0;
				bResetted = 1;
				*CoordX = Sum_X >> ave_shift;
				*CoordY = Sum_Y >> ave_shift;

				bResetted = 1;

				last_step = ave_step;
				last_shift = ave_shift;
				return ;
			}
		}

		if (!transition && (last_step != ave_step)) {
			last_step = ave_step;
			last_shift = ave_shift;
		}
	}
#endif
	if (bFirstLscan && (bResetted == 0)) {
		AvePtr = 0;
		ave_step = 4;
		ave_shift = 2;
#if defined(WACOM_USE_AVE_TRANSITION)
		tras_counter = ave_step;
#endif
		for (i = 0; i < ave_step; i++) {
			AveBuffX[i] = *CoordX;
			AveBuffY[i] = *CoordY;
		}
		Sum_X = (unsigned int)*CoordX << ave_shift;
		Sum_Y = (unsigned int)*CoordY << ave_shift;
		bResetted = 1;
	} else if (bFirstLscan) {
		Sum_X = Sum_X - AveBuffX[AvePtr] + (*CoordX);
		AveBuffX[AvePtr] = *CoordX;
		work = Sum_X >> ave_shift;
		*CoordX = (unsigned int)work;

		Sum_Y = Sum_Y - AveBuffY[AvePtr] + (*CoordY);
		AveBuffY[AvePtr] = (*CoordY);
		work = Sum_Y >> ave_shift;
		*CoordY = (unsigned int)work;

		if (++AvePtr >= ave_step)
			AvePtr = 0;
	}
#ifdef WACOM_USE_AVE_TRANSITION
	if (transition) {
		--tras_counter;
		if (tras_counter < 0)
			transition = false;
	}
#endif
}
#endif

u8 wacom_i2c_coord_level(u16 gain)
{
	if (gain >= 0 && gain <= 14)
		return 0;
	else if (gain > 14 && gain <= 24)
		return 1;
	else
		return 2;
}

#ifdef WACOM_USE_BOX_FILTER
void boxfilt(short *CoordX, short *CoordY,
			int height, int bFirstLscan)
{
	bool isMoved = false;
	static bool bFirst = true;
	static short lastX_loc, lastY_loc;
	static unsigned char bResetted;
	int threshold = 0;
	int distance = 0;
	static short bounce;

	/*Reset filter*/
	if (bFirstLscan == 0) {
		bResetted = 0;
		return ;
	}

	if (bFirstLscan && (bResetted == 0)) {
		lastX_loc = *CoordX;
		lastY_loc = *CoordY;
		bResetted = 1;
	}

	if (bFirst) {
		lastX_loc = *CoordX;
		lastY_loc = *CoordY;
		bFirst = false;
	}

	/*Start Filtering*/
	threshold = 30;

	/*X*/
	distance = abs(*CoordX - lastX_loc);

	if (distance >= threshold)
		isMoved = true;

	if (isMoved == false) {
		distance = abs(*CoordY - lastY_loc);
		if (distance >= threshold)
			isMoved = true;
	}

	/*Update position*/
	if (isMoved) {
		lastX_loc = *CoordX;
		lastY_loc = *CoordY;
	} else {
		*CoordX = lastX_loc + bounce;
		*CoordY = lastY_loc;
		if (bounce)
			bounce = 0;
		else
			bounce += 5;
	}
}
#endif

#if defined(WACOM_USE_AVE_TRANSITION)
int g_aveLevel_C[] = {2, 2, 4, };
int g_aveLevel_X[] = {3, 3, 4, };
int g_aveLevel_Y[] = {3, 3, 4, };
int g_aveLevel_Trs[] = {3, 4, 4, };
int g_aveLevel_Cor[] = {4, 4, 4, };

void ave_level(short CoordX, short CoordY,
			int height, int *aveStrength)
{
	bool transition = false;
	bool edgeY = false, edgeX = false;
	bool cY = false, cX = false;

	if (CoordY > (WACOM_MAX_COORD_Y - 800))
		cY = true;
	else if (CoordY < 800)
		cY = true;

	if (CoordX > (WACOM_MAX_COORD_X - 800))
		cX = true;
	else if (CoordX < 800)
		cX = true;

	if (cX && cY) {
		*aveStrength = g_aveLevel_Cor[height];
		return ;
	}

	/*Start Filtering*/
	if (CoordX > X_INC_E1)
		edgeX = true;
	else if (CoordX < X_INC_S1)
		edgeX = true;

	/*Right*/
	if (CoordY > Y_INC_E1) {
		/*Transition*/
		if (CoordY < Y_INC_E3)
			transition = true;
		else
			edgeY = true;
	}
	/*Left*/
	else if (CoordY < Y_INC_S1) {
		/*Transition*/
		if (CoordY > Y_INC_S3)
			transition = true;
		else
			edgeY = true;
	}

	if (transition)
		*aveStrength = g_aveLevel_Trs[height];
	else if (edgeX)
		*aveStrength = g_aveLevel_X[height];
	else if (edgeY)
		*aveStrength = g_aveLevel_Y[height];
	else
		*aveStrength = g_aveLevel_C[height];
}
#endif
#endif /*WACOM_IMPORT_FW_ALGO*/

static int keycode[] = {
	KEY_RECENT, KEY_BACK,
};

void forced_release_key(struct wacom_i2c *wac_i2c)
{
	input_info(true, &wac_i2c->client->dev, "%s : [%x/%x]!\n",
		__func__, wac_i2c->soft_key_pressed[0], wac_i2c->soft_key_pressed[1]);

	input_report_key(wac_i2c->input_dev, KEY_RECENT, 0);
	input_report_key(wac_i2c->input_dev, KEY_BACK, 0);
	input_sync(wac_i2c->input_dev);
}

void wacom_i2c_softkey(struct wacom_i2c *wac_i2c, s16 key, s16 pressed)
{
	struct i2c_client *client = wac_i2c->client;

#ifdef WACOM_USE_SOFTKEY_BLOCK
	if (wac_i2c->block_softkey && pressed) {
		cancel_delayed_work_sync(&wac_i2c->softkey_block_work);
		input_info(true, &client->dev, "block p\n");
		return ;
	} else if (wac_i2c->block_softkey && !pressed) {
		input_info(true, &client->dev, "block r\n");
		wac_i2c->block_softkey = false;
		return ;
	}
#endif
	input_report_key(wac_i2c->input_dev,
		keycode[key], pressed);
	input_sync(wac_i2c->input_dev);

	wac_i2c->soft_key_pressed[key] = pressed;
	if(pressed) cancel_delayed_work_sync(&wac_i2c->fullscan_check_work);

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	input_info(true, &client->dev, "%d %s\n",
		keycode[key], !!pressed ? "PRESS" : "RELEASE");
#else
	input_info(true, &client->dev, "softkey %s\n",
		!!pressed ? "PRESS" : "RELEASE");
#endif
}

void forced_release_fullscan(struct wacom_i2c *wac_i2c)
{
	input_info(true, &wac_i2c->client->dev, "%s full scan OUT \n",__func__);
	wac_i2c->tsp_noise_mode = set_spen_mode(0);
	wac_i2c->fullscan_mode = false;
}

void wacom_set_scan_mode(struct wacom_i2c *wac_i2c, int mode)
{
	if(mode) input_info(true, &wac_i2c->client->dev, "set - full scan IN \n");
	else input_info(true, &wac_i2c->client->dev, "set - full scan OUT \n");

	wac_i2c->tsp_noise_mode = set_spen_mode(mode);
}

int wacom_get_scan_mode(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;
	int retval;
	char data[COM_COORD_NUM+1] = {0, };
	int i, retry = 3, temp=0,ret =0;

	input_err(true, &wac_i2c->client->dev, "get scan mode \n");

	data[0] = COM_SAMPLERATE_STOP;
	retval = wacom_i2c_send(wac_i2c, &data[0], 1, WACOM_NORMAL_MODE);
	if (retval != 1) {
		input_err(true, &client->dev,  "%s: failed to read wacom i2c send (stop), %d\n", __func__, retval);
		return 0;
	}

	data[0] = COM_REQUESTSCANMODE;
	retval = wacom_i2c_send(wac_i2c, &data[0], 1, WACOM_NORMAL_MODE);
	if (retval != 1) {
		input_err(true, &client->dev,  "%s: failed to read wacom i2c send (request data), %d\n", __func__, retval);
		return 0;
	}
	msleep(35);

	while (retry--) {
		retval = wacom_i2c_recv(wac_i2c, data, COM_COORD_NUM+1, false);
		if (retval < 0) {
			input_err(true, &client->dev,  "%s: failed to read wacom i2c send survey, %d\n", __func__, retval);
		}
		temp = 0;
		for(i=0 ; i<= COM_COORD_NUM; i++)
			temp += data[i];

		input_info(true, &client->dev, "%x %x %x %x %x, %x %x %x %x %x, %x %x %x\n",
					data[0], data[1], data[2], data[3], data[4], data[5], data[6],
					data[7], data[8], data[9], data[10], data[11], data[12]);

		if(temp == 0){ // unlock
			ret = 1;
			break;
		}
		else if(data[12] == 0x01){ // send noise mode to tsp
			ret = 2;
			break;
		}
		msleep(10);
	}

	input_info(true, &client->dev, "data[0] = %x, data[12] =%x, retry(%d) ret(%d)\n",
					data[0], data[12], 3-retry, ret);

	data[0] = COM_SAMPLERATE_133;
	retval = wacom_i2c_send(wac_i2c, &data[0], 1, WACOM_NORMAL_MODE);
	if (retval != 1) {
		input_err(true, &client->dev,  "%s: failed to read wacom i2c send (start), %d\n", __func__, retval);
		return 0;
	}
	return ret;
}

int wacom_get_aop_data(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;
	int retval;
	char data[COM_COORD_NUM+1] = {0, };
	int i, retry = 5, temp = 0, ret = 0;

	input_info(true, &wac_i2c->client->dev, "%s: get aop irq\n", __func__);

	while (retry--) {
		retval = wacom_i2c_recv(wac_i2c, data, COM_COORD_NUM+1, false);
		if (retval < 0) {
			input_err(true, &client->dev,  "%s: failed to read wacom i2c send survey, %d\n", __func__, retval);
		}

		input_info(true, &client->dev, "%x, %x, %x, %x, %x, %x, %x %x %x %x %x %x %x\n",
			data[0], data[1], data[2], data[3], data[4], data[5], data[6],
			data[7], data[8], data[9], data[10], data[11], data[12]);

		/* AOP event : 0, 0, 0, 0, 0, 0, 0 0 0 0 aa aa 0 */
		if((data[10] == 0xaa) && (data[11] == 0xaa)){
			temp = 0;
			for (i=0 ; i<10; i++) temp += data[i];
			if(temp == 0) ret = 1;
			break;
		}

		/* READ IRQ status*/
		if (!wac_i2c->pdata->get_irq_state())
			return 0;

		msleep(10);
	}

	input_info(true, &client->dev, "data[10] = %x data[11] = %x, retry(%d) ret(%d)\n",
					data[10], data[11], 5-retry, ret);

	return ret;
}


#ifdef LCD_FREQ_SYNC
void wacom_i2c_lcd_freq_check(struct wacom_i2c *wac_i2c, u8 *data)
{
	u32 lcd_freq = 0;

	if (wac_i2c->lcd_freq_wait == false) {
		lcd_freq = ((u16) data[10] << 8) + (u16) data[11];
		wac_i2c->lcd_freq = 2000000000 / (lcd_freq + 1);
		wac_i2c->lcd_freq_wait = true;
		schedule_work(&wac_i2c->lcd_freq_work);
	}
}
#endif

void wacom_fullscan_check_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, fullscan_check_work.work);
	int ret;

	input_info(true, &wac_i2c->client->dev, "fullscan_check_work irq-cnt(%d)\n",coordc);
	if(coordc <= 2){
		ret = wacom_get_scan_mode(wac_i2c);
		if(ret == 0){
			input_info(true, &wac_i2c->client->dev, "work - stay scan mode\n");
		}
		else if(ret == 1){
			input_info(true, &wac_i2c->client->dev, "work - full scan OUT\n");
			wac_i2c->tsp_noise_mode = set_spen_mode(0);
			wac_i2c->fullscan_mode = false;
		}
		else if(ret == 2){
			input_info(true, &wac_i2c->client->dev, "work - wacom noise mode\n");
			wac_i2c->tsp_noise_mode = set_spen_mode(2);
		}
	}
	return;
}

int wacom_i2c_coord(struct wacom_i2c *wac_i2c)
{
	struct wacom_g5_platform_data *pdata = wac_i2c->pdata;
	struct i2c_client *client = wac_i2c->client;
	u8 data[COM_COORD_NUM+1] = {0, };
	bool prox = false;
	bool rdy = false;
	int ret = 0,tsp = 0;
	int stylus;
	s16 x, y, pressure;
	s16 tmp;
	u8 gain = 0;
	s16 softkey, pressed, keycode;
	s8 tilt_x = 0;
	s8 tilt_y = 0;
	s8 retry = 3;

	while (retry--) {
		ret = wacom_i2c_recv(wac_i2c, data, COM_COORD_NUM+1, false);
		if (ret >= 0)
			break;

		input_err(true, &client->dev, "%s failed to read i2c.retry %d.L%d\n",
			__func__, retry, __LINE__);
	}
	if (ret < 0) {
		input_err(true, &client->dev, "i2c err, exit %s\n", __func__);
		return -1;
	}

#if defined(CONFIG_SAMSUNG_KERNEL_DEBUG_USER)
	/*input_info(true, &client->dev, "%x, %x, %x, %x, %x, %x, %x %x %x %x %x %x %x\n",
		data[0], data[1], data[2], data[3], data[4], data[5], data[6],
		data[7], data[8], data[9], data[10], data[11], data[12]);*/
#endif

	rdy = data[0] & 0x80;

#ifdef LCD_FREQ_SYNC
	if (!rdy && !data[1] && !data[2] && !data[3] && !data[4]) {
		if (likely(wac_i2c->use_lcd_freq_sync)) {
			if (unlikely(!wac_i2c->pen_prox)) {
				wacom_i2c_lcd_freq_check(wac_i2c, data);
			}
		}
	}
#endif
	if(coordc<100) coordc++;
	tsp = data[12] & 0x01;
	if(!rdy && tsp && wac_i2c->fullscan_mode == false){
		input_info(true, &client->dev, "%x, %x, %x, %x, %x, %x, %x %x %x %x %x %x %x coordc(%d)\n",
			data[0], data[1], data[2], data[3], data[4], data[5], data[6],
			data[7], data[8], data[9], data[10], data[11], data[12], coordc);
		if(data[10] == HSYNC_COUNTER_UMAGIC && data[11] == HSYNC_COUNTER_LMAGIC)
		{
			input_info(true, &client->dev, "x-y scan IN, rdy(%d) tsp(%d) \n",rdy,tsp);
			wac_i2c->fullscan_mode = true;
			if(wac_i2c->wacom_noise_state != WACOM_NOISE_HIGH) wac_i2c->tsp_noise_mode = set_spen_mode(1);
			else input_info(true, &client->dev, "high noise mode, skip tsp (75 1)\n");
			coordc = 0;
			cancel_delayed_work_sync(&wac_i2c->fullscan_check_work);
			schedule_delayed_work(&wac_i2c->fullscan_check_work, msecs_to_jiffies(2000));
		}
	}

	if(data[0] == 0x0F){
		input_info(true, &client->dev, "%x %x %x %x %x, %x %x %x %x %x, %x %x %x\n",
			data[0], data[1], data[2], data[3], data[4], data[5], data[6],
			data[7], data[8], data[9], data[10], data[11], data[12]);
		if((data[10] == WACOM_NOISE_HIGH) && (data[11] == WACOM_NOISE_HIGH)){
			input_err(true, &wac_i2c->client->dev, "11 11 high-noise mode \n");
			wac_i2c->wacom_noise_state = WACOM_NOISE_HIGH;
			wac_i2c->tsp_noise_mode = set_spen_mode(2);
		}
		else if((data[10] == WACOM_NOISE_LOW) && (data[11] == WACOM_NOISE_LOW)){
			input_err(true, &wac_i2c->client->dev, "22 22 low-noise mode \n");
			wac_i2c->wacom_noise_state = WACOM_NOISE_LOW;
			wac_i2c->tsp_noise_mode = set_spen_mode(0);
		}
	}

	if (rdy) {
		/* checking softkey */
		softkey = !!(data[12] & 0x80);
		if (unlikely(softkey)) {
			if (unlikely(wac_i2c->pen_prox))
				forced_release(wac_i2c);

			pressed = !!(data[12] & 0x40);
			keycode = (data[12] & 0x30) >> 4;

			wacom_i2c_softkey(wac_i2c, keycode, pressed);
			return 0;
		}

		/* prox check */
		if (!wac_i2c->pen_prox) {
#ifndef WACOM_USE_SURVEY_MODE
			/* check pdct */
			if (unlikely(wac_i2c->pen_pdct == PDCT_NOSIGNAL)) {
				input_info(true, &client->dev, "pdct is not active\n");
				return 0;
			}
#endif
#ifdef CONFIG_INPUT_BOOSTER
			input_booster_send_event(BOOSTER_DEVICE_PEN, BOOSTER_MODE_ON);
#endif
			wac_i2c->pen_prox = 1;

			if (data[0] & 0x40)
				wac_i2c->tool = BTN_TOOL_RUBBER;
			else
				wac_i2c->tool = BTN_TOOL_PEN;
#if defined(CONFIG_SAMSUNG_KERNEL_DEBUG_USER)
			input_info(true, &client->dev, "pen is in(%s)\n",
						wac_i2c->tool == BTN_TOOL_PEN ? "pen" : "rubber");
#else
			input_info(true, &client->dev, "pen is in\n");
#endif
			return 0;
		}

		prox = !!(data[0] & 0x10);
		stylus = !!(data[0] & 0x20);
		x = ((u16) data[1] << 8) + (u16) data[2];
		y = ((u16) data[3] << 8) + (u16) data[4];
		pressure = ((u16) data[5] << 8) + (u16) data[6];
		gain = data[7];
		tilt_x = (s8)data[9];
		tilt_y = -(s8)data[8];

		/* origin */
		x = x - pdata->origin[0];
		y = y - pdata->origin[1];

		/* change axis from wacom to lcd */
		if (pdata->x_invert)
			x = pdata->max_x - x;
		if (pdata->y_invert)
			y = pdata->max_y - y;

		if (pdata->xy_switch) {
			tmp = x;
			x = y;
			y = tmp;
		}

		/* validation check */
		if (unlikely(x < 0 || y < 0 || x > pdata->max_y || y > pdata->max_x)) {
#if defined(CONFIG_SAMSUNG_KERNEL_DEBUG_USER)
			input_info(true, &client->dev, "raw data x=%d, y=%d\n", x, y);
#endif
			return 0;
		}

		/* report info */
		input_report_abs(wac_i2c->input_dev, ABS_X, x);
		input_report_abs(wac_i2c->input_dev, ABS_Y, y);
		input_report_abs(wac_i2c->input_dev,
			ABS_PRESSURE, pressure);
		input_report_abs(wac_i2c->input_dev,
			ABS_DISTANCE, gain);
		input_report_abs(wac_i2c->input_dev,
			ABS_TILT_X, tilt_x);
		input_report_abs(wac_i2c->input_dev,
			ABS_TILT_Y, tilt_y);
		input_report_key(wac_i2c->input_dev,
			BTN_STYLUS, stylus);
		input_report_key(wac_i2c->input_dev, BTN_TOUCH, prox);
		input_report_key(wac_i2c->input_dev, wac_i2c->tool, 1);
		input_sync(wac_i2c->input_dev);

		/* log */
		if (prox && !wac_i2c->pen_pressed) {
#if defined(CONFIG_SAMSUNG_KERNEL_DEBUG_USER)
			input_info(true, &client->dev, "[P] x:%d, y:%d, pre:%d, tool:%x ps:%d\n",
							x, y, pressure, wac_i2c->tool, wac_i2c->battery_saving_mode);
#else
			input_info(true, &client->dev,  "Pressed ps:%d\n",
							wac_i2c->battery_saving_mode);
#endif
		} else if (!prox && wac_i2c->pen_pressed) {
#if defined(CONFIG_SAMSUNG_KERNEL_DEBUG_USER)
			input_info(true, &client->dev, "[R] x:%d, y:%d, pre:%d, tool:%x [0x%x]\n",
							x, y, pressure, wac_i2c->tool, wac_i2c->wac_feature->fw_version);
#else
			input_info(true, &client->dev, "Released [0x%x]\n",
							wac_i2c->wac_feature->fw_version);
#endif
		}
		wac_i2c->pen_pressed = prox;

		/* check side */
		if (stylus && !wac_i2c->side_pressed)
			input_info(true, &client->dev, "side on\n");
		else if (!stylus && wac_i2c->side_pressed)
			input_info(true, &client->dev, "side off\n");

		wac_i2c->side_pressed = stylus;
	} else {
		if (wac_i2c->pen_prox) {
			/* input_report_abs(wac->input_dev,
			   ABS_X, x); */
			/* input_report_abs(wac->input_dev,
			   ABS_Y, y); */

			input_report_abs(wac_i2c->input_dev, ABS_PRESSURE, 0);
			input_report_abs(wac_i2c->input_dev,
				ABS_DISTANCE, 0);
			input_report_key(wac_i2c->input_dev, BTN_STYLUS, 0);
			input_report_key(wac_i2c->input_dev, BTN_TOUCH, 0);
			input_report_key(wac_i2c->input_dev,
				BTN_TOOL_RUBBER, 0);
			input_report_key(wac_i2c->input_dev, BTN_TOOL_PEN, 0);
			input_sync(wac_i2c->input_dev);
#ifdef CONFIG_INPUT_BOOSTER
			input_booster_send_event(BOOSTER_DEVICE_PEN, BOOSTER_MODE_OFF);
#endif
#ifdef WACOM_USE_SOFTKEY_BLOCK
			if (wac_i2c->pen_pressed) {
				cancel_delayed_work_sync(&wac_i2c->softkey_block_work);
				wac_i2c->block_softkey = true;
				schedule_delayed_work(&wac_i2c->softkey_block_work,
								SOFTKEY_BLOCK_DURATION);
			}
#endif
			input_info(true, &client->dev, "pen is out\n");

		}

		if(!tsp){
			input_info(true, &client->dev, "full scan out (00 00)\n");
			if(wac_i2c->wacom_noise_state != WACOM_NOISE_HIGH) wac_i2c->tsp_noise_mode = set_spen_mode(0);
			else input_info(true, &client->dev, "high noise mode, skip tsp (75 0)\n");
			cancel_delayed_work_sync(&wac_i2c->fullscan_check_work);
			wac_i2c->fullscan_mode = false;
		}	

		wac_i2c->pen_prox = 0;
		wac_i2c->pen_pressed = 0;
		wac_i2c->side_pressed = 0;
	}

	return 0;
}
