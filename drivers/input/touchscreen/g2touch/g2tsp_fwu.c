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

#include "g2tsp.h"

extern struct g2tsp_info *current_ts;
extern void g2tsp_dbg_event(struct g2tsp_info *ts, u8 event, u16 len, u8 *data);

/*	referece dac --> it can be changed */
const u16 Refence_dac[390] =
{
	479,429,448,448,455,439,429,412,402,374,376,364,358,341,325,
	413,423,440,454,442,432,414,405,390,378,366,359,347,337,320,
	417,428,453,453,453,434,425,408,399,377,375,362,357,340,330,
	423,443,458,477,461,450,432,423,406,395,381,375,362,353,338,
	438,453,476,483,479,459,448,431,422,398,395,382,376,361,350,
	450,474,488,509,490,477,458,448,432,419,405,398,385,377,361,
	468,486,509,516,510,488,477,459,450,424,421,407,401,388,374,
	481,507,522,543,521,508,487,477,459,448,431,424,411,404,386,
	471,522,514,551,526,520,502,488,471,455,449,434,427,414,400,
	516,545,562,581,557,543,520,510,490,480,460,452,438,432,415,
	540,560,589,591,584,556,544,523,513,488,479,464,456,441,430,
	509,588,572,625,579,582,552,547,515,515,492,484,467,462,447,
	585,607,639,637,628,598,586,562,550,525,516,496,489,473,464,
	604,641,659,676,645,629,600,589,564,556,531,521,502,497,484,
	583,660,657,691,657,647,626,607,582,568,559,535,528,511,503,
	658,701,720,735,699,684,648,639,610,602,576,564,543,538,526,
	698,723,766,753,739,703,687,656,644,617,607,577,570,550,545,
	654,766,740,800,729,742,696,692,644,652,620,605,579,576,565,
	757,784,833,815,796,752,737,700,688,659,648,613,606,583,582,
	835,886,925,922,881,849,816,788,760,742,714,691,669,655,669,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,346,  0,  0,  0,  0,  0,263,  0,  0,  0,  0,  0
};

void g2tsp_flash_eraseall(struct g2tsp_info *ts, u8 cmd)
{
	int i;

	input_info(true, ts->pdev, "%s(Enter) %d\n", __func__, __LINE__);

	// reset
	ts->platform_data->reset(ts);

	input_info(true, ts->pdev, "%s: %d\r\n", __func__, __LINE__);

	mdelay(50);
	g2tsp_write_reg(ts, 0x00, 0x10);

	// ok...
	g2tsp_write_reg(ts, 0x01, 0x01);
	g2tsp_write_reg(ts, 0x00, 0x00);
	mdelay(50);
	g2tsp_write_reg(ts, 0xcc, cmd);   //partial Erase
	g2tsp_write_reg(ts, 0xc7, 0x98);

	input_info(true, ts->pdev, "%s: %d\r\n", __func__, __LINE__);
	udelay(1);

	input_info(true, ts->pdev, "%s: %d\r\n", __func__, __LINE__);
	for (i = 0 ; i < 1000 ; i++) {
		g2tsp_write_reg(ts, 0xc7, 0xc8);
		g2tsp_write_reg(ts, 0xc7, 0xd8);
	}

	for (i = 0 ; i < 8 ; i++) {
		g2tsp_write_reg(ts, 0xc7, 0xcc);
		g2tsp_write_reg(ts, 0xc7, 0xdc);
	}

	input_info(true, ts->pdev, "%s: %d\r\n", __func__, __LINE__);
	// BIST Command Start
	for (i = 0 ; i < 7 ; i++) {
		g2tsp_write_reg(ts, 0xc7, 0xc5);
		g2tsp_write_reg(ts, 0xc7, 0xd5);
	}

	input_info(true, ts->pdev, "%s: %d\r\n", __func__, __LINE__);
	for (i = 0 ; i < 40 ; i++) {
		g2tsp_write_reg(ts, 0xc7, 0xc4);
		g2tsp_write_reg(ts, 0xc7, 0xd4);
	}

	input_info(true, ts->pdev, "%s: %d\r\n", __func__, __LINE__);
	for (i = 0 ; i < 4 ; i++) {
		g2tsp_write_reg(ts, 0xc7, 0xc5);
		g2tsp_write_reg(ts, 0xc7, 0xd5);
	}

	input_info(true, ts->pdev, "%s: %d\r\n", __func__, __LINE__);
	g2tsp_write_reg(ts, 0xc7, 0xc4);
	g2tsp_write_reg(ts, 0xc7, 0xd4);
	g2tsp_write_reg(ts, 0xc7, 0xc5);
	g2tsp_write_reg(ts, 0xc7, 0xd5);

	input_info(true, ts->pdev, "%s: %d\r\n", __func__, __LINE__);
	for (i = 0 ; i < 6 ; i++) {
		g2tsp_write_reg(ts, 0xc7, 0xc4);
		g2tsp_write_reg(ts, 0xc7, 0xd4);
	}

	input_info(true, ts->pdev, "%s: %d\r\n", __func__, __LINE__);
	// BIST Command End

	// Internal TCK set to SCL
	g2tsp_write_reg(ts, 0xc7, 0xcc);
	g2tsp_write_reg(ts, 0xc7, 0xdc);
	g2tsp_write_reg(ts, 0xc7, 0xfc);

	input_info(true, ts->pdev, "%s: %d\r\n", __func__, __LINE__);

	// Internal Flash Erase Operation End
	// SCL is returned to I2C, Mode Desable.
	g2tsp_write_reg(ts, 0xc7, 0x88);
	g2tsp_write_reg(ts, 0xc7, 0x00);

	input_info(true, ts->pdev, "%s: %d\r\n", __func__, __LINE__);
	// remark 2012.11.05
	udelay(10);
	g2tsp_write_reg(ts, 0xcc, 0x00);

	input_info(true, ts->pdev, "%s: %d\r\n", __func__, __LINE__);

	mdelay(50);

	input_info(true, ts->pdev, " FLASH ERASE Complete !!\n");
}

