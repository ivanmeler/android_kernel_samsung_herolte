/* linux/driver/input/touchscreen/g2tsp.c
*
* Copyright (c) 2009-2016 G2Touch Co., Ltd.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*       
*/

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/byteorder/generic.h>
#include <linux/bitops.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/firmware.h>
#include <linux/sec_sysfs.h>
#include <linux/pinctrl/consumer.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/regulator/consumer.h>
#include <asm/uaccess.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_sysfs.h>
#endif


#include "g2tsp.h"

struct g2tsp_info *current_ts;
const struct firmware *fw_info;

struct g2tsp_trk {
	unsigned touch_id_num:4;
	unsigned sfk_id_num:4;

	unsigned fr_num:4;
	unsigned af1:1;
	unsigned rnd:1;
	unsigned sfkrnd:1;
	unsigned padding:1;

#ifndef USE_COORD_4K
	unsigned y_h:3;
	unsigned x_h:3;
	unsigned area_h:2;
#else
	unsigned y_h:4;
	unsigned x_h:4;
#endif

	u8 x_l;
	u8 y_l;
	u8 area_l;
} __attribute((packed));

struct g2tsp_reg_data {
	unsigned tfnum:4;	
	unsigned sfknum:4; 		//special function key
	struct g2tsp_trk trk[DEF_TRACKING_ID+2];	// 2 is reserved point
} __attribute((packed));

struct g2tsp_G6000_info {
	u8  Vender;
	u8  Model_H;
	u8  Model_L;
	u16 FwVer;
	u16 FwSvnRev;
	u8  IC_Type;
	u8  IC_Ver;
	u8  tsp_type[3];
	u8  Ch_x;
	u8  Ch_y;
	u16 Inch;
	u16 Manufacture;
	u16 DptVer;
	u16 DptRev;
	u16 CorVer;
	u16 CorRev;
}  __attribute((packed)); 



#ifdef G2_SUPPORT_TOUCH_KEY
static struct g2tsp_keys_button g2tsp_keys_table[] = 
{
    {
        .glue_id = 3,
        .code = KEY_MENU,
    },
    {
        .glue_id = 9,
        .code = KEY_BACK,
    },
};
#endif

t_I2cWriteQBuf g_i2cWriteQBuf;
static unsigned char g6000_rev01_init_reg[2][2] = {
	{0xE0, 0x9F},
	{0x00, 0x00},
};

static unsigned char g7400_rev01_init_reg[2][2] = {
	{0xE0, 0x94},
	{0x00, 0x00},
};

#define G2TSP_G6000_REV01_INIT_REG	(sizeof(g6000_rev01_init_reg) / sizeof(g6000_rev01_init_reg[0]))
#define G2TSP_G7400_REV01_INIT_REG	(sizeof(g7400_rev01_init_reg) / sizeof(g7400_rev01_init_reg[0]))

void dbgMsgToCheese(const char *fmt, ...);
static void g2tsp_init_register(struct g2tsp_info *ts);

int g2tsp_FactoryCal(struct g2tsp_info *ts)
{
	int ret = -1;
	int time_out = 0;

	input_info(true, ts->pdev, "%s() Enter + \r\n", __func__);
	dbgMsgToCheese("%s() Enter + \r\n", __func__);

	ts->workMode		= eMode_Factory_Cal;
	ts->factory_flag	= 0;
	ts->calfinish		= 0;

	g2tsp_write_reg(ts, 0x00, 0x10);
	g2tsp_write_reg(ts, 0xce, 0x05);
	g2tsp_write_reg(ts, 0x30, 0x18); 
	g2tsp_write_reg(ts, 0xc6, 0x00);
	udelay(10);
	g2tsp_write_reg(ts, 0x00, 0x00);

	while(1) {
		msleep(10);

		if (ts->factory_flag) {
			g2tsp_dac_write(ts, (u8*)ts->dacBuf);
			ret = 0;
			break;
		}

		if (time_out++ > 1000) {		//wait 10sec
			input_err(true, ts->pdev, "Factory Cal Time Out !! \r\n");
			dbgMsgToCheese("Factory Cal Time Out !! \r\n");
			break;
		}
	}

	ts->workMode = eMode_Normal;
	g2tsp_write_reg(ts, 0x00, 0x10);
	g2tsp_write_reg(ts, 0xce, 0x00);
	g2tsp_write_reg(ts, 0xc6, 0x00);
	g2tsp_write_reg(ts, 0x30, 0x18);
	udelay(10);
	g2tsp_write_reg(ts, 0x00, 0x00);

	return ret;
}

void check_firmware_version(struct g2tsp_info *ts, char *buf)
{
	input_info(true, ts->pdev, "Firmware Version !! \r\n");

	ts->fw_ver_ic[0] = (buf[0]<<16) |(buf[1]<<8) | buf[2];
	ts->fw_ver_ic[1] = (buf[3]<<24) |(buf[4]<<16) | (buf[5]<<8) | buf[6];        

	dbgMsgToCheese("fw_ver ic[0] = %06x, fw_ver ic[1] = %06x\n",
						ts->fw_ver_ic[0],ts->fw_ver_ic[1]);
	if((ts->fw_ver_ic[0] == 0) && (ts->fw_ver_ic[1] == 0)) {
		return;
	}

	if((ts->platform_data->fw_version[0] != ts->fw_ver_ic[0]) 
			|| (ts->platform_data->fw_version[1] != ts->fw_ver_ic[1])) {
		if(ts->firmware_download == 1) {
			ts->fw_ver_ic[0] = 0;
			ts->fw_ver_ic[1] = 0;
		}
	} else {
		input_info(true, ts->pdev, "Version is matched!! -> Ready To Run!!\n");
		cancel_delayed_work(&ts->fw_work);
	}

//    if(ts->workMode == eMode_Normal) 
//	    g2tsp_watchdog_enable(ts, 10000);	
}

static void g2tsp_EventModeProcess(struct g2tsp_info *ts, u8 event, u16 len, u8 *data)
{
	static u8 frame_cnt = 0;
	u8 reg02 = 0x21;

	switch(ts->workMode)
	{
		case eMode_Normal:
			if(event == 0xf5) {
				input_info(true, ts->pdev, "Cal Finished !! \r\n");
//				g2tsp_watchdog_enable(ts, 3000);	
			}
			break;
		case eMode_FW_Update:
			break;
		case eMode_Factory_Cal:
			if(event == 0xf5) {
				ts->calfinish = 1;
				frame_cnt = 0;
				g2tsp_read_reg(ts, 0x02,&reg02);
				g2tsp_write_reg(ts, 0x02, reg02 | 0x10);
				g2tsp_write_reg(ts, 0xC6, 0x04);
			} else if((event == 0xf7) && (ts->calfinish == 1)) {
			if(++frame_cnt > 1) {
				TSPFrameCopytoBuf_swap16(ts->dacBuf, &data[5+42]);
				ts->calfinish = 0;
				frame_cnt = 0;
				ts->factory_flag = 1;			
				}
			}
			break;
		case eMode_FactoryTest:
			if (event == 0xf5) {
				ts->calfinish = 1;
				input_info(true, ts->pdev, "Factory Test Cal Finished !! \r\n");

				if(ts->factory_flag &  TSP_DAC_FLAG) {
					g2tsp_write_reg(ts, 0x02, 0x31);
					g2tsp_write_reg(ts, 0xc6, 0x04);
				} else if (ts->factory_flag &  TSP_ADC_FLAG) {
					g2tsp_write_reg(ts, 0x02, 0x31);
					g2tsp_write_reg(ts, 0xc6, 0x08);
				} else if (ts->factory_flag &  TSP_AREA_FLAG) {
					g2tsp_write_reg(ts, 0x02, 0x21);
					g2tsp_write_reg(ts, 0xc6, 0x08);
				}
			} else if (event == 0xf7) {
				input_info(true, ts->pdev, "factory test --> get 0xF2 \r\n");
				if(((ts->factory_flag & TSP_GET_FRAME_FLAG)==0) && (ts->factory_flag & TSP_FACTORY_FLAG)) {
					TSPFrameCopytoBuf(ts->dacBuf, &data[5+42]);
					ts->factory_flag |= TSP_GET_FRAME_FLAG;				
				}

				if((ts->calfinish == 1) && (ts->factory_flag & (TSP_DAC_FLAG|TSP_AREA_FLAG|TSP_ADC_FLAG))) {
					input_info(true, ts->pdev, "frame_cnt = %d \r\n", frame_cnt);
					if(++frame_cnt > 1) {
						ts->calfinish = 0;
						frame_cnt = 0;
						TSPFrameCopytoBuf(ts->dacBuf, &data[5+42]);
						ts->factory_flag |= TSP_GET_FRAME_FLAG;
					}
				}
			}
			break;
	}
}

