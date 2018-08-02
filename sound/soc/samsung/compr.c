/* sound/soc/samsung/compr.c
 *
 * ALSA SoC Audio Layer - Samsung Compress platform driver
 *
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *	Yeongman Seo <yman.seo@samsung.com>
 *      Lee Tae Ho <taeho07.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/pm_runtime.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include "compr.h"
#include "lpass.h"
#include "./seiren/seiren.h"
#ifdef CONFIG_SND_ESA_SA_EFFECT
#include "esa_sa_effect.h"
#endif
static struct snd_compr_caps compr_cap = {
	.direction		= SND_COMPRESS_PLAYBACK,
	.min_fragment_size	= 4 * 1024,
	.max_fragment_size	= 32 * 1024,
	.min_fragments		= 1,
	.max_fragments		= 5,
	.num_codecs		= 2,
	.codecs[COMPR_MP3]	= SND_AUDIOCODEC_MP3,
	.codecs[COMPR_AAC]	= SND_AUDIOCODEC_AAC,
};

struct audio_processor* compr_audio_processor_alloc(seiren_ops ops, void* priv)
{
	struct audio_processor *ap;

	ap = kzalloc(sizeof(struct audio_processor), GFP_KERNEL);
	if (!ap)
		return NULL;
	ap->ops = ops;
	ap->priv = priv;
	ap->reg_ack = esa_compr_get_mem() + COMPR_ACK;

	return ap;
}

#ifdef AUDIO_PERF
enum CHECK_TIMES {
	OPEN_T = 0x0,
	WRITE_T,
	POINTER_T,
	DRAIN_T,
	ISR_T,
	TOTAL_TIMES,
};
#endif

struct runtime_data {
	struct snd_compr_stream *cstream;
	struct snd_compr_caps *compr_cap;
	struct snd_compr_params codec_param;

	spinlock_t lock;
	int				state;
	struct snd_soc_dai		*cpu_dai;
	struct snd_soc_dai		*codec_dai;
	struct snd_pcm_substream	substream;
	struct snd_pcm_hw_params	hw_params;

	uint32_t byte_offset;
	u64 copied_total;
	u64 received_total;
	u64 app_pointer;
	void *buffer;

#ifdef AUDIO_PERF
	uint32_t start_time[TOTAL_TIMES];
	uint32_t end_time[TOTAL_TIMES];
	u64 total_time[TOTAL_TIMES];
#endif
	atomic_t start;
	atomic_t eos;
	atomic_t created;

	wait_queue_head_t flush_wait;
	wait_queue_head_t exit_wait;

	uint32_t stop_ack;
	uint32_t exit_ack;

	struct audio_processor* ap;
};

int compr_dai_cmd(struct runtime_data *prtd, int cmd);
static int compr_event_handler(uint32_t cmd, uint32_t size, void* priv)
{
	struct runtime_data *prtd = priv;
	struct snd_compr_runtime *runtime = prtd->cstream->runtime;
	u64 bytes_available;
	int ret;

	pr_debug("%s: event handler cmd(%x)\n", __func__, cmd);

#ifdef AUDIO_PERF
	prtd->start_time[ISR_T] = sched_clock();
#endif
	switch(cmd) {
	case INTR_CREATED:
		pr_debug("%s: offload instance is created\n", __func__);
		break;
	case INTR_DECODED:
		spin_lock(&prtd->lock);

		/* update copied total bytes */
		prtd->copied_total += size;
		prtd->byte_offset += size;
		if (prtd->byte_offset >= runtime->buffer_size)
			prtd->byte_offset -= runtime->buffer_size;

		snd_compr_fragment_elapsed(prtd->cstream);

		if (!atomic_read(&prtd->start) &&
			runtime->state != SNDRV_PCM_STATE_PAUSED) {
			/* Writes must be restarted from _copy() */
			pr_err("%s: write_done received while not started(%d)",
				__func__, runtime->state);
			spin_unlock(&prtd->lock);
			return -EIO;
		}

		bytes_available = prtd->received_total -
					prtd->copied_total;

		pr_debug("%s: current free bufsize(%llu)\n", __func__,
			runtime->buffer_size - bytes_available);

		if (bytes_available < runtime->fragment_size) {
			pr_debug("%s: WRITE_DONE Insufficient data to send.(avail:%llu)\n",
				__func__, bytes_available);
		}
		spin_unlock(&prtd->lock);
		break;
	case INTR_FLUSH:
		prtd->stop_ack = 1;
		wake_up(&prtd->flush_wait);
		break;
	case INTR_PAUSED:
		ret = compr_dai_cmd(prtd, cmd);
		if (ret)
			pr_err("%s: compr_dai_cmd fail(%d)\n", __func__, ret);
		break;
	case INTR_EOS:
		if (atomic_read(&prtd->eos)) {
			if (prtd->copied_total != prtd->received_total)
				pr_err("%s: EOS is not sync!(%llu/%llu)\n", __func__,
					prtd->copied_total, prtd->received_total);
			/* ALSA Framework callback to notify drain complete */
			snd_compr_drain_notify(prtd->cstream);
			atomic_set(&prtd->eos, 0);
			pr_info("%s: DATA_CMD_EOS wake up\n", __func__);
		}
		break;
	case INTR_DESTROY:
		prtd->exit_ack = 1;
		wake_up(&prtd->exit_wait);
		break;
	default:
		pr_err("%s: unknown command(%x)\n", __func__, cmd);
		break;
	}
