/*
 * drivers/misc/logger.c
 *
 * A Logging Subsystem
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * Robert Love <rlove@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "logger: " fmt

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/aio.h>
#include "logger.h"

#include <asm/ioctls.h>
#ifdef CONFIG_SEC_DEBUG
#include <linux/sec_debug.h>
#define SUPPORT_GETLOG_TOOL
#endif

#ifdef CONFIG_SEC_BSP
#include <linux/sec_bsp.h>
#endif

/**
 * struct logger_log - represents a specific log, such as 'main' or 'radio'
 * @buffer:	The actual ring buffer
 * @misc:	The "misc" device representing the log
 * @wq:		The wait queue for @readers
 * @readers:	This log's readers
 * @mutex:	The mutex that protects the @buffer
 * @w_off:	The current write head offset
 * @head:	The head, or location that readers start reading at.
 * @size:	The size of the log
 * @logs:	The list of log channels
 *
 * This structure lives from module insertion until module removal, so it does
 * not need additional reference counting. The structure is protected by the
 * mutex 'mutex'.
 */
struct logger_log {
	unsigned char		*buffer;
	struct miscdevice	misc;
	wait_queue_head_t	wq;
	struct list_head	readers;
	struct mutex		mutex;
	size_t			w_off;
	size_t			head;
	size_t			size;
#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	bool			ess_hook;
	char			*ess_buf;
	char			*ess_sync_buf;
	size_t			ess_size;
	size_t			ess_sync_size;
	size_t			header_size;
#endif
	struct list_head	logs;
};

static LIST_HEAD(log_list);


/**
 * struct logger_reader - a logging device open for reading
 * @log:	The associated log
 * @list:	The associated entry in @logger_log's list
 * @r_off:	The current read head offset.
 * @r_all:	Reader can read all entries
 * @r_ver:	Reader ABI version
 *
 * This object lives from open to release, so we don't need additional
 * reference counting. The structure is protected by log->mutex.
 */
struct logger_reader {
	struct logger_log	*log;
	struct list_head	list;
	size_t			r_off;
	bool			r_all;
	int			r_ver;
};

/* logger_offset - returns index 'n' into the log via (optimized) modulus */
static size_t logger_offset(struct logger_log *log, size_t n)
{
	return n & (log->size - 1);
}


/*
 * file_get_log - Given a file structure, return the associated log
 *
 * This isn't aesthetic. We have several goals:
 *
 *	1) Need to quickly obtain the associated log during an I/O operation
 *	2) Readers need to maintain state (logger_reader)
 *	3) Writers need to be very fast (open() should be a near no-op)
 *
 * In the reader case, we can trivially go file->logger_reader->logger_log.
 * For a writer, we don't want to maintain a logger_reader, so we just go
 * file->logger_log. Thus what file->private_data points at depends on whether
 * or not the file was opened for reading. This function hides that dirtiness.
 */
static inline struct logger_log *file_get_log(struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;

		return reader->log;
	}
	return file->private_data;
}

/*
 * get_entry_header - returns a pointer to the logger_entry header within
 * 'log' starting at offset 'off'. A temporary logger_entry 'scratch' must
 * be provided. Typically the return value will be a pointer within
 * 'logger->buf'.  However, a pointer to 'scratch' may be returned if
 * the log entry spans the end and beginning of the circular buffer.
 */
static struct logger_entry *get_entry_header(struct logger_log *log,
		size_t off, struct logger_entry *scratch)
{
	size_t len = min(sizeof(struct logger_entry), log->size - off);

	if (len != sizeof(struct logger_entry)) {
		memcpy(((void *) scratch), log->buffer + off, len);
		memcpy(((void *) scratch) + len, log->buffer,
			sizeof(struct logger_entry) - len);
		return scratch;
	}

	return (struct logger_entry *) (log->buffer + off);
}

/*
 * get_entry_msg_len - Grabs the length of the message of the entry
 * starting from from 'off'.
 *
 * An entry length is 2 bytes (16 bits) in host endian order.
 * In the log, the length does not include the size of the log entry structure.
 * This function returns the size including the log entry structure.
 *
 * Caller needs to hold log->mutex.
 */