void g2tsp_dbg_event(struct g2tsp_info *ts, u8 event, u16 len, u8 *data)
{
	int *jig;
	struct g2tsp_G6000_info *tspInfo;

	switch(event)
	{
		case 0xF1:
			{
				tspInfo = (struct g2tsp_G6000_info *)(&data[5]);
				check_firmware_version(ts,&data[5]);
				input_info(true, ts->pdev, " ###############################################\r\n"); 
				input_info(true, ts->pdev, " Vendor      : %02d \r\n",         tspInfo->Vender);
				input_info(true, ts->pdev, " Model       : %02d.%02d \r\n",    tspInfo->Model_H,tspInfo->Model_L);
				input_info(true, ts->pdev, " FwVer       : %02d.%02d \r\n",    (tspInfo->FwVer >> 8) & 0xFF,tspInfo->FwVer & 0xFF);
				input_info(true, ts->pdev, " FwSvnRev    : %04d \r\n",         SWAP16(tspInfo->FwSvnRev));
				input_info(true, ts->pdev, " IC Type     : %02d \r\n",         tspInfo->IC_Type);
				input_info(true, ts->pdev, " IC Ver      : %02d \r\n",         tspInfo->IC_Ver);
				input_info(true, ts->pdev, " Ch X        : %02d \r\n",         tspInfo->Ch_x);
				input_info(true, ts->pdev, " Ch Y        : %02d \r\n",         tspInfo->Ch_y);
				input_info(true, ts->pdev, " Manufacture : %02d.%02d \r\n",    (tspInfo->Manufacture >> 8) & 0xFF,tspInfo->Manufacture & 0xFF);
				input_info(true, ts->pdev, " DptVer      : %02d,%02d \r\n",    (tspInfo->DptVer >> 8) & 0xFF,tspInfo->DptVer & 0xFF);
				input_info(true, ts->pdev, " DptRev      : %04d \r\n",         SWAP16(tspInfo->DptRev));
				input_info(true, ts->pdev, " CorVer      : %02d.%02d \r\n",    (tspInfo->CorVer >> 8) & 0xFF,tspInfo->CorVer & 0xFF);
				input_info(true, ts->pdev, " CorRev      : %04d \r\n",         SWAP16(tspInfo->CorRev));
				input_info(true, ts->pdev, " ###############################################\r\n"); 
				dbgMsgToCheese("G2Touch Device Version = %06x", G2_DRIVER_VERSION);
			}
			break;
		case 0xc0:		// Jig ID
			memcpy(ts->jigId, &data[5], 16);
			input_info(true, ts->pdev, "JIG ID = 0x%02x%02x%02x%02x \r\n", ts->jigId[0],ts->jigId[1],ts->jigId[2],ts->jigId[3]);
			jig = (int*)ts->jigId;
			dbgMsgToCheese("JIG ID = 0x%08x, 0x%08x, 0x%08x, 0x%08x ", jig[0],jig[1],jig[2],jig[3]);
			break;
		default:
			g2tsp_EventModeProcess(ts, event, len, data);
			break;
	}
}

/* TSP Debug 데이터를 I2C로 받는 부분 - 총 60개 Byte중 49개 사용 가능. */
void MakeI2cDebugBuffer(u8 *dst, u8 *src, u8 fcnt)
{
	u16 i, pos = 0;

	pos = (fcnt * 49);

	for (i = 0 ; i < 10 ; i++) {
		if (i != 1) {
			dst[pos++] = src[(i*6)];
		}

		memcpy(&dst[pos], &src[(i * 6) + 2], 4);
		pos += 4;
	}
}


/* 한번에 모든 데이터를 받지 않고 60Byte  Packet 단위로 받아 들인다. 
    그 중 11Byte는 Read가 제대로 되지 않아 버리고 49개의 Byte만 실제로 사용 가능 하다. */
bool ReadTSPDbgData(struct g2tsp_info *ts, u8 type, u8 *frameBuf) 
{
	bool ret = false;
	u8 buf[61];
	u8 maxFrameCnt, frameCnt = 0;

	g2tsp_read_block_data(ts, G2_REG_TOUCH_BASE + 1, 60, buf);

	maxFrameCnt = buf[1]& 0x1F; //frameCnt (0 ~ 12)
	frameCnt    = buf[7]& 0x1F;

	if (frameCnt > maxFrameCnt) {
		goto err;
	}

	if (type == 0xF2) {
		MakeI2cDebugBuffer(frameBuf,buf,frameCnt);

		if (frameCnt == (maxFrameCnt-1)) {
			u16 len, checkSum=0, i, bufCheckSum;
			len = (frameBuf[3]<<8) | (frameBuf[4]);

			if ((len + 9) >= 2048)
				goto err;

			for (i = 0 ; i < (len + 7) ; i++) {
				checkSum += frameBuf[i];
			}
			bufCheckSum = (frameBuf[len+7] << 8) | (frameBuf[len+8]);

			if(bufCheckSum != checkSum) {
				input_info(true, ts->pdev, "Packet Err: len=%d, checkSum = 0x%04x, bufCheckSum = 0x%02x%02x",
								len+7, checkSum, frameBuf[len+7],frameBuf[len+8]);
				ret = false;
			} else {
				PushDbgPacket(ts, frameBuf,len+7);
				ret = true;
			}
		}
	}

err:
	g2tsp_write_reg(ts, G2_REG_AUX1, ts->auxreg1 | 0x01);

	return ret;
}

void MakeI2cDebugBuffer_0xF9(u8 *dst, u8 *src, u8 fcnt)
{
	u16 i, pos = 0;

	pos = (fcnt * 58/*49*/);

	for (i = 0 ; i < 10 ; i++) {
		if (i == 0) {
			memcpy(&dst[pos] ,&src[(i*6)+2], 4);
			pos += 4;
		} else {
			memcpy(&dst[pos] ,&src[(i*6)+0], 6);
			pos += 6;
		}
	}
}

bool ReadTSPDbgData_0xF9(struct g2tsp_info *ts, u8 type, u8 *frameBuf) 
{
	bool ret = false;
	u8 buf[61];
	u8 maxFrameCnt, frameCnt = 0;

	g2tsp_read_block_data(ts, G2_REG_TOUCH_BASE+1,60, buf); 

	/*
	input_info(true, ts->pdev, "[JHRO]BUF:%02x,%02x,%02x,%02x,%02x,%02x|%02x,%02x,%02x,%02x,%02x,%02x\n"
					, buf[0+0], buf[0+1], buf[0+2], buf[0+3], buf[0+4], buf[0+5]
					, buf[6+0], buf[6+1], buf[6+2], buf[6+3], buf[6+4], buf[6+5]);
	*/

	maxFrameCnt = buf[0] & 0x3F;	//frameCnt (0 ~ 12)
	frameCnt    = buf[1] & 0x3F;

//	input_info(true, ts->pdev, "[JHRO]%s, 0x%x, 0x%x, 0x%x\n", __func__, type, maxFrameCnt, frameCnt);

	if(frameCnt > maxFrameCnt) {
		input_err(true, ts->pdev, "[JHRO]%s, 0x%x > 0x%x\n", __func__, frameCnt, maxFrameCnt);
		goto err;
	}

    if (type == 0xF9) {
		MakeI2cDebugBuffer_0xF9(frameBuf, buf, frameCnt);

		if (frameCnt == (maxFrameCnt - 1)) {
			u16 len, checkSum=0, i, bufCheckSum;
			len = (frameBuf[3]<<8) | (frameBuf[4]);

			//input_info(true, ts->pdev, "[JHRO]%s, frameCnt=%d, len=%d\n", __func__, frameCnt, len);

			if ((len + 9) >= 2048) {
				input_err(true, ts->pdev, "[JHRO]%s, len+9=%d\n", __func__, len+9);
				goto err;
			}

			for (i = 0 ; i < (len + 7) ; i++) {
				checkSum += frameBuf[i];
			}
			bufCheckSum = (frameBuf[len+7] << 8) | (frameBuf[len+8]);

			if(bufCheckSum != checkSum) {
				input_err(true, ts->pdev, "Packet Err2: len=%d, checkSum = 0x%04x, bufCheckSum = 0x%02x%02x",
								len+7, checkSum, frameBuf[len+7],frameBuf[len+8]);
				ret = false;
			} else {
				PushDbgPacket(ts, frameBuf,len+7);
				ret = true;
			}
		}
	}

err:
	g2tsp_write_reg(ts,  G2_REG_AUX1, ts->auxreg1 | 0x01);

	return ret;
}

// ??? case별 의미 확인
static void g2tsp_i2c_event_process(struct g2tsp_info *ts, u8 type)
{
	u8 buf[60];
	static u8 	frameBuf[2048];	
	struct g2tsp_ic_info tspInfo;

	g2tsp_dbg_event(ts, 0xF2, (frameBuf[3]<<8)|(frameBuf[4]), frameBuf);

	switch(type)
	{
		case 0xF1:
			if (g2tsp_read_block_data(ts, G2_REG_TOUCH_BASE,18, ts->tsp_info) >= 0) {      
				memcpy((u8*)&tspInfo, &ts->tsp_info[1],17);
				check_firmware_version(ts,ts->tsp_info);

				input_info(true, ts->pdev, "ver(%02x.%02x.%02x),IC Name(%d), IC Rev(%d), TSP Type(%02x:%02x:%02x),"
										" ChInfo(%02x:%02x), Inch(%04x), Manuf(%04x)\n"
										,tspInfo.fw_ver[0], tspInfo.fw_ver[1], tspInfo.fw_ver[2], tspInfo.ic_name,
										tspInfo.ic_rev, tspInfo.tsp_type[0], tspInfo.tsp_type[1], tspInfo.tsp_type[2],
										tspInfo.ch_x,tspInfo.ch_y, tspInfo.inch,tspInfo.manufacture);

				dbgMsgToCheese("G2Touch Device Version = %06x", G2_DRIVER_VERSION);
			}
			g2tsp_write_reg(ts, G2_REG_AUX1, ts->auxreg1 | 0x01);
			break;

		case 0xF2:
			if(ReadTSPDbgData(ts,type, frameBuf) == true) {
				g2tsp_dbg_event(ts, frameBuf[2], (frameBuf[3]<<8)|(frameBuf[4]), frameBuf);
			}
			break;

		case 0xF3:
			if(g2tsp_read_reg(ts,  G2_REG_TOUCH_BASE+1,buf) >= 0) {
				input_info(true, ts->pdev, " Request Reset CMD buf[0] = 0x%02x", buf[0]);
				ts->platform_data->reset(ts);
				mdelay(10);
				g2tsp_init_register(ts);
			}
			break;
		case 0xF4:
			break;

		case 0xF9:
			if(ReadTSPDbgData_0xF9(ts,type, frameBuf) == true) {
				g2tsp_dbg_event(ts, frameBuf[2], (frameBuf[3]<<8)|(frameBuf[4]), frameBuf);
			}
			break;

		default:
			dbgMsgToCheese(" Default EVENT(%02X)",type);
			break;
		}

}

