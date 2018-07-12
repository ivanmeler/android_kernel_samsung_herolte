/*
 *  linux/fs/proc/stlog.c
 *
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/stlog.h>

extern wait_queue_head_t ringbuf_wait;

/*
 * Compatibility Issue :
 * dumpstate process of Android M tries to open stlog node with O_NONBLOCK.
 * But on Android L, it tries to open stlog node without O_NONBLOCK.
 * In order to resolve this issue, stlog_open() always works as NONBLOCK mode.
 * If you want runtime debugging, please use stlog_open_pipe().
 */
static int stlog_open(struct inode * inode, struct file * file)
{
	//Open as non-blocking mode for printing once.
	file->f_flags |= O_NONBLOCK;
	return do_stlog(STLOG_ACTION_OPEN, NULL, 0, STLOG_FROM_PROC);
}

static int stlog_open_pipe(struct inode * inode, struct file * file)
{
	//Open as blocking mode for runtime debugging
	file->f_flags &= ~(O_NONBLOCK);
	return do_stlog(STLOG_ACTION_OPEN, NULL, 0, STLOG_FROM_PROC);
}

static int stlog_release(struct inode * inode, struct file * file)
{
	(void) do_stlog(STLOG_ACTION_CLOSE, NULL, 0, STLOG_FROM_PROC);
	return 0;
}

static ssize_t stlog_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	//Blocking mode for runtime debugging
	if (!(file->f_flags & O_NONBLOCK))
		return do_stlog(STLOG_ACTION_READ, buf, count, STLOG_FROM_PROC);

	//Non-blocking mode, print once, consume all the buffers
	return do_stlog(STLOG_ACTION_READ_ALL, buf, count, STLOG_FROM_PROC);
}

static ssize_t stlog_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	return do_stlog_write(STLOG_ACTION_WRITE, buf, count, STLOG_FROM_READER);
}


loff_t stlog_llseek(struct file *file, loff_t offset, int whence)
{
	return (loff_t)do_stlog(STLOG_ACTION_SIZE_BUFFER, 0, 0, STLOG_FROM_READER);
}

static const struct file_operations stlog_operations = {
	.read		= stlog_read,
	.write		= stlog_write,
	.open		= stlog_open,
	.release	= stlog_release,
	.llseek		= stlog_llseek,
};

static const struct file_operations stlog_pipe_operations = {
	.read		= stlog_read,
	.open		= stlog_open_pipe,
	.release	= stlog_release,
	.llseek		= stlog_llseek,
};

static const char DEF_STLOG_VER_STR[] = "1.0.3\n";

static ssize_t stlog_ver_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	loff_t off = *ppos;
	ssize_t len = strlen(DEF_STLOG_VER_STR);

	if (off >= len)
		return 0;

	len -= off;
	if (count < len)
		return -ENOMEM;

	ret = copy_to_user(buf, &DEF_STLOG_VER_STR[off], len);
	if (ret < 0)
		return ret;

	len -= ret;
	*ppos += len;

	return len;
}

static const struct file_operations stlog_ver_operations = {
	.read		= stlog_ver_read,
};

static int __init stlog_init(void)
{
	proc_create("stlog", S_IRUGO, NULL, &stlog_operations);
	proc_create("stlog_pipe", S_IRUGO, NULL, &stlog_pipe_operations);
	proc_create("stlog_version", S_IRUGO, NULL, &stlog_ver_operations);
	return 0;
}
module_init(stlog_init);

//#define CONFIG_STLOG_CPU_ID
#define CONFIG_STLOG_PID
#define CONFIG_STLOG_TIME
#define CONFIG_RINGBUF_SHIFT 15 /*32KB*/
//#define DEBUG_STLOG

#if defined(CONFIG_STLOG_CPU_ID)
static bool stlog_cpu_id = 1;
#else
static bool stlog_cpu_id;
#endif

module_param_named(cpu, stlog_cpu_id, bool, S_IRUGO | S_IWUSR);

#if defined(CONFIG_STLOG_PID)
static bool stlog_pid = 1;
#else
static bool stlog_pid;
#endif
module_param_named(pid, stlog_pid, bool, S_IRUGO | S_IWUSR);


enum ringbuf_flags {
	RINGBUF_NOCONS	= 1,
	RINGBUF_NEWLINE	= 2,
	RINGBUF_PREFIX	= 4,
	RINGBUF_CONT	= 8,
};