static __u32 get_entry_msg_len(struct logger_log *log, size_t off)
{
	struct logger_entry scratch;
	struct logger_entry *entry;

	entry = get_entry_header(log, off, &scratch);
	return entry->len;
}

static size_t get_user_hdr_len(int ver)
{
	if (ver < 2)
		return sizeof(struct user_logger_entry_compat);
	return sizeof(struct logger_entry);
}

static ssize_t copy_header_to_user(int ver, struct logger_entry *entry,
					 char __user *buf)
{
	void *hdr;
	size_t hdr_len;
	struct user_logger_entry_compat v1;

	if (ver < 2) {
		v1.len      = entry->len;
		v1.__pad    = 0;
		v1.pid      = entry->pid;
		v1.tid      = entry->tid;
		v1.sec      = entry->sec;
		v1.nsec     = entry->nsec;
		hdr         = &v1;
		hdr_len     = sizeof(struct user_logger_entry_compat);
	} else {
		hdr         = entry;
		hdr_len     = sizeof(struct logger_entry);
	}

	return copy_to_user(buf, hdr, hdr_len);
}

/*
 * do_read_log_to_user - reads exactly 'count' bytes from 'log' into the
 * user-space buffer 'buf'. Returns 'count' on success.
 *
 * Caller must hold log->mutex.
 */
static ssize_t do_read_log_to_user(struct logger_log *log,
				   struct logger_reader *reader,
				   char __user *buf,
				   size_t count)
{
	struct logger_entry scratch;
	struct logger_entry *entry;
	size_t len;
	size_t msg_start;

	/*
	 * First, copy the header to userspace, using the version of
	 * the header requested
	 */
	entry = get_entry_header(log, reader->r_off, &scratch);
	if (copy_header_to_user(reader->r_ver, entry, buf))
		return -EFAULT;

	count -= get_user_hdr_len(reader->r_ver);
	buf += get_user_hdr_len(reader->r_ver);
	msg_start = logger_offset(log,
		reader->r_off + sizeof(struct logger_entry));

	/*
	 * We read from the msg in two disjoint operations. First, we read from
	 * the current msg head offset up to 'count' bytes or to the end of
	 * the log, whichever comes first.
	 */
	len = min(count, log->size - msg_start);
	if (copy_to_user(buf, log->buffer + msg_start, len))
		return -EFAULT;

	/*
	 * Second, we read any remaining bytes, starting back at the head of
	 * the log.
	 */
	if (count != len)
		if (copy_to_user(buf + len, log->buffer, count - len))
			return -EFAULT;

	reader->r_off = logger_offset(log, reader->r_off +
		sizeof(struct logger_entry) + count);

	return count + get_user_hdr_len(reader->r_ver);
}

/*
 * get_next_entry_by_uid - Starting at 'off', returns an offset into
 * 'log->buffer' which contains the first entry readable by 'euid'
 */
static size_t get_next_entry_by_uid(struct logger_log *log,
		size_t off, kuid_t euid)
{
	while (off != log->w_off) {
		struct logger_entry *entry;
		struct logger_entry scratch;
		size_t next_len;

		entry = get_entry_header(log, off, &scratch);

		if (uid_eq(entry->euid, euid))
			return off;

		next_len = sizeof(struct logger_entry) + entry->len;
		off = logger_offset(log, off + next_len);
	}

	return off;
}

/*
 * logger_read - our log's read() method
 *
 * Behavior:
 *
 *	- O_NONBLOCK works
 *	- If there are no log entries to read, blocks until log is written to
 *	- Atomically reads exactly one log entry
 *
 * Will set errno to EINVAL if read
 * buffer is insufficient to hold next entry.
 */
static ssize_t logger_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	struct logger_reader *reader = file->private_data;
	struct logger_log *log = reader->log;
	ssize_t ret;
	DEFINE_WAIT(wait);

