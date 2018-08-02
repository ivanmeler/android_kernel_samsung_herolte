/*
 * dma.c  --  ALSA Soc Audio Layer
 *
 * (c) 2006 Wolfson Microelectronics PLC.
 * Graeme Gregory graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 * Copyright 2004-2005 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/iommu.h>
#include <linux/dma/dma-pl330.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/exynos.h>

#if 0
#include <mach/map.h>
#endif

#include "dma.h"
#include "lpass.h"

#ifdef CONFIG_SND_SAMSUNG_SEIREN_DMA
#include "seiren/seiren-dma.h"
#endif

#define PERIOD_MIN		4
#define ST_RUNNING		(1<<0)
#define ST_OPENED		(1<<1)

#define SRAM_END		(0x04000000)
#define RX_SRAM_SIZE		(0x2000)	/* 8 KB */
#define MAX_DEEPBUF_SIZE	(0xA000)	/* 40 KB */

static struct snd_dma_buffer  *sram_rx_buf = NULL;
static struct snd_dma_buffer  *dram_uhqa_tx_buf = NULL;

static const struct snd_pcm_hardware dma_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_BLOCK_TRANSFER |
				  SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID,
	.formats		= SNDRV_PCM_FMTBIT_S24_LE |
				  SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_U16_LE |
				  SNDRV_PCM_FMTBIT_U8 |
				  SNDRV_PCM_FMTBIT_S8,
	.channels_min		= 1,
	.channels_max		= 8,
	.buffer_bytes_max	= 256*1024,
	.period_bytes_min	= 128,
	.period_bytes_max	= 64*1024,
	.periods_min		= 2,
	.periods_max		= 128,
	.fifo_size		= 32,
};

struct runtime_data {
	spinlock_t lock;
	int state;
	unsigned int dma_loaded;
	unsigned int dma_period;
	dma_addr_t dma_start;
	dma_addr_t dma_pos;
	dma_addr_t dma_end;
	struct s3c_dma_params *params;
	struct snd_pcm_hardware hw;
	bool cap_dram_used;
	dma_addr_t irq_pos;
	u32 irq_cnt;
};

#ifdef CONFIG_SND_SAMSUNG_IOMMU
struct dma_iova {
	dma_addr_t		iova;
	dma_addr_t		pa;
	unsigned char		*va;
	struct list_head	node;
};

static LIST_HEAD(iova_list);
#endif

static void audio_buffdone(void *data);

/* check_adma_status
 *
 * ADMA status is checked for AP Power mode.
 * return 1 : ADMA use dram area and it is running.
 * return 0 : ADMA has a fine condition to enter Low Power Mode.
 */
int check_adma_status(void)
{
	return lpass_get_dram_usage_count() ? 1 : 0;
}

/* dma_enqueue
 *
 * place a dma buffer onto the queue for the dma system
 * to handle.
 */
static void dma_enqueue(struct snd_pcm_substream *substream)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	dma_addr_t pos = prtd->dma_pos;
	unsigned int limit;
	struct samsung_dma_prep dma_info;

	pr_info("Entered %s\n", __func__);

	limit = (prtd->dma_end - prtd->dma_start) / prtd->dma_period;

	pr_debug("%s: loaded %d, limit %d\n",
				__func__, prtd->dma_loaded, limit);

	dma_info.cap = prtd->params->esa_dma ? DMA_CYCLIC :
			(samsung_dma_has_circular() ? DMA_CYCLIC : DMA_SLAVE);
	dma_info.direction =
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK
		? DMA_MEM_TO_DEV : DMA_DEV_TO_MEM);
	dma_info.fp = audio_buffdone;
	dma_info.fp_param = substream;
	dma_info.period = prtd->dma_period;
	dma_info.len = prtd->dma_period*limit;

	if (prtd->params->esa_dma || samsung_dma_has_infiniteloop()) {
		dma_info.buf = prtd->dma_pos;
		dma_info.infiniteloop = limit;
		prtd->params->ops->prepare(prtd->params->ch, &dma_info);
	} else {
		dma_info.infiniteloop = 0;
		while (prtd->dma_loaded < limit) {
			pr_debug("dma_loaded: %d\n", prtd->dma_loaded);

			if ((pos + dma_info.period) > prtd->dma_end) {
				dma_info.period  = prtd->dma_end - pos;
				pr_debug("%s: corrected dma len %ld\n",
						__func__, dma_info.period);
			}

			dma_info.buf = pos;
			prtd->params->ops->prepare(prtd->params->ch, &dma_info);

			prtd->dma_loaded++;
			pos += prtd->dma_period;
			if (pos >= prtd->dma_end)
				pos = prtd->dma_start;
		}
		prtd->dma_pos = pos;
	}
}

