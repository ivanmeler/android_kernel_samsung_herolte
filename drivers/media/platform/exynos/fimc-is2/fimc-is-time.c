/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/time.h>

#include "fimc-is-time.h"
#include "fimc-is-groupmgr.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-ischain.h"

static struct timeval itime[10];

#define JITTER_CNT 50
static u64 jitter_array[JITTER_CNT];
static u64 jitter_prio;
static u64 jitter_cnt = 0;

void TIME_STR(unsigned int index)
{
	do_gettimeofday(&itime[index]);
}

void TIME_END(unsigned int index, const char *name)
{
	u32 time;
	struct timeval temp;

	do_gettimeofday(&temp);
	time = (temp.tv_sec - itime[index].tv_sec)*1000000 +
		(temp.tv_usec - itime[index].tv_usec);

	info("TIME_MEASURE(%s) : %dus\n", name, time);
}

void fimc_is_jitter(u64 timestamp)
{
	if (jitter_cnt == 0) {
		jitter_prio = timestamp;
		jitter_cnt++;
		return;
	}

	jitter_array[jitter_cnt-1] = timestamp - jitter_prio;
	jitter_prio = timestamp;

	if (jitter_cnt >= JITTER_CNT) {
		u64 i, variance, tot = 0, square_tot = 0, avg = 0, square_avg = 0;;

		for (i = 0; i < JITTER_CNT; ++i) {
			tot += jitter_array[i];
			square_tot += (jitter_array[i] * jitter_array[i]);
		}

		avg = tot / JITTER_CNT;
		square_avg = square_tot / JITTER_CNT;
		variance = square_avg - (avg * avg);

		info("[TIM] variance : %lld, average : %lld\n", variance, avg);
		jitter_cnt = 0;
	} else {
		jitter_cnt++;
	}
}

u64 fimc_is_get_timestamp(void)
{
	struct timespec curtime;

	do_posix_clock_monotonic_gettime(&curtime);

	return (u64)curtime.tv_sec*1000000000 + curtime.tv_nsec;
}

u64 fimc_is_get_timestamp_boot(void)
{
	struct timespec curtime;

	curtime = ktime_to_timespec(ktime_get_boottime());

	return (u64)curtime.tv_sec*1000000000 + curtime.tv_nsec;
}

static inline u32 fimc_is_get_time(struct timeval *str, struct timeval *end)
{
	return (end->tv_sec - str->tv_sec)*1000000 + (end->tv_usec - str->tv_usec);
}

#ifdef MEASURE_TIME
#ifdef MONITOR_TIME
void monitor_init(struct fimc_is_time *time)
{
	time->time_count = 0;
	time->time1_min = 0;
	time->time1_max = 0;
	time->time1_tot = 0;
	time->time2_min = 0;
	time->time2_max = 0;
	time->time2_tot = 0;
	time->time3_min = 0;
	time->time3_max = 0;
	time->time3_tot = 0;
	time->time4_cur = 0;
	time->time4_old = 0;
	time->time4_tot = 0;
}

void monitor_period(struct fimc_is_time *time,
	u32 report_period)
{
	time->report_period = report_period;
}

static void monitor_report(void *group_data,
	void *frame_data)
{
	u32 index, shotindex;
	u32 fduration, ctime, dtime;
	u32 temp1, temp2, temp3, temp4, temp;
	struct fimc_is_monitor *mp;
	struct fimc_is_device_ischain *device;
	struct fimc_is_group *group;
	struct fimc_is_frame *frame;
	struct fimc_is_time *time;
	bool valid = true;

	BUG_ON(!group_data);
	BUG_ON(!frame_data);

	group = group_data;
	device = group->device;
	time = &group->time;
	frame = frame_data;
	mp = frame->mpoint;
	fduration = (1000000 / fimc_is_sensor_g_framerate(device->sensor)) + MONITOR_TIMEOUT;
	ctime = 15000;
	dtime = 15000;

	/* Q interval */
	if (!frame->result && mp[TMM_QUEUE].check && mp[TMM_START].check) {
		temp1 = fimc_is_get_time(&mp[TMM_QUEUE].time, &mp[TMM_START].time);
		if (temp1 > fduration) {
			mgrinfo("[TIM] Q(%dus > %dus)\n", device, group, frame, temp1, fduration);
		}
	} else {
		valid = false;
	}

	/* Callback interval */
	if (!frame->result && mp[TMM_SCALL].check && mp[TMM_ECALL].check) {
		temp2 = fimc_is_get_time(&mp[TMM_SCALL].time, &mp[TMM_ECALL].time);
		if (temp2 > ctime) {
			mgrinfo("[TIM] C(%dus > %dus)\n", device, group, frame, temp2, ctime);
			for (index = TMM_SCALL; index < TMM_ECALL; ++index) {
				temp = fimc_is_get_time(&mp[index].time, &mp[index + 1].time);
				mgrinfo("[TIM] %02d-%02d(%dus)\n", device, group, frame, index, index + 1, temp);
			}
		}
	} else {
		valid = false;
	}

	/* Shot interval */
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		shotindex = TMM_SHOT2;
	} else {
		shotindex = TMM_SHOT1;
	}

	if (!frame->result && mp[shotindex].check && mp[TMM_SDONE].check) {
		temp3 = fimc_is_get_time(&mp[shotindex].time, &mp[TMM_SDONE].time);
		if (temp3 > fduration) {
			mgrinfo("[TIM] S(%dus > %dus)\n", device, group, frame, temp3, fduration);
		}
	} else {
		valid = false;
	}

	/* Deque interval */
	if (!frame->result && mp[TMM_SDONE].check && mp[TMM_DEQUE].check) {
		temp4 = fimc_is_get_time(&mp[TMM_SDONE].time, &mp[TMM_DEQUE].time);
		if (temp4 > dtime) {
			mgrinfo("[TIM] D(%dus > %dus)\n", device, group, frame, temp4, dtime);
			for (index = TMM_SDONE; index < TMM_DEQUE; ++index) {
				temp = fimc_is_get_time(&mp[index].time, &mp[index + 1].time);
				mgrinfo("[TIM] %02d-%02d(%dus)\n", device, group, frame, index, index + 1, temp);
			}
		}
	} else {
		valid = false;
	}

	for (index = 0; index < TMM_END; ++index)
		mp[index].check = false;

	if (!valid)
		return;

