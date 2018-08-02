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

#ifndef _SESSION_H_
#define _SESSION_H_

#include <linux/list.h>

#include "mcp.h"

struct tee_object;
struct tee_mmu;
struct mc_ioctl_buffer;

struct tee_session {
	/* Session closing lock, so two calls cannot be made simultaneously */
	struct mutex		close_lock;
	/* Asynchronous session close (GP) requires a callback to unblock */
	struct completion	close_completion;
	/* MCP session descriptor (MUST BE FIRST) */
	struct mcp_session	mcp_session;
	/* Owner */
	struct tee_client	*client;
	/* Number of references kept to this object */
	struct kref		kref;
	/* The list entry to attach to session list of owner */
	struct list_head	list;
	/* Session WSMs lock */
	struct mutex		wsms_lock;
	/* List of WSMs for a session */
	struct list_head	wsms;
};

struct tee_session *session_create(struct tee_client *client, bool is_gp,
				   struct mc_identity *identity);
int session_open(struct tee_session *session, const struct tee_object *obj,
		 const struct tee_mmu *obj_mmu, uintptr_t tci, size_t len);
int session_close(struct tee_session *session);
static inline void session_get(struct tee_session *session)
{
	kref_get(&session->kref);
}

int session_put(struct tee_session *session);
int session_kill(struct tee_session *session);
int session_wsms_add(struct tee_session *session,
		     struct mc_ioctl_buffer *bufs);
int session_wsms_remove(struct tee_session *session,
			const struct mc_ioctl_buffer *bufs);
s32 session_exitcode(struct tee_session *session);
int session_notify_swd(struct tee_session *session);
int session_waitnotif(struct tee_session *session, s32 timeout,
		      bool silent_expiry);
int session_debug_structs(struct kasnprintf_buf *buf,
			  struct tee_session *session, bool is_closing);

#endif /* _SESSION_H_ */
