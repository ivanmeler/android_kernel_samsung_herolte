
#ifndef __PANEL_POC_H__
#define __PANEL_POC_H__

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/notifier.h>

#define POC_IMG_SIZE	(460816)
#define POC_IMG_ADDR	(0x000000)
#define POC_PAGE		(4096)
#define POC_TEST_PATTERN_SIZE	(4096)
#define POC_IMG_WR_SIZE	(POC_IMG_SIZE)
#define POC_IMG_PATH "/data/poc_img"
#define POC_CHKSUM_DATA_LEN		(4)
#define POC_CHKSUM_RES_LEN		(1)
#define POC_CHKSUM_LEN		(POC_CHKSUM_DATA_LEN + POC_CHKSUM_RES_LEN)
#define POC_CHKSUM_REG			0xEC
#define POC_CTRL_LEN			(4)
#define POC_CTRL_REG			0xEB

#ifdef CONFIG_DISPLAY_USE_INFO
#define POC_TOTAL_TRY_COUNT_FILE_PATH	("/efs/etc/poc/totaltrycount")
#define POC_TOTAL_FAIL_COUNT_FILE_PATH	("/efs/etc/poc/totalfailcount")
#endif

#define ERASE_WAIT_COUNT	(180)
#define WR_DONE_UDELAY		(4000)
#define QD_DONE_MDELAY		(30)
#define RD_DONE_UDELAY		(200)

enum poc_flash_state {
	POC_FLASH_STATE_UNKNOWN = -1,
	POC_FLASH_STATE_NOT_USE = 0,
	POC_FLASH_STATE_USE = 1,
	MAX_POC_FLASH_STATE,
};

enum {
	POC_OP_NONE = 0,
	POC_OP_ERASE = 1,
	POC_OP_WRITE = 2,
	POC_OP_READ = 3,
	POC_OP_CANCEL = 4,
	POC_OP_CHECKSUM = 5,
	POC_OP_CHECKPOC = 6,
	POC_OP_BACKUP = 7,
	POC_OP_SELF_TEST = 8,
	POC_OP_READ_TEST = 9,
	POC_OP_WRITE_TEST = 10,
	MAX_POC_OP,
};

#define IS_VALID_POC_OP(_op_)	\
	((_op_) > POC_OP_NONE && (_op_) < MAX_POC_OP)

#define IS_POC_OP_ACCESS_FLASH(_op_)	\
	((_op_) == POC_OP_ERASE ||\
	 (_op_) == POC_OP_WRITE ||\
	 (_op_) == POC_OP_READ ||\
	 (_op_) == POC_OP_BACKUP ||\
	 (_op_) == POC_OP_SELF_TEST)

enum poc_state {
	POC_STATE_NONE,
	POC_STATE_FLASH_EMPTY,
	POC_STATE_FLASH_FILLED,
	POC_STATE_ER_START,
	POC_STATE_ER_PROGRESS,
	POC_STATE_ER_COMPLETE,
	POC_STATE_ER_FAILED,
	POC_STATE_WR_START,
	POC_STATE_WR_PROGRESS,
	POC_STATE_WR_COMPLETE,
	POC_STATE_WR_FAILED,
	MAX_POC_STATE,
};

struct panel_poc_info {
	bool enabled;
	bool erased;

	enum poc_state state;

	u8 poc;
	u8 poc_chksum[POC_CHKSUM_LEN];
	u8 poc_ctrl[POC_CTRL_LEN];

	u8 *wbuf;
	u32 wpos;
	u32 wsize;

	u8 *rbuf;
	u32 rpos;
	u32 rsize;

#ifdef CONFIG_DISPLAY_USE_INFO
	int total_failcount;
	int total_trycount;
#endif
};

struct panel_poc_device {
	struct miscdevice dev;
	__u64 count;
	unsigned int flags;
	atomic_t cancel;
	bool opened;
	struct panel_poc_info poc_info;
#ifdef CONFIG_DISPLAY_USE_INFO
	struct notifier_block poc_notif;
#endif
};
	
#define IOC_GET_POC_STATUS	_IOR('A', 100, __u32)		/* 0:NONE, 1:ERASED, 2:WROTE, 3:READ */
#define IOC_GET_POC_CHKSUM	_IOR('A', 101, __u32)		/* 0:CHKSUM ERROR, 1:CHKSUM SUCCESS */
#define IOC_GET_POC_CSDATA	_IOR('A', 102, __u32)		/* CHKSUM DATA 4 Bytes */
#define IOC_GET_POC_ERASED	_IOR('A', 103, __u32)		/* 0:NONE, 1:NOT ERASED, 2:ERASED */
#define IOC_GET_POC_FLASHED	_IOR('A', 104, __u32)		/* 0:NOT POC FLASHED(0x53), 1:POC FLAHSED(0x52) */
#define IOC_SET_POC_ERASE	_IOR('A', 110, __u32)		/* ERASE POC FLASH */
#define IOC_SET_POC_TEST	_IOR('A', 112, __u32)		/* POC FLASH TEST - ERASE/WRITE/READ/COMPARE */

static const unsigned char SEQ_POC_F0_KEY_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static const unsigned char SEQ_POC_F0_KEY_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static const unsigned char SEQ_POC_F1_KEY_ENABLE[] = { 0xF1, 0xF1, 0xA2 };
static const unsigned char SEQ_POC_F1_KEY_DISABLE[] = { 0xF1, 0xA5, 0xA5 };
static const unsigned char SEQ_POC_PGM_ENABLE[] = { 0xC0, 0x02 };
static const unsigned char SEQ_POC_PGM_DISABLE[] = { 0xC0, 0x00 };
static const unsigned char SEQ_POC_EXECUTE[] = { 0xC0, 0x03 };
static const unsigned char SEQ_POC_WR_ENABLE[] = { 0xC1, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
static const unsigned char SEQ_POC_QD_ENABLE[] = { 0xC1, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x10 };
static const unsigned char SEQ_POC_WR_STT[] = { 0xC1, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x01 };
static const unsigned char SEQ_POC_WR_END[] = { 0xC1, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01 };
static const unsigned char SEQ_POC_RD_STT[] = { 0xC1, 0x00, 0x6B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01 };
static const unsigned char SEQ_POC_ERASE[] = { 0xC1, 0x00, 0xC7, 0x00, 0x10, 0x00 };

extern int panel_poc_probe(struct panel_poc_device *poc_dev);
extern int set_panel_poc(struct panel_poc_device *poc_dev, u32 cmd);


#endif