start:
	while (1) {
		mutex_lock(&log->mutex);

		prepare_to_wait(&log->wq, &wait, TASK_INTERRUPTIBLE);

		ret = (log->w_off == reader->r_off);
		mutex_unlock(&log->mutex);
		if (!ret)
			break;

		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		schedule();
	}

	finish_wait(&log->wq, &wait);
	if (ret)
		return ret;

	mutex_lock(&log->mutex);

	if (!reader->r_all)
		reader->r_off = get_next_entry_by_uid(log,
			reader->r_off, current_euid());

	/* is there still something to read or did we race? */
	if (unlikely(log->w_off == reader->r_off)) {
		mutex_unlock(&log->mutex);
		goto start;
	}

	/* get the size of the next entry */
	ret = get_user_hdr_len(reader->r_ver) +
		get_entry_msg_len(log, reader->r_off);
	if (count < ret) {
		ret = -EINVAL;
		goto out;
	}

	/* get exactly one entry from the log */
	ret = do_read_log_to_user(log, reader, buf, ret);

out:
	mutex_unlock(&log->mutex);

	return ret;
}

/*
 * get_next_entry - return the offset of the first valid entry at least 'len'
 * bytes after 'off'.
 *
 * Caller must hold log->mutex.
 */
static size_t get_next_entry(struct logger_log *log, size_t off, size_t len)
{
	size_t count = 0;

	do {
		size_t nr = sizeof(struct logger_entry) +
			get_entry_msg_len(log, off);
		off = logger_offset(log, off + nr);
		count += nr;
	} while (count < len);

	return off;
}

/*
 * is_between - is a < c < b, accounting for wrapping of a, b, and c
 *    positions in the buffer
 *
 * That is, if a<b, check for c between a and b
 * and if a>b, check for c outside (not between) a and b
 *
 * |------- a xxxxxxxx b --------|
 *               c^
 *
 * |xxxxx b --------- a xxxxxxxxx|
 *    c^
 *  or                    c^
 */
static inline int is_between(size_t a, size_t b, size_t c)
{
	if (a < b) {
		/* is c between a and b? */
		if (a < c && c <= b)
			return 1;
	} else {
		/* is c outside of b through a? */
		if (c <= b || a < c)
			return 1;
	}

	return 0;
}

/*
 * fix_up_readers - walk the list of all readers and "fix up" any who were
 * lapped by the writer; also do the same for the default "start head".
 * We do this by "pulling forward" the readers and start head to the first
 * entry after the new write head.
 *
 * The caller needs to hold log->mutex.
 */