#ifdef MONITOR_REPORT
	temp1 += temp2;
	temp2 = temp3;
	temp3 = temp4;

	if (!time->time_count) {
		time->time1_min = temp1;
		time->time1_max = temp1;
		time->time2_min = temp2;
		time->time2_max = temp2;
		time->time3_min = temp3;
		time->time3_max = temp3;
	} else {
		if (time->time1_min > temp1)
			time->time1_min = temp1;

		if (time->time1_max < temp1)
			time->time1_max = temp1;

		if (time->time2_min > temp2)
			time->time2_min = temp2;

		if (time->time2_max < temp2)
			time->time2_max = temp2;

		if (time->time3_min > temp3)
			time->time3_min = temp3;

		if (time->time3_max < temp3)
			time->time3_max = temp3;
	}

	time->time1_tot += temp1;
	time->time2_tot += temp2;
	time->time3_tot += temp3;

	time->time4_cur = mp[TMM_QUEUE].time.tv_sec * 1000000 + mp[TMM_QUEUE].time.tv_usec;
	time->time4_tot += (time->time4_cur - time->time4_old);
	time->time4_old = time->time4_cur;

	time->time_count++;

	if (time->time_count % time->report_period)
		return;

	mginfo("[TIM] Q(%05d,%05d,%05d), S(%05d,%05d,%05d), D(%05d,%05d,%05d) : %d(%dfps)\n",
		device, group,
		temp1, time->time1_max, time->time1_tot / time->time_count,
		temp2, time->time2_max, time->time2_tot / time->time_count,
		temp3, time->time3_max, time->time3_tot / time->time_count,
		time->time4_tot / time->report_period,
		(1000000 * time->report_period) / time->time4_tot);

	time->time_count = 0;
	time->time1_tot = 0;
	time->time2_tot = 0;
	time->time3_tot = 0;
	time->time4_tot = 0;
#endif
}
#endif

#ifdef INTERFACE_TIME
void measure_init(struct fimc_is_interface_time *time, u32 cmd)
{
	time->cmd = cmd;
	time->time_max = 0;
	time->time_min = 0;
	time->time_tot = 0;
	time->time_cnt = 0;
}

void measure_time(struct fimc_is_interface_time *time,
	u32 instance,
	u32 group,
	struct timeval *start,
	struct timeval *end)
{
	u32 temp;

	temp = (end->tv_sec - start->tv_sec)*1000000 + (end->tv_usec - start->tv_usec);

	if (time->time_cnt) {
		time->time_max = temp;
		time->time_min = temp;
	} else {
		if (time->time_min > temp)
			time->time_min = temp;

		if (time->time_max < temp)
			time->time_max = temp;
	}

	time->time_tot += temp;
	time->time_cnt++;

	pr_info("cmd[%d][%d](%d) : curr(%d), max(%d), avg(%d)\n",
		instance, group, time->cmd, temp, time->time_max, time->time_tot / time->time_cnt);
}
#endif
#endif

void monitor_point(void *group_data,
	void *frame_data,
	u32 mpoint)
{
	struct fimc_is_group *group;

	BUG_ON(!group_data);
	BUG_ON(!frame_data);

	group = group_data;
#if defined(MEASURE_TIME) && defined(MONITOR_TIME)
	{
		struct fimc_is_frame *frame;
		struct fimc_is_monitor *point;

		frame = frame_data;
		point = &frame->mpoint[mpoint];

		do_gettimeofday(&point->time);
		/* only group thread postion */
		if (mpoint >= 2 && mpoint <= 12)
			group->pcount = mpoint;
		point->pcount = mpoint;
		point->check = true;

		if (mpoint == TMM_DEQUE)
			monitor_report(group_data, frame_data);
	}
#else
	group->pcount = mpoint;
#endif
}