void SendI2CWriteData(struct g2tsp_info *ts)
{
	u32 pos;

	// have send data.
	while(g_i2cWriteQBuf.head != g_i2cWriteQBuf.tail) 
	{
		pos = ((g_i2cWriteQBuf.tail) & (I2C_WRITE_Q_NUM-1)) * 2;
		g2tsp_write_reg(ts, g_i2cWriteQBuf.data[pos], g_i2cWriteQBuf.data[pos+1]);
		input_info(true, ts->pdev, "i2c_write reg = %02x, data = %02x",
						g_i2cWriteQBuf.data[pos],g_i2cWriteQBuf.data[pos+1]);
		g_i2cWriteQBuf.tail = ((g_i2cWriteQBuf.tail+1) & (I2C_WRITE_Q_NUM-1));
	}
}

void G6000_ReadComplete_Isr(struct g2tsp_info *ts)
{
	int i;
	int ret;

	for (i = 0; i < 5; i++) {
		ret = g2tsp_write_reg(ts, 0x0E, 0x10);

		if(ret >= 0)
		break;
	}

	if(i >= 5)
		dbgMsgToCheese("Trand Done Err!!");
}

static void g2tsp_input_worker(struct g2tsp_info *ts)
{
	struct g2tsp_reg_data rd;
	u8	touchNum,touchID, touchUpDown;
	u8 	sfkNum,sfkID,sfkUpDn;
	u16 x, y, area;
	int ret;
	int i, keycnt;
	int retry_cnt = 5;

	memset((u8 *)&rd, 0, 61);

	for (i = 0 ; i < 5 ; i++) {
		ret = g2tsp_read_block_data(ts, G2_REG_TOUCH_BASE, 60,(u8 *)&rd);
		if (ret >= 0) {
			break;
		}
	}
	if (ret < 0) {
		dbgMsgToCheese("Device I2C FAIL !! ");
		goto end_worker;
	}

	if (rd.sfknum == 0xf) {
		input_info(true, ts->pdev, "rd.sfknum = 0xf end worker ");
		g2tsp_i2c_event_process(ts, *((u8 *)&rd));

		goto end_worker;
	}

	//rd.sfknum = 0;

	if ((rd.tfnum != 0) || (rd.sfknum != 0)) {
		keycnt = (rd.tfnum >= rd.sfknum) ? rd.tfnum : rd.sfknum ;

		// for bring up
//		input_info(true, ts->pdev, "rd.tfnum[0x%x], rd.sfknum[0x%x]", rd.tfnum, rd.sfknum);

#if 0
		// abnormal case????
		if((rd.tfnum != 0) && (rd.sfknum != 0))
			goto end_worker;
#endif

		if (keycnt > DEF_TRACKING_ID) {
			dbgMsgToCheese("key count err !! ");
			goto end_worker;
		}

		if (keycnt) {
			u8	*rdData;
			u8	checkSum = 0, tspCheckSum = 0;

			for (retry_cnt = 0 ; retry_cnt < 5 ; retry_cnt++) {
				ret = g2tsp_read_block_data(ts, G2_REG_TOUCH_BASE + 1, (keycnt * 6), (u8 *)(rd.trk)); 
				if (ret < 0) {
					input_err(true, ts->pdev, "%s : Read IRQ data Error![%d]\n", __func__, retry_cnt);
					continue;
				}

				rdData = (u8 *)&rd;

				for (i = 0 ; i < ((keycnt*6) + 1) ; i++) {
					checkSum += *rdData++;
				}

				ret = g2tsp_read_reg(ts, G2_REG_CHECKSUM, &tspCheckSum);
				if (ret < 0){
					input_err(true, ts->pdev, "%s : Read G2_REG_CHECKSUM Error![%d]\n", __func__, retry_cnt);
					continue;
				}

				if(checkSum == tspCheckSum) {
					break;
				} else {
					input_err(true, ts->pdev, "checkSum Err!! deviceCheckSum = %02x, tspCheckSum = %02x [%d]\n",
									checkSum, tspCheckSum, retry_cnt);
					// ????
					//goto end_worker;
				}
			}

			// ?????
			//g2tsp_write_reg(ts, 0x0E, 0x10); // clear A0

			if ((ret < 0) || (checkSum != tspCheckSum)) {
				input_err(true, ts->pdev, "%s : Device I2C FAIL !!", __func__);
				dbgMsgToCheese("Device I2C FAIL !!");
				goto end_worker;
			}

		}
	} else {
		input_err(true, ts->pdev, "%s : rd.tfnum & rd.sfknum is 0!", __func__);
		goto end_worker;
	}

	if (rd.tfnum) {
		touchNum = rd.tfnum;

		if (touchNum > G2TSP_FINGER_MAX) {
			input_err(true, ts->pdev, "%s : Abnormal touchNum[%d]!", __func__, touchNum);
			goto end_worker;
		}

		for (i = 0 ; i < touchNum ; i++) {
			touchID = rd.trk[i].touch_id_num;
#ifndef USE_COORD_4K
			x = (rd.trk[i].x_h << 8) | rd.trk[i].x_l;
			y = (rd.trk[i].y_h << 8) | rd.trk[i].y_l;
			area = (rd.trk[i].area_h << 8) | rd.trk[i].area_l;
#else
			x = (rd.trk[i].x_h << 8) | rd.trk[i].x_l;
			y = (rd.trk[i].y_h << 8) | rd.trk[i].y_l;
			area = rd.trk[i].area_l;
#endif
			touchUpDown = rd.trk[i].rnd;

			// Read all data
//			input_info(true, ts->pdev, "%s %d ID:%d\t x:%d\t y:%d\t area:%d\t updn:%d\n", __func__, __LINE__, touchID, x, y, area, touchUpDown);

			if ((x > ts->platform_data->res_x) || (y > ts->platform_data->res_y)) {
				dbgMsgToCheese("resolution err!! [%d][%d/%d]", touchID, x, y);
				goto end_worker;
			}

			if (touchID >= G2TSP_FINGER_MAX) {
				dbgMsgToCheese("track_id err!![%d][%d/%d]", touchID, x, y);
				goto end_worker;
			}

			if (touchUpDown) {
				// ??? : why 
				if(ts->prev_Area[touchID] > 0) {
					area = (ts->prev_Area[touchID] * 19 + area) / 20;
				}

				if (area < 1) 
					area = 1;

				ts->prev_Area[touchID] = area;

				input_mt_slot(ts->input, touchID);
				input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);
				input_report_key(ts->input, BTN_TOUCH, 1);
				input_report_key(ts->input, BTN_TOOL_FINGER, 1);

				input_report_abs(ts->input, ABS_MT_POSITION_X, x);
				input_report_abs(ts->input, ABS_MT_POSITION_Y, y);
				input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, area);     
				input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, area);
				input_report_abs(ts->input, ABS_MT_PRESSURE, area);

			}  else {
				input_mt_slot(ts->input, touchID);		// event touch ID
				input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);

				if (ts->total_count > 0 )
					ts->total_count--;

				if (ts->total_count == 0) {
					input_report_key(ts->input, BTN_TOUCH, 0);
					input_report_key(ts->input, BTN_TOOL_FINGER, 0);
				}

				ts->prev_Area[touchID] = 0;
			}

			if (ts->finger[touchID].state == G2TSP_FINGER_RELEASE && touchUpDown == 1) {
				input_info(true, ts->pdev, "[P] ID:%d\t x:%d\t y:%d\t area:%d\t tc:%d\n",
								touchID, x, y, area, ts->total_count);
				ts->finger[touchID].state = G2TSP_FINGER_PRESS;

				ts->finger[touchID].mcount++;
				ts->total_count++;

			} else if (ts->finger[touchID].state == G2TSP_FINGER_PRESS && touchUpDown == 0) {
				input_info(true, ts->pdev, "[R] ID:%d\t x:%d\t y:%d\t area:%d\t mc:%d tc:%d lx:%d ly:%d\n",
								touchID, x, y, area, ts->finger[touchID].mcount,
								ts->total_count, ts->finger[touchID].lx, ts->finger[touchID].ly);
				ts->finger[touchID].state = G2TSP_FINGER_RELEASE;

				ts->finger[touchID].mcount = 0;

			} else if (ts->finger[touchID].state == G2TSP_FINGER_PRESS && touchUpDown == 1) {
				ts->finger[touchID].mcount++;
			} else {
				input_info(true, ts->pdev, "Abnormal touch [ID:%d] [%d]\n",
								touchID, ts->finger[touchID].state);
			}
			ts->finger[touchID].lx = x;
			ts->finger[touchID].ly = y;
		}
#if 0
		// by jhkim 13.12.23
		mod_timer(&ts->timer, jiffies + msecs_to_jiffies(200));
#endif
	}

	// touch key
	if (rd.sfknum) {
		sfkNum = rd.sfknum;

		if (sfkNum > DEF_TRACKING_ID)
			goto end_worker;

		for (i=0;i<sfkNum;i++) {
			sfkID = rd.trk[i].sfk_id_num;
			sfkUpDn= rd.trk[i].sfkrnd;

			for (keycnt = 0 ; keycnt < ts->platform_data->nkeys ; keycnt++) {
				struct g2tsp_keys_button *keys = &ts->platform_data->keys[keycnt];

				if (sfkID == keys->glue_id) {
					input_report_key(ts->input, keys->code, sfkUpDn);
					dbgMsgToCheese("sfk id(%d), upDn(%d)",sfkID, sfkUpDn);
				}
			}
		}
//        g2tsp_watchdog_enable(ts, 1000);
	}
	input_sync(ts->input);

end_worker:
	G6000_ReadComplete_Isr(ts);

	return;
}