static void fix_up_readers(struct logger_log *log, size_t len)
{
	size_t old = log->w_off;
	size_t new = logger_offset(log, old + len);
	struct logger_reader *reader;

	if (is_between(old, new, log->head))
		log->head = get_next_entry(log, log->head, len);

	list_for_each_entry(reader, &log->readers, list)
		if (is_between(old, new, reader->r_off))
			reader->r_off = get_next_entry(log, reader->r_off, len);
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
#define ESS_MAX_BUF_SIZE	(SZ_4K)
#define ESS_MAX_SYNC_BUF_SIZE	(SZ_1K)
#define ESS_MAX_TIMEBUF_SIZE	(20)

static void (*func_hook_logger)(const char *name, const char *buf, size_t size);
void register_hook_logger(void (*func)(const char *name, const char *buf, size_t size))
{
	func_hook_logger = func;
}
EXPORT_SYMBOL(register_hook_logger);

static int reparse_hook_logger_header(struct logger_log *log,
				      struct logger_entry *entry)
{
	struct tm tmBuf;
	char timeBuf[ESS_MAX_TIMEBUF_SIZE];
	char process[16];
	u64 tv_kernel;
	unsigned long rem_nsec;

	tv_kernel = local_clock();
	rem_nsec = do_div(tv_kernel, 1000000000);

	time_to_tm(entry->sec, 0, &tmBuf);

	strncpy(process, current->comm, sizeof(process));

	snprintf(timeBuf, sizeof(timeBuf), "%02d-%02d %02d:%02d:%02d",
			tmBuf.tm_mon + 1, tmBuf.tm_mday,
			tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec);

	snprintf(log->ess_buf, ESS_MAX_BUF_SIZE, "[%5lu.%06lu][%d:%16s] %s.%03d %5d %5d  ",
			(unsigned long)tv_kernel, rem_nsec / 1000,
			raw_smp_processor_id(), process,
			timeBuf, entry->nsec / 1000000,
			entry->pid,
			entry->tid);

	return log->header_size = strlen(log->ess_buf);
}

static size_t copy_hook_logger(struct logger_log *log, char *buf, size_t count,
				size_t filled_size, size_t max_size)
{
	size_t len = min(count, log->size - log->w_off);
	size_t m_off = (size_t)buf + filled_size;

	if (max_size <= filled_size) {
		pr_err("%s: failed to hooking platform log - count: %zu max: %zu, fill: %zu\n",
			__func__, count, max_size, filled_size);
		return filled_size;
	}

	/* Considering count size */
	if (filled_size + count < max_size) {
		memcpy((void *)m_off, log->buffer + log->w_off, len);
		if (count != len)
			memcpy((void *)m_off + len, log->buffer, count - len);
		filled_size += count;
	} else {
		/* Cut off over max_size in count == len */
		if (count == len) {
			memcpy((void *)m_off, log->buffer + log->w_off,
					max_size - filled_size - 1);
		} else {
			/* Considering len size */
			if (filled_size + len < max_size) {
				/* Enough to fill len size */
				memcpy((void *)m_off, log->buffer + log->w_off, len);
				/* Cut off over max_size */
				memcpy((void *)(m_off + len), log->buffer,
						max_size - filled_size - len - 1);
			} else {
				/* Cut off over max size */
				memcpy((void *)m_off, log->buffer + log->w_off,
						max_size - filled_size - 1);
			}
		}
		filled_size = max_size;
	}
	return filled_size;
}
#endif

/*
 * do_write_log - writes 'len' bytes from 'buf' to 'log'
 *
 * The caller needs to hold log->mutex.
 */
static void do_write_log(struct logger_log *log, const void *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	memcpy(log->buffer + log->w_off, buf, len);

	if (count != len)
		memcpy(log->buffer, buf + len, count - len);

#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	if (func_hook_logger && log->ess_buf)
		log->ess_size = reparse_hook_logger_header(log,
					(struct logger_entry *)buf);
#endif
	log->w_off = logger_offset(log, log->w_off + count);
}

/*
 * do_write_log_user - writes 'len' bytes from the user-space buffer 'buf' to
 * the log 'log'
 *
 * The caller needs to hold log->mutex.
 *
 * Returns 'count' on success, negative error code on failure.
 */
static ssize_t do_write_log_from_user(struct logger_log *log,
				      const void __user *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	if (len && copy_from_user(log->buffer + log->w_off, buf, len))
		return -EFAULT;

	if (count != len)
		if (copy_from_user(log->buffer, buf + len, count - len))
			/*
			 * Note that by not updating w_off, this abandons the
			 * portion of the new entry that *was* successfully
			 * copied, just above.  This is intentional to avoid
			 * message corruption from missing fragments.
			 */
			return -EFAULT;

#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	/*
	 *  There are times when log buffer is just 1 bytes
	 *  for sync with kernel log buffer
	 */
	if (log->ess_sync_buf && len > 1 &&
		log->ess_sync_size < ESS_MAX_SYNC_BUF_SIZE - 1 &&
		strncmp(log->buffer + log->w_off, "!@", 2) == 0) {
		log->ess_sync_size = copy_hook_logger(log,
						      log->ess_sync_buf,
						      count,
						      log->ess_sync_size,
						      ESS_MAX_SYNC_BUF_SIZE);
#ifdef CONFIG_SEC_BSP
	if (strncmp(log->buffer + log->w_off, "!@Boot", 6) == 0) {
		sec_boot_stat_add(log->buffer + log->w_off);
	}
#endif
	}
	if (func_hook_logger && log->ess_hook) {
		if (log->ess_size < ESS_MAX_BUF_SIZE - 1) {
			log->ess_size = copy_hook_logger(log,
							 log->ess_buf,
							 count,
							 log->ess_size,
							 ESS_MAX_BUF_SIZE);
		}
	}
#endif
	log->w_off = logger_offset(log, log->w_off + count);

	return count;
}

/*
 * logger_aio_write - our write method, implementing support for write(),
 * writev(), and aio_write(). Writes are our fast path, and we try to optimize
 * them above all else.
 */
static ssize_t logger_aio_write(struct kiocb *iocb, const struct iovec *iov,
			 unsigned long nr_segs, loff_t ppos)
{
	struct logger_log *log = file_get_log(iocb->ki_filp);
	size_t orig;
	struct logger_entry header;
	struct timespec now;
	ssize_t ret = 0;

	now = current_kernel_time();

	header.pid = current->tgid;
	header.tid = current->pid;
	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
	header.euid = current_euid();
	header.len = min_t(size_t, iocb->ki_nbytes, LOGGER_ENTRY_MAX_PAYLOAD);
	header.hdr_size = sizeof(struct logger_entry);

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return 0;

	if (copy_from_user(header.msg, iov->iov_base, sizeof(header.msg[0])))
		return -EFAULT;

	mutex_lock(&log->mutex);

	orig = log->w_off;

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));

	while (nr_segs-- > 0) {
		size_t len;
		ssize_t nr;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, iov->iov_len, header.len - ret);

		/* write out this segment's payload */
		nr = do_write_log_from_user(log, iov->iov_base, len);
		if (unlikely(nr < 0)) {
			log->w_off = orig;
			mutex_unlock(&log->mutex);
			return nr;
		}

		iov++;
		ret += nr;
	}