void g2tsp_firmware_load(struct g2tsp_info	*ts, const unsigned char *data, size_t size)
{
//	int	i;
	int latest_version[0];

	input_info(true, ts->pdev, "%s %d (size : %d)", __func__,__LINE__, (int)size);

	disable_irq_nosync(ts->irq);
//	del_timer(&ts->watchdog_timer);

	latest_version[0] = ts->platform_data->fw_version[0];
	latest_version[1] = ts->platform_data->fw_version[1];

	input_info(true, ts->pdev, "fw ver ic [0x%06x.%06x], fw ver bin [0x%06x.%06x]\n",
					ts->fw_ver_ic[0], ts->fw_ver_ic[1],
					latest_version[0], latest_version[1]);

	dbgMsgToCheese("Curent device version = 0x%06x.%06x, latest version = 0x%06x.%06x \r\n",
					ts->fw_ver_ic[0], ts->fw_ver_ic[1],
					latest_version[0],latest_version[1]);

	if((ts->fw_ver_ic[0] != latest_version[0]) || (ts->fw_ver_ic[1] != latest_version[1])) {
		input_info(true, ts->pdev, "G2TOUCH: Firmware Update Start !!\r\n");
		dbgMsgToCheese("G2TOUCH: Firmware Update Start !!\r\n");
#if 1
		G7400_firmware_update(ts, data, size);
#else
		// erase flash
		g2tsp_flash_eraseall(ts, 0x80);

		// write binary
		g2tsp_write_reg(ts, 0xa1, 0x00);
		g2tsp_write_reg(ts, 0xa2, 0x00);
		g2tsp_write_reg(ts, 0xa3, 0x00);
		g2tsp_write_reg(ts, 0xa0, 0x01);

		for (i = 0 ; i < size ; i++) {
			if ((i >= 0xF0) && (i <= 0xFF)) {
				g2tsp_write_reg(ts, 0xa4,(ts->jigId[i&0x0f] & 0xff));
			} else {
				g2tsp_write_reg(ts, 0xa4, (data[i] & 0xff));
			}
			g2tsp_write_reg(ts, 0xa5, 0x01);
			g2tsp_write_reg(ts, 0xa5, 0x00);
		}

		g2tsp_write_reg(ts, 0xa4, 0x00);
		g2tsp_write_reg(ts, 0xa5, 0x01);
		g2tsp_write_reg(ts, 0xa0, 0x00);
		g2tsp_write_reg(ts, 0xa5, 0x00);

		msleep(10);
		g2tsp_write_reg(ts, 0x00, 0x10);	//soft reset On	
		udelay(10);
		g2tsp_write_reg(ts, 0x01, 0x00);	//Flash Enable	
		g2tsp_write_reg(ts, 0x30, 0x18);	//PowerUp Calibration.

		ts->workMode = eMode_Normal;

		g2tsp_write_reg(ts, 0x00, 0x00);	//soft reset Off	
		msleep(10);

		input_info(true, ts->pdev, "written 0x%x Bytes finished. \r\n", i);
		input_info(true, ts->pdev, "Firmware Download Completed  \r\n");
#endif

		dbgMsgToCheese("Firmware Download Completed  \r\n");

//        if(ts->factory_cal == 0) 
//        {
//            input_info(true, ts->pdev, "%s()-> Run WatchDog Timer !! \r\n", __func__);
//            g2tsp_watchdog_enable(ts,5000);
//        }
	} else {
		input_info(true, ts->pdev, "current version is the latest version !! \r\n");
	}

	enable_irq(ts->irq);
	//ts->firmware_download = 0;
    return;
}

