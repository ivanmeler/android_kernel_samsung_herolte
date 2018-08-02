/* sound/soc/samsung/eax-dma.c
 *
 * Exynos Audio Mixer DMA driver
 *
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *	Yeongman Seo <yman.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/iommu.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/dma/dma-pl330.h>
#include <linux/interrupt.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/exynos.h>

#include "lpass.h"
#include "dma.h"
#include "eax.h"

#undef EAX_DMA_PCM_DUMP

#ifdef EAX_DMA_PCM_DUMP
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <trace/events/sched.h>

static int dump_count = 0;
static struct file *mfilp_uhqa = NULL;
static struct file *mfilp_normal = NULL;

#endif

#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
#define NMIXBUF_441_110_SIZE		110 /* PCM 16bit 2ch */
#define NMIXBUF_441_110_BYTE		(NMIXBUF_441_110_SIZE * 4) /* PCM 16bit 2ch */
#define NMIXBUF_441_441_SIZE		441 /* PCM 16bit 2ch */
#define NMIXBUF_441_441_BYTE		(NMIXBUF_441_441_SIZE * 4) /* PCM 16bit 2ch */
#endif

#define NMIXBUF_SIZE		120 /* PCM 16bit 2ch */
#define NMIXBUF_BYTE		(NMIXBUF_SIZE * 4) /* PCM 16bit 2ch */
#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
#define UMIXBUF_SIZE		512 /* Total 16384 byte / 2ch / 4byte / 4 periods */
#else
#define UMIXBUF_SIZE		480 /* Total 15360 byte / 2ch / 4byte / 4 periods */
#endif
#define UMIXBUF_BYTE		(UMIXBUF_SIZE * 8) /* PCM 32bit 2ch */
#define DMA_PERIOD_CNT		4
#define DMA_START_THRESHOLD	(DMA_PERIOD_CNT - 1)
#define SOUNDCAMP_HIGH_PERIOD_BYTE	(480 * 4)  /* PCM 16bit 2ch */

static const struct snd_pcm_hardware dma_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_BLOCK_TRANSFER |
				  SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S24_LE,
	.channels_min		= 1,
	.channels_max		= 8,
	.buffer_bytes_max	= 128 * 1024,
	.period_bytes_min	= 128,
	.period_bytes_max	= 64 * 1024,
	.periods_min		= 2,
	.periods_max		= 128,
	.fifo_size		= 32,
};

struct runtime_data {
	spinlock_t		lock;
	bool			running;
	struct snd_pcm_substream *substream;
	struct snd_soc_dai	*cpu_dai;
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	snd_pcm_format_t 	format;
	unsigned int 		rate;
#endif
	unsigned int		dma_period;
	dma_addr_t		dma_start;
	dma_addr_t		dma_pos;
	dma_addr_t		dma_end;
	u32			*dma_buf;
	short			*normal_dma_mono;
	int			*uhqa_dma_mono;
	unsigned long		dma_bytes;
#ifdef EAX_DMA_PCM_DUMP
        struct file             *filp;
        mm_segment_t            old_fs;
        char                    name[50];
#endif
};

struct mixer_info {
	spinlock_t		lock;
	struct snd_soc_dai	*cpu_dai;
	short			*nmix_buf;
	int			*umix_buf;
        unsigned long		mixbuf_size;
        unsigned long		mixbuf_byte;
	bool			is_uhqa;
	bool			buf_fill;
} mi;

struct buf_info {
	struct runtime_data	*prtd;
	struct list_head	node;
};

static LIST_HEAD(buf_list);

static void eax_mixer_interrupt_callback(void);
static DECLARE_WAIT_QUEUE_HEAD(mixer_run_wq);
static DECLARE_WAIT_QUEUE_HEAD(mixer_buf_wq);

