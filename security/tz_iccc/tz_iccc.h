#ifndef TZ_ICCC_H
#define TZ_ICCC_H

/* ICCC Implementation in Kernel */

#include <../../drivers/gud/gud-exynos8890/MobiCoreDriver/public/mobicore_driver_api.h>
#include <linux/security/Iccc_Interface.h>
#include <linux/selinux.h>

#define TL_TZ_ICCC_UUID {{ 0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x41 }}

#define CMD_ICCC_INIT				0x00000001
#define CMD_ICCC_SAVEDATA			0x00000002
#define CMD_ICCC_READDATA			0x00000003

#define	RET_ICCC_SUCCESS			0
#define	RET_ICCC_FAIL				-1

#define ICCC_READ	1
#define ICCC_WRITE	2

#define MIN_PADDING_NEEDED	9

typedef struct tz_msg_header {
	/* * First 4 bytes should always be id: either cmd_id or resp_id */
	uint32_t id;
	uint32_t content_id;
	uint32_t len;
	uint32_t status;
} __attribute__ ((packed)) tz_msg_header_t;

typedef struct iccc_req_s
{
	uint32_t cmd_id;
	uint32_t type;
	uint32_t value;
	uint32_t padding[MIN_PADDING_NEEDED];    //only padding , just to make tciMessage_t's size >= 64
} __attribute__ ((packed)) iccc_req_t;

typedef struct iccc_rsp_s
{
	uint32_t cmd_id;
	uint32_t type;
	uint32_t value;
	int ret;
} __attribute__ ((packed)) iccc_rsp_t;
 
typedef struct {
	union content_u {
        iccc_req_t				iccc_req;
        iccc_rsp_t				iccc_rsp;
    } __attribute__ ((packed)) content;
} __attribute__ ((packed)) iccc_generic_payload_t;

typedef struct {
	tz_msg_header_t header;
	union payload_u {
		iccc_generic_payload_t generic;
	} __attribute__ ((packed)) payload;
} __attribute__ ((packed)) iccc_message_t;

typedef iccc_message_t tciMessage_t;

#endif	/* TZ_ICCC_H */