#ifdef AUDIO_PERF
	prtd->end_time[ISR_T] = sched_clock();
	prtd->total_time[ISR_T] +=
		prtd->end_time[ISR_T] - prtd->start_time[ISR_T];
#endif
	return 0;
}

static int compr_config_substream(struct snd_compr_stream *cstream,
				   struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	int ret;

	pr_debug("%s\n", __func__);

	substream->pid = get_task_pid(current, PIDTYPE_PID);
	substream->private_data = rtd;
	substream->stream = (cstream->direction == SND_COMPRESS_PLAYBACK) ?
			SNDRV_PCM_STREAM_PLAYBACK : SNDRV_PCM_STREAM_CAPTURE;
	substream->runtime = kzalloc(sizeof(struct snd_pcm_runtime), GFP_KERNEL);
	if (substream->runtime == NULL) {
		ret = -ENOMEM;
		goto config_substream_runtime_err;
	}
	substream->runtime->hw_constraints.rules_num = 0;
	substream->runtime->hw_constraints.rules_all = 1;
	substream->runtime->hw_constraints.rules =
		kzalloc(sizeof(struct snd_pcm_hw_rule), GFP_KERNEL);
	if (substream->runtime->hw_constraints.rules == NULL) {
		ret = -ENOMEM;
		goto config_substream_runtime_rules_err;
	}
	return 0;

config_substream_runtime_rules_err:
	kfree(substream->runtime);
config_substream_runtime_err:
	return ret;
}

static int compr_dai_setup(struct runtime_data *prtd, struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm_substream *substream = &prtd->substream;
	struct snd_soc_dai *cpu_dai = prtd->cpu_dai;
	struct snd_soc_dai *codec_dai = prtd->codec_dai;
	const struct snd_soc_dai_ops *cpu_dai_ops = cpu_dai->driver->ops;
	const struct snd_soc_dai_ops *codec_dai_ops = codec_dai->driver->ops;
	struct snd_pcm_hw_params *params = &prtd->hw_params;
	int ret;

	if (cpu_dai_ops->startup) {
		ret = (*cpu_dai_ops->startup)(substream, cpu_dai);
		if (ret < 0) {
			dev_err(cpu_dai->dev, "can't open interface"
				" %s: %d\n", cpu_dai->name, ret);
			goto cpu_dai_err;
		}
	}

	if (codec_dai_ops->startup) {
		ret = (*codec_dai_ops->startup)(substream, codec_dai);
		if (ret < 0) {
			dev_err(codec_dai->dev, "can't open codec"
				" %s: %d\n", codec_dai->name, ret);
			goto codec_dai_err;
		}
	}

	if (rtd->dai_link->ops->hw_params) {
		ret = (*rtd->dai_link->ops->hw_params)(substream, params);
		if (ret < 0) {
			pr_err("%s: hw_params err(%d)\n", __func__, ret);
			goto hw_params_err;
		}
	}

	if (codec_dai_ops->hw_params) {
		ret = (*codec_dai_ops->hw_params)(substream, params, codec_dai);
		if (ret < 0) {
			dev_err(codec_dai->dev, "can't set %s hw params:"
				" %d\n", codec_dai->name, ret);
			goto codec_dai_hw_param_err;
		}
	}

	if (cpu_dai_ops->hw_params) {
		ret = (*cpu_dai_ops->hw_params)(substream, params, cpu_dai);
		if (ret < 0) {
			dev_err(cpu_dai->dev, "can't set %s hw params:"
				" %d\n", cpu_dai->name, ret);
			goto cpu_dai_hw_param_err;
		}
	}
	prtd->cpu_dai->rate = params_rate(params);
	prtd->codec_dai->rate = params_rate(params);

	return 0;
cpu_dai_hw_param_err:
	if (cpu_dai_ops->hw_free)
		(*cpu_dai_ops->hw_free)(substream, prtd->cpu_dai);
codec_dai_hw_param_err:
	if (codec_dai_ops->hw_free)
		(*codec_dai_ops->hw_free)(substream, prtd->codec_dai);
hw_params_err:
codec_dai_err:
	if (codec_dai_ops->shutdown)
		(*codec_dai_ops->shutdown)(substream, codec_dai);
cpu_dai_err:
	if (cpu_dai_ops->shutdown)
		(*cpu_dai_ops->shutdown)(substream, cpu_dai);
	return ret;
}