static struct dma_info {
	spinlock_t		lock;
	struct mutex		mutex;
	struct snd_soc_dai	*cpu_dai;
	struct s3c_dma_params	*params;
	unsigned int		set_params_cnt;
	bool			params_init;
	bool			params_done;
	bool			prepare_done;
	bool			running;
	unsigned char		*buf_wr_p[DMA_PERIOD_CNT];
	int			buf_rd_idx;
	int			buf_wr_idx;
	u32			*dma_buf;
	unsigned int		dma_period;
	dma_addr_t		dma_start;
	dma_addr_t		dma_pos;
	dma_addr_t		dma_end;
} di;

static int eax_mixer_add(struct runtime_data *prtd);
static int eax_mixer_remove(struct runtime_data *prtd);
static void eax_mixer_trigger(bool on);
#ifdef CONFIG_SND_SAMSUNG_SEIREN_DMA
extern void *samsung_esa_dma_get_ops(void);
#endif

#ifdef EAX_DMA_PCM_DUMP
struct kobject *eax_mixer_kobj = NULL;

static int dump_enabled = 0;

static ssize_t show_dump_enabled(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 3, "%d\n", dump_enabled);
}

static ssize_t store_dump_enabled(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%1d", &input))
		return -EINVAL;

	dump_enabled = !!input;

	return count;
}

static struct kobj_attribute pcm_dump_attribute =
	__ATTR(pcm_dump, S_IRUGO | S_IWUSR, show_dump_enabled, store_dump_enabled);

static void open_file(struct runtime_data *prtd, char *filename)
{
    /* kernel memory access setting */
    prtd->old_fs = get_fs();
    set_fs(KERNEL_DS);

    /* open a file */
    prtd->filp = filp_open(filename, O_RDWR|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR);
    printk("Audio dump open %s\n",filename);
    if (!mfilp_uhqa)
	mfilp_uhqa = filp_open("/data/pcm/mix_buf_uhqa.raw",
			O_RDWR|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR);

    if (!mfilp_normal)
	mfilp_normal = filp_open("/data/pcm/mix_buf_normal.raw",
			O_RDWR|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR);

    if (IS_ERR(prtd->filp)) {
        printk("open error\n");
        return;
    }
    else {
        printk("open success\n");
    }
}

static void close_file(struct runtime_data *prtd)
{
    printk("Audio dump close %s\n", prtd->name);
    vfs_fsync(prtd->filp, 0);

    filp_close(prtd->filp, NULL);  /* filp_close(filp, current->files) ?  */
    /* restore kernel memory setting */
    set_fs(prtd->old_fs);
}
#endif

/* check_eax_dma_status
 *
 * EAX-DMA status is checked for AP Power mode.
 * return 1 : EAX-DMA is running.
 * return 0 : EAX-DMA is idle.
 */
int check_eax_dma_status(void)
{
	return di.running;
}

#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
static int eax_dma_is_uhqa(unsigned long dma_bytes)
{
	return (dma_bytes > (SOUNDCAMP_HIGH_PERIOD_BYTE * DMA_PERIOD_CNT));
}
#else
static int eax_dma_is_uhqa(snd_pcm_format_t format)
{
	return (SNDRV_PCM_FORMAT_S24_LE == format);
}
#endif

static inline bool eax_mixer_any_buf_running(void)
{
	struct buf_info *bi;

	if (!list_empty(&buf_list)) {
		list_for_each_entry(bi, &buf_list, node) {
			if (bi->prtd && bi->prtd->running)
				return true;
		}
	}

	return false;
}

static void eax_adma_alloc_buf(void)
{
#ifdef CONFIG_SND_SAMSUNG_IOMMU
	size_t size = 128 * 1024;
	struct iommu_domain *domain = lpass_get_iommu_domain();

	di.dma_buf = dma_alloc_coherent(di.cpu_dai->dev,
				size, &di.dma_start, GFP_KERNEL);
	iommu_map(domain, 0x48000000, di.dma_start, size, 0);
	di.dma_start = 0x48000000;
#else
	size_t size = 480 * DMA_PERIOD_CNT; /* default: 16bit 2ch */
	di.dma_buf = dma_alloc_coherent(di.cpu_dai->dev,
				size, &di.dma_start, GFP_KERNEL);
#endif
	memset(di.buf_wr_p, 0, sizeof(unsigned char *) * DMA_PERIOD_CNT);
}