void g2tsp_dac_write(struct g2tsp_info *ts, u8 *data)
{
	u32 addr = 0x013000;
	int i=0,j=0;

	// erase flash (dac area)
	input_info(true, ts->pdev, "%s: Enter + \r\n", __func__);
	dbgMsgToCheese("%s: Enter + ", __func__);

	for(i=0; i<390; i++) {
		printk("[%04d]", (data[(i*2)]<<8) | data[(i*2)+1]);
		if ((i % 15) == 14 )
			printk("\r\n");
	}

	g2tsp_flash_eraseall(ts, 0xb3);

	// Flash Dac address write
	g2tsp_write_reg(ts, 0xA1, (addr>>16) & 0xff);
	g2tsp_write_reg(ts, 0xA2, (addr>>8) & 0xff);
	g2tsp_write_reg(ts, 0xA3, addr & 0xff);
	g2tsp_write_reg(ts, 0xA0, 0x01);

	// Dac data write
	for (j = 0 ; j < 26 ; j++) {
		for (i = 0 ; i < 30 ; i++) {

			g2tsp_write_reg(ts, 0xA4, data[(j*30)+i]);
			g2tsp_write_reg(ts, 0xA5, 0x01);
			g2tsp_write_reg(ts, 0xA5, 0x00);

			// Write Dummy Data
			if (i == 29) {
				g2tsp_write_reg(ts, 0xA4, 0);
				g2tsp_write_reg(ts, 0xA5, 0x01);
				g2tsp_write_reg(ts, 0xA5, 0x00);
				g2tsp_write_reg(ts, 0xA4, 0);
				g2tsp_write_reg(ts, 0xA5, 0x01);
				g2tsp_write_reg(ts, 0xA5, 0x00);
			}
		}
	}

    // Dac Write finish
    g2tsp_write_reg(ts, 0xA4, 0x00);
    g2tsp_write_reg(ts, 0xA5, 0x00);
    g2tsp_write_reg(ts, 0xA0, 0x00);
    g2tsp_write_reg(ts, 0xA5, 0x00);

    g2tsp_write_reg(ts, 0x01, 0x00);
    g2tsp_write_reg(ts, 0x00, 0x10);
    udelay(10);
    g2tsp_write_reg(ts, 0xce, 0x00);	// disable debug mode
    g2tsp_write_reg(ts, 0xc6, 0x00);	// debug reset
    g2tsp_write_reg(ts, 0x30, 0x18);	// powerup-cal enable
    g2tsp_write_reg(ts, 0x00, 0x00);	// standby off--> tsp run

    ts->workMode = eMode_Normal;

    input_info(true, ts->pdev, "Factory Cal Data Write Finished !! \r\n");
    dbgMsgToCheese("Factory Cal Data Write Finished !! \r\n");
}

