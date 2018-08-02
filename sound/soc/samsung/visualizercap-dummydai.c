/*
 * ALSA SoC dummy cpu & platform dai driver
 *
 *  This driver provides one dummy dai.
 *
 * Copyright (c) 2014 Samsung Electronics
 * http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/dma/dma-pl330.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>

#define PERIOD_MIN		4

static DECLARE_WAIT_QUEUE_HEAD(compr_cap_wq);
extern ssize_t esa_copy(unsigned long hwbuf, ssize_t size);
extern int esa_compr_running(void);
extern void esa_compr_ctrl_fxintr(bool fxon);

static const struct snd_pcm_hardware dma_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_BLOCK_TRANSFER |
				  SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_U16_LE |
				  SNDRV_PCM_FMTBIT_U8 |
				  SNDRV_PCM_FMTBIT_S8,
	.channels_min		= 1,
	.channels_max		= 8,
	.buffer_bytes_max	= 256*1024,
	.period_bytes_min	= 128,
	.period_bytes_max	= 32*1024,
	.periods_min		= 2,
	.periods_max		= 128,
	.fifo_size		= 32,
};

struct runtime_data {
	spinlock_t lock;
	int state;
	unsigned int dma_loaded;
	unsigned int dma_period;
	unsigned long buf_start;
	unsigned long buf_pos;
	unsigned long buf_end;
	unsigned long period_bytes;
	struct snd_pcm_hardware hw;
	struct snd_pcm_substream *substream;
	struct task_struct *compr_cap_kthread;
	bool running;
	bool opened;
	bool dram_used;
} rd;

static int dummy_dma_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	unsigned long totbytes = params_buffer_bytes(params);

	pr_debug("Entered %s\n", __func__);

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	runtime->dma_bytes = totbytes;

	spin_lock_irq(&prtd->lock);
	prtd->dma_loaded = 0;
	prtd->dma_period = params_period_bytes(params);
	prtd->buf_start = (unsigned long)runtime->dma_area;
	prtd->buf_pos = prtd->buf_start;
	prtd->buf_end = prtd->buf_start + totbytes;
	while ((totbytes / prtd->dma_period) < PERIOD_MIN)
		prtd->dma_period >>= 1;
	spin_unlock_irq(&prtd->lock);

	pr_info("Dummy DMA:%s:Addr=@0x%lx Total=%d PrdSz=%d(%d) #Prds=%d \n",
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? "P" : "C",
		prtd->buf_start, (u32)runtime->dma_bytes,
		params_period_bytes(params),(u32) prtd->dma_period,
		params_periods(params));

	return 0;
}

static int dummy_dma_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);

	return 0;
}

static int dummy_dma_prepare(struct snd_pcm_substream *substream)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_debug("Entered %s +\n", __func__);

	prtd->dma_loaded = 0;
	prtd->buf_pos = prtd->buf_start;

	pr_debug("Entered %s -\n", __func__);

	return ret;
}

static snd_pcm_uframes_t dummy_dma_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	unsigned long res;

	pr_debug("Entered %s\n", __func__);
	res = prtd->buf_pos - prtd->buf_start;

	pr_debug("%s res = %lx\n", __func__, res);

	return bytes_to_frames(substream->runtime, res);
}

static int dummy_dma_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_info("Entered %s\n", __func__);

	if (rd.opened)
		return -EBUSY;

	if (!esa_compr_running())
		return -ENODEV;

	spin_lock_init(&rd.lock);

	memcpy(&rd.hw, &dma_hardware, sizeof(struct snd_pcm_hardware));

	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	runtime->private_data = &rd;
	snd_soc_set_runtime_hwparams(substream, &rd.hw);

	rd.opened = true;

	pr_info("%s: prtd = %p\n", __func__, &rd);

	return 0;
}

static int dummy_dma_close(struct snd_pcm_substream *substream)
{
	pr_info("Entered %s\n", __func__);

	rd.opened = false;

	return 0;
}

static int dummy_dma_copy(struct snd_pcm_substream *substream, int channel,
		snd_pcm_uframes_t pos, void __user *buf, snd_pcm_uframes_t count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	char *hwbuf = runtime->dma_area + frames_to_bytes(runtime, pos);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (copy_from_user(hwbuf, buf, frames_to_bytes(runtime, count)))
			return -EFAULT;
	} else {
		if (copy_to_user(buf, hwbuf, frames_to_bytes(runtime, count)))
			return -EFAULT;
	}

	return 0;
}

static int dummy_dma_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_info("Entered %s\n", __func__);

	spin_lock(&prtd->lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* Enable seiren firmware effect Fx external interrupt
		   to capture offload's PCM data from firmware */
		esa_compr_ctrl_fxintr(true);
		rd.running = true;
		if (waitqueue_active(&compr_cap_wq))
			wake_up_interruptible(&compr_cap_wq);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
		rd.running = false;
		/* Disable seiren firmware effect Fx externalinterrupt */
		esa_compr_ctrl_fxintr(false);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock(&prtd->lock);

	return ret;
}