static void eax_adma_free_buf(void)
{
#ifdef CONFIG_SND_SAMSUNG_IOMMU
	size_t size = 128 * 1024;
	struct iommu_domain *domain = lpass_get_iommu_domain();

	dma_free_coherent(di.cpu_dai->dev, size, (void *)di.dma_buf, di.dma_start);
	iommu_unmap(domain, 0x48000000, size);
	di.dma_start = 0;
#else
	size_t size = 480 * DMA_PERIOD_CNT; /* default: 16bit 2ch */
	dma_free_coherent(di.cpu_dai->dev, size, (void *)di.dma_buf, di.dma_start);
#endif
	memset(di.buf_wr_p, 0, sizeof(unsigned char *) * DMA_PERIOD_CNT);
}

int eax_dma_dai_register(struct snd_soc_dai *dai)
{
	spin_lock_init(&di.lock);
	mutex_init(&di.mutex);

	di.cpu_dai = dai;
	di.running = false;
	di.params_init = false;
	di.params_done = false;
	di.prepare_done = false;

	spin_lock_init(&mi.lock);
	mi.cpu_dai = dai;

	eax_adma_alloc_buf();

	return 0;
}

int eax_dma_dai_unregister(void)
{
	mutex_destroy(&di.mutex);

	eax_adma_free_buf();

	di.cpu_dai = NULL;
	di.running = false;
	di.params_init = false;
	di.params_done = false;
	di.prepare_done = false;

	mi.cpu_dai = NULL;

	return 0;
}

int eax_dma_params_register(struct s3c_dma_params *dma)
{
	di.params = dma;

	return 0;
}

static void eax_adma_buffdone(void *data)
{
	dma_addr_t src, dst, pos;

	if (!di.running || !di.params->ch || !di.params_init)
		return;

	di.params->ops->getposition(di.params->ch, &src, &dst);
	pos = src - di.dma_start;
	pos /= di.dma_period;
	di.buf_rd_idx = pos;
	pos = di.dma_start + (pos * di.dma_period);
	if (pos >= di.dma_end)
		pos = di.dma_start;

	di.dma_pos = pos;
	if (di.buf_rd_idx == di.buf_wr_idx)
	{
		if (--di.buf_wr_idx < 0)
			di.buf_wr_idx += DMA_PERIOD_CNT;
	}

	spin_lock(&mi.lock);
	eax_mixer_interrupt_callback();
	spin_unlock(&mi.lock);
}

static void eax_adma_hw_params(unsigned long dma_period_bytes)
{
	struct samsung_dma_req req;
	struct samsung_dma_config config;
	int n;

	mutex_lock(&di.mutex);

	if (di.params_done)
		goto out;

	di.params_done = true;

	if (!di.params_init) {
		di.params_init = true;
		di.params->ops = samsung_dma_get_ops();

		req.cap = DMA_CYCLIC;
		req.client = di.params->client;
		config.direction = DMA_MEM_TO_DEV;
		config.width = di.params->dma_size;
		config.fifo = di.params->dma_addr;
		di.params->ch = di.params->ops->request(di.params->channel,
				&req, di.cpu_dai->dev, di.params->ch_name);
		if (!di.params->ch) {
			pr_err("EAXDMA: Failed to request DMA channel %s\n",
				di.params->ch_name);
			return;
		}
		di.params->ops->config(di.params->ch, &config);
	}

	di.dma_period = dma_period_bytes;
	di.dma_pos = di.dma_start;
	di.dma_end = di.dma_start + di.dma_period * DMA_PERIOD_CNT;
	for (n = 0; n < DMA_PERIOD_CNT; n++) {
		di.buf_wr_p[n] = (unsigned char *)di.dma_buf;
		di.buf_wr_p[n] += dma_period_bytes * n;
	}

	pr_info("EAXDMA:DmaAddr=@%x Total=%d PrdSz=%d #Prds=%d dma_area=0x%p\n",
		(u32)di.dma_start, (u32)(di.dma_end - di.dma_start),
		di.dma_period, DMA_PERIOD_CNT, di.dma_buf);
out:
	mutex_unlock(&di.mutex);
}