static void audio_buffdone(void *data)
{
	struct snd_pcm_substream *substream = data;
	struct runtime_data *prtd;
	dma_addr_t src, dst, pos;

	pr_debug("Entered %s\n", __func__);

	if (!substream)
		return;

	prtd = substream->runtime->private_data;

	if (!prtd->params || !prtd->params->ch)
		return;

	if (prtd->state & ST_RUNNING) {
		prtd->params->ops->getposition(prtd->params->ch, &src, &dst);
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			pos = dst - prtd->dma_start;
		else
			pos = src - prtd->dma_start;

		prtd->irq_cnt++;
		prtd->irq_pos = pos;
		pos /= prtd->dma_period;
		pos = prtd->dma_start + (pos * prtd->dma_period);
		if (pos >= prtd->dma_end)
			pos = prtd->dma_start;

		prtd->dma_pos = pos;
		snd_pcm_period_elapsed(substream);

		if (!prtd->params->esa_dma && !samsung_dma_has_circular()) {
			spin_lock(&prtd->lock);
			prtd->dma_loaded--;
			if (!samsung_dma_has_infiniteloop())
				dma_enqueue(substream);
			spin_unlock(&prtd->lock);
		}
	}
}

static int dma_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned long totbytes = params_buffer_bytes(params);
	struct s3c_dma_params *dma =
		snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	struct samsung_dma_req req;
	struct samsung_dma_config config;

	pr_debug("Entered %s\n", __func__);

	/* return if this is a bufferless transfer e.g.
	 * codec <--> BT codec or GSM modem -- lg FIXME */
	if (!dma)
		return 0;

	/* this may get called several times by oss emulation
	 * with different params -HW */
	if (prtd->params == NULL) {
		/* prepare DMA */
		prtd->params = dma;

		pr_debug("params %p, client %p, channel %d\n", prtd->params,
			prtd->params->client, prtd->params->channel);

		if (prtd->params->esa_dma) {
			prtd->params->ops = samsung_dma_get_ops();
			req.cap = DMA_CYCLIC;
		} else {
			pr_info("No esa_dma %s\n", __func__);
			prtd->params->ops = samsung_dma_get_ops();
			req.cap = (samsung_dma_has_circular() ?
				DMA_CYCLIC : DMA_SLAVE);
		}
		req.client = prtd->params->client;
		config.direction =
			(substream->stream == SNDRV_PCM_STREAM_PLAYBACK
			? DMA_MEM_TO_DEV : DMA_DEV_TO_MEM);
		config.width = prtd->params->dma_size;
		/* config.maxburst = 1; */
		config.fifo = prtd->params->dma_addr;
		if (prtd->params->compr_dma) {
			pr_info("%s: %d\n", __func__, __LINE__);
			prtd->params->ch = prtd->params->ops->request(
				prtd->params->channel, &req,
				prtd->params->sec_dma_dev,
				prtd->params->ch_name);
		} else {
			pr_info("%s: %d\n", __func__, __LINE__);
			prtd->params->ch = prtd->params->ops->request(
				prtd->params->channel, &req, rtd->cpu_dai->dev,
				prtd->params->ch_name);
		}
		pr_info("dma_request: ch %d, req %p, dev %p, ch_name [%s]\n",
			prtd->params->channel, &req, rtd->cpu_dai->dev,
			prtd->params->ch_name);
		prtd->params->ops->config(prtd->params->ch, &config);
	}

	if ((substream->stream == SNDRV_PCM_STREAM_CAPTURE) &&
		(totbytes <= RX_SRAM_SIZE) && sram_rx_buf)
		snd_pcm_set_runtime_buffer(substream, sram_rx_buf);
	else if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK) &&
		(totbytes > MAX_DEEPBUF_SIZE) && dram_uhqa_tx_buf)
		snd_pcm_set_runtime_buffer(substream, dram_uhqa_tx_buf);
	else
		snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	runtime->dma_bytes = totbytes;

	spin_lock_irq(&prtd->lock);
	prtd->dma_loaded = 0;
	prtd->dma_period = params_period_bytes(params);
	prtd->dma_start = runtime->dma_addr;
	prtd->dma_pos = prtd->dma_start;
	prtd->dma_end = prtd->dma_start + totbytes;
	prtd->cap_dram_used = runtime->dma_addr < SRAM_END ? false : true;
	while ((totbytes / prtd->dma_period) < PERIOD_MIN)
		prtd->dma_period >>= 1;
	spin_unlock_irq(&prtd->lock);

	pr_info("ADMA:%s:DmaAddr=@%x Total=%d PrdSz=%d(%d) #Prds=%d dma_area=0x%p\n",
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? "P" : "C",
		(u32)prtd->dma_start, (u32)runtime->dma_bytes,
		params_period_bytes(params),(u32) prtd->dma_period,
		params_periods(params), runtime->dma_area);

	return 0;
}

