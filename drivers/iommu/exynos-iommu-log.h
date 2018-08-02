/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Data structure definition for Exynos IOMMU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _EXYNOS_IOMMU_LOG_H_
#define _EXYNOS_IOMMU_LOG_H_

#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>
#include <linux/device.h>

enum sysmmu_event_log_event {
	EVENT_SYSMMU_NONE, /* initialized value */
	EVENT_SYSMMU_ENABLE,
	EVENT_SYSMMU_DISABLE,
	EVENT_SYSMMU_TLB_INV_RANGE,
	EVENT_SYSMMU_TLB_INV_VPN,
	EVENT_SYSMMU_TLB_INV_ALL,
	EVENT_SYSMMU_FLPD_FLUSH,
	EVENT_SYSMMU_DF,
	EVENT_SYSMMU_DF_UNLOCK,
	EVENT_SYSMMU_DF_UNLOCK_ALL,
	EVENT_SYSMMU_PBLMM,
	EVENT_SYSMMU_PBSET,
	EVENT_SYSMMU_BLOCK, /* TODO: consider later */
	EVENT_SYSMMU_UNBLOCK, /* TODO: consider later */
#ifdef CONFIG_PM_RUNTIME
	EVENT_SYSMMU_POWERON,
	EVENT_SYSMMU_POWEROFF,
#endif
	EVENT_SYSMMU_IOMMU_ATTACH,
	EVENT_SYSMMU_IOMMU_DETACH,
	EVENT_SYSMMU_IOMMU_MAP,
	EVENT_SYSMMU_IOMMU_UNMAP,
	EVENT_SYSMMU_IOMMU_ALLOCSLPD,
	EVENT_SYSMMU_IOMMU_FREESLPD,
	EVENT_SYSMMU_IOVMM_MAP,
	EVENT_SYSMMU_IOVMM_UNMAP
};

struct sysmmu_event_range {
	u32 start;
	u32 end;
};

struct sysmmu_event_PBLMM {
	u32	lmm;
	u32	buf_num;
};
struct sysmmu_event_PBSET {
	u32	config;
	u32	start;
	u32	end;
};

struct sysmmu_event_IOMMU_MAP {
	u32	start;
	u32	end;
	unsigned int	pfn;
};

struct sysmmu_event_IOVMM_MAP {
	u32	start;
	u32	end;
	unsigned int	dummy;
};

/**
 * event must be updated before eventdata because of eventdata.dev
 * sysmmu_event_log is not protected by any locks. That means it permits
 * some data inconsistency by race condition between updating and reading.
 * However the problem arises when event is either IOMMU_ATTACH or
 * IOMMU_DETACH because they stores a pointer to device descriptor to
 * eventdata.dev and reading the sysmmu_event_log of those events refers
 * to values pointed by eventdata.dev.
 * Therefore, eventdata must be updated before event not to access invalid
 * pointer by reading debugfs entries.
 */
struct sysmmu_event_log {
	ktime_t				timestamp;
	union {
		struct sysmmu_event_range	range;
		struct sysmmu_event_PBLMM	pblmm;
		struct sysmmu_event_PBSET	pbset;
		struct sysmmu_event_IOMMU_MAP	iommu;
		struct sysmmu_event_IOVMM_MAP	iovmm;
		u32			addr;
		struct device			*dev;
	} eventdata;
	enum sysmmu_event_log_event	event;
};

struct exynos_iommu_event_log {
	atomic_t index;
	unsigned int log_len;
	struct sysmmu_event_log *log;
	struct dentry *debugfs_root;
};

/* sizeof(struct sysmmu_event_log) = 8 + 4 * 3 + 4 = 24 bytes */
#define SYSMMU_LOG_LEN 1024
#define IOMMU_LOG_LEN 4096
#define IOVMM_LOG_LEN 512

#ifdef CONFIG_EXYNOS_IOMMU_EVENT_LOG

#define SYSMMU_DRVDATA_TO_LOG(data) (&(data)->log)
#define IOMMU_PRIV_TO_LOG(data) (&(data)->log)
#define IOMMU_TO_LOG(data) (&((struct exynos_iommu_domain *)(data)->priv)->log)
#define IOVMM_TO_LOG(data) (&(data)->log)

static inline struct sysmmu_event_log *sysmmu_event_log_get(
					struct exynos_iommu_event_log *plog)
{
	struct sysmmu_event_log *log;
	unsigned int index =
		(unsigned int)atomic_inc_return(&plog->index) - 1;
	log = &plog->log[index % plog->log_len];
	log->timestamp = ktime_get();
	return log;
}