static void eax_adma_hw_free(void)
{
	mutex_lock(&di.mutex);
	pr_info("Entered %s ++\n", __func__);

	if (di.running || eax_mixer_any_buf_running()) {
		pr_info("EAXADMA: some mixer channel is running, (%d), (%d)\n",
			di.running, eax_mixer_any_buf_running());
		goto out;
	}

	if (di.params_init && (di.set_params_cnt == 1)) {
		pr_info("EAXADMA: release dma channel : %s\n", di.params->ch_name);
		di.params_init = false;
		if (di.params->ch) {
			di.params->ops->flush(di.params->ch);
			di.params->ops->release(di.params->ch, di.params->client);
		}
	}

	di.params_done = false;
	di.prepare_done = false;

out:
	di.set_params_cnt--;
	pr_info("Entered %s --\n", __func__);
	mutex_unlock(&di.mutex);
}

static void eax_adma_prepare(unsigned long dma_period_bytes)
{
	struct samsung_dma_prep dma_info;

	mutex_lock(&di.mutex);

	if (di.prepare_done)
		goto out;

	if (!di.params_init || !di.params_done) {
		pr_err("EAXADMA: hw_params are not set. init = %d, done = %d\n",
			di.params_init, di.params_done);
		goto out;
	}

	di.prepare_done = true;

	/* zero fill */
	di.buf_wr_idx = 0;
	memset(di.dma_buf, 0, dma_period_bytes * DMA_PERIOD_CNT);

	/* prepare */
	if (di.params->ch)
		di.params->ops->flush(di.params->ch);
	di.dma_pos = di.dma_start;

	/* enqueue */
	dma_info.cap = DMA_CYCLIC;
	dma_info.direction = DMA_MEM_TO_DEV;
	dma_info.fp = eax_adma_buffdone;
	dma_info.fp_param = NULL;
	dma_info.period = di.dma_period;
	dma_info.len = di.dma_period * DMA_PERIOD_CNT;

	dma_info.buf = di.dma_pos;
	dma_info.infiniteloop = DMA_PERIOD_CNT;
	if (di.params->ch)
		di.params->ops->prepare(di.params->ch, &dma_info);
out:
	mutex_unlock(&di.mutex);
}

static void eax_adma_trigger(bool on)
{
	unsigned long flags;

	spin_lock_irqsave(&di.lock, flags);

	if (on) {
		di.running = on;
		lpass_dma_enable(true);
		lpass_inc_dram_usage_count();
		/* eax always uses dram */
		lpass_update_lpclock(LPCLK_CTRLID_LEGACY, true);
		if (di.params->ch)
			di.params->ops->trigger(di.params->ch);
	} else {
		if (di.params->ch)
			di.params->ops->stop(di.params->ch);
		lpass_dma_enable(false);
		lpass_dec_dram_usage_count();
		di.prepare_done = false;
		di.running = on;
		lpass_update_lpclock(LPCLK_CTRLID_LEGACY, false);
	}

	spin_unlock_irqrestore(&di.lock, flags);
}