void firmware_request_handler(const struct firmware *fw, void *context)
{
	struct g2tsp_info *ts;
	ts = (struct g2tsp_info*) context;

	if (fw != NULL) {
		g2tsp_firmware_load(ts, fw->data, fw->size);
		release_firmware(fw);

		if(ts->factory_cal == 1) {
			g2tsp_FactoryCal(ts);
		}
	} else {
	    input_info(true, ts->pdev, "failed to load G2Touch firmware will not working\n");
	}
}

void PushDbgPacket(struct g2tsp_info *ts, u8 *src, u16 len)
{
	int i,pos;
	u16 head;

	head = ts->dbgPacket.head;

	for (i = 0 ; i < len ; i++) {
		pos = (head + i) & (DBG_PACKET_SIZE-1);
		ts->dbgPacket.buf[pos]= src[i];
	}

	ts->dbgPacket.head = (head + len) & (DBG_PACKET_SIZE-1);  
	g2debug1(ts->pdev, "%s: head = %d, cmd=0x%02x, len=%d \r\n",
						__func__, head, src[2], len);
}

int PopDbgPacket(struct g2tsp_info *ts, u8 *src)
{
	int len = 0;
	u16 tail,head;
	u16 ntail;

	tail = ts->dbgPacket.tail;
	head = ts->dbgPacket.head;

	if (tail == head) 
		return 0;

	//search STX --> 0x02, 0xA3
	while(1)	    
	{
		if(tail == head) 
			break;

		ntail = (tail +1) & (DBG_PACKET_SIZE-1);
		if ((ts->dbgPacket.buf[tail] == 0x02) && (ts->dbgPacket.buf[ntail] == 0xA3))    //Get STX Word
			break;

		tail = (tail+1) & (DBG_PACKET_SIZE-1);
	}

	//Search ETX -->0x03,0xB3
	while(1)
	{
		if((tail == head) || (len >= 1204))	{
			len = 0;
			break;
		}

		src[len++] = ts->dbgPacket.buf[tail];
		ntail = (tail +1) & (DBG_PACKET_SIZE-1);

		if((ts->dbgPacket.buf[tail] == 0x03) && (ts->dbgPacket.buf[ntail] == 0xB3)) {
			src[len++] = 0xB3;
			tail = ntail;
			tail = (tail+1) & (DBG_PACKET_SIZE-1);
			break;
		}
		tail = (tail+1) & (DBG_PACKET_SIZE-1);
	}

	ts->dbgPacket.tail = tail;
	g2debug1(ts->pdev, "%s: tail = %d, cmd=0x%02x, len=%d\r\n",
						__func__, tail, src[2], len);

	return len;
}


/* send prink to Cheese */
#if 1 //bringup_porting
void dbgMsgToCheese(const char *fmt, ...)
{
	static u8 print_buf[128];
	u8 i = 0, len = 0;
	u16 checkSum = 0;
	va_list va;

	print_buf[0] = 0x02;
	print_buf[1] = 0xA3;
	print_buf[2] = 0x61;

	va_start(va, fmt);
	len = vsprintf(&print_buf[5], fmt, va);
	va_end(va);

	print_buf[3] = (len)>>8;
	print_buf[4] = (len) & 0xFF;
	print_buf[len+5] = 0x03;
	print_buf[len+6] = 0xB3;

	for (i = 0 ; i < len + 7 ; i++) {
		checkSum += print_buf[i];
	}

	PushDbgPacket(current_ts, print_buf, len+7);
}
#else
void dbgMsgToCheese(const char *fmt, ...)
{
}
#endif //bringup_porting

void TSPFrameCopytoBuf(u16 *dacBuf, u8* data)
{
	int i;

	for (i = 0 ; i < 390 ; i++) {
		dacBuf[i] = (data[(i*2)]<<8) | data[(i*2)+1];
	}	
}

void TSPFrameCopytoBuf_swap16(u16 *dacBuf, u8* data)
{
	int i;

	for (i = 0 ; i < 390 ; i++) {
		dacBuf[i] = (data[(i*2)+1]<<8) | data[(i*2)];
	}	
}

#ifdef FEATURE_SHRIMP_G7400
#define G7400_BUF_SIZE 128