#define DEFINE_SYSMMU_EVENT_LOG(evt)					\
static inline void SYSMMU_EVENT_LOG_##evt(struct exynos_iommu_event_log *plog) \
{									\
	struct sysmmu_event_log *log = sysmmu_event_log_get(plog);	\
	log->event = EVENT_SYSMMU_##evt;				\
}

#define DEFINE_SYSMMU_EVENT_LOG_1ADDR(evt)				\
static inline void SYSMMU_EVENT_LOG_##evt(				\
		struct exynos_iommu_event_log *plog, u32 addr)		\
{									\
	struct sysmmu_event_log *log = sysmmu_event_log_get(plog);	\
	log->eventdata.addr = addr;					\
	log->event = EVENT_SYSMMU_##evt;				\
}

#define DEFINE_SYSMMU_EVENT_LOG_2ADDR(evt)				\
static inline void SYSMMU_EVENT_LOG_##evt(struct exynos_iommu_event_log *plog, \
				u32 start, u32 end)			\
{									\
	struct sysmmu_event_log *log = sysmmu_event_log_get(plog);	\
	log->eventdata.range.start = start;				\
	log->eventdata.range.end = end;					\
	log->event = EVENT_SYSMMU_##evt;				\
}

/* MMU_CFG is stored at pblmm.lmm for System MMU 3.1 and 3.2 */
static inline void SYSMMU_EVENT_LOG_PBLMM(struct exynos_iommu_event_log *plog,
					  u32 lmm, u32 buf_num)
{
	struct sysmmu_event_log *log = sysmmu_event_log_get(plog);
	log->eventdata.pblmm.lmm = lmm;
	log->eventdata.pblmm.buf_num = buf_num;
	log->event = EVENT_SYSMMU_PBLMM;
}

/* PB index is stored at pbset.config for System MMU 3.1 and 3.2 */
static inline void SYSMMU_EVENT_LOG_PBSET(struct exynos_iommu_event_log *plog,
					  u32 config, u32 start, u32 end)
{
	struct sysmmu_event_log *log = sysmmu_event_log_get(plog);
	log->eventdata.pbset.config = config;
	log->eventdata.pbset.start = start;
	log->eventdata.pbset.end = end;
	log->event = EVENT_SYSMMU_PBSET;
}

static inline void SYSMMU_EVENT_LOG_IOVMM_MAP(
				struct exynos_iommu_event_log *plog,
				u32 start, u32 end, unsigned int dummy)
{
	struct sysmmu_event_log *log = sysmmu_event_log_get(plog);
	log->eventdata.iovmm.start = start;
	log->eventdata.iovmm.end = end;
	log->eventdata.iovmm.dummy = dummy;
	log->event = EVENT_SYSMMU_IOVMM_MAP;
}

static inline void SYSMMU_EVENT_LOG_IOMMU_ATTACH(
			struct exynos_iommu_event_log *plog, struct device *dev)
{
	struct sysmmu_event_log *log = sysmmu_event_log_get(plog);
	log->eventdata.dev = dev;
	log->event = EVENT_SYSMMU_IOMMU_ATTACH;
}

static inline void SYSMMU_EVENT_LOG_IOMMU_DETACH(
			struct exynos_iommu_event_log *plog, struct device *dev)
{
	struct sysmmu_event_log *log = sysmmu_event_log_get(plog);
	log->eventdata.dev = dev;
	log->event = EVENT_SYSMMU_IOMMU_DETACH;
}

static inline void SYSMMU_EVENT_LOG_IOMMU_MAP(
				struct exynos_iommu_event_log *plog,
				u32 start, u32 end, unsigned int pfn)
{
	struct sysmmu_event_log *log = sysmmu_event_log_get(plog);
	log->event = EVENT_SYSMMU_IOMMU_MAP;
	log->eventdata.iommu.start = start;
	log->eventdata.iommu.end = end;
	log->eventdata.iommu.pfn = pfn;
}

int exynos_iommu_init_event_log(struct exynos_iommu_event_log *log,
				unsigned int log_len);

void sysmmu_add_log_to_debugfs(struct dentry *debugfs_root,
			struct exynos_iommu_event_log *log, const char *name);

void iommu_add_log_to_debugfs(struct dentry *debugfs_root,
			struct exynos_iommu_event_log *log, const char *name);

#if defined(CONFIG_EXYNOS_IOVMM)
void iovmm_add_log_to_debugfs(struct dentry *debugfs_root,
			struct exynos_iommu_event_log *log, const char *name);