static inline void eax_dma_xfer(struct runtime_data *prtd,
			short *npcm_l, short *npcm_r, int *upcm_l, int *upcm_r)
{
	dma_addr_t dma_pos;

#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	if (eax_dma_is_uhqa(prtd->dma_bytes)) {
#else
	if (eax_dma_is_uhqa(prtd->format)) {
#endif
		if (!prtd->uhqa_dma_mono || !upcm_l || !upcm_r) {
			pr_err("%s : UHQA DMA MONO Pointer is NULL\n", __func__);
			return;
		}
		*upcm_l = *prtd->uhqa_dma_mono++;
		*upcm_r = *prtd->uhqa_dma_mono++;
		dma_pos = prtd->dma_pos + 8;

		if (dma_pos == prtd->dma_end) {
			prtd->dma_pos = prtd->dma_start;
			prtd->uhqa_dma_mono = (int *)prtd->dma_buf;
		} else {
			prtd->dma_pos = dma_pos;
		}

		if (prtd->running &&
			((prtd->dma_pos - prtd->dma_start) % prtd->dma_period == 0))
			snd_pcm_period_elapsed(prtd->substream);
	} else {
		if (!prtd->normal_dma_mono || !npcm_l || !npcm_r) {
			pr_err("%s : NORMAL DMA MONO Pointer is NULL\n", __func__);
			return;
		}
		*npcm_l = *prtd->normal_dma_mono++;
		*npcm_r = *prtd->normal_dma_mono++;
		dma_pos = prtd->dma_pos + 4;

		if (dma_pos == prtd->dma_end) {
			prtd->dma_pos = prtd->dma_start;
			prtd->normal_dma_mono = (short *)prtd->dma_buf;
		} else {
			prtd->dma_pos = dma_pos;
		}

		if (prtd->dma_period > NMIXBUF_BYTE * 2) {
			if (prtd->running &&
				((prtd->dma_pos - prtd->dma_start) % prtd->dma_period == 0))
				snd_pcm_period_elapsed(prtd->substream);
		} else {
			if (prtd->running &&
				((prtd->dma_pos - prtd->dma_start) % prtd->dma_period == 0))
				snd_pcm_period_elapsed(prtd->substream);
		}
	}
}

static int eax_dma_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	unsigned long totbytes = params_buffer_bytes(params);
	unsigned long flags;
#ifdef EAX_DMA_PCM_DUMP
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
#endif

	pr_debug("Entered %s\n", __func__);

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = totbytes;

	spin_lock_irqsave(&prtd->lock, flags);
	prtd->dma_period = params_period_bytes(params);
	prtd->dma_start = runtime->dma_addr;
	prtd->dma_pos = prtd->dma_start;
	prtd->dma_end = prtd->dma_start + totbytes;
	prtd->dma_buf = (u32 *)(runtime->dma_area);
	prtd->dma_bytes = totbytes;
	if (eax_dma_is_uhqa(totbytes)) {
		prtd->uhqa_dma_mono = (int *)prtd->dma_buf;
		prtd->normal_dma_mono = NULL;
	} else {
		prtd->normal_dma_mono = (short *)prtd->dma_buf;
		prtd->uhqa_dma_mono = NULL;
	}
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	prtd->format = params_format(params);
	prtd->rate = params_rate(params);
#endif
	spin_unlock_irqrestore(&prtd->lock, flags);

	spin_lock_irqsave(&mi.lock, flags);
#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	if (mi.is_uhqa && !eax_dma_is_uhqa(prtd->dma_bytes))
		return -EINVAL;
#endif

#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	if (eax_dma_is_uhqa(prtd->dma_bytes)) {
#else
	if (eax_dma_is_uhqa(prtd->format)) {
#endif
		mi.is_uhqa = true;
#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
		mi.mixbuf_size = totbytes / DMA_PERIOD_CNT / 8;
		mi.mixbuf_byte = totbytes / DMA_PERIOD_CNT;
#else
		mi.mixbuf_size = UMIXBUF_SIZE;
		mi.mixbuf_byte = UMIXBUF_BYTE;
#endif
	} else {
		mi.is_uhqa = false;
#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
		mi.mixbuf_size = NMIXBUF_SIZE;
		mi.mixbuf_byte = NMIXBUF_BYTE;
#else
		if (prtd->rate == 44100) {
			if (prtd->dma_period > NMIXBUF_441_110_BYTE) {
				mi.mixbuf_size = NMIXBUF_441_441_SIZE;
				mi.mixbuf_byte = NMIXBUF_441_441_BYTE;
			} else {
				mi.mixbuf_size = NMIXBUF_441_110_SIZE;
				mi.mixbuf_byte = NMIXBUF_441_110_BYTE;
			}
		} else {
			mi.mixbuf_size = NMIXBUF_SIZE;
			mi.mixbuf_byte = NMIXBUF_BYTE;
		}
#endif
	}
	spin_unlock_irqrestore(&mi.lock, flags);