void G7400_Rev00_I2C_Write(t_I2cWriteQBuf *i2cWriteQBuf, u8 addr, u8 data)
{
	u32 pos;
	pos = (i2cWriteQBuf->head & (I2C_WRITE_Q_NUM-1)) * 2;
	i2cWriteQBuf->data[pos]   = addr;     //cmd
	i2cWriteQBuf->data[pos+1] = data;      // reg
	i2cWriteQBuf->head = ((i2cWriteQBuf->head+1) & (I2C_WRITE_Q_NUM-1));
}

int G7400_flash_erase_all(struct g2tsp_info *ts)
{
	int ret = 0;

	input_info(true, ts->pdev, "%s() enter + \r\n",__func__);

	ts->platform_data->reset(ts);
	udelay(100);
	g2tsp_write_reg(ts, 0x01, 0x01);         // Set Flash Acces Mode
	g2tsp_write_reg(ts, 0x00, 0x00);         // Set Standby mode off
	udelay(100);
	g2tsp_write_reg(ts, 0x80, 0x00);         //eFlash INF=0
	g2tsp_write_reg(ts, 0x40, 0x00);         // Set Sector Address 
	g2tsp_write_reg(ts, 0x41, 0x00);         // Set Page Address
	g2tsp_write_reg(ts, 0x42, 0x00);         // Set Column Address
	g2tsp_write_reg(ts, 0x43, 0x03);         // Set Function Type => Mass erase
	g2tsp_write_reg(ts, 0x44, 0x01);         // Set Enabler
	mdelay(30);
	g2tsp_write_reg(ts, 0x44, 0x00);         // Disable Enabler

	input_info(true, ts->pdev, "%s() exit + \r\n", __func__);
	return ret;
}

int G7400_flash_page_erase(struct g2tsp_info *ts, unsigned short page)
{
	int ret = 0;
	int sector_addr, page_addr;

	g2debug1(ts->pdev, "%s(%d) Enter + \r\n", __func__, page);

	sector_addr = ((page/128)/32)&0x0F;
	page_addr = ((page/128)%32)&0x1F;
#if 1
	g2tsp_write_reg(ts, 0x00, 0x10);		// Set Flash Acces Mode
	udelay(100);
	g2tsp_write_reg(ts, 0x00, 0x01);		// Set Standby mode off
	udelay(100);
#else
	ts->platform_data->reset(ts);
	udelay(100);
#endif
	g2tsp_write_reg(ts, 0x01, 0x01);		// Set Flash Acces Mode
	g2tsp_write_reg(ts, 0x00, 0x00);		// Set Standby mode off
	udelay(100);
	g2tsp_write_reg(ts, 0x80, 0x00);		// INF

	g2tsp_write_reg(ts, 0x40, (u8)sector_addr);	// page  address[15:8]
	g2tsp_write_reg(ts, 0x41, (u8)page_addr);	// page  address[7:0]

	g2tsp_write_reg(ts, 0x42, 0x00);		// Set Column Address
	g2tsp_write_reg(ts, 0x43, 0x01);		// Set Function Type ==> For Erase 0x01
	g2tsp_write_reg(ts, 0x44, 0x01);		// Set Enabler
	mdelay(20);
	g2tsp_write_reg(ts, 0x44, 0x00);

	input_info(true, ts->pdev, "%s() exit +\r\n", __func__);

	return ret;
}