int compr_dai_cmd(struct runtime_data *prtd, int cmd)
{
	struct snd_pcm_substream *substream = &prtd->substream;
	struct snd_soc_dai *cpu_dai = prtd->cpu_dai;
	struct snd_soc_dai *codec_dai = prtd->codec_dai;
	const struct snd_soc_dai_ops *cpu_dai_ops = cpu_dai->driver->ops;
	const struct snd_soc_dai_ops *codec_dai_ops = codec_dai->driver->ops;
	int ret;

	if (codec_dai_ops->trigger) {
		ret = (*codec_dai_ops->trigger)(substream, cmd, codec_dai);
		if (ret < 0) {
			pr_err("%s: error codec_dai trigger(%d)\n", __func__, cmd);
			goto trigger_err;
		}
	}

	if (cpu_dai_ops->trigger) {
		ret = (*cpu_dai_ops->trigger)(substream, cmd, cpu_dai);
		if (ret < 0) {
			pr_err("%s: error cpu_dai trigger(%d)\n", __func__, cmd);
			goto trigger_err;
		}
	}
	return 0;
trigger_err:
	return ret;
}

static int compr_dai_prepare(struct runtime_data *prtd)
{
	struct snd_pcm_substream *substream = &prtd->substream;
	struct snd_soc_dai *cpu_dai = prtd->cpu_dai;
	struct snd_soc_dai *codec_dai = prtd->codec_dai;
	const struct snd_soc_dai_ops *cpu_dai_ops = cpu_dai->driver->ops;
	const struct snd_soc_dai_ops *codec_dai_ops = codec_dai->driver->ops;
	int ret;

	if (codec_dai_ops->prepare) {
		ret = (*codec_dai_ops->prepare)(substream, codec_dai);
		if (ret < 0) {
			dev_err(codec_dai->dev, "DAI prepare error: %d\n",
				ret);
			goto prepare_err;
		}
	}

	if (cpu_dai_ops->prepare) {
		ret = (*cpu_dai_ops->prepare)(substream, cpu_dai);
		if (ret < 0) {
			dev_err(codec_dai->dev, "DAI prepare error: %d\n",
				ret);
			goto prepare_err;
		}
	}
	return 0;
prepare_err:
	return ret;
}

static void compr_config_hw_params(struct snd_pcm_hw_params *params,
			struct snd_compr_params *compr_params)
{
	u64 fmt;
	int acodec_rate = 48000;

	pr_debug("%s\n", __func__);

	fmt = ffs(SNDRV_PCM_FMTBIT_S16_LE) - 1;
	snd_mask_set(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT), fmt);

	hw_param_interval(params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS)->min = 16;
	hw_param_interval(params, SNDRV_PCM_HW_PARAM_FRAME_BITS)->min = 32;
	hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS)->min = 2;

#ifdef CONFIG_SND_ESA_SA_EFFECT
	acodec_rate = esa_compr_get_sample_rate();
	if (!acodec_rate)
		acodec_rate = 48000;