#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	if (func_hook_logger && log->ess_hook) {
		/* it is allowed to hook if ess_size < ESS_MAX_BUF_SIZE */
		if (log->ess_size < ESS_MAX_BUF_SIZE) {
			char *eatnl = log->ess_buf + log->ess_size - 1;

			if (log->ess_size > log->header_size) {
				static const char* kPrioChars = "!.VDIWEFS";
				unsigned char prio = log->ess_buf[log->header_size];

				log->ess_buf[log->header_size - 1] =
					prio < strlen(kPrioChars) ? kPrioChars[prio] : '?';
				log->ess_buf[log->header_size] = ' ';
			}
			*eatnl = '\n';
			while (--eatnl >= log->ess_buf) {
				if (*eatnl == '\n' || *eatnl == '\0')
					*eatnl = ' ';
			};
			func_hook_logger(log->misc.name, log->ess_buf, log->ess_size);
		}
	}
	/* if it is kernel sync logs */
	if (log->ess_sync_buf && log->ess_sync_size) {
		/* save code to prevent overflow during printk */
		if (log->ess_sync_size < ESS_MAX_SYNC_BUF_SIZE)
			log->ess_sync_buf[log->ess_sync_size - 1] = '\0';
		else
			log->ess_sync_buf[ESS_MAX_SYNC_BUF_SIZE - 1] = '\0';
		pr_info("%s\n", log->ess_sync_buf);
		/* clear ess_sync_buf */
		memset(log->ess_sync_buf, 0, ESS_MAX_SYNC_BUF_SIZE);
		log->ess_sync_size = 0;
	}
#endif
	mutex_unlock(&log->mutex);

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);

	return ret;
}

static struct logger_log *get_log_from_minor(int minor)
{
	struct logger_log *log;

	list_for_each_entry(log, &log_list, logs)
		if (log->misc.minor == minor)
			return log;
	return NULL;
}

/*
 * logger_open - the log's open() file operation
 *
 * Note how near a no-op this is in the write-only case. Keep it that way!
 */
static int logger_open(struct inode *inode, struct file *file)
{
	struct logger_log *log;
	int ret;

	ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	log = get_log_from_minor(MINOR(inode->i_rdev));
	if (!log)
		return -ENODEV;

	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader;

		reader = kmalloc(sizeof(struct logger_reader), GFP_KERNEL);
		if (!reader)
			return -ENOMEM;

		reader->log = log;
		reader->r_ver = 1;
		reader->r_all = in_egroup_p(inode->i_gid) ||
			capable(CAP_SYSLOG);

		INIT_LIST_HEAD(&reader->list);

		mutex_lock(&log->mutex);
		reader->r_off = log->head;
		list_add_tail(&reader->list, &log->readers);
		mutex_unlock(&log->mutex);

		file->private_data = reader;
	} else
		file->private_data = log;

	return 0;
}

/*
 * logger_release - the log's release file operation
 *
 * Note this is a total no-op in the write-only case. Keep it that way!
 */
static int logger_release(struct inode *ignored, struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		struct logger_log *log = reader->log;

		mutex_lock(&log->mutex);
		list_del(&reader->list);
		mutex_unlock(&log->mutex);

		kfree(reader);
	}

	return 0;
}