#if 0
static void g2tsp_timer_evnet(unsigned long data)
{	
    struct g2tsp_info *ts = (struct g2tsp_info *)data;	
    int i;

    input_info(true, ts->pdev, "%s( Enter+ ) \r\n",__func__);

    // search didn't release event id
    for(i=0; i<DEF_TRACKING_ID; i++)
    {
        if(ts->prev_Area[i] != 0)
        break;
    }

    if(i != DEF_TRACKING_ID)
    {
        // release event id
        for(i=0; i<DEF_TRACKING_ID; i++)
        {
            if(ts->prev_Area[i] == 0) 
            continue;

            input_info(true, ts->pdev, "event not released ID(%d) \r\n",i);
            dbgMsgToCheese("### event not released ID(%d) ### ",i);
            ts->prev_Area[i] = 0;

            input_report_key(ts->input, BTN_TOUCH, 0);
            input_report_key(ts->input, BTN_TOOL_FINGER, 0);

            input_mt_slot(ts->input, i);		
            input_report_abs(ts->input, ABS_MT_TRACKING_ID, -1);
            input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
        }

        input_sync(ts->input);
    }

}
void g2tsp_watchdog_enable(struct g2tsp_info *ts, u32 msec)
{
    //input_info(true, " Run WatchDog Timer !! \r\n");
    del_timer(&ts->watchdog_timer);
    setup_timer(&ts->watchdog_timer, g2tsp_watchdog_timer_evnet, (unsigned long)ts);
    mod_timer(&ts->watchdog_timer, jiffies + msecs_to_jiffies(msec));	
}


static void g2tsp_watchdog_timer_evnet(unsigned long data)
{	
    struct g2tsp_info *ts = (struct g2tsp_info *)data;	

    input_info(true, ts->pdev, "%s( Enter+ )\r\n",__func__);

    queue_work(ts->watchdog_work_queue, &ts->watchdog_work);
}

static void g2tsp_watchdog_work(struct work_struct *work)
{
    struct g2tsp_info 	*ts = container_of(work, struct g2tsp_info, watchdog_work);
    u8 reg_c5;
    static u8 errCnt=0,last_c5 = 0x7f;

    input_info(true, ts->pdev, " %s(Enter +) \r\n", __func__);

    if(ts->platform_data->hw_ver >= 0x00020000) 
    {
        return ;
    }

    disable_irq_nosync(ts->irq);

    if(watchdoc_enable)
    g2tsp_read_reg(ts,  0xC5, &reg_c5);	

    if((reg_c5 == last_c5) && (watchdoc_enable == 1))
    {
        errCnt++;

        if(errCnt >= 2)
        {
            input_info(true, ts->pdev, "TSP sync err !!, reg_c5 = %02x\r\n", reg_c5);
            dbgMsgToCheese("TSP sync err !!, reg_c5 = %02x\r\n", reg_c5);

            errCnt = 0;

            ts->platform_data->reset(ts);
            mdelay(10);
            g2tsp_write_reg(ts,  0x00, 0x10);	
            g2tsp_write_reg(ts,  0xC5, 0x80);	
            g2tsp_write_reg(ts,  0x30, 0x18);	
            udelay(10);
            g2tsp_write_reg(ts,  0x00, 0x00);	
//            g2tsp_watchdog_enable(ts,5000);
        }
        else 
            mod_timer(&ts->watchdog_timer, jiffies + msecs_to_jiffies(100));	
    }
    else 
    {
        errCnt = 0;
        last_c5 = reg_c5;

        mod_timer(&ts->watchdog_timer, jiffies + msecs_to_jiffies(1000));	
    }

    enable_irq(ts->irq);
}
static void g2tsp_interrupt_work(struct work_struct *work)
{
    struct g2tsp_info 	*ts = container_of(work, struct g2tsp_info, irq_work);

    g2tsp_input_worker(ts);
    enable_irq(ts->irq);
}
#endif

static void g2tsp_fw_recovery_work(struct work_struct *work)
{
	struct g2tsp_info 	*ts = container_of(work, struct g2tsp_info, fw_work.work);

	input_info(true, ts->pdev, "%s() Enter + fw_ver_ic %06x, %06x",
					__func__,ts->fw_ver_ic[0],ts->fw_ver_ic[1]);

	if((ts->fw_ver_ic[0] == 0) && (ts->fw_ver_ic[1] == 0)) {
		input_err(true, ts->pdev, "TSP Reset!!");
		g2tsp_write_reg(ts,  0x0, 0x10);
		udelay(5);
		g2tsp_write_reg(ts,  0x0, 0x00);
		mdelay(150);
	}

	if (ts->firmware_download) {
		input_info(true, ts->pdev, "[G2TSP] : Emergency Firmware Update !! \r\n");

		request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, G2TSP_FW_NAME, ts->pdev, GFP_KERNEL, ts, firmware_request_handler);
		ts->firmware_download = 0;
		ts->workMode = eMode_FW_Update;
	}
//	else {
//		input_info(true, ts->pdev, "%s()-> Run WatchDog Timer !! \r\n",__func__);
//		g2tsp_watchdog_enable(ts, 5000);
//	}
	cancel_delayed_work(&ts->fw_work);
}

static irqreturn_t g2tsp_irq_func(int irq, void *handle)
{
	struct g2tsp_info *info = (struct g2tsp_info *)handle;

	if(gpio_get_value(info->platform_data->irq_gpio) == 1) {
		dbgMsgToCheese("%s : IRQ is High! Skip!\n", __func__);
		return IRQ_HANDLED;
	}

	g2tsp_input_worker(info);
	return IRQ_HANDLED;
}

#if 0
static int charge_status = 0;
static void inform_charger_connection(struct tsp_callbacks *_cb, int mode)
{
    u8 reg_95; //,reg_0fh;
    struct g2tsp_info 	*ts = container_of(_cb, struct g2tsp_info, cb);

    dbgMsgToCheese("%s mode = %d \r\n",__func__,mode);
    input_info(true, ts->pdev,"%s mode = %d \r\n",__func__,mode);
    g2tsp_read_reg(ts,  0x95, &reg_95);	  

    charge_status = mode;
    if(mode)
    {
		ts->bus_ops->write(current_ts->bus_ops, 0x95, reg_95 | 0x01);
    }
    else 
    {
		ts->bus_ops->write(current_ts->bus_ops, 0x95, reg_95 & 0xFE);
    }
}


static void inform_earjack_connection(struct tsp_callbacks *_cb, int mode)
{
    u8 reg_95;
    struct g2tsp_info *ts = container_of(_cb, struct g2tsp_info, cb);

    dbgMsgToCheese("%s mode = %d \r\n",__func__,mode);
    input_info(true, ts->pdev,"[G2TSP] : %s  mode = %d \r\n", __func__, mode);
    g2tsp_read_reg(ts,  0x95, &reg_95);	    
    if(mode)
    {
		ts->bus_ops->write(current_ts->bus_ops, 0x95, reg_95 | 0x02);
    }
    else 
    {
		ts->bus_ops->write(current_ts->bus_ops, 0x95, reg_95 & 0xFD);
    }
}
#endif

static void g2tsp_release_all_finger(struct g2tsp_info *ts)
{
	int i;

	for (i = 0; i < G2TSP_FINGER_MAX ; i++) {
		input_mt_slot(ts->input, i);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);

		if (ts->finger[i].state == G2TSP_FINGER_PRESS) {
			input_info(true, ts->pdev, "[RA] ID:%d\t mc:%d tc:%d lx:%d ly:%d\n",
							i, ts->finger[i].mcount, ts->total_count,
							ts->finger[i].lx, ts->finger[i].ly);
		}

		ts->finger[i].state = G2TSP_FINGER_RELEASE;
		ts->finger[i].mcount = 0;

		ts->prev_Area[i] = 0;
	}

	input_report_key(ts->input, BTN_TOUCH, 0);
	input_report_key(ts->input, BTN_TOOL_FINGER, 0);
	input_sync(ts->input);

	ts->total_count = 0;
}

#ifdef CONFIG_PM
static void g2tsp_suspend_register(struct g2tsp_info *ts)
{
	int i;
	u8 data;

	input_info(true, ts->pdev, "%s\n", __func__);

	g2tsp_release_all_finger(ts);

	dbgMsgToCheese("G2TSP SUSPEND !!");

	for (i = 0 ; i < 5 ; i++) {
		data = 0;
		g2tsp_write_reg(ts,  0x00, 0x01);
		g2tsp_read_reg(ts,  0x00,&data);

		if (data & 0x01)
		break;
	}
}

