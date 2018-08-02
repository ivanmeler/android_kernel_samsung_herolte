/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MC_USER_H_
#define _MC_USER_H_

#define MCDRVMODULEAPI_VERSION_MAJOR 2
#define MCDRVMODULEAPI_VERSION_MINOR 10

#include <linux/types.h>

#define MC_USER_DEVNODE		"mobicore-user"

/** Maximum length of MobiCore product ID string. */
#define MC_PRODUCT_ID_LEN	64

/** Number of buffers that can be mapped at once */
#define MC_MAP_MAX		4

/** Max length for buffers */
#define BUFFER_LENGTH_MAX	0x100000

/** Flags for buffers to map (aligned on GP) */
#define MC_IO_MAP_INPUT		0x1
#define MC_IO_MAP_OUTPUT	0x2
#define MC_IO_MAP_INPUT_OUTPUT	(MC_IO_MAP_INPUT | MC_IO_MAP_OUTPUT)

/*
 * Universally Unique Identifier (UUID) according to ISO/IEC 11578.
 */
struct mc_uuid_t {
	__u8		value[16];	/* Value of the UUID. */
};

/*
 * GP TA login types.
 */
enum mc_login_type {
	LOGIN_PUBLIC = 0,
	LOGIN_USER,
	LOGIN_GROUP,
	LOGIN_APPLICATION = 4,
	LOGIN_USER_APPLICATION,
	LOGIN_GROUP_APPLICATION,
};

/*
 * GP TA identity structure.
 */
struct mc_identity {
	enum mc_login_type	login_type;
	union {
		__u8		login_data[16];
		gid_t		gid;		/* Requested group id */
		struct {
			uid_t	euid;
			uid_t	ruid;
		} uid;
	};
	pid_t			pid;		/* Client, when using proxy */
};

/*
 * Data exchange structure of the MC_IO_OPEN_SESSION ioctl command.
 */
struct mc_ioctl_open_session {
	struct mc_uuid_t uuid;		/* trustlet uuid */
	__u32		is_gp_uuid;	/* uuid is for GP TA */
	__u32		sid;            /* session id (out) */
	__u64		tci;		/* tci buffer pointer */
	__u32		tcilen;		/* tci length */
	struct mc_identity identity;	/* GP TA identity */
};

/*
 * Data exchange structure of the MC_IO_OPEN_TRUSTLET ioctl command.
 */
struct mc_ioctl_open_trustlet {
	__u32		sid;		/* session id (out) */
	__u32		spid;		/* trustlet spid */
	__u64		buffer;		/* trustlet binary pointer */
	__u32		tlen;		/* binary length  */
	__u64		tci;		/* tci buffer pointer */
	__u32		tcilen;		/* tci length */
};

/*
 * Data exchange structure of the MC_IO_WAIT ioctl command.
 */
struct mc_ioctl_wait {
	__u32		sid;		/* session id (in) */
	__s32		timeout;	/* notification timeout */
	__u32		partial;	/* for proxy server to retry silently */
};

/*
 * Data exchange structure of the MC_IO_ALLOC ioctl command.
 */
struct mc_ioctl_alloc {
	__u32		len;		/* buffer length  */
	__u32		handle;		/* user handle for the buffer (out) */
};

/*
 * Buffer mapping incoming and outgoing information.
 */
struct mc_ioctl_buffer {
	__u64		va;		/* user space address of buffer */
	__u32		len;		/* buffer length  */
	__u64		sva;		/* SWd virt address of buffer (out) */
	__u32		flags;		/* buffer flags  */
};

/*
 * Data exchange structure of the MC_IO_MAP and MC_IO_UNMAP ioctl commands.
 */
struct mc_ioctl_map {
	__u32		sid;		/* session id */
	struct mc_ioctl_buffer bufs[MC_MAP_MAX]; /* buffers info */
};

/*
 * Data exchange structure of the MC_IO_ERR ioctl command.
 */
struct mc_ioctl_geterr {
	__u32		sid;		/* session id */
	__s32		value;		/* error value (out) */
};

/*
 * Global MobiCore Version Information.
 */
struct mc_version_info {
	char product_id[MC_PRODUCT_ID_LEN]; /** Product ID string */
	__u32 version_mci;		/** Mobicore Control Interface */
	__u32 version_so;		/** Secure Objects */
	__u32 version_mclf;		/** MobiCore Load Format */
	__u32 version_container;	/** MobiCore Container Format */
	__u32 version_mc_config;	/** MobiCore Config. Block Format */
	__u32 version_tl_api;		/** MobiCore Trustlet API */
	__u32 version_dr_api;		/** MobiCore Driver API */
	__u32 version_nwd;		/** This Driver */
};

/*
 * defines for the ioctl mobicore driver module function call from user space.
 */
/* MobiCore IOCTL magic number */
#define MC_IOC_MAGIC	'M'

/*
 * Implement corresponding functions from user api
 */
#define MC_IO_OPEN_SESSION	\
	_IOWR(MC_IOC_MAGIC, 0, struct mc_ioctl_open_session)
#define MC_IO_OPEN_TRUSTLET	\
	_IOWR(MC_IOC_MAGIC, 1, struct mc_ioctl_open_trustlet)
#define MC_IO_CLOSE_SESSION	_IO(MC_IOC_MAGIC, 2)
#define MC_IO_NOTIFY		_IO(MC_IOC_MAGIC, 3)
#define MC_IO_WAIT		_IOW(MC_IOC_MAGIC, 4, struct mc_ioctl_wait)
#define MC_IO_MAP		_IOWR(MC_IOC_MAGIC, 5, struct mc_ioctl_map)
#define MC_IO_UNMAP		_IOW(MC_IOC_MAGIC, 6, struct mc_ioctl_map)
#define MC_IO_ERR		_IOWR(MC_IOC_MAGIC, 7, struct mc_ioctl_geterr)
#define MC_IO_HAS_SESSIONS	_IO(MC_IOC_MAGIC, 8)
#define MC_IO_VERSION		_IOR(MC_IOC_MAGIC, 9, struct mc_version_info)

#endif /* _MC_USER_H_ */
