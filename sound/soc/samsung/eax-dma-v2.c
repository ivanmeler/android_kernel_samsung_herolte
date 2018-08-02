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
#include <linux/kthread.h>
#include <linux/iommu.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/dma/dma-pl330.h>
#include <linux/interrupt.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/namei.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/exynos.h>

#include "lpass.h"
#include "dma.h"
#include "eax.h"

#define NMIXBUF_SIZE		96 /* PCM 16bit 2ch */
#define NMIXBUF_BYTE		(NMIXBUF_SIZE * 4) /* PCM 16bit 2ch */
#define NMIXBUF_441_SIZE	240 /* PCM 16bit 2ch */
#define NMIXBUF_441_BYTE	(NMIXBUF_441_SIZE * 4) /* PCM 16bit 2ch */
#define UMIXBUF_SIZE		480 /* Total 15360 byte / 2ch / 4byte / 4 periods */
#define UMIXBUF_BYTE		(UMIXBUF_SIZE * 8) /* PCM 32bit 2ch */

#define DMA_PERIOD_CNT		4
#define DMA_START_THRESHOLD	(DMA_PERIOD_CNT - 1)

#define	SOUNCCAMP_CH_ID		1
#define LOWLATENCY_CH_ID	0

#define BASE_IOVA	0x48000000
static dma_addr_t iommu_base = BASE_IOVA;

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
	struct ch_info *ci;
#ifdef CONFIG_SND_SAMSUNG_SEIREN_OFFLOAD
	snd_pcm_format_t 	format;
	unsigned int 		rate;
#endif
	unsigned int		dma_period;
	dma_addr_t		dma_start;
	dma_addr_t		dma_pos;
	dma_addr_t		dma_end;
	u32			*dma_buf;
	short			*dma_mono;
	unsigned long		dma_bytes;
	struct s3c_dma_params	*params;
        struct file             *filp;
        char                    name[50];
};

static struct mixer_info {
	spinlock_t		lock;
	struct snd_soc_dai	*cpu_dai;
	short			*mix_buf;
        unsigned long		mixbuf_size;
        unsigned long		mixbuf_byte;
	bool			buf_fill;
} mi;

struct buf_info {
	struct runtime_data	*prtd;
	struct list_head	node;
};

void eax_mixer_tasklet_handler(unsigned long data);

static LIST_HEAD(buf_list);
static DECLARE_TASKLET(eax_mixer_tasklet, eax_mixer_tasklet_handler, 0);
static DECLARE_WAIT_QUEUE_HEAD(mixer_run_wq);
static DECLARE_WAIT_QUEUE_HEAD(mixer_buf_wq);

static struct dma_info {
	spinlock_t		lock;
	struct mutex		mutex;
	struct snd_soc_dai	*cpu_dai;
	struct s3c_dma_params	*params;
	bool			params_init;
	bool			params_done;
	bool			prepare_done;
	bool			running;
	unsigned char		*buf_wr_p[DMA_PERIOD_CNT];
	int			buf_idx;
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

static struct buf_info *is_lowlatency_only_running(void)
{
	struct buf_info *lowlatency_bi = NULL;
	struct buf_info *bi;
	int bi_cnt = 0;

	list_for_each_entry(bi, &buf_list, node) {
		if (bi->prtd && bi->prtd->running &&
				bi->prtd->ci->ch_id == LOWLATENCY_CH_ID)
			lowlatency_bi = bi;
		bi_cnt++;
	}