static void g2tsp_resume_register(struct g2tsp_info *ts)
{
    int i;
    u8 data;

    input_info(true, ts->pdev, "%s\n", __func__);

	dbgMsgToCheese("G2TSP G6000_REV01 Resume !!");

	for (i = 0 ; i < 5 ; i++) {
		data = 1;
		g2tsp_write_reg(ts,  0x00, 0x00);

		if (g2tsp_read_reg(ts,  0x00,&data) != 1)
			dbgMsgToCheese("G2TSP Resume fail (%d) !!",i);

		if(data == 0x00)
			break;
	}
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void g2tsp_early_suspend(struct early_suspend *h)
{
	struct g2tsp_info *ts = container_of(h, struct g2tsp_info, early_suspend);

	dbgMsgToCheese("%s", __func__);
	input_info(true, ts->pdev, "%s : ( Enter+++++ )",__func__);

	mutex_lock(&ts->mutex);
	disable_irq_nosync(ts->irq);
	ts->platform_data->suspend(ts->pdev);
	g2tsp_suspend_register(ts);

	ts->suspend = 1;
	cancel_work_sync(&ts->irq_work);
//    del_timer(&ts->watchdog_timer);
	mutex_unlock(&ts->mutex);
}


static void g2tsp_late_resume(struct early_suspend *h)
{
	struct g2tsp_info *ts = container_of(h, struct g2tsp_info, early_suspend);

	dbgMsgToCheese("        %s", __func__);
	input_info(true, ts->pdev, "%s\n", __func__);

	mutex_lock(&ts->mutex);
	ts->platform_data->wakeup(ts->pdev);
	g2tsp_resume_register(ts);
	ts->suspend = 0;

	//    g2tsp_watchdog_enable(ts, 2000);
	enable_irq(ts->irq);
	mutex_unlock(&ts->mutex);
}
#endif
#endif

void g2tsp_reset_register(struct g2tsp_info *ts)
{
	int i;

	input_info(true, ts->pdev, "%s\n", __func__);

	for (i = 0 ; i < G2TSP_G7400_REV01_INIT_REG ; i++) {
		g2tsp_write_reg(ts,  g7400_rev01_init_reg[i][0], g7400_rev01_init_reg[i][1]);
	}

	input_info(true, ts->pdev, "%s ~~~ exit \n", __func__);
}

static void g2tsp_init_register(struct g2tsp_info *ts)
{
//    int ret;
	int i;

	input_info(true, ts->pdev, "%s\n", __func__);

	for (i = 0 ; i < G2TSP_G6000_REV01_INIT_REG ; i++) {
		g2tsp_write_reg(ts,  g6000_rev01_init_reg[i][0], g6000_rev01_init_reg[i][1]);
		input_info(true, ts->pdev, "%s : write [0x%02x]  0x%02x\n", __func__, 
						g6000_rev01_init_reg[i][0], g6000_rev01_init_reg[i][1]);
	}

	input_info(true, ts->pdev, "%s ~~~ exit \n", __func__);
}

/*****************************
G2TSP IOCTL sysfs
******************************/
#define I2C_READ_WORD       0x9010
#define I2C_WRITE_WORD      0x9020
#define I2C_READ_FRAME      0x9040
#define I2C_APK_CMD			0x9060
#define I2C_FLASH_ERASE     0x9080
#define I2C_CHIP_INIT       0x9089
#define I2C_GS1001_CMD      0x908A

#define APK_CMD_SW_RST                      0x01
#define APK_CMD_HW_RST                      0x02
#define APK_CMD_FAC_CAL                     0x03
#define APK_CMD_GET_JIG_ID                  0x08
#define APK_CMD_WD_ON                       0x10
#define APK_CMD_WD_OFF                      0x11
#define APK_CMD_CHIP_REV                    0x20
#define APK_CMD_GEN2_FLASH_WRITE            0x80
#define APK_CMD_GEN2_FLAH_PAGE_ERASE        0x81
#define APK_CMD_GEN2_FLAH_PAGE_READ         0x82


struct reg_data{
	unsigned int addr;
	unsigned int data;
};

struct reg_data_packet {
	unsigned int len;
	u8 buf[1024*4];
};


#if 0
void g2tsp_i2c_to_gpio_enable(void *context, int enable)
{
	struct g2tsp_info *info = (struct g2tsp_info*)context;
	struct g2tsp_platform_data *pdata = info->platform_data;
	int ret;

	input_info(true, info->pdev, "%s: %d\n", __func__, __LINE__);

	if (enable) {
		ret = pinctrl_select_state(pdata->pinctrl_i2c, pdata->pins_i2c_off);
		if (ret < 0)
			input_info(true, info->pdev, "%s: Failed to configure i2c_off pin\n", __func__);
		else
			input_info(true, info->pdev, "%s: configure i2c_off pin\n", __func__);

		ret = gpio_request(pdata->gpio_sda, "i2c7_gpio_sda");
		if (ret)
			input_info(true, info->pdev, "failed to request gpio(i2c_gpio_sda)\n");
		else
			input_info(true, info->pdev, "%s: request gpio_sda\n", __func__);

		ret = gpio_request(pdata->gpio_scl, "i2c7_gpio_scl");
		if (ret)
			input_info(true, info->pdev, "failed to request gpio(i2c_gpio_scl)\n");
		else
			input_info(true, info->pdev, "%s: request gpio_scl\n", __func__);
	} else {
		gpio_free(pdata->gpio_sda);
		gpio_free(pdata->gpio_scl);
		input_info(true, info->pdev, "%s: free gpio_scl, gpio_sda pin\n", __func__);

		ret = pinctrl_select_state(pdata->pinctrl_i2c, pdata->pins_i2c_on);
		if (ret < 0)
			input_info(true, info->pdev, "%s: Failed to configure i2c_on pin\n", __func__);
		else
			input_info(true, info->pdev, "%s: configure i2c_on pin\n", __func__);
	}
}
#endif

#if 1
static int g2tsp_vdd_power(struct g2tsp_info *info, bool on)
{
	struct g2tsp_platform_data *pdata = info->platform_data;
	struct regulator *regulator_vdd = NULL;
	struct regulator *regulator_pullup = NULL;
	int ret = 0;

	if (info->power_status == on)
		return 0;

	input_info(true, info->pdev, "%s: %d\n", __func__, __LINE__);

	regulator_vdd = regulator_get(NULL, pdata->regulator_dvdd);
	if (IS_ERR(regulator_vdd))
		return PTR_ERR(regulator_vdd);
	regulator_pullup = regulator_get(NULL, pdata->regulator_avdd);
	if (IS_ERR(regulator_pullup))
		return PTR_ERR(regulator_pullup);

	if (on) {
		ret = regulator_enable(regulator_vdd);
		if (ret) {
			input_err(true, info->pdev, "%s regulator_enable regulator_vdd error!\n",__func__);
			goto exit;
		}

		ret = regulator_enable(regulator_pullup);
		if (ret) {
			input_err(true, info->pdev, "%s regulator_enable regulator_pullup error!\n",__func__);
			goto exit;
		}
	} else {
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_force_disable(regulator_vdd);

		if (regulator_is_enabled(regulator_pullup))
			regulator_disable(regulator_pullup);
		else
			regulator_force_disable(regulator_pullup);
	}
	regulator_put(regulator_vdd);
	regulator_put(regulator_pullup);

	info->power_status = on;

exit:
	return ret;
}

int g2tsp_hw_power(int on)
{
    struct g2tsp_info *info = current_ts;
    struct g2tsp_platform_data *pdata = info->platform_data;
    int ret = 0;

    struct regulator *regulator_vdd = NULL;
    struct regulator *regulator_pullup = NULL;

    if (info->power_status == on)
    return 0;

    printk(KERN_DEBUG "[TSP] %s %s\n", __func__, on ? "on" : "off");

    input_info(true, info->pdev, "%s: %d\n", __func__, __LINE__);

    regulator_vdd = regulator_get(NULL, pdata->regulator_dvdd);
    if (IS_ERR(regulator_vdd))
    return PTR_ERR(regulator_vdd);
    regulator_pullup = regulator_get(NULL, pdata->regulator_avdd);
    
    if (IS_ERR(regulator_pullup))
    return PTR_ERR(regulator_pullup);

    if (on) 
    {
        ret = regulator_enable(regulator_vdd);
        if (ret) 
        {
            input_err(true, info->pdev, "%s regulator_enable regulator_vdd error!\n", __func__);
            goto exit;
        }

        ret = regulator_enable(regulator_pullup);
        if (ret) 
        {
            input_err(true, info->pdev, "%s regulator_enable regulator_pullup error!\n", __func__);
            goto exit;
        }

        ret = pinctrl_select_state(pdata->pinctrl, pdata->pins_default);
        if (ret < 0)
        	input_err(true, info->pdev, "%s: Failed to configure tsp_attn pin\n", __func__);
    } 
    else 
    {
        if (regulator_is_enabled(regulator_vdd))
        regulator_disable(regulator_vdd);
        else
        regulator_force_disable(regulator_vdd);

        if (regulator_is_enabled(regulator_pullup))
        regulator_disable(regulator_pullup);
        else
        regulator_force_disable(regulator_pullup);

        ret = pinctrl_select_state(pdata->pinctrl, pdata->pins_sleep);
        if (ret < 0)
        	input_err(true, info->pdev, "%s: Failed to configure tsp_attn pin\n", __func__);
    }
    regulator_put(regulator_vdd);
    regulator_put(regulator_pullup);

    info->power_status = on;

    exit:
    return ret;
}

static int g2tsp_power(bool onoff)
{
    if(onoff) 
    {
        g2tsp_hw_power(1);
    } 
    else 
    {
        g2tsp_hw_power(0);
    }
    return 0;
}

static void g2tsp_low_init(void)
{

}

static int g2tsp_reset(void *context)
{
	struct g2tsp_info *ts = (struct g2tsp_info*)context;

	input_info(true, ts->pdev, "%s: %d\n", __func__, __LINE__);

	g2tsp_vdd_power(ts, 0);
	mdelay(5);
	g2tsp_vdd_power(ts, 1);
	mdelay(3);

	// ?????
	g2tsp_write_reg(ts, 0x00, 0x10);
	mdelay(1);
	g2tsp_write_reg(ts, 0x00, 0x01);
	mdelay(1);

	return 0;
}
#endif

struct tsp_callbacks *tsp_callbacks;
void tsp_charger_infom(bool en)
{
    struct g2tsp_info *info = current_ts;
    input_info(true, info->pdev, "%s  earjack=%d \r\n",__func__, en);

    if (tsp_callbacks && tsp_callbacks->inform_charger)
    tsp_callbacks->inform_charger(tsp_callbacks, en);

}

#ifdef USE_OPEN_CLOSE
static int g2_ts_input_open(struct input_dev *dev){
//	struct sec_ts_data *ts = input_get_drvdata(dev);
	struct g2tsp_info *ts = current_ts;
	int i;
	u8 data;

	input_info(true, ts->pdev, "%s called!", __func__);

	mutex_lock(&ts->mutex);
	for(i=0; i<5; i++) 
	{
		data = 1;
		g2tsp_write_reg(ts,  0x00, 0x00);
		g2tsp_read_reg(ts,  0x00, &data);
	
		if(data == 0x00) {
			dbgMsgToCheese("G2TSP Open sucess data= %x try cnt = %d ",data, i); 
			break;
		} else {
			dbgMsgToCheese("G2TSP Open fail data= %x try cnt = %d ",data, i); 
		}
	}

//	del_timer(&ts->watchdog_timer);
	mutex_unlock(&ts->mutex);

	return 0;
}
static void g2_ts_input_close(struct input_dev *dev){
//	struct sec_ts_data *ts = input_get_drvdata(dev);
	struct g2tsp_info *ts = current_ts;
	int i;
	u8 data;

	input_info(true, ts->pdev, "%s called!", __func__);

	mutex_lock(&ts->mutex);
	for (i = 0 ; i < 5; i++) {
		data = 0;
		g2tsp_write_reg(ts,  0x00, 0x01);
		g2tsp_read_reg(ts,  0x00,&data);
	
		if(data == 0x01) {
			dbgMsgToCheese("G2TSP Close sucess data= %x try cnt = %d ",data, i); 
			break;
		} else {
			dbgMsgToCheese("G2TSP Close fail data= %x try cnt = %d ",data, i); 
		}
	}
	mutex_unlock(&ts->mutex);

	g2tsp_release_all_finger(ts);

}
#endif

void tsp_reset_for_i2c(int en)
{
    struct g2tsp_info *ts = current_ts;

    if(en == 1)
    {
        g2tsp_vdd_power(ts, 0);
        mdelay(3);
        g2tsp_vdd_power(ts, 1);
        mdelay(1);
        g2tsp_reset_register(ts);
    }
}

static void g2tsp_register_callback(void *cb)
{
    tsp_callbacks = cb;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void g2tsp_wakeup(void)
{

}

static void g2tsp_suspend(void)
{
// Nothing To Do
}
#else
static void g2tsp_suspend(struct device *dev)
{

    struct g2tsp_info *ts = dev_get_drvdata(dev);

    dbgMsgToCheese("        %s", __func__);
    input_info(true, ts->pdev, "%s( Enter+++++ ) \r\n",__func__);

    mutex_lock(&ts->mutex);

    disable_irq_nosync(ts->irq);

    ts->platform_data->suspend(ts->pdev);

    g2tsp_suspend_register(ts);

    ts->suspend = 1;
    cancel_work_sync(&ts->irq_work);
//    del_timer(&ts->watchdog_timer);

    mutex_unlock(&ts->mutex);
}

static void g2tsp_wakeup(struct device *dev)
{
    struct g2tsp_info *ts = dev_get_drvdata(dev);

    dbgMsgToCheese("        %s", __func__);
    input_info(true, ts->pdev, "%s( Enter+++++ ) \r\n",__func__);

    mutex_lock(&ts->mutex);

    ts->platform_data->wakeup(ts->pdev);
    g2tsp_resume_register(ts);
    ts->suspend = 0;
//    g2tsp_watchdog_enable(ts, 2000);
    enable_irq(ts->irq);
    
    mutex_unlock(&ts->mutex);
}
#endif

#if 0
static void inform_sleep_mode(struct tsp_callbacks *_cb, int mode)
{
    u8 data;
    int i;
    struct g2tsp_info *ts = container_of(_cb, struct g2tsp_info, cb);

    dbgMsgToCheese("%s mode = %d \r\n",__func__,mode);
    input_info(true, ts->pdev,"[G2TSP] : %s  mode = %d \r\n", __func__, mode);

    if(mode)
    {
        for(i=0; i<5; i++) 
        {
            data = 1;
            g2tsp_write_reg(ts,  0x00, 0x00);
            if( g2tsp_read_reg(ts,  0x00,&data) != 1)
            dbgMsgToCheese("G2TSP Resume fail (%d) !!",i);       

            if(data == 0x00)
            break;
        }
    }
    else 
    {
        for(i=0; i<5; i++) 
        {
            data = 0;
            g2tsp_write_reg(ts,  0x00, 0x01);
            if( g2tsp_read_reg(ts,  0x00,&data) != 1)
            dbgMsgToCheese("G2TSP Suspend fail (%d) !!",i);       

            if(data == 0x01)
            break;
        }
    }
}
#endif

#ifdef SEC_TSP_FACTORY_TEST
// For factory app
#include "g2tsp_sec.c"

#endif

void g2tsp_release(void *handle)
{
    struct g2tsp_info *ts = handle;

    input_info(true, ts->pdev, "%s\n", __func__);
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&ts->early_suspend);
#endif

    cancel_work_sync(&ts->irq_work);
    free_irq(ts->irq, ts);

//    sysfs_remove_group(&sec_touchscreen_dev->kobj, &sec_touch_attributes_group);
//    sec_device_destroy(sec_touchscreen_dev->devt);

//    list_del(&ts->cmd_list_head);

//    mutex_destroy(&ts->cmd_lock);

    input_unregister_device(ts->input);
    input_free_device(ts->input);

    if (ts->platform_data->power)
    ts->platform_data->power(0);

    kfree(ts);
}

static int g2tsp_parse_dt(struct i2c_client *client)
{
    struct device *dev = &client->dev;
    struct g2tsp_platform_data *pdata = dev->platform_data;
    struct device_node *np = dev->of_node;
    int retval = 0;

	pdata->init = g2tsp_low_init;
	pdata->reset = g2tsp_reset;
	pdata->wakeup = g2tsp_wakeup;
	pdata->suspend = g2tsp_suspend;
	pdata->register_cb = g2tsp_register_callback;
//	pdata->i2c_to_gpio = g2tsp_i2c_to_gpio_enable;

#ifdef G2_SUPPORT_TOUCH_KEY
    pdata->keys = g2tsp_keys_table;
    pdata->nkeys = ARRAY_SIZE(g2tsp_keys_table);
#endif


	/* madatory */
    if (of_property_read_u32(np, "g2tsp,res_x", &pdata->res_x)) 
    {
        input_err(true, dev, "%s Failed to get res_x property\n", __func__);
        return -EINVAL;
    }
    if (of_property_read_u32(np, "g2tsp,res_y", &pdata->res_y)) 
    {
        input_err(true, dev, "%s Failed to get res_y property\n", __func__);
        return -EINVAL;
    }
    input_info(true, dev, "%s res_x : %d, res_y : %d\n",  __func__, pdata->res_x, pdata->res_y);

    //fw_version0	
    if (of_property_read_u32(np, "g2tsp,fw_version0", &pdata->fw_version[0])) 
    {
        input_err(true, dev, "%s Failed to get fw_version0 property\n", __func__);
    }
    //fw_version1	
    if (of_property_read_u32(np, "g2tsp,fw_version1", &pdata->fw_version[1])) 
    {
        input_err(true, dev, "%s Failed to get fw_version1 property\n", __func__);
    }
    input_info(true, dev, "%s fw_ver0 : 0x%x, fw_ver1 : 0x%x\n", __func__,
					pdata->fw_version[0], pdata->fw_version[1]);

#if 0
    //hw_ver - optional
    if (of_property_read_u32(np, "g2tsp,hw_ver", &pdata->hw_ver)) 
    {
        input_info(true, dev, "%s Failed to get hw_ver property\n", __func__);
    }
    input_info(true, dev, "%s hw_ver : 0x%x\n", __func__, pdata->hw_ver);
#endif

    // irq_gpio
    pdata->irq_gpio = of_get_named_gpio(np, "g2tsp,irq_gpio", 0);
    if (gpio_is_valid(pdata->irq_gpio)) 
    {
        retval = gpio_request_one(pdata->irq_gpio, GPIOF_DIR_IN, "g2tsp,tsp_int");
        if (retval)
        {
            input_err(true, dev, "%s Unable to reqeust gpio[%d]\n", __func__, pdata->irq_gpio);
            return -EINVAL;
        }
    }
    input_info(true, dev, "%s irq_gpio : irq(0x%x)\n", __func__, pdata->irq_gpio);

    // irq_flag
    if (of_property_read_u32(np, "g2tsp,irq_flag", &pdata->irq_flag)) 
    {
        input_err(true, dev, "%s Failed to get irq_flag property\n", __func__);
        return -EINVAL;
    }
    input_info(true, dev, "%s irq_flag : %x\n", __func__,pdata->irq_flag);

    // regulator
    if (of_property_read_string(np, "g2tsp,regulator_dvdd", &pdata->regulator_dvdd)) 
    {
        input_err(true, dev, "%s Failed to get regulator_dvdd name property\n", __func__);
        return -EINVAL;
    }
    if (of_property_read_string(np, "g2tsp,regulator_avdd", &pdata->regulator_avdd)) 
    {
        input_err(true, dev, "%s Failed to get regulator_avdd name property\n", __func__);
        return -EINVAL;
    }
    input_info(true, dev, "%s dvdd : %s, avdd : %s\n", __func__, 
					pdata->regulator_dvdd, pdata->regulator_avdd);
    pdata->power = g2tsp_power;

    // gpio_scl, gpio_sda
    pdata->gpio_scl= of_get_named_gpio(np, "g2tsp,gpio_scl", 0);
    if (!gpio_is_valid(pdata->gpio_scl))
    {
        input_err(true, dev, "%s gpio_scl not valid.\n", __func__);
        return -EINVAL;
    }
    pdata->gpio_sda = of_get_named_gpio(np, "g2tsp,gpio_sda", 0);
    if (!gpio_is_valid(pdata->gpio_sda))
    {
        input_err(true, dev, "%s gpio_scl not valid.\n", __func__);
        return -EINVAL;
    }
	input_info(true, dev, "%s gpio_scl : 0x%x, gpio_sda : 0x%x\n", __func__,
				pdata->gpio_scl, pdata->gpio_sda);

	// pinctrl
	pdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pdata->pinctrl)) 
	{
		input_err(true, dev, "%s could not get pinctrl\n", __func__);
		return PTR_ERR(pdata->pinctrl);
	}

	pdata->pins_default = pinctrl_lookup_state(pdata->pinctrl, "on_state");
	if (IS_ERR(pdata->pins_default))
		input_err(true, dev, "%s could not get default pinstate\n", __func__);

	pdata->pins_sleep = pinctrl_lookup_state(pdata->pinctrl, "off_state");
	if (IS_ERR(pdata->pins_sleep))
		input_err(true, dev ,"%s could not get sleep pinstate\n", __func__);

    // hw i2c
    dev = client->dev.parent->parent;
    pdata->pinctrl_i2c = devm_pinctrl_get(dev);
    if (IS_ERR(pdata->pinctrl_i2c))
	    input_err(true, dev ,"%s could not get pinctrl_i2c\n", __func__);

    pdata->pins_i2c_default = pinctrl_lookup_state(pdata->pinctrl_i2c, "default");
    if (IS_ERR(pdata->pins_i2c_default))
		input_err(true, dev ,"%s could not get i2c_default pinstate\n", __func__);

    pdata->pins_i2c_on = pinctrl_lookup_state(pdata->pinctrl_i2c, "on_i2c");
    if (IS_ERR(pdata->pins_i2c_on))
		input_err(true, dev ,"%s could not get i2c_on pinstate\n", __func__);

    pdata->pins_i2c_off = pinctrl_lookup_state(pdata->pinctrl_i2c, "off_i2c");
    if (IS_ERR(pdata->pins_i2c_off))
		input_err(true, dev ,"%s could not get i2c_off pinstate\n", __func__);

	
	/* optional */
	// tsp fw filename
	  if (of_property_read_string(np, "g2tsp,firmware_name", &pdata->firmware_name)) 
	  {
		  input_info(true, dev, "%s Failed to get firmware_name property\n", __func__);
	  } else {
		  input_info(true, dev, "%s g2tsp,firmware_name: %s\n", __func__, pdata->firmware_name);
	  }

    input_info(true, dev, "%s pinctrl finished.\n", __func__);

    return retval;
}