int G7400_flash_page_read(struct g2tsp_info *ts, int page, u8 *rdBuf)
{
	int ret = 0;
	int i;
	unsigned char checksum = 0;
	int sector_addr, page_addr;

//	input_info(true, ts->pdev, "%s() [%d] enter + \r\n", __func__, page);

	sector_addr = ((page/128)/32) & 0x0F;
	page_addr = ((page/128)%32) & 0x1F;

#if 1
	g2tsp_write_reg(ts, 0x00, 0x10);     // Set Flash Acces Mode
	udelay(100);
	g2tsp_write_reg(ts, 0x00, 0x01);     // Set Standby mode off
	udelay(100);
#else
	ts->platform_data->reset(ts);
	udelay(100);
#endif
	g2tsp_write_reg(ts, 0x01, 0x01);     // Set Flash Acces Mode
	g2tsp_write_reg(ts, 0x00, 0x00);     // Set Standby mode off
	udelay(100);
	g2tsp_write_reg(ts, 0x80, 0x00);     // INF

	g2tsp_write_reg(ts, 0x40, (u8)sector_addr);     // page  address[15:8]
	g2tsp_write_reg(ts, 0x41, (u8)page_addr);         // page  address[7:0]

	g2tsp_write_reg(ts, 0x42, 0x00);
	g2tsp_write_reg(ts, 0x43, 0x00);
	g2tsp_write_reg(ts, 0x44, 0x04);

	memset(rdBuf, 0, G7400_BUF_SIZE +1);

	g2tsp_read_block_data(ts, 0x4F,G7400_BUF_SIZE +1, rdBuf);

	for (i = 0 ; i < G7400_BUF_SIZE ; i++) {
		checksum += (rdBuf[i] & 0xFF);
	}

	udelay(5);

	if (checksum != rdBuf[128]) {
		g2debug1(ts->pdev, "Read Checksum Error!\n");
		return -1;
	}

	g2tsp_write_reg(ts, 0x44, 0x00);
#if 0
	for(i=0;i< (G7400_BUF_SIZE);i+=8)
	{
		input_info(true, ts->pdev,
		"%s: Header %x [%02x] [%02x] [%02x] [%02x] [%02x] [%02x] [%02x] [%02x]\n",
		__func__, i, rdBuf[i+0], rdBuf[i+1], rdBuf[i+2], rdBuf[i+3],
		rdBuf[i+4], rdBuf[i+5], rdBuf[i+6], rdBuf[i+7]);
	}  
#endif
	return ret;
}

int G7400_flash_write(struct g2tsp_info *ts, int page, u8 *data)
{
	int i,ret = 0;
	unsigned char checksum = 0;
	int sector_addr, page_addr;

//	input_info(true, ts->pdev, "%s() enter + \r\n",__func__);
#if 0
	for(i=0;i < (G7400_BUF_SIZE);i+=8)
	{
		input_info(true, ts->pdev,
		"%s: Header %x [%02x] [%02x] [%02x] [%02x] [%02x] [%02x] [%02x] [%02x]\n",
		__func__, i, data[i+0], data[i+1], data[i+2], data[i+3],
		data[i+4], data[i+5], data[i+6], data[i+7]);
	}
#endif

	sector_addr = ((page/128)/32) & 0x0F;
	page_addr = ((page/128)%32) & 0x1F;

	// for debugging
//	input_info(true, ts->pdev,"write sector addr=[0x%x] page_addr=[0x%x]", sector_addr, page_addr);
#if 1
	g2tsp_write_reg(ts, 0x00, 0x10);     // Set Flash Acces Mode
	udelay(100);
	g2tsp_write_reg(ts, 0x00, 0x01);     // Set Standby mode off
	udelay(100);
#else
	ts->platform_data->reset(ts);
	udelay(100);
#endif
	g2tsp_write_reg(ts, 0x01, 0x01);     // Set Flash Acces Mode
	g2tsp_write_reg(ts, 0x00, 0x00);     // Set Standby mode off
	udelay(100);
	g2tsp_write_reg(ts, 0x80, 0x00);     // INF
	g2tsp_write_reg(ts, 0x40, (u8)sector_addr);     // page  address[15:8]
	g2tsp_write_reg(ts, 0x41, (u8)page_addr);         // page  address[7:0]

	g2tsp_write_reg(ts, 0x42, 0x00);
	g2tsp_write_reg(ts, 0x43, 0x00);
	g2tsp_write_reg(ts, 0x44, 0x02);

	for (i = 0 ; i < G7400_BUF_SIZE ; i++) {
		g2tsp_write_reg(ts, 0x4F, data[i]);
		checksum += (data[i] & 0xFF);
	}

	g2tsp_write_reg(ts, 0x4F, checksum);
	mdelay(1);
	g2tsp_write_reg(ts, 0x44, 0x00);

//	input_info(true, ts->pdev, "%s() exit + \r\n", __func__);

	return ret;
}