	if (lowlatency_bi && (bi_cnt == 1))
		return lowlatency_bi;
	else
		return NULL;
}

static struct kobject *eax_mixer_kobj = NULL;
static int dump_enabled = 0;
static int dump_count = 0;
static struct file *dump_filp = NULL;

static int pcm_mkdir(const char *name, umode_t mode)
{
	struct dentry *dentry;
	struct path path;
	int err;

	dentry = kern_path_create(AT_FDCWD, name, &path, LOOKUP_DIRECTORY);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	err = vfs_mkdir(path.dentry->d_inode, dentry, mode);
	done_path_create(&path, dentry);
	return err;
}

static void open_file(struct runtime_data *prtd, char *filename)
{
	/* open a file */
	prtd->filp = filp_open(filename, O_RDWR|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
	pr_info("Audio dump open %s\n",filename);

	if (IS_ERR(prtd->filp)) {
		pr_info("open error\n");
		return;
	} else {
		pr_info("open success\n");
	}
}

static void close_file(struct runtime_data *prtd)
{
	pr_info("Audio dump close %s\n", prtd->name);
	if (IS_ERR_OR_NULL(prtd->filp))
		return;

	vfs_fsync(prtd->filp, 0);
	filp_close(prtd->filp, NULL);
}

static ssize_t show_dump_enabled(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 3, "%d\n", dump_enabled);
}

static ssize_t store_dump_enabled(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	struct buf_info *bi;
	int input;

	if (!sscanf(buf, "%1d", &input))
		return -EINVAL;

	pcm_mkdir("/data/pcm", 0777);
	dump_count = 0;

	dump_enabled = !!input;
	list_for_each_entry(bi, &buf_list, node) {
		if (!bi->prtd->filp) {
			struct snd_pcm_substream *substream = bi->prtd->substream;
			struct snd_soc_pcm_runtime *rtd = substream->private_data;
			snprintf(bi->prtd->name, 50, "/data/pcm/P_%s_%d.raw",
					rtd->dai_link->name, dump_count);
			open_file(bi->prtd, bi->prtd->name);
			dump_count++;
		}
	}
	if (!dump_filp)
		dump_filp = filp_open("/data/pcm/mix_buf.raw",
				O_RDWR|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
	return count;
}

static struct kobj_attribute pcm_dump_attribute =
__ATTR(pcm_dump, S_IRUGO | S_IWUSR, show_dump_enabled, store_dump_enabled);

static bool is_24bit_format(snd_pcm_format_t format)
{
	return (format == SNDRV_PCM_FORMAT_S24_LE);
}
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

static inline bool eax_mixer_any_buf_running(void)
{
	struct buf_info *bi;

	list_for_each_entry(bi, &buf_list, node) {
		if (bi->prtd && bi->prtd->running)
			return true;
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
	iommu_map(domain, 0x49000000, di.dma_start, size, 0);
	di.dma_start = 0x49000000;
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
	iommu_unmap(domain, 0x49000000, size);
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
	struct buf_info *bi;
	dma_addr_t src, dst, pos;
	int buf_idx;

	if (!di.running)
		return;

	di.params->ops->getposition(di.params->ch, &src, &dst);
	pos = src - di.dma_start;
	pos /= di.dma_period;
	buf_idx = pos;
	pos = di.dma_start + (pos * di.dma_period);
	if (pos >= di.dma_end)
		pos = di.dma_start;

	di.dma_pos = pos;

	bi = is_lowlatency_only_running();
	if (bi) {
		bi->prtd->dma_pos = bi->prtd->dma_start + (di.dma_pos -
				di.dma_start);
		if (((bi->prtd->dma_pos - bi->prtd->dma_start) %
			bi->prtd->dma_period) == 0)
		snd_pcm_period_elapsed(bi->prtd->substream);
	} else {
		if (--buf_idx < 0)
			buf_idx += DMA_PERIOD_CNT;
		di.buf_idx = buf_idx;
		tasklet_schedule(&eax_mixer_tasklet);
	}
}

static void eax_dma_uhqa_buffdone(void *data)
{
	struct snd_pcm_substream *substream = data;
	struct runtime_data *prtd;
	dma_addr_t src, dst, pos;

	pr_debug("Entered %s\n", __func__);

	if (!substream)
		return;

	prtd = substream->runtime->private_data;
	if (is_24bit_format(prtd->format) && prtd->running) {
		prtd->params->ops->getposition(prtd->params->ch, &src, &dst);
		pos = src - prtd->dma_start;
		pos /= prtd->dma_period;
		pos = prtd->dma_start + (pos * prtd->dma_period);
		if (pos >= prtd->dma_end)
			pos = prtd->dma_start;

		prtd->dma_pos = pos;
		snd_pcm_period_elapsed(substream);
	}
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
		di.params->ops->config(di.params->ch, &config);
	}

	di.dma_period = dma_period_bytes;
	di.dma_pos = di.dma_start;
	di.dma_end = di.dma_start + di.dma_period * DMA_PERIOD_CNT;
	for (n = 0; n < DMA_PERIOD_CNT; n++) {
		di.buf_wr_p[n] = (unsigned char *)di.dma_buf;
		di.buf_wr_p[n] += dma_period_bytes * n;
	}

	pr_info("EAXDMA-V2:DmaAddr=@%x Total=%d PrdSz=%d #Prds=%d dma_area=0x%p\n",
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

	if (di.params_init) {
		pr_info("EAXADMA: release dma channel : %s\n", di.params->ch_name);
		di.params_init = false;
		di.params->ops->flush(di.params->ch);
		di.params->ops->release(di.params->ch, di.params->client);
	}

	di.params_done = false;
	di.prepare_done = false;
out:
	pr_info("Entered %s --\n", __func__);
	mutex_unlock(&di.mutex);
}

static void eax_adma_prepare(unsigned long dma_period_bytes)
{
	struct samsung_dma_prep dma_info;

	mutex_lock(&di.mutex);

	if (di.prepare_done)
		goto out;

	pr_info("Entered %s ++\n", __func__);
	if (!di.params_init || !di.params_done) {
		pr_err("EAXADMA: hw_params are not set. init = %d, done = %d\n",
			di.params_init, di.params_done);
		goto out;
	}

	di.prepare_done = true;

	/* zero fill */
	memset(di.dma_buf, 0, dma_period_bytes * DMA_PERIOD_CNT);

	/* prepare */
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
	di.params->ops->prepare(di.params->ch, &dma_info);
	pr_info("Entered %s --\n", __func__);
out:
	mutex_unlock(&di.mutex);
}

static void eax_adma_trigger(bool on)
{
	spin_lock(&di.lock);

	pr_info("Entered %s [%d] ++\n", __func__, on);
	if (on) {
		if (di.running) {
			spin_unlock(&di.lock);
			return;
		}
		lpass_update_lpclock(LPCLK_CTRLID_LEGACY, true);
		di.running = on;
		lpass_dma_enable(true);
		di.params->ops->trigger(di.params->ch);
	} else {
		if (!di.running) {
			spin_unlock(&di.lock);
			return;
		}
		di.params->ops->stop(di.params->ch);
		lpass_dma_enable(false);
		di.prepare_done = false;
		di.running = on;
		lpass_update_lpclock(LPCLK_CTRLID_LEGACY, false);
	}
	pr_info("Entered %s [%d] --\n", __func__, on);

	spin_unlock(&di.lock);
}

static inline void eax_dma_xfer(struct runtime_data *prtd, short *pcm_l, short *pcm_r)
{
	dma_addr_t dma_pos;

	if (!prtd->dma_mono || !pcm_l || !pcm_r) {
		pr_err("%s : NORMAL DMA MONO Pointer is NULL\n", __func__);
		return;
	}
	*pcm_l = *prtd->dma_mono++;
	*pcm_r = *prtd->dma_mono++;
	dma_pos = prtd->dma_pos + 4;

	if (dma_pos == prtd->dma_end) {
		prtd->dma_pos = prtd->dma_start;
		prtd->dma_mono = (short *)prtd->dma_buf;
	} else {
		prtd->dma_pos = dma_pos;
	}

	if (prtd->running &&
		((prtd->dma_pos - prtd->dma_start) % prtd->dma_period == 0))
		snd_pcm_period_elapsed(prtd->substream);
}

static int eax_dma_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	unsigned long totbytes = params_buffer_bytes(params);
	struct samsung_dma_req req;
	struct samsung_dma_config config;
#ifdef EAX_DMA_PCM_DUMP
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
#endif

	pr_info("Entered ++ %s\n", __func__);

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = totbytes;

	if (is_24bit_format(params_format(params)) && (prtd->params == NULL)) {
		prtd->dma_start = BASE_IOVA;
		/* prepare DMA */
		prtd->params = di.params;

		pr_info("params %p, client %p, channel %d\n", prtd->params,
			prtd->params->client, prtd->params->channel);

		prtd->params->ops = samsung_dma_get_ops();
		req.cap = DMA_CYCLIC;
		req.client = prtd->params->client;
		config.direction = DMA_MEM_TO_DEV;
		config.width = prtd->params->dma_size;
		config.fifo = prtd->params->dma_addr;
		prtd->params->ch = prtd->params->ops->request(prtd->params->channel,
				&req, di.cpu_dai->dev, prtd->params->ch_name);
		pr_info("dma_request: ch %d, req %p, dev %p, ch_name [%s]\n",
			prtd->params->channel, &req, di.cpu_dai->dev,
			prtd->params->ch_name);
		prtd->params->ops->config(prtd->params->ch, &config);
	} else {
		prtd->dma_start = runtime->dma_addr;

		if (params_rate(params) == 44100) {
			mi.mixbuf_size = NMIXBUF_441_SIZE;
			mi.mixbuf_byte = NMIXBUF_441_BYTE;
		} else {
			mi.mixbuf_size = NMIXBUF_SIZE;
			mi.mixbuf_byte = NMIXBUF_BYTE;
		}
	}

	spin_lock_irq(&prtd->lock);
	prtd->dma_period = params_period_bytes(params);
	prtd->dma_pos = prtd->dma_start;
	prtd->dma_end = prtd->dma_start + totbytes;
	prtd->dma_buf = (u32 *)(runtime->dma_area);
	prtd->dma_bytes = totbytes;
	prtd->dma_mono = (short *)prtd->dma_buf;
	prtd->format = params_format(params);
	prtd->rate = params_rate(params);
	spin_unlock_irq(&prtd->lock);

#ifdef EAX_DMA_PCM_DUMP
	snprintf(prtd->name, 50, "/data/pcm/%s_%s_%d_N.raw",
                        (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? "P" : "C",
                        rtd->dai_link->name, dump_count);
        open_file(prtd, prtd->name);
        dump_count++;
#endif

	pr_info("EAX-V2:%c:DmaAddr=@%x Total=%d PrdSz=%d #Prds=%d area=0x%p\n",
			(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? 'P' : 'C',
			(u32)prtd->dma_start, (int)runtime->dma_bytes,
			params_period_bytes(params), params_periods(params),
			runtime->dma_area);

	return 0;
}

static int eax_dma_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;

	pr_info("Entered %s\n", __func__);

	snd_pcm_set_runtime_buffer(substream, NULL);

        close_file(prtd);
	if (is_24bit_format(prtd->format) && prtd->params) {
		prtd->params->ops->flush(prtd->params->ch);
		prtd->params->ops->release(prtd->params->ch,
				prtd->params->client);
		prtd->params = NULL;
	} else {
		eax_adma_hw_free();
	}

	return 0;
}

static int eax_dma_prepare(struct snd_pcm_substream *substream)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_info("Entered %s\n", __func__);

	if (is_24bit_format(prtd->format)) {
		unsigned int limit;
		struct samsung_dma_prep dma_info;

		prtd->dma_pos = BASE_IOVA;
		limit = (prtd->dma_end - prtd->dma_start) / prtd->dma_period;

		if (prtd->params->ch)
			prtd->params->ops->flush(prtd->params->ch);

		/* enqueue */
		dma_info.cap = DMA_CYCLIC;
		dma_info.direction = DMA_MEM_TO_DEV;
		dma_info.fp = eax_dma_uhqa_buffdone;
		dma_info.fp_param = substream;
		dma_info.period = prtd->dma_period;
		dma_info.len = prtd->dma_period * limit;

		dma_info.buf = prtd->dma_pos;
		dma_info.infiniteloop = limit;
		if (prtd->params->ch)
			prtd->params->ops->prepare(prtd->params->ch, &dma_info);
	} else {
			prtd->dma_pos = prtd->dma_start;
		prtd->dma_mono = (short *)prtd->dma_buf;

		eax_adma_hw_params(mi.mixbuf_byte);
		eax_adma_prepare(mi.mixbuf_byte);
	}

	return ret;
}

static int eax_dma_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	spin_lock(&prtd->lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->running = true;
		if (is_24bit_format(prtd->format)) {
			lpass_dma_enable(true);
			lpass_inc_dram_usage_count();
			lpass_update_lpclock(LPCLK_CTRLID_LEGACY, false);
			prtd->params->ops->trigger(prtd->params->ch);
		} else {
			lpass_inc_dram_usage_count();
			eax_mixer_trigger(true);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		prtd->running = false;
		if (is_24bit_format(prtd->format)) {
			prtd->params->ops->stop(prtd->params->ch);
			lpass_dma_enable(false);
			lpass_dec_dram_usage_count();
			lpass_update_lpclock(LPCLK_CTRLID_LEGACY, false);
		} else {
			eax_mixer_trigger(false);
			lpass_dec_dram_usage_count();
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock(&prtd->lock);

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
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct ch_info *ci;
	struct runtime_data *prtd;

	pr_debug("Entered %s\n", __func__);

	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	snd_soc_set_runtime_hwparams(substream, &dma_hardware);

	ci = snd_soc_dai_get_drvdata(rtd->cpu_dai);

	prtd = kzalloc(sizeof(struct runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	spin_lock_init(&prtd->lock);

	runtime->private_data = prtd;
	prtd->substream = substream;
	prtd->ci = ci;

	eax_mixer_add(prtd);

	return 0;
}

static int eax_dma_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;

	pr_debug("Entered %s\n", __func__);

	if (!prtd)
		pr_debug("dma_close called with prtd == NULL\n");

	eax_mixer_remove(prtd);
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

static int eax_dma_copy(struct snd_pcm_substream *substream, int channel,
		    snd_pcm_uframes_t pos,
		    void __user *buf, snd_pcm_uframes_t count) {
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	unsigned char *hwbuf;

	if (is_lowlatency_only_running() &&
		!is_24bit_format(prtd->format)) {
		hwbuf = (char *)di.dma_buf + frames_to_bytes(runtime, pos);
	} else {
		hwbuf = (char *)runtime->dma_area + frames_to_bytes(runtime, pos);
	}

	if (copy_from_user(hwbuf, buf, frames_to_bytes(runtime, count)))
		return -EFAULT;
	if (!IS_ERR_OR_NULL(dump_filp))
		vfs_write(dump_filp, hwbuf, frames_to_bytes(runtime, count), &dump_filp->f_pos);
	return 0;
}

static struct snd_pcm_ops eax_dma_ops = {
	.open		= eax_dma_open,
	.close		= eax_dma_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= eax_dma_hw_params,
	.hw_free	= eax_dma_hw_free,
	.prepare	= eax_dma_prepare,
	.trigger	= eax_dma_trigger,
	.copy		= eax_dma_copy,
	.pointer	= eax_dma_pointer,
	.mmap		= eax_dma_mmap,
};

void eax_mixer_tasklet_handler(unsigned long data)
{
	struct buf_info *bi;
	short pcm_l, pcm_r;
	int mix_l, mix_r;
	short *mix_buf;
	int n;

	mix_buf = mi.mix_buf;
	if (!mix_buf)
		return;
	for (n = 0; n < mi.mixbuf_size; n++) {
		mix_l = 0;
		mix_r = 0;

		list_for_each_entry(bi, &buf_list, node) {
			if (bi->prtd && bi->prtd->running) {
				eax_dma_xfer(bi->prtd, &pcm_l, &pcm_r);
				mix_l += pcm_l;
				mix_r += pcm_r;
			}
		}
		if (mix_l > 0x7fff)
			mix_l = 0x7fff;
		else if (mix_l < -0x7fff)
			mix_l = -0x7fff;

		if (mix_r > 0x7fff)
			mix_r = 0x7fff;
		else if (mix_r < -0x7fff)
			mix_r = -0x7fff;

		*mix_buf++ = (short)mix_l;
		*mix_buf++ = (short)mix_r;
	}

	memcpy(di.buf_wr_p[di.buf_idx], mi.mix_buf, mi.mixbuf_byte);
}

static int eax_prealloc_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = dma_hardware.buffer_bytes_max;
	struct iommu_domain *domain = lpass_get_iommu_domain();
	int ret;

	pr_debug("Entered %s\n", __func__);

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_coherent(pcm->card->dev, size,
					&buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	ret = iommu_map(domain, iommu_base, buf->addr, size, 0);
	if (ret) {
		dma_free_coherent(pcm->card->dev, size,
				buf->area, buf->addr);
		pr_err("%s: Failed to iommu_map: %d\n", __func__, ret);
		return -ENOMEM;
	} else {
		iommu_base += 0x100000;
	}

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

	if (!eax_mixer_kobj) {
		eax_mixer_kobj = kobject_create_and_add("eax-mixer", NULL);
		if (sysfs_create_file(eax_mixer_kobj, &pcm_dump_attribute.attr))
			pr_err("%s: failed to create sysfs to control PCM dump\n", __func__);
	}

	mi.mix_buf = kzalloc(dma_hardware.buffer_bytes_max, GFP_KERNEL);
	if (mi.mix_buf == NULL)
		return -ENOMEM;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = eax_prealloc_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
		if (ret) {
			kfree(mi.mix_buf);
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
