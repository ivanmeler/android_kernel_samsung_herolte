/* sound/soc/samsung/eax-dma-slowpath.c
 *
 * Exynos Audio Mixer Slowpath DMA driver
 *
 * Copyright (c) 2015 Samsung Electronics Co. Ltd.
 *	Hyuwnoong Kim <khw0178.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/dma/dma-pl330.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/exynos.h>

#include "./seiren/seiren.h"
#include "lpass.h"
#include "dma.h"
#include "eax.h"

#undef EAX_SLOWPATH_DMA_PCM_DUMP

#ifdef EAX_SLOWPATH_DMA_PCM_DUMP
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/namei.h>

static int dump_count = 0;
static struct file *mfilp = NULL;

#endif

#define NMIXBUF_SIZE		512 /* PCM 16bit 2ch */
#define NMIXBUF_BYTE		(NMIXBUF_SIZE * 4) /* PCM 16bit 2ch */
#define DMA_PERIOD_CNT		8
#define DMA_START_THRESHOLD	(DMA_PERIOD_CNT - 1)

#define PHYS_SRAM_ADDR		0x3000000
#define PHYS_SRAM_SIZE		0x24000
#define DMA_BUF_OFFSET		0x1C000
#define DMA_BUF_SIZE		0x4000 /* 16KB */
#define DMA_BUF_IDX_BASE	0x23FF0

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
	struct s3c_dma_params 	*params;
	struct snd_pcm_substream *substream;
	struct snd_soc_dai	*cpu_dai;
	snd_pcm_format_t 	format;
	unsigned int 		rate;
	unsigned int		dma_period;
	dma_addr_t		dma_start;
	dma_addr_t		dma_pos;
	dma_addr_t		dma_end;
	u32			*dma_buf;
	short			*dma_mono;
	unsigned long		dma_bytes;
#ifdef EAX_SLOWPATH_DMA_PCM_DUMP
        struct file             *filp;
        mm_segment_t            old_fs;
        char                    name[50];
#endif
};

struct mixer_info {
	spinlock_t		lock;
	struct snd_soc_dai	*cpu_dai;
        unsigned long		mixbuf_size;
        unsigned long		mixbuf_byte;
	bool			buf_fill;
	int			mix_cnt;
	bool			running;
} msi;

struct buf_info {
	struct runtime_data	*prtd;
	struct list_head	node;
};

static void eax_slowpath_mixer_run(unsigned long data);

static LIST_HEAD(slowpath_buf_list);
static DECLARE_TASKLET(eax_slowpath_tasklet, eax_slowpath_mixer_run, 0);

static struct dma_info {
	spinlock_t		lock;
	struct mutex		mutex;
	struct snd_soc_dai	*cpu_dai;
	unsigned int		set_params_cnt;
	bool			params_init;
	bool			running;
	unsigned char		*buf_fill;
	short			*buf_wr_p;
	int			buf_wr_idx;
	void __iomem		*sram_base;
	void __iomem		*sram_dma_buf;
	dma_addr_t		sram_dma_start;
	unsigned int		dma_period;
	dma_addr_t		dma_pos;
	dma_addr_t		dma_end;
	int			dma_buf_cnt;
	void __iomem		*addr;
	struct s3c_dma_params 	*params;
} dsi;

static int eax_slowpath_mixer_add(struct runtime_data *prtd);
static int eax_slowpath_mixer_remove(struct runtime_data *prtd);
static void eax_slowpath_mixer_trigger(bool on);

#define BASE_IOVA	0x46000000
static dma_addr_t iommu_base = 0x46000000;

void eax_slowpath_wakeup_buf_wq(int idx)
{
	dsi.buf_wr_idx = idx;
	tasklet_schedule(&eax_slowpath_tasklet);
}

#ifdef EAX_SLOWPATH_DMA_PCM_DUMP
struct kobject *eax_slowpath_mixer_kobj = NULL;

static int input_dump_enabled = 0;
static int mix_dump_enabled = 0;

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
	/* kernel memory access setting */
	prtd->old_fs = get_fs();
	set_fs(KERNEL_DS);

	/* open a file */
	prtd->filp = filp_open(filename, O_RDWR|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR);
	pr_info("Audio dump open %s\n",filename);
	if (IS_ERR(prtd->filp)) {
		pr_info("open error\n");
	} else {
		pr_info("open success\n");
	}
}

static void close_file(struct runtime_data *prtd)
{
	pr_info("Audio dump close %s\n", prtd->name);
	vfs_fsync(prtd->filp, 0);

	filp_close(prtd->filp, NULL);  /* filp_close(filp, current->files) ?  */
	/* restore kernel memory setting */
	set_fs(prtd->old_fs);
}

