/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_TIME_H
#define FIMC_IS_TIME_H

#include <linux/time.h>

/* #define MEASURE_TIME */
/* #define MONITOR_TIME */
#define MONITOR_TIMEOUT 5000 /* 5ms */
/* #define MONITOR_REPORT */
/* #define EXTERNAL_TIME */
/* #define INTERFACE_TIME */

#define INSTANCE_MASK   0x3

#define TM_FLITE_STR	0
#define TM_FLITE_END	1
#define TM_SHOT		2
#define TM_SHOT_D	3
#define TM_META_D	4
#define TM_MAX_INDEX	5

#define TMM_QUEUE	0
#define TMM_START	1
#define TMM_SCALL	7
#define TMM_SHOT1	11
#define TMM_ECALL	12
#define TMM_SDONE	13
#define TMM_DEQUE	15
#define TMM_SHOT2	16
#define TMM_END		17

struct fimc_is_monitor {
	struct timeval		time;
	u32			pcount;
	bool			check;
};

struct fimc_is_time {
	u32 report_period;
	u32 time_count;
	u32 time1_min;
	u32 time1_max;
	u32 time1_tot;
	u32 time2_min;
	u32 time2_max;
	u32 time2_tot;
	u32 time3_min;
	u32 time3_max;
	u32 time3_tot;
	u32 time4_cur;
	u32 time4_old;
	u32 time4_tot;
};

struct fimc_is_interface_time {
	u32 cmd;
	u32 time_tot;
	u32 time_min;
	u32 time_max;
	u32 time_cnt;
};

void TIME_STR(unsigned int index);
void TIME_END(unsigned int index, const char *name);
void fimc_is_jitter(u64 timestamp);
u64 fimc_is_get_timestamp(void);
u64 fimc_is_get_timestamp_boot(void);
#define PROGRAM_COUNT(count) monitor_point(group, frame, count)
void monitor_point(void *group_data, void *frame_data, u32 mpoint);

#ifdef MEASURE_TIME
#ifdef MONITOR_TIME
void monitor_init(struct fimc_is_time *time);
void monitor_period(struct fimc_is_time *time, u32 report_period);
#endif
#ifdef INTERFACE_TIME
void measure_init(struct fimc_is_interface_time *time, u32 cmd);
void measure_time(struct fimc_is_interface_time *time,
	u32 instance,
	u32 group,
	struct timeval *start,
	struct timeval *end);
#endif
#endif

#endif