static int dma_hw_free(struct snd_pcm_substream *substream)
{
	struct runtime_data *prtd = substream->runtime->private_data;

	pr_debug("Entered %s\n", __func__);

	snd_pcm_set_runtime_buffer(substream, NULL);

	if (prtd->params) {
		prtd->params->ops->flush(prtd->params->ch);
		prtd->params->ops->release(prtd->params->ch,
					prtd->params->client);
		prtd->params = NULL;
	}

	return 0;
}

static int dma_prepare(struct snd_pcm_substream *substream)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_info("Entered %s\n", __func__);

	/* return if this is a bufferless transfer e.g.
	 * codec <--> BT codec or GSM modem -- lg FIXME */
	if (!prtd->params)
		return 0;

	/* flush the DMA channel */
	prtd->params->ops->flush(prtd->params->ch);
	prtd->dma_loaded = 0;
	prtd->dma_pos = prtd->dma_start;
	prtd->irq_pos = prtd->dma_start;
	prtd->irq_cnt = 0;

	/* enqueue dma buffers */
	dma_enqueue(substream);

	return ret;
}

static int dma_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	spin_lock(&prtd->lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->state |= ST_RUNNING;
		lpass_dma_enable(true);
		prtd->params->ops->trigger(prtd->params->ch);
		if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ||
			(prtd->cap_dram_used)) {
			lpass_inc_dram_usage_count();
			lpass_update_lpclock(LPCLK_CTRLID_LEGACY, false);
		} else {
			lpass_update_lpclock(LPCLK_CTRLID_RECORD, true);
		}
		break;

	case SNDRV_PCM_TRIGGER_STOP:
		prtd->state &= ~ST_RUNNING;
		prtd->params->ops->stop(prtd->params->ch);
		lpass_dma_enable(false);
		if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ||
			(prtd->cap_dram_used)) {
			lpass_dec_dram_usage_count();
			lpass_update_lpclock(LPCLK_CTRLID_LEGACY, false);
		} else {
			lpass_update_lpclock(LPCLK_CTRLID_RECORD, false);
			lpass_disable_mif_status(false);
		}
		break;

	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock(&prtd->lock);

	return ret;
}