#endif

	pr_info("%s input_SR %d PCM_HW_PARAM_RATE %d \n", __func__,
			compr_params->codec.sample_rate, acodec_rate);
	hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE)->min = acodec_rate;
}

static int compr_open(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct runtime_data *prtd;
	struct snd_pcm_substream *substream;
	int ret;

	pr_debug("%s\n", __func__);

	prtd = kzalloc(sizeof(struct runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	spin_lock_init(&prtd->lock);

	esa_compr_set_state(true);
	prtd->ap = compr_audio_processor_alloc((seiren_ops)compr_event_handler,
		prtd);
	if(!prtd->ap) {
		pr_err("%s: could not allocate memory\n", __func__);
		ret = -ENOMEM;
		goto compr_audio_processor_alloc_err;
	}
#ifdef CONFIG_SND_ESA_SA_EFFECT
	aud_vol.ap[COMPR_DAI_MULTIMEDIA_1] = prtd->ap;
#endif
	runtime->private_data = prtd;

	prtd->cpu_dai = rtd->cpu_dai;
	prtd->codec_dai = rtd->codec_dai;

	substream = &prtd->substream;

	ret = compr_config_substream(cstream, substream);
	if (ret) {
		pr_err("%s: could not config substream(%d)\n", __func__, ret);
		goto compr_audio_processor_alloc_err;
	}

	/* init runtime data */
	prtd->cstream = cstream;
	prtd->byte_offset = 0;
	prtd->app_pointer = 0;
	prtd->copied_total = 0;
	prtd->received_total = 0;
	prtd->compr_cap = &compr_cap;
	prtd->ap->sample_rate = 44100;
	prtd->ap->num_channels = 3; /* stereo channel */

	atomic_set(&prtd->eos, 0);
	atomic_set(&prtd->start, 0);
	atomic_set(&prtd->created, 0);

	init_waitqueue_head(&prtd->flush_wait);
	init_waitqueue_head(&prtd->exit_wait);
#ifdef AUDIO_PERF
	prtd->start_time[OPEN_T] = sched_clock();
#endif
	ret = esa_compr_open();
	if (ret) {
		pr_err("%s: could not open audio firmware(%d)\n", __func__, ret);
		goto compr_audio_firmware_open_err;
	}

	return 0;

compr_audio_firmware_open_err:
	if (substream->runtime)
		kfree(substream->runtime->hw_constraints.rules);
	kfree(substream->runtime);
compr_audio_processor_alloc_err:
	kfree(prtd->ap);
	kfree(prtd);
	esa_compr_set_state(false);
	return ret;
}

static int compr_free(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	struct snd_pcm_substream *substream;
	struct snd_soc_dai *cpu_dai;
	struct snd_soc_dai *codec_dai;
	const struct snd_soc_dai_ops *cpu_dai_ops;
	const struct snd_soc_dai_ops *codec_dai_ops;
	unsigned long flags;
	int ret;
#ifdef AUDIO_PERF
	u64 playback_time, total_time = 0;
	int idx;
#endif
	pr_debug("%s\n", __func__);

	if (!prtd) {
		pr_info("compress dai has already freed.\n");
		return 0;
	}

	substream = &prtd->substream;
	cpu_dai = prtd->cpu_dai;
	codec_dai = prtd->codec_dai;
	cpu_dai_ops = cpu_dai->driver->ops;
	codec_dai_ops = codec_dai->driver->ops;

	if (atomic_read(&prtd->eos)) {
		/* ALSA Framework callback to notify drain complete */
		snd_compr_drain_notify(cstream);
		atomic_set(&prtd->eos, 0);
		pr_debug("%s Call Drain notify to wakeup\n", __func__);
	}

	if (atomic_read(&prtd->created)) {
		spin_lock_irqsave(&prtd->lock, flags);
		atomic_set(&prtd->created, 0);
		prtd->exit_ack = 0;
		ret = esa_compr_send_cmd(CMD_COMPR_DESTROY, prtd->ap);
		if (ret) {
			esa_err("%s: can't send CMD_COMPR_DESTROY (%d)\n",
				__func__, ret);
			spin_unlock_irqrestore(&prtd->lock, flags);
		} else {
			spin_unlock_irqrestore(&prtd->lock, flags);
			ret = wait_event_interruptible_timeout(prtd->exit_wait,
							prtd->exit_ack, 1 * HZ);
			if (!ret)
				pr_err("%s: CMD_DESTROY timed out!!!\n", __func__);
		}
	}

#ifdef CONFIG_SND_ESA_SA_EFFECT
	aud_vol.ap[COMPR_DAI_MULTIMEDIA_1] = NULL;
#endif
	esa_compr_set_state(false);
	/* codec hw_free -> cpu hw_free ->
	   cpu shutdown -> codec shutdown */
	if (codec_dai_ops->hw_free)
		(*codec_dai_ops->hw_free)(substream, codec_dai);

	if (cpu_dai_ops->hw_free)
		(*cpu_dai_ops->hw_free)(substream, cpu_dai);

	if (cpu_dai_ops->shutdown)
		(*cpu_dai_ops->shutdown)(substream, cpu_dai);

	if (codec_dai_ops->shutdown)
		(*codec_dai_ops->shutdown)(substream, codec_dai);

	if (substream->runtime)
		kfree(substream->runtime->hw_constraints.rules);
	kfree(substream->runtime);
	esa_compr_close();
#ifdef AUDIO_PERF
	prtd->end_time[OPEN_T] = sched_clock();
	playback_time = prtd->end_time[OPEN_T] - prtd->start_time[OPEN_T];

	for (idx = 0; idx < TOTAL_TIMES; idx++) {
		total_time += prtd->total_time[idx];
	}
	pr_debug("%s: measure the audio waken time : %llu\n", __func__,
		total_time);
	pr_debug("%s: may be the ap sleep time : (%llu/%llu)\n", __func__,
		playback_time - total_time, playback_time);
#endif
	kfree(prtd->ap);
	kfree(prtd);
	return 0;
}

static int compr_set_params(struct snd_compr_stream *cstream,
			    struct snd_compr_params *params)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct runtime_data *prtd = runtime->private_data;
	unsigned long flags;
	int ret;

	pr_debug("%s\n", __func__);

	compr_config_hw_params(&prtd->hw_params, params);

	pr_debug("%s, cpu_dai name = %s\n",
			__func__, rtd->cpu_dai->name);

	/* startup -> hw_params */
	ret = compr_dai_setup(prtd, rtd);
	if (ret) {
		pr_err("%s: could not setup compr_dai(%d)\n", __func__, ret);
		return -ENXIO;
	}

	ret = compr_dai_prepare(prtd);
	if (ret) {
		pr_err("%s: compr_dai_prepare() fail(%d)\n", __func__, ret);
		return -ENXIO;
	}

	/* COMPR set_params */
	memcpy(&prtd->codec_param, params, sizeof(struct snd_compr_params));

	prtd->byte_offset = 0;
	prtd->app_pointer = 0;
	prtd->copied_total = 0;
	prtd->ap->buffer_size = runtime->fragments * runtime->fragment_size;
	prtd->ap->num_channels = prtd->codec_param.codec.ch_in;
	prtd->ap->sample_rate = prtd->codec_param.codec.sample_rate;

	if (prtd->ap->sample_rate == 0 ||
		prtd->ap->num_channels == 0) {
		pr_err("%s: invalid parameters: sample(%ld), ch(%ld)\n",
			__func__, prtd->ap->sample_rate,
			 prtd->ap->num_channels);
		return -EINVAL;
	}

	switch (prtd->codec_param.codec.id) {
	case SND_AUDIOCODEC_MP3:
		prtd->ap->codec_id = COMPR_MP3;
		break;
	case SND_AUDIOCODEC_AAC:
		prtd->ap->codec_id = COMPR_AAC;
		break;
	default:
		pr_err("%s: unknown codec id %d\n", __func__,
			prtd->codec_param.codec.id);
		break;
	}

	ret = esa_compr_set_param(prtd->ap, (uint8_t**)&prtd->buffer);
	if (ret) {
		pr_err("%s: esa_compr_set_param fail(%d)\n", __func__, ret);
		return ret;
	}
	spin_lock_irqsave(&prtd->lock, flags);
	atomic_set(&prtd->created, 1);
	spin_unlock_irqrestore(&prtd->lock, flags);

	pr_info("%s: sample rate:%ld, channels:%ld\n", __func__,
		prtd->ap->sample_rate, prtd->ap->num_channels);
	return 0;
}

static int compr_set_metadata(struct snd_compr_stream *cstream,
			      struct snd_compr_metadata *metadata)
{
	pr_debug("%s\n", __func__);

	if (!metadata || !cstream)
		return -EINVAL;

	if (metadata->key == SNDRV_COMPRESS_ENCODER_PADDING) {
		pr_debug("%s, got encoder padding %u", __func__, metadata->value[0]);
	} else if (metadata->key == SNDRV_COMPRESS_ENCODER_DELAY) {
		pr_debug("%s, got encoder delay %u", __func__, metadata->value[0]);
	}

	return 0;
}

static int compr_trigger(struct snd_compr_stream *cstream, int cmd)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	unsigned long flags;
	int ret;

	pr_debug("%s: trigger cmd(%d)\n", __func__, cmd);

	/* platform -> codec -> cpu */
	if (cstream->direction != SND_COMPRESS_PLAYBACK) {
		pr_err("%s: Unsupported stream type\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pr_info("%s: SNDRV_PCM_TRIGGER_PAUSE_PUSH\n", __func__);

		spin_lock_irqsave(&prtd->lock, flags);
		ret = esa_compr_send_cmd(CMD_COMPR_PAUSE, prtd->ap);
		if (ret) {
			pr_err("%s: pause cmd failed(%d)\n", __func__,
				ret);
			spin_unlock_irqrestore(&prtd->lock, flags);
			return ret;
		}
		spin_unlock_irqrestore(&prtd->lock, flags);
		atomic_set(&prtd->start, 0);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_info("%s: SNDRV_PCM_TRIGGER_STOP\n", __func__);

		spin_lock_irqsave(&prtd->lock, flags);

		if (atomic_read(&prtd->eos)) {
			/* ALSA Framework callback to notify drain complete */
			snd_compr_drain_notify(cstream);
			atomic_set(&prtd->eos, 0);
			pr_debug("%s: interrupt drain and eos wait queues", __func__);
		}

		pr_debug("CMD_STOP\n");
		prtd->stop_ack = 0;
		ret = esa_compr_send_cmd(CMD_COMPR_STOP, prtd->ap);
		if (ret) {
			pr_err("%s: stop cmd failed (%d)\n",
				__func__, ret);
			spin_unlock_irqrestore(&prtd->lock, flags);
			return ret;
		}
		spin_unlock_irqrestore(&prtd->lock, flags);

		ret = wait_event_interruptible_timeout(prtd->flush_wait,
			prtd->stop_ack, 1 * HZ);
		if (!ret) {
			pr_err("CMD_STOP cmd timeout(%d)\n", ret);
			ret = -ETIMEDOUT;
		} else
			ret = 0;

		ret = compr_dai_cmd(prtd, cmd);
		if (ret) {
			pr_err("%s: compr_dai_cmd fail(%d)\n", __func__, ret);
			return ret;
		}
		atomic_set(&prtd->start, 0);

		/* reset */
		prtd->stop_ack = 0;
		prtd->byte_offset = 0;
		prtd->app_pointer = 0;
		prtd->copied_total = 0;
		prtd->received_total = 0;
		break;
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (SNDRV_PCM_TRIGGER_START == cmd)
			pr_info("%s: SNDRV_PCM_TRIGGER_START\n", __func__);
		else if (SNDRV_PCM_TRIGGER_PAUSE_RELEASE == cmd)
			pr_info("%s: SNDRV_PCM_TRIGGER_PAUSE_RELEASE\n", __func__);

		ret = compr_dai_cmd(prtd, cmd);
		if (ret) {
			pr_err("%s: compr_dai_cmd fail(%d)\n", __func__, ret);
			return ret;
		}
		atomic_set(&prtd->start, 1);
		spin_lock_irqsave(&prtd->lock, flags);
		ret = esa_compr_send_cmd(CMD_COMPR_START, prtd->ap);
		if (ret) {
			pr_err("%s: start cmd failed\n", __func__);
			spin_unlock_irqrestore(&prtd->lock, flags);
			return ret;
		}
		spin_unlock_irqrestore(&prtd->lock, flags);
		break;
	case SND_COMPR_TRIGGER_NEXT_TRACK:
		pr_info("%s: SND_COMPR_TRIGGER_NEXT_TRACK\n", __func__);
		break;
	case SND_COMPR_TRIGGER_PARTIAL_DRAIN:
	case SND_COMPR_TRIGGER_DRAIN:
		pr_info("%s: %s\n", __func__, (SND_COMPR_TRIGGER_PARTIAL_DRAIN == cmd) ? "SND_COMPR_TRIGGER_PARTIAL_DRAIN" : "SND_COMPR_TRIGGER_DRAIN");

		/* Make sure all the data is sent to F/W before sending EOS */
		spin_lock_irqsave(&prtd->lock, flags);
#ifdef AUDIO_PERF
		prtd->start_time[DRAIN_T] = sched_clock();
#endif
		if (!atomic_read(&prtd->start)) {
			pr_err("%s: stream is not in started state\n",
				__func__);
			ret = -EPERM;
			spin_unlock_irqrestore(&prtd->lock, flags);
			break;
		}

		atomic_set(&prtd->eos, 1);
		pr_debug("%s: CMD_EOS\n", __func__);
		ret = esa_compr_send_cmd(CMD_COMPR_EOS, prtd->ap);
		if (ret) {
			pr_err("%s: can't send eos (%d)\n", __func__, ret);
			spin_unlock_irqrestore(&prtd->lock, flags);
			return ret;
		}
		spin_unlock_irqrestore(&prtd->lock, flags);
#ifdef AUDIO_PERF
		prtd->end_time[DRAIN_T] = sched_clock();
		prtd->total_time[DRAIN_T] +=
	        prtd->end_time[DRAIN_T] - prtd->start_time[DRAIN_T];
#endif
		pr_info("%s: Out of %s Drain", __func__,
			(cmd == SND_COMPR_TRIGGER_DRAIN ? "Full" : "Partial"));
		break;
	default:
		break;
	}

	return 0;
}

static int compr_pointer(struct snd_compr_stream *cstream,
			 struct snd_compr_tstamp *tstamp)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	struct snd_compr_tstamp timestamp;
	unsigned long flags;
	int pcm_size, bytes_available;
	int num_channel;

	pr_debug("%s\n", __func__);
#ifdef AUDIO_PERF
	prtd->start_time[POINTER_T] = sched_clock();
#endif
	memset(&timestamp, 0x0, sizeof(struct snd_compr_tstamp));

	spin_lock_irqsave(&prtd->lock, flags);
	timestamp.sampling_rate = prtd->ap->sample_rate;
	timestamp.byte_offset = prtd->byte_offset;
	timestamp.copied_total = prtd->copied_total;
	pcm_size = esa_compr_pcm_size();

	/* set the number of channels */
	if (prtd->ap->num_channels == 1 || prtd->ap->num_channels == 2)
		num_channel = 1;
	else if (prtd->ap->num_channels == 3)
		num_channel = 2;
	else
		num_channel = 2;
	spin_unlock_irqrestore(&prtd->lock, flags);

	if (pcm_size) {
		bytes_available = prtd->received_total - prtd->copied_total;

		timestamp.pcm_io_frames = (snd_pcm_uframes_t)div64_u64(pcm_size,
			2 * num_channel);
		pr_debug("%s: pcm_size(%u), frame_count(%u), copied_total(%llu), \
			free_size(%llu)\n", __func__, pcm_size,
			timestamp.pcm_io_frames, prtd->copied_total,
			runtime->buffer_size - bytes_available);

	}
	memcpy(tstamp, &timestamp, sizeof(struct snd_compr_tstamp));
#ifdef AUDIO_PERF
	prtd->end_time[POINTER_T] = sched_clock();
	prtd->total_time[POINTER_T] +=
		prtd->end_time[POINTER_T] - prtd->start_time[POINTER_T];
#endif
	return 0;
}