/*
 * logger_poll - the log's poll file operation, for poll/select/epoll
 *
 * Note we always return POLLOUT, because you can always write() to the log.
 * Note also that, strictly speaking, a return value of POLLIN does not
 * guarantee that the log is readable without blocking, as there is a small
 * chance that the writer can lap the reader in the interim between poll()
 * returning and the read() request.
 */
static unsigned int logger_poll(struct file *file, poll_table *wait)
{
	struct logger_reader *reader;
	struct logger_log *log;
	unsigned int ret = POLLOUT | POLLWRNORM;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	reader = file->private_data;
	log = reader->log;

	poll_wait(file, &log->wq, wait);

	mutex_lock(&log->mutex);
	if (!reader->r_all)
		reader->r_off = get_next_entry_by_uid(log,
			reader->r_off, current_euid());

	if (log->w_off != reader->r_off)
		ret |= POLLIN | POLLRDNORM;
	mutex_unlock(&log->mutex);

	return ret;
}

static long logger_set_version(struct logger_reader *reader, void __user *arg)
{
	int version;

	if (copy_from_user(&version, arg, sizeof(int)))
		return -EFAULT;

	if ((version < 1) || (version > 2))
		return -EINVAL;

	reader->r_ver = version;
	return 0;
}

static long logger_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct logger_log *log = file_get_log(file);
	struct logger_reader *reader;
	long ret = -EINVAL;
	void __user *argp = (void __user *) arg;

	mutex_lock(&log->mutex);

	switch (cmd) {
	case LOGGER_GET_LOG_BUF_SIZE:
		ret = log->size;
		break;
	case LOGGER_GET_LOG_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		if (log->w_off >= reader->r_off)
			ret = log->w_off - reader->r_off;
		else
			ret = (log->size - reader->r_off) + log->w_off;
		break;
	case LOGGER_GET_NEXT_ENTRY_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;

		if (!reader->r_all)
			reader->r_off = get_next_entry_by_uid(log,
				reader->r_off, current_euid());

		if (log->w_off != reader->r_off)
			ret = get_user_hdr_len(reader->r_ver) +
				get_entry_msg_len(log, reader->r_off);
		else
			ret = 0;
		break;
	case LOGGER_FLUSH_LOG:
		if (!(file->f_mode & FMODE_WRITE)) {
			ret = -EBADF;
			break;
		}
		if (!(in_egroup_p(file_inode(file)->i_gid) ||
				capable(CAP_SYSLOG))) {
			ret = -EPERM;
			break;
		}
		list_for_each_entry(reader, &log->readers, list)
			reader->r_off = log->w_off;
		log->head = log->w_off;
		ret = 0;
		break;
	case LOGGER_GET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = reader->r_ver;
		break;
	case LOGGER_SET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = logger_set_version(reader, argp);
		break;
	}

	mutex_unlock(&log->mutex);

	return ret;
}

static const struct file_operations logger_fops = {
	.owner = THIS_MODULE,
	.read = logger_read,
	.aio_write = logger_aio_write,
	.poll = logger_poll,
	.unlocked_ioctl = logger_ioctl,
	.compat_ioctl = logger_ioctl,
	.open = logger_open,
	.release = logger_release,
};

#ifdef SUPPORT_GETLOG_TOOL
/* Use the old way because the new logger gets log buffers by means of vmalloc().
    getlog tool considers that log buffers lie on physically contiguous memory area. */

/*
 * Defines a log structure with name 'NAME' and a size of 'SIZE' bytes, which
 * must be a power of two, and greater than
 * (LOGGER_ENTRY_MAX_PAYLOAD + sizeof(struct logger_entry)).
 */
#define DEFINE_LOGGER_DEVICE(VAR, NAME, SIZE) \
static unsigned char _buf_ ## VAR[SIZE]; \
static struct logger_log VAR = { \
	.buffer = _buf_ ## VAR, \
	.misc = { \
		.minor = MISC_DYNAMIC_MINOR, \
		.name = NAME, \
		.fops = &logger_fops, \
		.parent = NULL, \
	}, \
	.wq = __WAIT_QUEUE_HEAD_INITIALIZER(VAR .wq), \
	.readers = LIST_HEAD_INIT(VAR .readers), \
	.mutex = __MUTEX_INITIALIZER(VAR .mutex), \
	.w_off = 0, \
	.head = 0, \
	.size = SIZE, \
	.logs = LIST_HEAD_INIT(VAR .logs), \
};