static struct snd_pcm_ops dummy_dma_ops = {
	.open		= dummy_dma_open,
	.close		= dummy_dma_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= dummy_dma_hw_params,
	.hw_free	= dummy_dma_hw_free,
	.prepare	= dummy_dma_prepare,
	.trigger	= dummy_dma_trigger,
	.pointer	= dummy_dma_pointer,
	.copy		= dummy_dma_copy,
};

static int compr_cap_kthr(void *p)
{
	struct runtime_data *prtd = (struct runtime_data *)p;
	int ret = 0;


	while (!kthread_should_stop()) {
		wait_event_interruptible(compr_cap_wq, rd.running);

		ret = esa_copy(prtd->buf_pos, prtd->dma_period);
		if (ret < 0) {
			pr_err("Failed to get f/w decoded pcm data\n");
			rd.running = false;
			continue;
		}

		prtd->buf_pos = prtd->buf_pos + prtd->dma_period;
		if (prtd->buf_pos >= prtd->buf_end)
			prtd->buf_pos = prtd->buf_start;
		snd_pcm_period_elapsed(rd.substream);
	}

	return 0;
}

static int preallocate_dma_buffer_of(struct snd_pcm *pcm, int stream,
					struct device_node *np)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	dma_addr_t dma_addr;
	size_t size = dma_hardware.buffer_bytes_max;

	pr_debug("Entered %s\n", __func__);

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;

	buf->area = dma_alloc_coherent(pcm->card->dev, size, &dma_addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;

	buf->addr = dma_addr;
	buf->bytes = size;

	rd.substream = substream;

	return 0;
}

static void dummy_dma_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;

	pr_debug("Entered %s\n", __func__);

	substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	if (!substream)
		return;

	buf = &substream->dma_buffer;
	if (!buf->area)
		return;

	dma_free_coherent(pcm->card->dev, buf->bytes, buf->area, buf->addr);
	buf->area = NULL;
}

static u64 dma_mask = DMA_BIT_MASK(32);

static int dummy_dma_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct device_node *np = rtd->cpu_dai->dev->of_node;
	struct sched_param param = { .sched_priority = 0 };
	struct task_struct *ret_task;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = 0;
		if (np)
			ret = preallocate_dma_buffer_of(pcm,
				SNDRV_PCM_STREAM_CAPTURE, np);
		if (ret)
			goto out;
	}

	ret_task = kthread_run(compr_cap_kthr, &rd, "compr_cap_kthr");
	if (IS_ERR(ret_task)) {
		pr_info("%s: failed to create compr_cap thread(%ld)\n",
				__func__, PTR_ERR(ret_task));
		ret = PTR_ERR(ret_task);
	} else {
		sched_setscheduler(ret_task, SCHED_NORMAL, &param);
	}

out:
	return ret;
}

static struct snd_soc_platform_driver dummy_asoc_platform = {
	.ops		= &dummy_dma_ops,
	.pcm_new	= dummy_dma_new,
	.pcm_free	= dummy_dma_free_dma_buffers,
};

#define SAMSUNG_I2S_RATES	SNDRV_PCM_RATE_8000_192000

#define SAMSUNG_I2S_FMTS	(SNDRV_PCM_FMTBIT_S8 | \
					SNDRV_PCM_FMTBIT_S16_LE | \
					SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_driver dummy_i2s_dai_drv = {
	.name		= "dummy-i2s-dai-driver",
};

static const struct snd_soc_component_driver dummy_i2s_component = {
	.name		= "dummy-i2s",
};

static int dummy_cpu_probe(struct platform_device *pdev)
{
	dummy_i2s_dai_drv.symmetric_rates = 1;
	dummy_i2s_dai_drv.capture.channels_min = 1;
	dummy_i2s_dai_drv.capture.channels_max = 2;
	dummy_i2s_dai_drv.capture.rates = SAMSUNG_I2S_RATES;
	dummy_i2s_dai_drv.capture.formats = SAMSUNG_I2S_FMTS;

	snd_soc_register_component(&pdev->dev, &dummy_i2s_component,
			&dummy_i2s_dai_drv, 1);

	snd_soc_register_platform(&pdev->dev, &dummy_asoc_platform);

	return 0;
}

static int dummy_cpu_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

static const struct of_device_id dummy_cpu_of_match[] = {
	{ .compatible = "samsung,dummy-i2s", },
	{},
};
MODULE_DEVICE_TABLE(of, dummy_cpu_of_match);

static struct platform_driver dummy_cpu_driver = {
	.probe		= dummy_cpu_probe,
	.remove		= dummy_cpu_remove,
	.driver		= {
		.name	= "dummy-i2s",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(dummy_cpu_of_match),
	},
};

module_platform_driver(dummy_cpu_driver);

MODULE_AUTHOR("Hyunwoong Kim <khw0178.kim@samsung.com>");
MODULE_DESCRIPTION("Dummy dai driver");
MODULE_LICENSE("GPL");