#ifdef EAX_DMA_PCM_DUMP
	snprintf(prtd->name, 50, "/data/pcm/%s_%s_%d_%s.raw",
                        (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? "P" : "C",
                        rtd->dai_link->name, dump_count,
			(mi.is_uhqa == true) ? "U" : "N");
        open_file(prtd, prtd->name);
        dump_count++;
#endif

	pr_info("EAX:%s:DmaAddr=@%x Total=%d PrdSz=%d #Prds=%d area=0x%p\n",
			(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? "P" : "C",
			(u32)prtd->dma_start, (int)runtime->dma_bytes,
			params_period_bytes(params), params_periods(params),
			runtime->dma_area);

	return 0;
}

static int eax_dma_hw_free(struct snd_pcm_substream *substream)
{
#ifdef EAX_DMA_PCM_DUMP
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
#endif

	pr_debug("Entered %s\n", __func__);

	snd_pcm_set_runtime_buffer(substream, NULL);

#ifdef EAX_DMA_PCM_DUMP
        close_file(prtd);
#endif
	eax_adma_hw_free();

	return 0;
}

static int eax_dma_prepare(struct snd_pcm_substream *substream)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	mutex_lock(&di.mutex);
	di.set_params_cnt++;
	mutex_unlock(&di.mutex);

	prtd->dma_pos = prtd->dma_start;
#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	if (eax_dma_is_uhqa(prtd->dma_bytes)) {
#else
	if (eax_dma_is_uhqa(prtd->format)) {
#endif
		prtd->uhqa_dma_mono = (int *)prtd->dma_buf;
		prtd->normal_dma_mono = NULL;
#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
		eax_adma_hw_params(prtd->dma_bytes / DMA_PERIOD_CNT);
		eax_adma_prepare(prtd->dma_bytes / DMA_PERIOD_CNT);
#else
		eax_adma_hw_params(UMIXBUF_BYTE);
		eax_adma_prepare(UMIXBUF_BYTE);
#endif
	} else {
		prtd->normal_dma_mono = (short *)prtd->dma_buf;
		prtd->uhqa_dma_mono = NULL;
#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
		eax_adma_hw_params(NMIXBUF_BYTE);
		eax_adma_prepare(NMIXBUF_BYTE);
#else
		eax_adma_hw_params(mi.mixbuf_byte);
		eax_adma_prepare(mi.mixbuf_byte);
#endif
	}

	return ret;
}

static int eax_dma_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	unsigned long flags;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		spin_lock_irqsave(&prtd->lock, flags);
		prtd->running = true;
		spin_unlock_irqrestore(&prtd->lock, flags);
		eax_mixer_trigger(true);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		spin_lock_irqsave(&prtd->lock, flags);
		prtd->running = false;
		spin_unlock_irqrestore(&prtd->lock, flags);
		eax_mixer_trigger(false);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static snd_pcm_uframes_t eax_dma_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	unsigned long res;

	pr_debug("Entered %s\n", __func__);

	res = prtd->dma_pos - prtd->dma_start;

	pr_debug("Pointer offset: %lu\n", res);

	if (res >= snd_pcm_lib_buffer_bytes(substream)) {
		if (res == snd_pcm_lib_buffer_bytes(substream))
			res = 0;
	}

	return bytes_to_frames(substream->runtime, res);
}

static int eax_dma_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd;
#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	unsigned long flags;
#endif

	pr_debug("Entered %s\n", __func__);

	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	snd_soc_set_runtime_hwparams(substream, &dma_hardware);

	prtd = kzalloc(sizeof(struct runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	spin_lock_init(&prtd->lock);

	runtime->private_data = prtd;
	prtd->substream = substream;

	eax_mixer_add(prtd);

#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	spin_lock_irqsave(&mi.lock, flags);
	mi.is_uhqa= 0;
	spin_unlock_irqrestore(&mi.lock, flags);
#endif

	return 0;
}

static int eax_dma_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	unsigned long flags;
#endif

	pr_debug("Entered %s\n", __func__);
#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	spin_lock_irqsave(&mi.lock, flags);
	mi.is_uhqa= 0;
	spin_unlock_irqrestore(&mi.lock, flags);

	eax_mixer_remove(prtd);
#endif

	if (!prtd)
		pr_debug("dma_close called with prtd == NULL\n");

#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	eax_mixer_remove(prtd);
#endif
	kfree(prtd);

	return 0;
}