DEFINE_LOGGER_DEVICE(log_main, LOGGER_LOG_MAIN, 2*1024*1024)
DEFINE_LOGGER_DEVICE(log_events, LOGGER_LOG_EVENTS,256*1024)
DEFINE_LOGGER_DEVICE(log_radio, LOGGER_LOG_RADIO, 1024*1024)
DEFINE_LOGGER_DEVICE(log_system, LOGGER_LOG_SYSTEM, 256*1024)

struct logger_log * log_buffers[]={
	&log_main,
	&log_events,
	&log_radio,
	&log_system,
	NULL,
};

struct logger_log *sec_get_log_buffer(char *log_name, int size)
{
	struct logger_log **log_buf=&log_buffers[0];

	while (*log_buf) {
		if (!strcmp(log_name,(*log_buf)->misc.name)) {
			return *log_buf;
		}

		log_buf++;
	}
	return NULL;
}
#endif

/*
 * Log size must must be a power of two, and greater than
 * (LOGGER_ENTRY_MAX_PAYLOAD + sizeof(struct logger_entry)).
 */
static int __init create_log(char *log_name, int size)
{
	int ret = 0;
	struct logger_log *log;
	unsigned char *buffer;

#ifdef SUPPORT_GETLOG_TOOL
	log = sec_get_log_buffer(log_name,size);
	if (!log) {
		pr_err("No \"%s\" buffer registered\n",log_name);
		return -1;
	}

	list_add_tail(&log->logs, &log_list);

	/* finally, initialize the misc device for this log */
	ret = misc_register(&log->misc);
	if (unlikely(ret)) {
		pr_err("failed to register misc device for log '%s'!\n",
			log->misc.name);
		return ret;
	}

	pr_info("created %luK log '%s'\n",
		(unsigned long) log->size >> 10, log->misc.name);

#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	buffer = vmalloc(ESS_MAX_SYNC_BUF_SIZE);
	if (buffer)
		log->ess_sync_buf = buffer;
	else
		pr_err("failed to vmalloc ess_sync_buf %s log\n",
				log->misc.name);

	if (exynos_ss_get_enable(log->misc.name, false) == true) {
		buffer = vmalloc(ESS_MAX_BUF_SIZE);
		if (buffer)
			log->ess_buf = buffer;
		else
			pr_err("failed to vmalloc ess_buf %s log\n",
					log->misc.name);

		if (log->ess_sync_buf) {
			if (log->ess_buf)
				log->ess_hook = true;
			else
				vfree(log->ess_sync_buf);
		}
		if (!log->ess_hook)
			pr_err("failed to use hooking platform %s log\n",
					log->misc.name);
		else
			pr_info("Enable to hook %s, Buffer:%p\n", log->misc.name, buffer);
	}
#endif

#ifdef CONFIG_SEC_DEBUG
	sec_getlog_supply_platform(log->buffer, log->misc.name);
#endif

	return ret;
#else /* SUPPORT_GETLOG_TOOL */
#ifdef CONFIG_SEC_DEBUG
	buffer = kmalloc(size, GFP_KERNEL);
#else
	buffer = vmalloc(size);
#endif
	if (buffer == NULL)
		return -ENOMEM;

	log = kzalloc(sizeof(struct logger_log), GFP_KERNEL);
	if (log == NULL) {
		ret = -ENOMEM;
		goto out_free_buffer;
	}
	log->buffer = buffer;

	log->misc.minor = MISC_DYNAMIC_MINOR;
	log->misc.name = kstrdup(log_name, GFP_KERNEL);
	if (log->misc.name == NULL) {
		ret = -ENOMEM;
		goto out_free_log;
	}

	log->misc.fops = &logger_fops;
	log->misc.parent = NULL;

	init_waitqueue_head(&log->wq);
	INIT_LIST_HEAD(&log->readers);
	mutex_init(&log->mutex);
	log->w_off = 0;
	log->head = 0;
	log->size = size;

	INIT_LIST_HEAD(&log->logs);
	list_add_tail(&log->logs, &log_list);

	/* finally, initialize the misc device for this log */
	ret = misc_register(&log->misc);
	if (unlikely(ret)) {
		pr_err("failed to register misc device for log '%s'!\n",
				log->misc.name);
		goto out_free_misc_name;
	}

	pr_info("created %luK log '%s'\n",
		(unsigned long) log->size >> 10, log->misc.name);

#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	buffer = vmalloc(ESS_MAX_SYNC_BUF_SIZE);
	if (buffer)
		log->ess_sync_buf = buffer;
	else
		pr_err("failed to vmalloc ess_sync_buf %s log\n",
				log->misc.name);

	if (exynos_ss_get_enable(log->misc.name, false) == true) {
		buffer = vmalloc(ESS_MAX_BUF_SIZE);
		if (!buffer) {
			pr_err("failed to use hooking platform %s log\n", log->misc.name);
		} else {
			log->ess_buf = buffer;
			log->ess_hook = true;
			pr_info("Enable to hook %s, Buffer:%p\n",
					log->misc.name, log->ess_buf);
		}
	}
#endif

#ifdef CONFIG_SEC_DEBUG
		sec_getlog_supply_platform(log->buffer, log->misc.name);
#endif
	return 0;

out_free_misc_name:
	kfree(log->misc.name);

out_free_log:
	kfree(log);

out_free_buffer:
#ifdef CONFIG_SEC_DEBUG
	kfree(buffer);
#else
	vfree(buffer);
#endif
	return ret;
#endif /* SUPPORT_GETLOG_TOOL */
}