static int compr_copy(struct snd_compr_stream *cstream, char __user* buf, size_t bytes)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	u64 bytes_available;
	unsigned long flags;
	unsigned long copy;
	void *dstn;
	int ret;

	pr_debug("%s\n", __func__);
#ifdef AUDIO_PERF
	prtd->start_time[WRITE_T] = sched_clock();
#endif
	if (!prtd->buffer) {
		pr_err("%s: Buffer is not allocated yet ??", __func__);
		return -ENOMEM;
	}

	/* check the free area */
	if (bytes <= 0) {
		pr_err("%s: Buffer size is zero(%ld)\n", __func__, bytes);
		return 0;
	}

	pr_debug("copying %ld at %lld\n",
			(unsigned long)bytes, prtd->app_pointer);

	dstn = prtd->buffer + prtd->app_pointer;
	if (bytes < runtime->buffer_size - prtd->app_pointer) {
		if (copy_from_user(dstn, buf, bytes))
			return -EFAULT;
		prtd->app_pointer += bytes;
	} else {
		copy = runtime->buffer_size - prtd->app_pointer;
		if (copy_from_user(dstn, buf, copy))
			return -EFAULT;
		if (copy_from_user(prtd->buffer, buf + copy, bytes - copy))
			return -EFAULT;
		prtd->app_pointer = bytes - copy;
	}

	/*
	 * since the available bytes fits fragment_size, copy the data right away
	 */
	spin_lock_irqsave(&prtd->lock, flags);
	prtd->received_total += bytes;
	bytes_available = prtd->received_total - prtd->copied_total;
	spin_unlock_irqrestore(&prtd->lock, flags);

	pr_debug("%s: bytes_received(%llu), free_size(%llu)\n",
		__func__, prtd->received_total,
		runtime->buffer_size - bytes_available);

	/* get the bytes to write */
	if (bytes_available > 0) {
//TODO: issue: unknown mp3 fragment should be checked
#if 0
		u64 pointer = div64_u64(prtd->copied_total,
			runtime->buffer_size);
		pointer = prtd->copied_total - (pointer * runtime->buffer_size);
		pr_info("%s: bytes to write offset in buffer(%d/%llu)\n",
			__func__, prtd->byte_offset, prtd->app_pointer);
		pr_info("%s: [%2llx][%2llx][%2llx][%2llx] (%d)\n",
			__func__, (u64)(((char*)prtd->buffer)[pointer]),
			(u64)(((char*)prtd->buffer)[pointer + 1]),
			(u64)(((char*)prtd->buffer)[pointer + 2]),
			(u64)(((char*)prtd->buffer)[pointer + 3]),
			bytes);
#endif
		pr_debug("%s: needs to be copied to the buffer = %llu\n",
			__func__, bytes_available);

		ret = esa_compr_send_buffer(bytes, prtd->ap);
		if (ret) {
			pr_err("%s: can't send buffer %ld bytes (%d)",
				__func__, bytes, ret);
			return -EFAULT;
		}
	}