struct ringbuf {
	u64 ts_nsec;
	u16 len;
	u16 text_len;
#ifdef CONFIG_STLOG_CPU_ID
	u8 cpu_id;
#endif
#ifdef CONFIG_STLOG_PID
	char comm[TASK_COMM_LEN];
	pid_t pid;
#endif
	u8 flags:5;
	u8 level:3;
};

/*
 * The ringbuf_lock protects smsg buffer, indices, counters.
 */
static DEFINE_RAW_SPINLOCK(ringbuf_lock);

DECLARE_WAIT_QUEUE_HEAD(ringbuf_wait);
/* the next stlog record to read by /proc/stlog */
static u64 stlog_seq;
static u32 stlog_idx;
static enum ringbuf_flags ringbuf_prev;

static u64 ringbuf_first_seq;
static u32 ringbuf_first_idx;

static u64 ringbuf_next_seq;
static u32 ringbuf_next_idx;

static u64 stlog_clear_seq;
static u32 stlog_clear_idx;

static u64 stlog_end_seq = -1;

#define S_PREFIX_MAX		32
#define RINGBUF_LINE_MAX		1024 - S_PREFIX_MAX

/* record buffer */
#define RINGBUF_ALIGN __alignof__(struct ringbuf)
#define __RINGBUF_LEN (1 << CONFIG_RINGBUF_SHIFT)

static char __ringbuf_buf[__RINGBUF_LEN] __aligned(RINGBUF_ALIGN);
static char *ringbuf_buf = __ringbuf_buf;

static u32 ringbuf_buf_len = __RINGBUF_LEN;

/* cpu currently holding logbuf_lock */
static volatile unsigned int ringbuf_cpu = UINT_MAX;

/* human readable text of the record */
static char *ringbuf_text(const struct ringbuf *msg)
{
	return (char *)msg + sizeof(struct ringbuf);
}

static struct ringbuf *ringbuf_from_idx(u32 idx)
{
	struct ringbuf *msg = (struct ringbuf *)(ringbuf_buf + idx);

	if (!msg->len)
		return (struct ringbuf *)ringbuf_buf;
	return msg;
}

static u32 ringbuf_next(u32 idx)
{
	struct ringbuf *msg = (struct ringbuf *)(ringbuf_buf + idx);

	if (!msg->len) {
		msg = (struct ringbuf *)ringbuf_buf;
		return msg->len;
	}
	return idx + msg->len;
}

static void ringbuf_store(enum ringbuf_flags flags, const char *text, u16 text_len,
		      u8 cpu_id, struct task_struct *owner)
{
	struct ringbuf *msg;
	u32 size, pad_len;


	#ifdef	DEBUG_STLOG
	printk("[STLOG] %s stlog_seq %llu stlog_idx %lu ringbuf_first_seq %llu ringbuf_first_idx %lu ringbuf_next_seq %llu ringbuf_next_idx %lu \n",
			__func__,stlog_seq,stlog_idx,ringbuf_first_seq,ringbuf_first_idx,ringbuf_next_seq,ringbuf_next_idx);
	#endif


	/* number of '\0' padding bytes to next message */
	size = sizeof(struct ringbuf) + text_len;
	pad_len = (-size) & (RINGBUF_ALIGN - 1);
	size += pad_len;

	while (ringbuf_first_seq < ringbuf_next_seq) {
		u32 free;

		if (ringbuf_next_idx > ringbuf_first_idx)
			free = max(ringbuf_buf_len - ringbuf_next_idx, ringbuf_first_idx);
		else
			free = ringbuf_first_idx - ringbuf_next_idx;

		if (free > size + sizeof(struct ringbuf))
			break;

		/* drop old messages until we have enough space */
		ringbuf_first_idx = ringbuf_next(ringbuf_first_idx);
		ringbuf_first_seq++;
	}

	if (ringbuf_next_idx + size + sizeof(struct ringbuf) >= ringbuf_buf_len) {
		memset(ringbuf_buf + ringbuf_next_idx, 0, sizeof(struct ringbuf));
		ringbuf_next_idx = 0;
	}

	/* fill message */
	msg = (struct ringbuf *)(ringbuf_buf + ringbuf_next_idx);
	memcpy(ringbuf_text(msg), text, text_len);
	msg->text_len = text_len;
	msg->flags = flags & 0x1f;
#ifdef CONFIG_STLOG_CPU_ID
	msg->cpu_id = cpu_id;
#endif
#ifdef CONFIG_STLOG_PID
	msg->pid = owner->pid;
	memcpy(msg->comm, owner->comm, TASK_COMM_LEN);
#endif
	msg->ts_nsec = local_clock();
	msg->len = sizeof(struct ringbuf) + text_len + pad_len;

	/* insert message */
	ringbuf_next_idx += msg->len;
	ringbuf_next_seq++;
	wake_up_interruptible(&ringbuf_wait);

}