int G7400_firmware_update(struct g2tsp_info *ts, const u8 *data, int size)
{
	int ret = 0;
	int i, k;
	int error_cnt = 0;
	int page = 0;
	u8 buf[G7400_BUF_SIZE];
	u8 rbuf[G7400_BUF_SIZE+1];

	input_info(true, ts->pdev, "%s(size = %d) Enter + \r\n", __func__, size); //size => 45824

	disable_irq_nosync(ts->irq);
	// flash erase
	G7400_flash_erase_all(ts);

	// write data
	input_info(true, ts->pdev, "Flash Write Start !! \r\n");

	for (i = 0 ; i < ((size/G7400_BUF_SIZE)+1) ; i++) {
		memset(buf, 0, G7400_BUF_SIZE);
		memset(rbuf, 0, G7400_BUF_SIZE + 1);

		if ((size - page) >= G7400_BUF_SIZE) {
			memcpy(buf, &data[page], G7400_BUF_SIZE);
		} else {
			memcpy(buf, &data[page], size - page);
		}

retry_write: 
		G7400_flash_write(ts, page, buf);
		G7400_flash_page_read(ts, page, rbuf);

		for (k = 0 ; k < G7400_BUF_SIZE ; k++) {
			if (buf[k] != rbuf[k]) {
				input_info(true, ts->pdev, " G2TSP Flash Write Compare Error! Page:%d Data[%2x]\r\n", page, buf[k]);
				error_cnt++;
				if (error_cnt < 10)
					goto retry_write;
			}
		}
		page += 0x80;       // page size is 128
	}
	input_info(true, ts->pdev, "Flash Write End !! \r\n");
	g2tsp_write_reg(ts, 0x0F, 0x01);

#if 1
	g2tsp_write_reg(ts, 0x00, 0x10);     // Set Flash Acces Mode
	udelay(100);
	g2tsp_write_reg(ts, 0x00, 0x01);     // Set Standby mode off
	udelay(100);
#else
	ts->platform_data->reset(ts);
#endif

	mdelay(2);
	enable_irq(ts->irq);    

	g2tsp_write_reg(ts, 0xE0, 0x9F);
	g2tsp_write_reg(ts, 0xEC, 0x0A);
	g2tsp_write_reg(ts, 0x00, 0x00);

	input_info(true, ts->pdev, "%s() Exit \r\n", __func__);
	return ret;
}
#endif

static void g2tsp_version_update(struct g2tsp_info *ts)
{
	struct g2tsp_ic_info tspInfo;

	if (g2tsp_read_block_data(ts, G2_REG_TOUCH_BASE, 18, ts->tsp_info) >= 0) {
		memcpy((u8*)&tspInfo, &ts->tsp_info[1], 17);
		check_firmware_version(ts,ts->tsp_info);

		input_info(true, ts->pdev, "ver(%02x.%02x.%02x),IC Name(%d), IC Rev(%d), TSP Type(%02x:%02x:%02x),"
							" ChInfo(%02x:%02x), Inch(%04x), Manuf(%04x)\n"
					,tspInfo.fw_ver[0], tspInfo.fw_ver[1], tspInfo.fw_ver[2],
					tspInfo.ic_name, tspInfo.ic_rev, 
					tspInfo.tsp_type[0], tspInfo.tsp_type[1], tspInfo.tsp_type[2], 
					tspInfo.ch_x, tspInfo.ch_y, tspInfo.inch, tspInfo.manufacture);
	}
	g2tsp_write_reg(ts,  G2_REG_AUX1, ts->auxreg1 | 0x01);
}

// "/sdcard/Download/g2tsp/g2tsp_7400_fw.bin"
int g2tsp_ts_firmware_update_built_in(struct g2tsp_info *ts)
{
	const struct firmware *fw_entry;
	char fw_path[G2TSP_TS_MAX_FW_PATH];
	int result = -1;

	disable_irq(ts->client->irq);

	if (!ts->platform_data->firmware_name) {
		input_err(true, &ts->client->dev, "%s: firmware name is NULL!\n", __func__);
		goto err_request_fw;

//		snprintf(fw_path, G2TSP_TS_MAX_FW_PATH, "%s", G2TSP_TS_DEFAULT_FW_NAME);
	} else {
		snprintf(fw_path, G2TSP_TS_MAX_FW_PATH, "%s", ts->platform_data->firmware_name);
	}

	input_info(true, &ts->client->dev, "%s: initial firmware update [%s]\n", __func__, fw_path);

	//Loading Firmware------------------------------------------
	if (request_firmware(&fw_entry, fw_path, &ts->client->dev) != 0) {
		input_err(true, &ts->client->dev, "%s: firmware is not available\n", __func__);
		goto err_request_fw;
	}
	input_info(true, &ts->client->dev, "%s: request firmware done! size = %d\n", __func__, (int)fw_entry->size);
//	result = sec_ts_check_firmware_version(ts, fw_entry->data);


	if (G7400_firmware_update(ts, fw_entry->data, fw_entry->size) < 0)
		result = -1;
	else
		result = 0;

	if(ts->factory_cal == 1) {
		g2tsp_FactoryCal(ts);
	}

err_request_fw:
	release_firmware(fw_entry);
	enable_irq(ts->client->irq);
	return result;
}