static ssize_t show_dump_enabled(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	pr_info("Select dump option\n");
	pr_info("1: Input Buffer dump enable\n");
	pr_info("2: Mix Buffer dump enable\n");
	return snprintf(buf, 5, "%d %d\n",
			input_dump_enabled, mix_dump_enabled);
}

static ssize_t store_dump_enabled(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct buf_info *bi;
	int input;

	if (!sscanf(buf, "%1d", &input))
		return -EINVAL;

	pcm_mkdir("/data/pcm", 0777);

	if (input == 1) {
		input_dump_enabled = 1;
		list_for_each_entry(bi, &slowpath_buf_list, node) {
			if (!bi->prtd->filp) {
				struct snd_pcm_substream *substream = bi->prtd->substream;
				struct snd_soc_pcm_runtime *rtd = substream->private_data;
				snprintf(bi->prtd->name, 50, "/data/pcm/P_%s_%d.raw",
						rtd->dai_link->name, dump_count);
				open_file(bi->prtd, bi->prtd->name);
				dump_count++;
			}
		}
	} else if (input == 2) {
		mix_dump_enabled = 1;
		if (!mfilp)
			mfilp = filp_open("/data/pcm/slowpath_mix_buf.raw",
				O_RDWR|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
	}
	return count;
}

static struct kobj_attribute slowpath_pcm_dump_attribute =
	__ATTR(slowpath_pcm_dump, S_IRUGO | S_IWUSR, show_dump_enabled, store_dump_enabled);
#endif

int eax_slowpath_params_register(struct s3c_dma_params *dma)
{
	dsi.params = dma;

	return 0;
}

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
int check_eax_slowpath_dma_status(void)
{
	return dsi.running;
}

static void eax_slowpath_adma_alloc_buf(void)
{
	dsi.sram_base = lpass_get_mem();
	dsi.sram_dma_buf = dsi.sram_base + DMA_BUF_OFFSET;
	dsi.sram_dma_start = PHYS_SRAM_ADDR + DMA_BUF_OFFSET;
	memset(dsi.sram_dma_buf, 0x0, DMA_BUF_SIZE); /* Temporary Test */
}

static inline bool eax_slowpath_mixer_any_buf_running(void)
{
	struct buf_info *bi;

	list_for_each_entry(bi, &slowpath_buf_list, node) {
		if (bi->prtd && bi->prtd->running)
			return true;
	}

	return false;
}

int eax_slowpath_dma_dai_register(struct snd_soc_dai *dai)
{
	spin_lock_init(&dsi.lock);
	mutex_init(&dsi.mutex);

	dsi.cpu_dai = dai;
	dsi.running = false;
	dsi.params_init = false;

	spin_lock_init(&msi.lock);
	msi.cpu_dai = dai;
	msi.running = false;

	return 0;
}

int eax_slowpath_dma_dai_unregister(void)
{
	mutex_destroy(&dsi.mutex);

	dsi.cpu_dai = NULL;
	dsi.running = false;
	dsi.params_init = false;

	msi.cpu_dai = NULL;
	msi.running = false;
	/*
	eax_adma_free_buf();
	*/

	return 0;
}

static void eax_dma_slowpath_buffdone(void *data)
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

static void eax_slowpath_adma_hw_params(unsigned long dma_period_bytes)
{
	int n;

	mutex_lock(&dsi.mutex);

	if (!dsi.params_init) {
		dsi.params_init = true;
		dsi.dma_period = dma_period_bytes;
		dsi.dma_pos = dsi.sram_dma_start;
		dsi.dma_end = dsi.sram_dma_start + dsi.dma_period * DMA_PERIOD_CNT;
		dsi.dma_buf_cnt = DMA_BUF_SIZE / DMA_PERIOD_CNT / dsi.dma_period;
		dsi.buf_fill = (unsigned char *)
				(dsi.sram_base + DMA_BUF_IDX_BASE);
		for (n = 0; n < 2; n++)
			dsi.buf_fill[n] = -1;

		msi.buf_fill = false;
		msi.mix_cnt = 0;

		esa_fw_start();
	}

	pr_info("EAX-SLOW-DMA:DmaAddr=@%x Total=%d PrdSz=%d #Prds=%d dma_area=0x%p\n",
		(u32)dsi.sram_dma_start, (u32)(dsi.dma_end - dsi.sram_dma_start),
		dsi.dma_period, DMA_PERIOD_CNT, dsi.sram_dma_buf);

	mutex_unlock(&dsi.mutex);
}

static void eax_slowpath_adma_hw_free(void)
{
	mutex_lock(&dsi.mutex);
	pr_info("Entered %s ++\n", __func__);

	if (dsi.running || eax_slowpath_mixer_any_buf_running()) {
		pr_info("EAXADMA: some mixer channel is running, (%d), (%d)\n",
			dsi.running, eax_slowpath_mixer_any_buf_running());
		goto out;
	}

	if (dsi.params_init && (dsi.set_params_cnt == 1)) {
		dsi.params_init = false;
		esa_fw_stop();
	}

out:
	dsi.set_params_cnt--;
	pr_info("Entered %s --\n", __func__);
	mutex_unlock(&dsi.mutex);
}

static void eax_slowpath_adma_trigger(bool on)
{
	unsigned long flags;

	spin_lock_irqsave(&dsi.lock, flags);

	if (on) {
		dsi.running = on;
		lpass_inc_dram_usage_count();
		lpass_update_lpclock(LPCLK_CTRLID_LEGACY, true);
		esa_compr_send_direct_cmd(CMD_DMA_START);
	} else {
		esa_compr_send_direct_cmd(CMD_DMA_STOP);
		lpass_dec_dram_usage_count();
		lpass_update_lpclock(LPCLK_CTRLID_LEGACY, false);
		dsi.running = on;
	}

	spin_unlock_irqrestore(&dsi.lock, flags);
}

static inline void eax_slowpath_dma_xfer(struct runtime_data *prtd, short *pcm_l, short *pcm_r)
{
	dma_addr_t dma_pos;

	if (!prtd->dma_mono || !pcm_l || !pcm_r) {
		pr_err("%s : SLOWPATH DMA MONO Pointer is NULL\n", __func__);
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

static int eax_slowpath_dma_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	unsigned long totbytes = params_buffer_bytes(params);
	struct samsung_dma_req req;
	struct samsung_dma_config config;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	pr_debug("Entered %s\n", __func__);

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = totbytes;

	if (is_24bit_format(params_format(params)) && (prtd->params == NULL)) {
		prtd->dma_start = BASE_IOVA;
		/* prepare DMA */
		prtd->params = dsi.params;

		pr_info("params %p, client %p, channel %d\n", prtd->params,
			prtd->params->client, prtd->params->channel);

		prtd->params->ops = samsung_dma_get_ops();
		req.cap = DMA_CYCLIC;
		req.client = prtd->params->client;
		config.direction = DMA_MEM_TO_DEV;
		config.width = prtd->params->dma_size;
		config.fifo = prtd->params->dma_addr;
		prtd->params->ch = prtd->params->ops->request(
				prtd->params->channel, &req,
				prtd->params->sec_dma_dev,
				prtd->params->ch_name);
		pr_info("dma_request: ch %d, req %p, dev %p, ch_name [%s]\n",
			prtd->params->channel, &req, rtd->cpu_dai->dev,
			prtd->params->ch_name);
		prtd->params->ops->config(prtd->params->ch, &config);
	} else {
		prtd->dma_start = runtime->dma_addr;
		spin_lock_irq(&msi.lock);
		msi.mixbuf_size = NMIXBUF_SIZE;
		msi.mixbuf_byte = NMIXBUF_BYTE;
		spin_unlock_irq(&msi.lock);
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

	/* Get OFFLOAD SRAM BUFFER ADDRESS */
	eax_slowpath_adma_alloc_buf();

#ifdef EAX_SLOWPATH_DMA_PCM_DUMP
	snprintf(prtd->name, 50, "/data/pcm/%s_%s_%d_%s.raw",
                        (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? "P" : "C",
                        rtd->dai_link->name, dump_count, "N");
        open_file(prtd, prtd->name);
        dump_count++;
#endif

	pr_info("EAX-SLOW:DmaAddr=@%llx Total=%d PrdSz=%d #Prds=%d area=0x%p\n",
			prtd->dma_start, (int)runtime->dma_bytes,
			params_period_bytes(params), params_periods(params),
			runtime->dma_area);

	return 0;
}

static int eax_slowpath_dma_hw_free(struct snd_pcm_substream *substream)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	pr_debug("Entered %s\n", __func__);

	snd_pcm_set_runtime_buffer(substream, NULL);

	if (is_24bit_format(prtd->format) && prtd->params) {
		prtd->params->ops->flush(prtd->params->ch);
		prtd->params->ops->release(prtd->params->ch,
				prtd->params->client);
		prtd->params = NULL;
	} else {
		eax_slowpath_adma_hw_free();
	}

	return 0;
}

static int eax_slowpath_dma_prepare(struct snd_pcm_substream *substream)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	mutex_lock(&dsi.mutex);
	dsi.set_params_cnt++;
	mutex_unlock(&dsi.mutex);

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
		dma_info.fp = eax_dma_slowpath_buffdone;
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
		eax_slowpath_adma_hw_params(msi.mixbuf_byte);
	}

	return ret;
}

static int eax_slowpath_dma_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_info("Entered %s\n", __func__);

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
				eax_slowpath_mixer_trigger(true);
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
				eax_slowpath_mixer_trigger(false);
			}
			break;
		default:
		ret = -EINVAL;
		break;
	}

	spin_unlock(&prtd->lock);

	return ret;
}