static void g2tsp_ts_prepare_debugfs(struct g2tsp_info *tsdata, const char *debugfs_name)
{
	tsdata->debug_dir = debugfs_create_dir(debugfs_name, NULL);
	if (!tsdata->debug_dir)
		return;

	input_info(true, tsdata->pdev, "%s         %s \n", __func__, debugfs_name);
}
#if 1
s32 g2tsp_read_reg(struct g2tsp_info *info, u8 addr, u8 *value)
{
    int retval = 0;

    retval = i2c_master_send(info->client, &addr, 1);
    if(retval == -EAGAIN || retval < 0)
    {
        input_err(true, info->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        tsp_reset_for_i2c(1);
    }

    if (retval < 0) 
    {
        input_err(true, info->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        return retval;
    }
    retval = i2c_master_recv(info->client, value, 1);

    return (retval < 0) ? retval : 0;
}

s32 g2tsp_write_reg(struct g2tsp_info *info, u8 addr, u8 value)
{
    u8 data[I2C_SMBUS_BLOCK_MAX+1];
    int retval;

    data[0] = addr;
    data[1] = value;
    retval = i2c_master_send(info->client, data, 2);

    if(retval == -EAGAIN || retval < 0)
    {
        input_err(true, info->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        tsp_reset_for_i2c(1);
    }

    if (retval < 0) 
    {
        input_err(true, info->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        return retval;
    }

    return 0;
}

s32 g2tsp_read_block_data(struct g2tsp_info *info, u8 addr, u8 length, void *values)
{
    int retval = 0;

    retval = i2c_master_send(info->client, &addr, 1);
    if(retval == -EAGAIN || retval < 0)
    {
        input_err(true, info->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        tsp_reset_for_i2c(1);
    }

    if (retval < 0)
    {
        input_err(true, info->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        return retval;
    }
    retval = i2c_master_recv(info->client, values, length);

    return (retval < 0) ? retval : 0;
}

s32 g2tsp_write_block_data(struct g2tsp_info *info, u8 addr, u8 length, const void *values)
{
    u8 data[I2C_SMBUS_BLOCK_MAX+1];
    int num_bytes, count;
    int retval;

    input_info(true, info->pdev, "%s\n", __func__);

    num_bytes = length;
    data[0] = addr;
    count = (num_bytes > I2C_SMBUS_BLOCK_MAX) ? I2C_SMBUS_BLOCK_MAX : num_bytes;
    memcpy(&data[1], values, count+1);
    num_bytes -= count;
    retval = i2c_master_send(info->client, data, count+1);

    if(retval == -EAGAIN || retval < 0)
    {
        input_err(true, info->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        tsp_reset_for_i2c(1);
    }

    if (retval < 0) 
    {
        input_err(true, info->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        printk("%s() send fail !! \n", __func__);	
        return retval;
    }
    while (num_bytes > 0) 
    {
        count = (num_bytes > I2C_SMBUS_BLOCK_MAX) ? I2C_SMBUS_BLOCK_MAX : num_bytes;
        memcpy(&data[0], values, count);
        num_bytes -= count;
        retval = i2c_master_send(info->client, data, count);
        if (retval < 0)
        return retval;
    }
    return 0;
}
#endif

#if 0

static s32 g2tsp_i2c_read_reg(void *handle, u8 addr, u8 *value)
{
    int retval = 0;
    struct g2tsp_i2c *ts = container_of(handle, struct g2tsp_i2c, ops);

    retval = i2c_master_send(ts->client, &addr, 1);
    if(retval == -EAGAIN || retval < 0)
    {
        input_info(true, current_ts->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        tsp_reset_for_i2c(1);
    }

    if (retval < 0) 
    {
        input_info(true, current_ts->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        printk("%s() send Fail addr(%02x) !! \n", __func__, addr);	
        return retval;
    }
    retval = i2c_master_recv(ts->client, value, 1);

    return (retval < 0) ? retval : 0;
}

static s32 g2tsp_i2c_write_reg(void *handle, u8 addr, u8 value)
{
    u8 data[I2C_SMBUS_BLOCK_MAX+1];
    int retval;
    struct g2tsp_i2c *ts = container_of(handle, struct g2tsp_i2c, ops);

    data[0] = addr;
    data[1] = value;
    retval = i2c_master_send(ts->client, data, 2);

    if(retval == -EAGAIN || retval < 0)
    {
        input_info(true, current_ts->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        tsp_reset_for_i2c(1);
    }

    if (retval < 0) 
    {
        input_info(true, current_ts->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        printk("%s() Fail !! \n", __func__);	
        return retval;
    }

    return 0;
}

static s32 g2tsp_i2c_read_block_data(void *handle, u8 addr, u8 length, void *values)
{
    int retval = 0;
    struct g2tsp_i2c *ts = container_of(handle, struct g2tsp_i2c, ops);

    retval = i2c_master_send(ts->client, &addr, 1);
    if(retval == -EAGAIN || retval < 0)
    {
        input_info(true, current_ts->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        tsp_reset_for_i2c(1);
    }

    if (retval < 0)
    {
        input_info(true, current_ts->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        printk("%s() send Fail !! \n", __func__);	
        return retval;
    }
    retval = i2c_master_recv(ts->client, values, length);

    return (retval < 0) ? retval : 0;
}


static s32 g2tsp_i2c_write_block_data(void *handle, u8 addr, u8 length, const void *values)
{
    u8 data[I2C_SMBUS_BLOCK_MAX+1];
    int num_bytes, count;
    int retval;
    struct g2tsp_i2c *ts = container_of(handle, struct g2tsp_i2c, ops);

    input_info(true, &ts->client->dev, "%s\n", __func__);
    num_bytes = length;
    data[0] = addr;
    count = (num_bytes > I2C_SMBUS_BLOCK_MAX) ? I2C_SMBUS_BLOCK_MAX : num_bytes;
    memcpy(&data[1], values, count+1);
    num_bytes -= count;
    retval = i2c_master_send(ts->client, data, count+1);

    if(retval == -EAGAIN || retval < 0)
    {
        input_info(true, current_ts->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        tsp_reset_for_i2c(1);
    }

    if (retval < 0) 
    {
        input_info(true, current_ts->pdev, "%s( I2C Fail +++++ ) \r\n",__func__);
        printk("%s() send fail !! \n", __func__);	
        return retval;
    }
    while (num_bytes > 0) 
    {
        count = (num_bytes > I2C_SMBUS_BLOCK_MAX) ? I2C_SMBUS_BLOCK_MAX : num_bytes;
        memcpy(&data[0], values, count);
        num_bytes -= count;
        retval = i2c_master_send(ts->client, data, count);
        if (retval < 0)
        return retval;
    }
    return 0;
}
#endif


static int g2tsp_i2c_probe(struct i2c_client *client, const struct i2c_device_id *idp)
{
	struct g2tsp_i2c *ts_i2c;
	struct device *pdev = &client->dev;
	struct g2tsp_platform_data *pdata; 
	struct g2tsp_info *ts_info;
	int retval = 0;
	int i;
#ifdef G2_SUPPORT_TOUCH_KEY
	int keycnt;
#endif

	input_info(true, pdev, "%s : (Ver : %02d.%02d.%02d)\n",
			__func__, (G2_DRIVER_VERSION>>16) & 0xFF,
			(G2_DRIVER_VERSION >> 8) & 0xFF, (G2_DRIVER_VERSION) & 0xFF);

#if 1
    /* allocate and clear memory */
    ts_i2c = kzalloc(sizeof(*ts_i2c), GFP_KERNEL);
    if (ts_i2c == NULL) 
    {
        input_err(true, pdev, "%s: Error, kzalloc.\n", __func__);
        retval = -ENOMEM;
        goto error_alloc_data_failed;
    }
#endif

    ts_i2c->client = client;
	i2c_set_clientdata(client, ts_info);

	ts_info = kzalloc(sizeof(*ts_info), GFP_KERNEL);
	if (ts_info == NULL) {
		input_err(true, pdev, "%s: Error, kzalloc\n", __func__);
		goto error_alloc_data_failed;
	}

	ts_info->pdev = &client->dev;
	ts_info->client = client;

	mutex_init(&ts_info->mutex);

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(struct g2tsp_platform_data), GFP_KERNEL);

		input_info(true, pdev, "%s : %s\n", __func__, client->dev.of_node);

		if (!pdata) 
		{
			input_err(true, pdev, "%s Failed to allocate platform data\n", __func__);
			goto error_init;
		}
		client->dev.platform_data = pdata;
		ts_info->platform_data = pdata;

		retval = g2tsp_parse_dt(client);
		if (retval) 
		{
			input_err(true, pdev, "%s: Failed to parse dt!\n", __func__);
			goto error_init;
		}
	} 
	else
	{
		pdata = client->dev.platform_data;
	}

	if (!pdata) 
	{
		input_err(true, pdev, "%s No platform data found\n",__func__);
		goto error_init;
	}

//	ts_info->cb.inform_charger = inform_charger_connection;

	current_ts = ts_info;

	// ??? : why reset? and reset delay????
	if (ts_info->platform_data->power) {
		retval = ts_info->platform_data->power(1);
//		ts_info->platform_data->reset(ts_info);
	}
	if (retval) {
		input_err(true, pdev, "%s: platform power control failed!\n", __func__);
		goto error_init;
	}

	/* Create the input device and register it. */
	ts_info->input = input_allocate_device();
	if (!ts_info->input) {
		retval = -ENOMEM;
		input_err(true, pdev, "%s: Error, failed to allocate input device\n", __func__);
		goto error_input_allocate_device;
	}

//	  ts->input = input_device;
	ts_info->input->name = "sec_touchscreen";
	ts_info->input->phys	= "s3c-g2touch/input0";
	ts_info->input->id.bustype	= BUS_I2C;
	ts_info->input->id.vendor	= 0x16B4;
	ts_info->input->id.product	= 0x0702;
	ts_info->input->id.version	= 0x0001;

	ts_info->input->dev.parent = &client->dev;
#ifdef USE_OPEN_CLOSE
	ts_info->input->open = g2_ts_input_open;
	ts_info->input->close = g2_ts_input_close;
#endif

	/* enable interrupts */
	ts_info->irq = gpio_to_irq(ts_info->platform_data->irq_gpio);
	if (retval < 0) {
		input_err(true, pdev, "%s: IRQ request failed r=%d\n", __func__, retval);
		goto error_set_irq;
	}

	set_bit(EV_SYN, ts_info->input->evbit);
	set_bit(EV_KEY, ts_info->input->evbit);
	set_bit(EV_ABS, ts_info->input->evbit);
	set_bit(BTN_TOUCH, ts_info->input->keybit);
	set_bit(BTN_TOOL_FINGER, ts_info->input->keybit);

	set_bit(EV_ABS, ts_info->input->evbit);
	set_bit(INPUT_PROP_DIRECT, ts_info->input->propbit);

#ifdef G2_SUPPORT_TOUCH_KEY
	for (keycnt = 0 ; keycnt < ts_info->platform_data->nkeys ; keycnt++) {
		struct g2tsp_keys_button *keys = &ts_info->platform_data->keys[keycnt];
		set_bit(keys->code & KEY_MAX, ts_info->input->keybit);
	}
#endif

	// prev_Area ?????
	memset(ts_info->prev_Area, 0, sizeof(ts_info->prev_Area));

	input_mt_init_slots(ts_info->input, G2TSP_FINGER_MAX, INPUT_MT_DIRECT);
	input_set_abs_params(ts_info->input, ABS_MT_POSITION_X, 0, ts_info->platform_data->res_x, 0, 0);
	input_set_abs_params(ts_info->input, ABS_MT_POSITION_Y, 0, ts_info->platform_data->res_y, 0, 0);
	input_set_abs_params(ts_info->input, ABS_MT_TOUCH_MAJOR, 1, 255, 0, 0);
	input_set_abs_params(ts_info->input, ABS_MT_WIDTH_MAJOR, 1, 255, 0, 0);
	input_set_abs_params(ts_info->input, ABS_MT_PRESSURE, 1, 255, 0, 0);
//	input_set_abs_params(ts_info->input, ABS_MT_TRACKING_ID, 0, DEF_TRACKING_ID, 0, 0);

	//init dbgPacket // ???
	ts_info->dbgPacket.head = 0;
	ts_info->dbgPacket.tail = 0;
	memset(ts_info->jigId, 0, sizeof(ts_info->jigId));

	// options check
	ts_info->firmware_download	= 0;
	ts_info->calfinish			= 0;
	ts_info->factory_cal		= 0;
	ts_info->workMode			= eMode_Normal;
	ts_info->factory_flag		= 0;

	input_info(true, ts_info->pdev, " Init flag = 0x%08x \r\n", ts_info->platform_data->options);


	for (i = 0; i < G2TSP_FINGER_MAX; i++) {
		ts_info->finger[i].state = G2TSP_FINGER_RELEASE;
		ts_info->finger[i].mcount = 0;
	}


	if (ts_info->platform_data->options & G2_FWDOWN)
		ts_info->firmware_download = 1;

	retval = input_register_device(ts_info->input);
	if (retval) 
	{
		input_err(true, pdev, "%s: Error, failed to register input device\n", __func__);
		goto error_input_register_device;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts_info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts_info->early_suspend.suspend = g2tsp_early_suspend;
	ts_info->early_suspend.resume = g2tsp_late_resume;
	register_early_suspend(&ts_info->early_suspend);
#endif

	// AUX1 mean???
	g2tsp_read_reg(ts_info, G2_REG_AUX1, &ts_info->auxreg1);

	// initialize version info.
	ts_info->fw_ver_ic[0] = 0;
	ts_info->fw_ver_ic[1] = 0;

	// 부팅 중 FW update logic make and add
	INIT_DEFERRABLE_WORK(&ts_info->fw_work, g2tsp_fw_recovery_work);

	memset(&g_i2cWriteQBuf, 0, sizeof(g_i2cWriteQBuf));

//	INIT_WORK(&ts_info->irq_work, g2tsp_interrupt_work);
//	ts_info->work_queue = create_singlethread_workqueue("isr_work_queue");

//	INIT_WORK(&ts_info->watchdog_work, g2tsp_watchdog_work);
//	ts_info->watchdog_work_queue = create_singlethread_workqueue("watchdog_work_queue");


	// ?? 좌표 박힘 ?
//	setup_timer(&ts_info->timer, g2tsp_timer_evnet, (unsigned long)ts_info);
//	setup_timer(&ts_info->watchdog_timer, g2tsp_watchdog_timer_evnet, (unsigned long)ts_info);

	retval = request_threaded_irq(ts_info->irq, NULL, g2tsp_irq_func, 
						ts_info->platform_data->irq_flag, ts_info->input->name, ts_info);
	if (retval < 0) {
		input_err(true, pdev, "%s: IRQ request failed r=%d\n", __func__, retval);
		goto error_set_irq;
	}

	g2tsp_init_register(ts_info);

#ifdef SEC_TSP_FACTORY_TEST
	retval = sec_cmd_init(&ts_info->sec, g2tsp_commands,
			ARRAY_SIZE(g2tsp_commands), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		input_err(true, &ts_info->client->dev, "%s: Failed to sec_cmd_init\n", __func__);
		retval = -ENODEV;
//		goto err_sec_cmd; // jh32.park
	}

#if 0
	retval = sysfs_create_group(&ts_info->sec.fac_dev->kobj,
				 &sec_touch_factory_attr_group);
	if (retval < 0) {
		input_err(true, &ts_info->client->dev, "FTS Failed to create sysfs group\n");
//		goto err_sysfs;
	}
#endif

	retval = sysfs_create_link(&ts_info->sec.fac_dev->kobj,
				&ts_info->input->dev.kobj, "input");

	if (retval < 0) {
		input_err(true, &ts_info->client->dev,
				"%s: Failed to create link\n", __func__);
//		goto err_sysfs;
	}

#else	// OLD
	INIT_LIST_HEAD(&ts_info->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
	list_add_tail(&tsp_cmds[i].list, &ts_info->cmd_list_head);

	mutex_init(&ts_info->cmd_lock);
	ts_info->cmd_is_running = false;

	sec_touchscreen_dev = sec_device_create(ts_info, "tsp");
	if (IS_ERR(sec_touchscreen_dev))
	input_info(true, ts_info->pdev, "Failed to create device for the sysfs\n");

	dev_set_drvdata(sec_touchscreen_dev, ts_info);

	retval = sysfs_create_group(&sec_touchscreen_dev->kobj, &sec_touch_attributes_group);
	if (retval)
	input_info(true, ts_info->pdev, "Failed to create sysfs .\n");	


	retval = sysfs_create_link(&sec_touchscreen_dev->kobj, &ts_info->input->dev.kobj, "input");

	if (retval < 0) {
		input_info(true, ts_info->pdev, "%s: Failed to create link\n", __func__);
	}
#endif

	g2tsp_ts_prepare_debugfs(ts_info, dev_driver_string(&client->dev));
	input_info(true, ts_info->pdev, "G2TSP : %s() Exit !!", __func__);
	return 0;

	error_input_register_device:
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts_info->early_suspend);
#endif

	input_unregister_device(ts_info->input);

	if (ts_info->irq >= 0)
	free_irq(ts_info->irq, ts_info);

	error_set_irq:
	input_free_device(ts_info->input);
	error_input_allocate_device:
	if (ts_info->platform_data->power)
	ts_info->platform_data->power(0);

//	del_timer_sync(&ts_info->watchdog_timer);
	error_init:
	kfree(ts_info);
	error_alloc_data_failed:
    kfree(ts_i2c);

    return retval;
}



static void g2tsp_i2c_shutdown(struct i2c_client *client)
{
//todo
}

static int g2tsp_i2c_remove(struct i2c_client *client)
{
#if 0
	struct g2tsp_i2c *ts;

    ts = i2c_get_clientdata(client);
    g2tsp_release(ts->g2tsp_handle);

    kfree(ts);
#endif
	//todo

    return 0;
}

static const struct i2c_device_id g2tsp_i2c_id[] = 
{
	{ G2TSP_I2C_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static struct of_device_id g2tsp_match_table[] = 
{
    { .compatible = "g2tsp,g2touch",},
    { },
};
#else
#define g2tsp_match_table NULL
#endif

static struct i2c_driver g2tsp_i2c_driver = 
{
    .driver = {
	    .name = G2TSP_I2C_NAME,
	    .owner = THIS_MODULE,
#ifdef CONFIG_OF
	    .of_match_table = g2tsp_match_table,
#endif
    },
    .probe = g2tsp_i2c_probe,
    .remove = g2tsp_i2c_remove,
    .shutdown = g2tsp_i2c_shutdown,
    .id_table = g2tsp_i2c_id,
};

static int __init g2tsp_driver_init(void)
{
    return i2c_add_driver(&g2tsp_i2c_driver);
}

static void __exit g2tsp_driver_exit(void)
{
    return i2c_del_driver(&g2tsp_i2c_driver);
}

module_init(g2tsp_driver_init);
module_exit(g2tsp_driver_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("G2touch touchscreen device driver");
MODULE_AUTHOR("G2TOUCH");