static snd_pcm_uframes_t dma_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	unsigned long res;

	pr_debug("Entered %s\n", __func__);

	res = prtd->dma_pos - prtd->dma_start;

	pr_debug("Pointer offset: %lu\n", res);

	/* we seem to be getting the odd error from the pcm library due
	 * to out-of-bounds pointers. this is maybe due to the dma engine
	 * not having loaded the new values for the channel before being
	 * called... (todo - fix )
	 */

	if (res >= snd_pcm_lib_buffer_bytes(substream)) {
		if (res == snd_pcm_lib_buffer_bytes(substream))
			res = 0;
	}

	return bytes_to_frames(substream->runtime, res);
}

static int dma_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd;

	pr_debug("Entered %s\n", __func__);

	prtd = kzalloc(sizeof(struct runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	spin_lock_init(&prtd->lock);

	memcpy(&prtd->hw, &dma_hardware, sizeof(struct snd_pcm_hardware));

	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	runtime->private_data = prtd;
	snd_soc_set_runtime_hwparams(substream, &prtd->hw);

	pr_info("%s: prtd = %p\n", __func__, prtd);

	return 0;
}

static int dma_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;

	pr_debug("Entered %s\n", __func__);

	if (!prtd) {
		pr_debug("dma_close called with prtd == NULL\n");
		return 0;
	}

	pr_info("%s: prtd = %p, irq_cnt %u\n",
			__func__, prtd, prtd->irq_cnt);
	kfree(prtd);

	return 0;
}

static int dma_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	dma_addr_t dma_pa = runtime->dma_addr;
#ifdef CONFIG_SND_SAMSUNG_IOMMU
	struct dma_iova *di;
#endif

	pr_debug("Entered %s\n", __func__);

#ifdef CONFIG_SND_SAMSUNG_IOMMU
	list_for_each_entry(di, &iova_list, node) {
		if (di->iova == runtime->dma_addr)
			dma_pa = di->pa;
	}
#endif
	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area, dma_pa,
				     runtime->dma_bytes);
}

static struct snd_pcm_ops pcm_dma_ops = {
	.open		= dma_open,
	.close		= dma_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= dma_hw_params,
	.hw_free	= dma_hw_free,
	.prepare	= dma_prepare,
	.trigger	= dma_trigger,
	.pointer	= dma_pointer,
	.mmap		= dma_mmap,
};

static int preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
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

static const char *dma_prop_addr[2] = {
	[SNDRV_PCM_STREAM_PLAYBACK] = "samsung,tx-buf",
	[SNDRV_PCM_STREAM_CAPTURE]  = "samsung,rx-buf"
};
static const char *dma_prop_size[2] = {
	[SNDRV_PCM_STREAM_PLAYBACK] = "samsung,tx-size",
	[SNDRV_PCM_STREAM_CAPTURE]  = "samsung,rx-size"
};
static const char *dma_prop_iommu[2] = {
	[SNDRV_PCM_STREAM_PLAYBACK] = "samsung,tx-iommu",
	[SNDRV_PCM_STREAM_CAPTURE]  = "samsung,rx-iommu"
};

static int preallocate_dma_buffer_of(struct snd_pcm *pcm, int stream,
					struct device_node *np)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	dma_addr_t dma_addr;
	size_t size;
	u32 val;
#ifdef CONFIG_SND_SAMSUNG_IOMMU
	struct iommu_domain *domain = lpass_get_iommu_domain();
	dma_addr_t dma_buf_pa;
	struct dma_iova *di, *di_uhqa;
	int ret;
#endif
	pr_debug("Entered %s\n", __func__);

	if (of_property_read_u32(np, dma_prop_addr[stream], &val))
		return -ENOMEM;
	dma_addr = (dma_addr_t)val;

	if (of_property_read_u32(np, dma_prop_size[stream], &val))
		return -ENOMEM;
	size = (size_t)val;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->addr = dma_addr;

