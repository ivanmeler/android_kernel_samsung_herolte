/*  stlog internals
 *
 */

#ifndef _LINUX_STLOG_H
#define  _LINUX_STLOG_H

#define STLOG_ACTION_CLOSE          	0
#define STLOG_ACTION_OPEN           	1
#define STLOG_ACTION_READ				2
#define STLOG_ACTION_READ_ALL			3
#define STLOG_ACTION_WRITE    		4
#define STLOG_ACTION_SIZE_BUFFER   	5

#define STLOG_FROM_READER           0
#define STLOG_FROM_PROC             1

int do_stlog(int type, char __user *buf, int count, bool from_file);
int do_stlog_write(int type, const char __user *buf, int count, bool from_file);
int vstlog(const char *fmt, va_list args);
int stlog(const char *fmt, ...);

#ifdef CONFIG_PROC_STLOG
#define ST_LOG(fmt,...) stlog(fmt,##__VA_ARGS__)	
#else
#define ST_LOG(fmt,...) 
#endif /* CONFIG_PROC_STLOG */


#endif /* _STLOG_H */
