#ifndef _SAMSUNG_AUDIO_DEBGFS_H
#define _SAMSUNG_AUDIO_DEBGFS_H

#include <linux/debugfs.h>
#include <sound/soc.h>

#define MAX_NUM_DEBUG_MSG 20
#define MAX_DEBUG_MSG_SIZE 200

#define MAX_NUM_REGDUMP 10
#define MAX_NUM_REGDUMP_MSG 500
#define MAX_REGDUMP_SIZE 30

#define MAX_NUM_STACKDUMP 10
#define MAX_NUM_STACKDUMP_MSG 10
#define MAX_STACKDUMP_SIZE 200

struct audio_log_data {
	int count;
	int full;
	char audio_log[MAX_NUM_DEBUG_MSG][MAX_DEBUG_MSG_SIZE];
};

struct audio_regdump_data {
	int created;
	int id;
	int num_regs;
	char reg_dump[MAX_NUM_REGDUMP_MSG][MAX_REGDUMP_SIZE];
};

struct audio_stackdump_data {
	int created;
	int id;
	int num_stacks;
	char stack_dump[MAX_NUM_STACKDUMP_MSG][MAX_STACKDUMP_SIZE];
};

#ifdef CONFIG_SND_SAMSUNG_DEBUGFS
extern struct snd_soc_codec *dump_codec;

extern void audio_debugfs_log(const char *fmt, ...);
extern int audio_debugfs_regdump(int start_reg, int end_reg);
extern int audio_debugfs_stackdump(void);
#else
#define audio_debugfs_log(fmt, ...)
#define audio_debugfs_regdump(start, end)	-1
#define audio_debugfs_stackdump()	-1
#endif

#endif /*_SAMSUNG_AUDIO_DEBGFS_H*/