#else
#define iovmm_add_log_to_debugfs(debugfs_root, log, name) do { } while (0)
#endif

#else /* !CONFIG_EXYNOS_IOMMU_EVENT_LOG */

#define SYSMMU_DRVDATA_TO_LOG(data) NULL
#define IOMMU_PRIV_TO_LOG(data) NULL
#define IOMMU_TO_LOG(data) NULL
#define IOVMM_TO_LOG(data) NULL

static inline int exynos_iommu_init_event_log(
		struct exynos_iommu_event_log *log, unsigned int log_len)
{
	return 0;
}

#define iovmm_add_log_to_debugfs(debugfs_root, log, name) do { } while (0)

#define DEFINE_SYSMMU_EVENT_LOG(event)					\
static inline void SYSMMU_EVENT_LOG_##event(				\
				struct exynos_iommu_event_log *plog)	\
{									\
}

#define DEFINE_SYSMMU_EVENT_LOG_1ADDR(event)				\
static inline void SYSMMU_EVENT_LOG_##event(				\
		struct exynos_iommu_event_log *plog, u32 start)		\
{									\
}

#define DEFINE_SYSMMU_EVENT_LOG_2ADDR(event)				\
static inline void SYSMMU_EVENT_LOG_##event(				\
		struct exynos_iommu_event_log *plog, u32 start, u32 end) \
{									\
}

static inline void SYSMMU_EVENT_LOG_PBLMM(struct exynos_iommu_event_log *plog,
			u32 lmm, u32 buf_num)
{
}

static inline void SYSMMU_EVENT_LOG_PBSET(struct exynos_iommu_event_log *plog,
			u32 config, u32 start, u32 end)
{
}

static inline void SYSMMU_EVENT_LOG_IOMMU_MAP(
				struct exynos_iommu_event_log *plog,
				u32 start, u32 end, unsigned int pfn)
{
}

static inline void SYSMMU_EVENT_LOG_IOVMM_MAP(
				struct exynos_iommu_event_log *plog,
				u32 start, u32 end, size_t dummy)
{
}

static inline void SYSMMU_EVENT_LOG_IOMMU_ATTACH(
			struct exynos_iommu_event_log *plog, struct device *dev)
{
}

static inline void SYSMMU_EVENT_LOG_IOMMU_DETACH(
			struct exynos_iommu_event_log *plog, struct device *dev)
{
}

#define sysmmu_add_log_to_debugfs(debugfs_root, log, name) do { } while (0)
#define iommu_add_log_to_debugfs(debugfs_root, log, name) do { } while (0)
#define iovmm_add_log_to_debugfs(debugfs_root, log, name) do { } while (0)

#endif /* CONFIG_EXYNOS_IOMMU_EVENT_LOG */

DEFINE_SYSMMU_EVENT_LOG(ENABLE)
DEFINE_SYSMMU_EVENT_LOG(DISABLE)
DEFINE_SYSMMU_EVENT_LOG(TLB_INV_ALL)
DEFINE_SYSMMU_EVENT_LOG(DF_UNLOCK_ALL)
DEFINE_SYSMMU_EVENT_LOG(BLOCK)
DEFINE_SYSMMU_EVENT_LOG(UNBLOCK)
#ifdef CONFIG_PM_RUNTIME
DEFINE_SYSMMU_EVENT_LOG(POWERON)
DEFINE_SYSMMU_EVENT_LOG(POWEROFF)
#endif

DEFINE_SYSMMU_EVENT_LOG_1ADDR(TLB_INV_VPN)
DEFINE_SYSMMU_EVENT_LOG_1ADDR(FLPD_FLUSH)
DEFINE_SYSMMU_EVENT_LOG_1ADDR(DF)
DEFINE_SYSMMU_EVENT_LOG_1ADDR(DF_UNLOCK)
DEFINE_SYSMMU_EVENT_LOG_1ADDR(IOMMU_ALLOCSLPD)
DEFINE_SYSMMU_EVENT_LOG_1ADDR(IOMMU_FREESLPD)

DEFINE_SYSMMU_EVENT_LOG_2ADDR(TLB_INV_RANGE)
DEFINE_SYSMMU_EVENT_LOG_2ADDR(IOMMU_UNMAP)
DEFINE_SYSMMU_EVENT_LOG_2ADDR(IOVMM_UNMAP)

#endif /*_EXYNOS_IOMMU_LOG_H_*/