#ifdef AUDIO_PERF
	prtd->end_time[WRITE_T] = sched_clock();
	prtd->total_time[WRITE_T] +=
		prtd->end_time[WRITE_T] - prtd->start_time[WRITE_T];
#endif
	return bytes;
}

static int compr_get_caps(struct snd_compr_stream *cstream,
			  struct snd_compr_caps *caps)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct runtime_data *prtd = runtime->private_data;

	pr_debug("%s\n", __func__);

	memcpy(caps, prtd->compr_cap, sizeof(struct snd_compr_caps));

	return 0;
}

static int compr_get_codec_caps(struct snd_compr_stream *cstream,
				struct snd_compr_codec_caps *codec)
{
	pr_debug("%s\n", __func__);

	return 0;
}

static struct snd_compr_ops compr_ops = {
	.open		= compr_open,
	.free		= compr_free,
	.set_params	= compr_set_params,
	.set_metadata	= compr_set_metadata,
	.trigger	= compr_trigger,
	.pointer	= compr_pointer,
	.copy		= compr_copy,
	.get_caps	= compr_get_caps,
	.get_codec_caps	= compr_get_codec_caps,
};

static struct snd_soc_platform_driver samsung_compr_platform = {
	.compr_ops	= &compr_ops,
};

int asoc_compr_platform_register(struct device *dev)
{
	return snd_soc_register_platform(dev, &samsung_compr_platform);
}
EXPORT_SYMBOL_GPL(asoc_compr_platform_register);

void asoc_compr_platform_unregister(struct device *dev)
{
	snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(asoc_compr_platform_unregister);

MODULE_AUTHOR("Yeongman Seo, <yman.seo@samsung.com>");
MODULE_AUTHOR("Taeho Lee <taeho07.lee@samsung.com>");
MODULE_DESCRIPTION("Samsung ASoC Compress Driver");
MODULE_LICENSE("GPL");