static snd_pcm_uframes_t eax_slowpath_dma_pointer(struct snd_pcm_substream *substream)
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

static int eax_slowpath_dma_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd;

	pr_debug("Entered %s\n", __func__);

	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	snd_soc_set_runtime_hwparams(substream, &dma_hardware);

	prtd = kzalloc(sizeof(struct runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	spin_lock_init(&prtd->lock);

	runtime->private_data = prtd;
	prtd->substream = substream;

	eax_slowpath_mixer_add(prtd);

	return 0;
}

static int eax_slowpath_dma_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;

	pr_debug("Entered %s\n", __func__);

	if (!prtd)
		pr_debug("dma_close called with prtd == NULL\n");

	eax_slowpath_mixer_remove(prtd);

	kfree(prtd);

	return 0;
}

static int eax_slowpath_dma_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("Entered %s\n", __func__);

	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);
}

static struct snd_pcm_ops eax_slowpath_dma_ops = {
	.open		= eax_slowpath_dma_open,
	.close		= eax_slowpath_dma_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= eax_slowpath_dma_hw_params,
	.hw_free	= eax_slowpath_dma_hw_free,
	.prepare	= eax_slowpath_dma_prepare,
	.trigger	= eax_slowpath_dma_trigger,
	.pointer	= eax_slowpath_dma_pointer,
	.mmap		= eax_slowpath_dma_mmap,
};