static int eax_dma_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("Entered %s\n", __func__);

	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);
}

static struct snd_pcm_ops eax_dma_ops = {
	.open		= eax_dma_open,
	.close		= eax_dma_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= eax_dma_hw_params,
	.hw_free	= eax_dma_hw_free,
	.prepare	= eax_dma_prepare,
	.trigger	= eax_dma_trigger,
	.pointer	= eax_dma_pointer,
	.mmap		= eax_dma_mmap,
};

void eax_mixer_interrupt_callback(void)
{
	struct buf_info *bi;
	short npcm_l, npcm_r;
	int nmix_l, nmix_r;
	short *nmix_buf;
	int upcm_l, upcm_r;
	long umix_l, umix_r;
	int *umix_buf;
	int m, n;

	if (!di.running || !di.params->ch)
		return;

	for (m = 0; m < DMA_PERIOD_CNT; m++) {
		if (di.buf_rd_idx == di.buf_wr_idx)
			break;

		if (mi.is_uhqa) {
			umix_buf = mi.umix_buf;
			if (!umix_buf)
				return;

			for (n = 0; n < mi.mixbuf_size; n++) {
				umix_l = 0;
				umix_r = 0;

				list_for_each_entry(bi, &buf_list, node) {
					if (bi->prtd && bi->prtd->running) {
						eax_dma_xfer(bi->prtd, NULL, NULL,
								&upcm_l, &upcm_r);
						umix_l += upcm_l;
						umix_r += upcm_r;
					}
				}
				/* check 24bit(UHQ) overflow */
				if (umix_l > 0x007fffff)
					umix_l = 0x007Fffff;
				else if (umix_l < -0x007Fffff)
					umix_l = -0x007Fffff;

				if (umix_r > 0x007Fffff)
					umix_r = 0x007Fffff;
				else if (umix_r < -0x007Fffff)
					umix_r = -0x007Fffff;

				*umix_buf++ = (int)umix_l;
				*umix_buf++ = (int)umix_r;
			}
		} else {
			nmix_buf = mi.nmix_buf;
			if (!nmix_buf)
				return;

			for (n = 0; n < mi.mixbuf_size; n++) {
				nmix_l = 0;
				nmix_r = 0;

				list_for_each_entry(bi, &buf_list, node) {
					if (bi->prtd && bi->prtd->running) {
						eax_dma_xfer(bi->prtd, &npcm_l, &npcm_r,
								NULL, NULL);
						nmix_l += npcm_l;
						nmix_r += npcm_r;
					}
				}
				if (nmix_l > 0x7fff)
					nmix_l = 0x7fff;
				else if (nmix_l < -0x7fff)
					nmix_l = -0x7fff;

				if (nmix_r > 0x7fff)
					nmix_r = 0x7fff;
				else if (nmix_r < -0x7fff)
					nmix_r = -0x7fff;

				*nmix_buf++ = (short)nmix_l;
				*nmix_buf++ = (short)nmix_r;
			}
		}

		if (mi.is_uhqa) {
			memcpy(di.buf_wr_p[di.buf_wr_idx++], mi.umix_buf, mi.mixbuf_byte);
		} else {
			memcpy(di.buf_wr_p[di.buf_wr_idx++], mi.nmix_buf, mi.mixbuf_byte);
		}

		if (di.buf_wr_idx == DMA_PERIOD_CNT)
			di.buf_wr_idx = 0;
	}
}

static int eax_prealloc_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = dma_hardware.buffer_bytes_max;

	pr_debug("Entered %s\n", __func__);

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_coherent(pcm->card->dev, size,
					&buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;

	return 0;
}