#ifdef CONFIG_SND_SAMSUNG_IOMMU
	if (of_find_property(np, dma_prop_iommu[stream], NULL)) {
		di = devm_kzalloc(pcm->card->dev,
					sizeof(struct dma_iova), GFP_KERNEL);
		if (!di)
			return -ENOMEM;

		buf->area = dma_alloc_coherent(pcm->card->dev, size,
						&dma_buf_pa, GFP_KERNEL);
		if (!buf->area)
			return -ENOMEM;

		ret = iommu_map(domain, dma_addr, dma_buf_pa, size, 0);
		if (ret) {
			dma_free_coherent(pcm->card->dev, size,
						buf->area, dma_buf_pa);
			pr_err("%s: Failed to iommu_map: %d\n", __func__, ret);
			return -ENOMEM;
		}

		di->iova = buf->addr;
		di->pa = dma_buf_pa;
		di->va = buf->area;
		list_add(&di->node, &iova_list);

		pr_info("%s: DmaAddr-iommu %08X dma_buf_pa %08X\n",
				__func__, (u32)dma_addr, (u32)dma_buf_pa);
	} else {
		buf->area = ioremap(buf->addr, size);
	}

	/*
	 * With LPC Recording, Platform DAI driver should provide USER with SRAM
	 * Buffer if recording buffer size is smaller than RX_SRAM_SIZE.
	 * Below code parses information of REC_SRAM.
	 */
	if (!sram_rx_buf) {
		if (of_find_property(np, "samsung,rx-sram", NULL)) {
			u32 sram_info[2];
			sram_rx_buf = devm_kzalloc(pcm->card->dev,
				sizeof(*sram_rx_buf), GFP_KERNEL);
			if (!sram_rx_buf) {
				pr_err("Failed to allocate rx-sram buffer = %pa\n",
					sram_rx_buf);
				return -ENOMEM;
			}
			sram_rx_buf->dev.type = SNDRV_DMA_TYPE_DEV;
			sram_rx_buf->dev.dev = pcm->card->dev;

			/* Array Value : RX-SRAM Base Address  RX-SRAM Size*/
			of_property_read_u32_array(np, "samsung,rx-sram",
				sram_info, 2);
			sram_rx_buf->addr = (dma_addr_t)sram_info[0];
			sram_rx_buf->bytes = (size_t)sram_info[1];

			if (!sram_rx_buf->addr || sram_rx_buf->addr > SRAM_END) {
				pr_err("Failed to find rx-sram addr = %pa\n",
					&sram_info[0]);
				return -ENOMEM;
			}

			if (sram_rx_buf->bytes > RX_SRAM_SIZE || sram_rx_buf->bytes == 0) {
				pr_err("Failed to find rx-sram size = %x\n",
					sram_info[1]);
				return -EINVAL;
			}
			sram_rx_buf->area = ioremap(sram_rx_buf->addr, sram_rx_buf->bytes);
			if (!sram_rx_buf->area) {
				pr_info("Failed to map RX-SRAM into kernel virtual\n");
				return -ENOMEM;
			}
			pr_info("Audio RX-SRAM Information, pa = %pa, size = %zx, kva = %p\n",
			&sram_rx_buf->addr, sram_rx_buf->bytes, sram_rx_buf->area);
		}
	}

	/*
	 * With UHQA Playback, Platform DAI driver should provide USER with DRAM
	 * Buffer if UHQA buffer size is bigger than MAX_DEEPBUF_SIZE(40KB).
	 */
	if (!dram_uhqa_tx_buf) {
		if (of_find_property(np, "samsung,tx-uhqa-buf", NULL)) {
			u32 dram_info[2];
			phys_addr_t tx_uhqa_buf_pa = 0;
			dram_uhqa_tx_buf = devm_kzalloc(pcm->card->dev,
				sizeof(*dram_uhqa_tx_buf), GFP_KERNEL);
			if (!dram_uhqa_tx_buf) {
				pr_err("Failed to allocate dram-tx-uhqa buffer = %pa\n",
					dram_uhqa_tx_buf);
				return -ENOMEM;
			}
			dram_uhqa_tx_buf->dev.type = SNDRV_DMA_TYPE_DEV;
			dram_uhqa_tx_buf->dev.dev = pcm->card->dev;

			/* Array Value : TX-UHQA DVA Base Address Size*/
			of_property_read_u32_array(np, "samsung,tx-uhqa-buf",
				dram_info, 2);
			dram_uhqa_tx_buf->addr = (dma_addr_t)dram_info[0];
			dram_uhqa_tx_buf->bytes = (size_t)dram_info[1];
			if (!dram_uhqa_tx_buf->addr || !dram_uhqa_tx_buf->bytes) {
				pr_err("Failed to find tx-uhqa-buf information\n");
				return -ENOMEM;
			}

			di_uhqa = devm_kzalloc(pcm->card->dev,
					sizeof(struct dma_iova), GFP_KERNEL);
			if (!di_uhqa)
				return -ENOMEM;

			dram_uhqa_tx_buf->area = dma_alloc_coherent(pcm->card->dev,
					dram_uhqa_tx_buf->bytes,
					&tx_uhqa_buf_pa, GFP_KERNEL);
			if (!dram_uhqa_tx_buf->area)
				return -ENOMEM;

			ret = iommu_map(domain, dram_uhqa_tx_buf->addr,
					tx_uhqa_buf_pa, dram_uhqa_tx_buf->bytes, 0);
			if (ret) {
				dma_free_coherent(pcm->card->dev, size,
						dram_uhqa_tx_buf->area, tx_uhqa_buf_pa);
				pr_err("%s: Failed to iommu_map: %d\n", __func__, ret);
				return -ENOMEM;
			}

			di_uhqa->iova = dram_uhqa_tx_buf->addr;
			di_uhqa->pa = tx_uhqa_buf_pa;
			di_uhqa->va = dram_uhqa_tx_buf->area;
			list_add(&di_uhqa->node, &iova_list);

			pr_info("Audio TX-UHQA-BUF Information, pa = %pa, size = %zx, kva = %p\n",
				&dram_uhqa_tx_buf->addr, dram_uhqa_tx_buf->bytes,
				dram_uhqa_tx_buf->area);
		}
	}