static int eax_slowpath_prealloc_buffer(struct snd_pcm *pcm, int stream)
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
		iommu_base += 0x1000000;
	}

	buf->bytes = size;

	return 0;
}

static u64 eax_slowpath_dma_mask = DMA_BIT_MASK(32);
static int eax_slowpath_dma_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &eax_slowpath_dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

#ifdef EAX_SLOWPATH_DMA_PCM_DUMP
	if (!eax_slowpath_mixer_kobj) {
		eax_slowpath_mixer_kobj = kobject_create_and_add("eax-slowpath-mixer", NULL);
		if (sysfs_create_file(eax_slowpath_mixer_kobj, &slowpath_pcm_dump_attribute.attr))
			pr_err("%s: failed to create sysfs to control PCM dump\n", __func__);
	}
#endif

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = eax_slowpath_prealloc_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = eax_slowpath_prealloc_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
out:
	return ret;
}

static void eax_slowpath_dma_free(struct snd_pcm *pcm)
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

static struct snd_soc_platform_driver eax_slowpath_asoc_platform = {
	.ops		= &eax_slowpath_dma_ops,
	.pcm_new	= eax_slowpath_dma_new,
	.pcm_free	= eax_slowpath_dma_free,
};

int eax_slowpath_asoc_platform_register(struct device *dev)
{
	return snd_soc_register_platform(dev, &eax_slowpath_asoc_platform);
}
EXPORT_SYMBOL_GPL(eax_slowpath_asoc_platform_register);