#if defined(CONFIG_STLOG_TIME)
static bool stlog_time = 1;
#else
static bool stlog_time;
#endif
module_param_named(time, stlog_time, bool, S_IRUGO | S_IWUSR);

static size_t stlog_print_time(u64 ts, char *buf)
{
	unsigned long rem_nsec;

	if (!stlog_time)
		return 0;

	rem_nsec = do_div(ts, 1000000000);

	if (!buf)
		return snprintf(NULL, 0, "[%5lu.000000] ", (unsigned long)ts);

	return sprintf(buf, "[%5lu.%06lu] ",
		       (unsigned long)ts, rem_nsec / 1000);
}

#ifdef CONFIG_STLOG_PID
static size_t stlog_print_pid(const struct ringbuf *msg, char *buf)
{
	if (!stlog_pid)
		return 0;

	if (!buf)
		return snprintf(NULL, 0, "[%15s, %d] ", msg->comm, msg->pid);

	return sprintf(buf, "[%15s, %d] ", msg->comm, msg->pid);
}
#else
static size_t stlog_print_pid(const struct ringbuf *msg, char *buf)
{
	return 0;
}
#endif

#ifdef CONFIG_STLOG_CPU_ID
static size_t stlog_print_cpuid(const struct ringbuf *msg, char *buf)
{

	if (!stlog_cpu_id)
		return 0;

	if (!buf)
		return snprintf(NULL, 0, "C%d ", msg->cpu_id);

	return sprintf(buf, "C%d ", msg->cpu_id);
}
#else
static size_t stlog_print_cpuid(const struct ringbuf *msg, char *buf)
{
	return 0;
}
#endif

static size_t stlog_print_prefix(const struct ringbuf *msg, bool ringbuf, char *buf)
{
	size_t len = 0;

	len += stlog_print_time(msg->ts_nsec, buf ? buf + len : NULL);
	len += stlog_print_cpuid(msg, buf ? buf + len : NULL);
	len += stlog_print_pid(msg, buf ? buf + len : NULL);
	return len;
}

static size_t stlog_print_text(const struct ringbuf *msg, enum ringbuf_flags prev,
			     bool ringbuf, char *buf, size_t size)
{
	const char *text = ringbuf_text(msg);
	size_t text_size = msg->text_len;
	bool prefix = true;
	bool newline = true;
	size_t len = 0;

	do {
		const char *next = memchr(text, '\n', text_size);
		size_t text_len;

		if (next) {
			text_len = next - text;
			next++;
			text_size -= next - text;
		} else {
			text_len = text_size;
		}

		if (buf) {
			if (stlog_print_prefix(msg, ringbuf, NULL) + text_len + 1 >= size - len)
				break;

			if (prefix)
				len += stlog_print_prefix(msg, ringbuf, buf + len);
			memcpy(buf + len, text, text_len);
			len += text_len;
			if (next || newline)
				buf[len++] = '\n';
		} else {
			/*  buffer size only calculation */
			if (prefix)
				len += stlog_print_prefix(msg, ringbuf, NULL);
			len += text_len;
			if (next || newline)
				len++;
		}

		prefix = true;
		text = next;
	} while (text);

	return len;
}