#else
	if (of_find_property(np, dma_prop_iommu[stream], NULL))
		return -ENOMEM;
	else
		buf->area = ioremap(buf->addr, size);
#endif

	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;
	return 0;
}

static void dma_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	struct runtime_data *prtd;
	int stream;

	pr_debug("Entered %s\n", __func__);

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		prtd = substream->runtime->private_data;
		if (prtd->cap_dram_used) {
			dma_free_coherent(pcm->card->dev, buf->bytes,
						buf->area, buf->addr);
		} else {
			iounmap(buf->area);
		}

		buf->area = NULL;
	}
}

static u64 dma_mask = DMA_BIT_MASK(32);

static int dma_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct device_node *np = rtd->cpu_dai->dev->of_node;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = 0;
		if (np)
			ret = preallocate_dma_buffer_of(pcm,
				SNDRV_PCM_STREAM_PLAYBACK, np);
		if (ret)
			ret = preallocate_dma_buffer(pcm,
				SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = 0;
		if (np)
			ret = preallocate_dma_buffer_of(pcm,
				SNDRV_PCM_STREAM_CAPTURE, np);
		if (ret)
			ret = preallocate_dma_buffer(pcm,
				SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
out:
	return ret;
}

static struct snd_soc_platform_driver samsung_asoc_platform = {
	.ops		= &pcm_dma_ops,
	.pcm_new	= dma_new,
	.pcm_free	= dma_free_dma_buffers,
};

int asoc_dma_platform_register(struct device *dev)
{
	return snd_soc_register_platform(dev, &samsung_asoc_platform);
}
EXPORT_SYMBOL_GPL(asoc_dma_platform_register);

void asoc_dma_platform_unregister(struct device *dev)
{
	snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(asoc_dma_platform_unregister);

MODULE_AUTHOR("Ben Dooks, <ben@simtec.co.uk>");
MODULE_DESCRIPTION("Samsung ASoC DMA Driver");
MODULE_LICENSE("GPL");