static u64 eax_dma_mask = DMA_BIT_MASK(32);
static int eax_dma_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &eax_dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

#ifdef EAX_DMA_PCM_DUMP
	if (!eax_mixer_kobj) {
		eax_mixer_kobj = kobject_create_and_add("eax-mixer", NULL);
		if (sysfs_create_file(eax_mixer_kobj, &pcm_dump_attribute.attr))
			pr_err("%s: failed to create sysfs to control PCM dump\n", __func__);
	}
#endif

#ifndef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	mi.nmix_buf = kzalloc(NMIXBUF_BYTE, GFP_KERNEL);
#else
	mi.nmix_buf = kzalloc(NMIXBUF_441_441_BYTE, GFP_KERNEL);
#endif
	if (mi.nmix_buf == NULL)
		return -ENOMEM;

	mi.umix_buf = kzalloc(UMIXBUF_BYTE, GFP_KERNEL);
	if (mi.umix_buf == NULL) {
		kfree(mi.nmix_buf);
		return -ENOMEM;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = eax_prealloc_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
		if (ret) {
			kfree(mi.nmix_buf);
			kfree(mi.umix_buf);
			goto out;
		}
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = eax_prealloc_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
		if (ret) {
			kfree(mi.nmix_buf);
			kfree(mi.umix_buf);
			goto out;
		}
	}

out:
	return ret;
}

static void eax_dma_free(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	pr_debug("Entered %s\n", __func__);

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		kfree(buf->area);
		buf->area = NULL;
	}
}

static struct snd_soc_platform_driver eax_asoc_platform = {
	.ops		= &eax_dma_ops,
	.pcm_new	= eax_dma_new,
	.pcm_free	= eax_dma_free,
};

int eax_asoc_platform_register(struct device *dev)
{
	return snd_soc_register_platform(dev, &eax_asoc_platform);
}
EXPORT_SYMBOL_GPL(eax_asoc_platform_register);

void eax_asoc_platform_unregister(struct device *dev)
{
	snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(eax_asoc_platform_unregister);

static int eax_mixer_add(struct runtime_data *prtd)
{
	struct buf_info *bi;
	unsigned long flags;

	bi = kzalloc(sizeof(struct buf_info), GFP_KERNEL);
	if (!bi) {
		pr_err("%s: Memory alloc fails!\n", __func__);
		return -ENOMEM;
	}

	bi->prtd = prtd;

	spin_lock_irqsave(&mi.lock, flags);
	list_add(&bi->node, &buf_list);
	spin_unlock_irqrestore(&mi.lock, flags);

	pr_debug("%s: prtd %p added\n", __func__, prtd);

	return 0;
}

static int eax_mixer_remove(struct runtime_data *prtd)
{
	struct buf_info *bi;
	unsigned long flags;
	bool node_found = false;

	spin_lock_irqsave(&mi.lock, flags);
	list_for_each_entry(bi, &buf_list, node) {
		if (bi->prtd == prtd) {
			node_found = true;
			break;
		}
	}

	if (!node_found) {
		spin_unlock_irqrestore(&mi.lock, flags);
		pr_err("%s: prtd %p not found\n", __func__, prtd);
		return -EINVAL;
	}

	list_del(&bi->node);
	kfree(bi);
	spin_unlock_irqrestore(&mi.lock, flags);
	pr_debug("%s: prtd %p removed\n", __func__, prtd);

	return 0;
}

static void eax_mixer_trigger(bool on)
{
	if (on) {
		if (!di.running) {
			if (!di.prepare_done) {
				eax_adma_hw_params(mi.mixbuf_byte);
				eax_adma_prepare(mi.mixbuf_byte);
			}
			eax_adma_trigger(true);
		}
	} else {
		if (!eax_mixer_any_buf_running() && di.running)
				eax_adma_trigger(false);
	}
}

MODULE_AUTHOR("Yeongman Seo, <yman.seo@samsung.com>");
MODULE_DESCRIPTION("Samsung ASoC EAX-DMA Driver");
MODULE_LICENSE("GPL");