static int stlog_print(char __user *buf, int size)
{
	char *text;
	struct ringbuf *msg;
	int len = 0;

	#ifdef	DEBUG_STLOG
	printk("[STLOG] %s stlog_seq %llu stlog_idx %lu ringbuf_first_seq %llu ringbuf_first_idx %lu ringbuf_next_seq %llu ringbuf_next_idx %lu \n",
			__func__,stlog_seq,stlog_idx,ringbuf_first_seq,ringbuf_first_idx,ringbuf_next_seq,ringbuf_next_idx);
	#endif

	text = kmalloc(RINGBUF_LINE_MAX + S_PREFIX_MAX, GFP_KERNEL);
	if (!text)
		return -ENOMEM;

	while (size > 0) {
		size_t n;

		raw_spin_lock_irq(&ringbuf_lock);
		if (stlog_seq < ringbuf_first_seq) {
			/* messages are gone, move to first one */
			stlog_seq = ringbuf_first_seq;
			stlog_idx = ringbuf_first_idx;
			ringbuf_prev = 0;
		}
		if (stlog_seq == ringbuf_next_seq) {
			raw_spin_unlock_irq(&ringbuf_lock);
			break;
		}

		msg = ringbuf_from_idx(stlog_idx);
		n = stlog_print_text(msg, ringbuf_prev, false, text, RINGBUF_LINE_MAX + S_PREFIX_MAX);
		if (n <= size) {
			/* message fits into buffer, move forward */
			stlog_idx = ringbuf_next(stlog_idx);
			stlog_seq++;
			ringbuf_prev = msg->flags;
		} else if (!len){
			n = size;
		} else
			n = 0;
		raw_spin_unlock_irq(&ringbuf_lock);

		if (!n)
			break;

		if (copy_to_user(buf, text, n)) {
			if (!len)
				len = -EFAULT;
			break;
		}

		len += n;
		size -= n;
		buf += n;
	}

	kfree(text);
	return len;
}

static int stlog_print_all(char __user *buf, int size)
{
	char *text;
	int len = 0;
	u64 seq = ringbuf_next_seq;
	u32 idx = ringbuf_next_idx;

	if (stlog_end_seq == -1)
		stlog_end_seq = ringbuf_next_seq;

	text = kmalloc(RINGBUF_LINE_MAX + S_PREFIX_MAX, GFP_KERNEL);
	if (!text)
		return -ENOMEM;
	raw_spin_lock_irq(&ringbuf_lock);
	if (buf) {
		enum ringbuf_flags prev = 0;

		if (stlog_clear_seq < ringbuf_first_seq) {
			/* messages are gone, move to first available one */
			stlog_clear_seq = ringbuf_first_seq;
			stlog_clear_idx = ringbuf_first_idx;
		}

		seq = stlog_clear_seq;
		idx = stlog_clear_idx;

		while (seq < stlog_end_seq) {
			struct ringbuf *msg = ringbuf_from_idx(idx);
			int textlen;

			textlen = stlog_print_text(msg, prev, false, text,
						 RINGBUF_LINE_MAX + S_PREFIX_MAX);
			if (textlen < 0) {
				len = textlen;
				break;
			} else if(len + textlen > size) {
				break;
			}
			idx = ringbuf_next(idx);
			seq++;
			prev = msg->flags;

			raw_spin_unlock_irq(&ringbuf_lock);
			if (copy_to_user(buf + len, text, textlen))
				len = -EFAULT;
			else
				len += textlen;
			raw_spin_lock_irq(&ringbuf_lock);

			if (seq < ringbuf_first_seq) {
				/* messages are gone, move to next one */
				seq = ringbuf_first_seq;
				idx = ringbuf_first_idx;
				prev = 0;
			}
		}
	}

	stlog_clear_seq = seq;
	stlog_clear_idx = idx;

	raw_spin_unlock_irq(&ringbuf_lock);

	kfree(text);
	return len;
}