void eax_slowpath_asoc_platform_unregister(struct device *dev)
{
	snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(eax_slowpath_asoc_platform_unregister);

static void eax_slowpath_mixer_exe(short *dma_buf)
{
	struct buf_info *bi;
	short pcm_l, pcm_r;
	int mix_l, mix_r;
	int n;

#ifdef EAX_SLOWPATH_DMA_PCM_DUMP
	list_for_each_entry(bi, &slowpath_buf_list, node) {
		if (bi->prtd && bi->prtd->running && input_dump_enabled) {
			if (bi->prtd->filp) {
				vfs_write(bi->prtd->filp, (char *)bi->prtd->dma_mono,
					msi.mixbuf_byte, &bi->prtd->filp->f_pos);
			}
		}
	}
#endif

	spin_lock(&msi.lock);

	pr_debug("%s: buf[%d] = %p, size = %lx\n",
		__func__, dsi.buf_wr_idx, dsi.buf_wr_p, msi.mixbuf_byte);
	for (n = 0; n < msi.mixbuf_size; n++) {
		mix_l = 0;
		mix_r = 0;

		list_for_each_entry(bi, &slowpath_buf_list, node) {
			if (bi->prtd && bi->prtd->running) {
				eax_slowpath_dma_xfer(bi->prtd, &pcm_l, &pcm_r);
				mix_l += pcm_l;
				mix_r += pcm_r;
			}
		}

		mix_l += *dma_buf++;
		mix_r += *dma_buf++;

		if (mix_l > 0x7fff)
			mix_l = 0x7fff;
		else if (mix_l < -0x7fff)
			mix_l = -0x7fff;

		if (mix_r > 0x7fff)
			mix_r = 0x7fff;
		else if (mix_r < -0x7fff)
			mix_r = -0x7fff;

		*dsi.buf_wr_p++ = (short)mix_l;
		*dsi.buf_wr_p++ = (short)mix_r;
	}

	msi.mix_cnt++;
	if (msi.mix_cnt == dsi.dma_buf_cnt) {
		msi.mix_cnt = 0;
		spin_unlock(&msi.lock);
#ifdef EAX_SLOWPATH_DMA_PCM_DUMP
		if (mix_dump_enabled) {
			vfs_write(mfilp, (void *)(dsi.buf_wr_p - msi.mixbuf_byte),
					msi.mixbuf_byte, &mfilp->f_pos);
		}
#endif
	} else {
		spin_unlock(&msi.lock);
#ifdef EAX_SLOWPATH_DMA_PCM_DUMP
		if (mix_dump_enabled) {
			vfs_write(mfilp, (void *)(dsi.buf_wr_p - msi.mixbuf_byte),
					msi.mixbuf_byte, &mfilp->f_pos);
		}
#endif
		eax_slowpath_mixer_exe(dsi.buf_wr_p);
	}
}

static void eax_slowpath_mixer_run(unsigned long data)
{
	short *dma_buf;

	dsi.buf_wr_p = (short *)(dsi.sram_dma_buf +
			(DMA_BUF_SIZE / DMA_PERIOD_CNT) * dsi.buf_wr_idx);
	if (!dsi.buf_wr_p || dsi.buf_wr_idx < 0) {
		pr_err("%s: EAX-SLOWPATH-MIXER failed to get SRAM buffer\n",
				__func__);
		return;
	}

	dma_buf = dsi.buf_wr_p;
	eax_slowpath_mixer_exe(dma_buf);
}

static int eax_slowpath_mixer_add(struct runtime_data *prtd)
{
	struct buf_info *bi;
	unsigned long flags;

	bi = kzalloc(sizeof(struct buf_info), GFP_KERNEL);
	if (!bi) {
		pr_err("%s: Memory alloc fails!\n", __func__);
		return -ENOMEM;
	}

	bi->prtd = prtd;

	spin_lock_irqsave(&msi.lock, flags);
	list_add(&bi->node, &slowpath_buf_list);
	spin_unlock_irqrestore(&msi.lock, flags);

	pr_debug("%s: prtd %p added\n", __func__, prtd);

	return 0;
}

static int eax_slowpath_mixer_remove(struct runtime_data *prtd)
{
	struct buf_info *bi;
	unsigned long flags;
	bool node_found = false;

	spin_lock_irqsave(&msi.lock, flags);
	list_for_each_entry(bi, &slowpath_buf_list, node) {
		if (bi->prtd == prtd) {
			node_found = true;
#ifdef EAX_SLOWPATH_DMA_PCM_DUMP
			close_file(bi->prtd);
#endif
			break;
		}
	}

	if (!node_found) {
		spin_unlock_irqrestore(&msi.lock, flags);
		pr_err("%s: prtd %p not found\n", __func__, prtd);
		return -EINVAL;
	}

	list_del(&bi->node);
	kfree(bi);
	spin_unlock_irqrestore(&msi.lock, flags);
	pr_debug("%s: prtd %p removed\n", __func__, prtd);

	return 0;
}

static void eax_slowpath_mixer_trigger(bool on)
{
	if (on) {
		msi.running = true;
		if (!dsi.running) {
			eax_slowpath_adma_trigger(true);
		}
	} else {
		if (!eax_slowpath_mixer_any_buf_running()) {
			if (dsi.running)
				eax_slowpath_adma_trigger(false);

			msi.running = false;
		}
	}
}

MODULE_AUTHOR("Hyunwoong Kim, <khw0178.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung ASoC EAX-SLOWPATH-DMA Driver");
MODULE_LICENSE("GPL");