int g2tsp_ts_load_fw_from_ums(struct g2tsp_info *ts)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fw_size, nread;
	int error = 0;
	const char* firmware_name = TSP_FW_DEFALT_PATH;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (firmware_name) {
		input_info(true, ts->pdev, "%s Firmware Filename : %s\r\n", __func__, firmware_name);

		fp = filp_open(firmware_name, O_RDONLY, S_IRUSR);
		if (IS_ERR(fp)) {
			input_info(true, ts->pdev, "%s: failed to open %s.\n", __func__, firmware_name);
			error = -ENOENT;
			goto open_err;
		}

		fw_size = fp->f_path.dentry->d_inode->i_size;

		if (0 < fw_size) {
			unsigned char *fw_data;

			fw_data = kzalloc(fw_size, GFP_KERNEL);
			nread = vfs_read(fp, (char __user *)fw_data, fw_size, &fp->f_pos);

			input_info(true, ts->pdev, "%s: start, file path %s, size %ld Bytes\n",
								__func__, firmware_name, fw_size);
#if 0
			// for debugging
			for (i = 0 ; i < (nread / 16) ; i += 16) {
				
				input_info(true, ts->pdev,
					"FW Header %x [%02x] [%02x] [%02x] [%02x] [%02x] [%02x] [%02x] [%02x] "
					"[%02x] [%02x] [%02x] [%02x] [%02x] [%02x] [%02x] [%02x]\n",
					i, fw_data[i+0], fw_data[i+1], fw_data[i+2], fw_data[i+3],
					fw_data[i+4], fw_data[i+5], fw_data[i+6], fw_data[i+7],
					fw_data[i+8], fw_data[i+9], fw_data[i+10], fw_data[i+11],
					fw_data[i+12], fw_data[i+13], fw_data[i+14], fw_data[i+15]);
			}
#endif
			if (nread != fw_size) {
				input_info(true, ts->pdev, "%s: failed to read firmware file, nread %ld Bytes\n", __func__, nread);
				error = -EIO;
			} else {
				if ((ts->fw_ver_ic[0] == 0) && (ts->fw_ver_ic[1] == 0)) {
					input_info(true, ts->pdev, "TSP Reset !! \r\n");
					g2tsp_write_reg(ts, 0x0, 0x10);
					udelay(5);
					g2tsp_write_reg(ts, 0x0, 0x00);
					mdelay(150);
				}

				G7400_firmware_update(ts, fw_data, fw_size);

				g2tsp_version_update(ts);

				if(ts->factory_cal == 1) {
					g2tsp_FactoryCal(ts);
				}
			}
			kfree(fw_data);

		}

		filp_close(fp, NULL);
	}

	open_err:
	set_fs(old_fs);
	return error;
}

int g2tsp_ts_firmware_update_on_hidden_menu(struct g2tsp_info *ts, int update_type)
{
	int ret = 0;

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
		ret = g2tsp_ts_firmware_update_built_in(ts);
		break;
	case UMS:
		ret = g2tsp_ts_load_fw_from_ums(ts);
//		ret = sec_ts_load_fw_from_ums(ts);
		break;
//	case FFU:
//		ret = sec_ts_load_fw_from_ffu(ts);
//		break;
	default:
		input_err(true, ts->pdev, "%s: Not support command[%d]\n",
					__func__, update_type);
		break;
	}
	return ret;
}
//EXPORT_SYMBOL(g2tsp_ts_firmware_update_on_hidden_menu);