int do_stlog(int type, char __user *buf, int len, bool from_file)
{
	int error=0;

	switch (type) {
	case STLOG_ACTION_CLOSE:	/* Close log */
		break;
	case STLOG_ACTION_OPEN:	/* Open log */

	#ifdef	DEBUG_STLOG
		printk("[STLOG] %s OPEN stlog_seq %llu stlog_idx %lu ringbuf_first_seq %llu ringbuf_first_idx %lu ringbuf_next_seq %llu ringbuf_next_idx %lu \n",
				__func__,stlog_seq,stlog_idx,ringbuf_first_seq,ringbuf_first_idx,ringbuf_next_seq,ringbuf_next_idx);
	#endif
		break;
	case STLOG_ACTION_READ:	/* cat -f /proc/stlog */
		#ifdef	DEBUG_STLOG
		printk("[STLOG] %s READ stlog_seq %llu stlog_idx %lu ringbuf_first_seq %llu ringbuf_first_idx %lu ringbuf_next_seq %llu ringbuf_next_idx %lu \n",
				__func__,stlog_seq,stlog_idx,ringbuf_first_seq,ringbuf_first_idx,ringbuf_next_seq,ringbuf_next_idx);
		#endif
		error = -EINVAL;
		if (!buf || len < 0)
			goto out;
		if (!len)
			goto out;
		if (!access_ok(VERIFY_WRITE, buf, len)) {
			error = -EFAULT;
			goto out;
		}
		error = wait_event_interruptible(ringbuf_wait,
			stlog_seq != ringbuf_next_seq);
		if (error)
			goto out;
		error = stlog_print(buf, len);
		break;
	case STLOG_ACTION_READ_ALL: /* cat /proc/stlog */ /* dumpstate */
		#ifdef	DEBUG_STLOG
		printk("[STLOG] %s READ_ALL stlog_seq %llu stlog_idx %lu ringbuf_first_seq %llu ringbuf_first_idx %lu ringbuf_next_seq %llu ringbuf_next_idx %lu \n",
				__func__,stlog_seq,stlog_idx,ringbuf_first_seq,ringbuf_first_idx,ringbuf_next_seq,ringbuf_next_idx);
		#endif
		error = -EINVAL;
		if (!buf || len < 0)
			goto out;
		error = 0;
		if (!len)
			goto out;
		if (!access_ok(VERIFY_WRITE, buf, len)) {
			error = -EFAULT;
			goto out;
		}
		if(stlog_clear_seq==ringbuf_next_seq){
			stlog_clear_seq=ringbuf_first_seq;
			stlog_clear_idx=ringbuf_first_idx;
			stlog_end_seq = -1;
			error=0;
			goto out;
		}
		error = stlog_print_all(buf, len);
		if (error == 0)
			stlog_end_seq = -1;
		break;
	/* Size of the log buffer */
	case STLOG_ACTION_SIZE_BUFFER:
		#ifdef	DEBUG_STLOG
		printk("[STLOG] %s SIZE_BUFFER %lu\n",__func__,ringbuf_buf_len);
		#endif
		error = ringbuf_buf_len;
		break;
	default:
		error = -EINVAL;
		break;
	}
out:
	return error;
}


int do_stlog_write(int type, const char __user *buf, int len, bool from_file)
{
	int error=0;
	char *kern_buf=0;
	char *line=0;

	if (!buf || len < 0)
		goto out;
	if (!len)
		goto out;
	if (len > RINGBUF_LINE_MAX)
	return -EINVAL;

	kern_buf = kmalloc(len+1, GFP_KERNEL);
	if (kern_buf == NULL)
		return -ENOMEM;

	line = kern_buf;
	if (copy_from_user(line, buf, len)) {
		error = -EFAULT;
		goto out;
	}

	line[len] = '\0';
	error = stlog("%s", line);
	if ((line[len-1] == '\n') && (error == (len-1)))
		error++;
out:
	kfree(kern_buf);
	return error;

}

asmlinkage int vstlog(const char *fmt, va_list args)
{
	static char textbuf[RINGBUF_LINE_MAX];
	char *text = textbuf;
	size_t text_len;
	enum ringbuf_flags lflags = 0;
	unsigned long flags;
	int this_cpu;
	int printed_len = 0;
	bool stored = false;

	local_irq_save(flags);
	this_cpu = smp_processor_id();

	raw_spin_lock(&ringbuf_lock);
	ringbuf_cpu = this_cpu;

	text_len = vscnprintf(text, sizeof(textbuf), fmt, args);


	/* mark and strip a trailing newline */
	if (text_len && text[text_len-1] == '\n') {
		text_len--;
		lflags |= RINGBUF_NEWLINE;
	}

	if (!stored)
		ringbuf_store(lflags,text, text_len, ringbuf_cpu, current);

	printed_len += text_len;

	raw_spin_unlock(&ringbuf_lock);
	local_irq_restore(flags);


	return printed_len;
}

EXPORT_SYMBOL(vstlog);

/**
 * stlog - print a storage message
 * @fmt: format string
 */
asmlinkage int stlog(const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	r = vstlog(fmt, args);

	va_end(args);

	return r;
}
EXPORT_SYMBOL(stlog);