#if defined(CONFIG_SEC_DUMP_SUMMARY)
int sec_debug_summary_set_logger_info(
	struct sec_debug_summary_logger_log_info *log_info)
{
	struct logger_log *log;

	log_info->stinfo.buffer_offset = offsetof(struct logger_log, buffer);
	log_info->stinfo.w_off_offset = offsetof(struct logger_log, w_off);
	log_info->stinfo.head_offset = offsetof(struct logger_log, head);
	log_info->stinfo.size_offset = offsetof(struct logger_log, size);
	log_info->stinfo.size_t_typesize = sizeof(size_t);

	list_for_each_entry(log, &log_list, logs)
	{
		if(!strcmp(log->misc.name,LOGGER_LOG_MAIN)) {
			log_info->main.log_paddr = virt_to_phys(log);
			log_info->main.buffer_paddr = virt_to_phys(log->buffer);
		}
		else if(!strcmp(log->misc.name,LOGGER_LOG_SYSTEM)) {
			log_info->system.log_paddr = virt_to_phys(log);
			log_info->system.buffer_paddr = virt_to_phys(log->buffer);
		}
		else if(!strcmp(log->misc.name,LOGGER_LOG_EVENTS)) {
			log_info->events.log_paddr = virt_to_phys(log);
			log_info->events.buffer_paddr = virt_to_phys(log->buffer);
		}
		else if(!strcmp(log->misc.name,LOGGER_LOG_RADIO)) {
			log_info->radio.log_paddr = virt_to_phys(log);
			log_info->radio.buffer_paddr = virt_to_phys(log->buffer);
		}
	}

	return 0;
}
#endif

static int __init logger_init(void)
{
	int ret;

	ret = create_log(LOGGER_LOG_MAIN, 2*1024*1024);
	if (unlikely(ret))
		goto out;

	ret = create_log(LOGGER_LOG_EVENTS, 256*1024);
	if (unlikely(ret))
		goto out;

	ret = create_log(LOGGER_LOG_RADIO, 1024*1024);
	if (unlikely(ret))
		goto out;

	ret = create_log(LOGGER_LOG_SYSTEM, 256*1024);
	if (unlikely(ret))
		goto out;

out:
	return ret;
}

static void __exit logger_exit(void)
{
	struct logger_log *current_log, *next_log;

	list_for_each_entry_safe(current_log, next_log, &log_list, logs) {
		/* we have to delete all the entry inside log_list */
		misc_deregister(&current_log->misc);
		vfree(current_log->buffer);
		kfree(current_log->misc.name);
		list_del(&current_log->logs);
		kfree(current_log);
	}
}


device_initcall(logger_init);
module_exit(logger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert Love, <rlove@google.com>");
MODULE_DESCRIPTION("Android Logger");
