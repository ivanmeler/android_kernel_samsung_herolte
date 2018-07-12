/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "ssp_sensorhub.h"

static void ssp_sensorhub_log(const char *func_name,
				const char *data, int length)
{
	char buf[6];
	char *log_str;
	int log_size;
	int i;

	if (likely(length <= BIG_DATA_SIZE))
		log_size = length;
	else
		log_size = PRINT_TRUNCATE * 2 + 1;

	log_size = sizeof(buf) * log_size + 1;
	log_str = kzalloc(log_size, GFP_ATOMIC);
	if (unlikely(!log_str)) {
		ssp_errf("allocate memory for data log err");
		return;
	}

	for (i = 0; i < length; i++) {
		if (length < BIG_DATA_SIZE ||
			i < PRINT_TRUNCATE || i >= length - PRINT_TRUNCATE) {
			snprintf(buf, sizeof(buf), "%d", (signed char)data[i]);
			strlcat(log_str, buf, log_size);

			if (i < length - 1)
				strlcat(log_str, ", ", log_size);
		}
		if (length > BIG_DATA_SIZE && i == PRINT_TRUNCATE)
			strlcat(log_str, "..., ", log_size);
	}

	ssp_info("%s(%d): %s", func_name, length, log_str);
	kfree(log_str);
}

static int ssp_sensorhub_send_cmd(struct ssp_sensorhub_data *hub_data,
					const char *buf, int count)
{
	int ret = 0;

	if (buf[2] < MSG2SSP_AP_STATUS_WAKEUP ||
		buf[2] >= MSG2SSP_AP_TEMPHUMIDITY_CAL_DONE) {
		ssp_errf("MSG2SSP_INST_LIB_NOTI err(%d)", buf[2]);
		return -EINVAL;
	}

	ret = ssp_send_cmd(hub_data->ssp_data, buf[2], 0);

	if (buf[2] == MSG2SSP_AP_STATUS_WAKEUP ||
		buf[2] == MSG2SSP_AP_STATUS_SLEEP)
		hub_data->ssp_data->uLastAPState = buf[2];

	if (buf[2] == MSG2SSP_AP_STATUS_SUSPEND ||
		buf[2] == MSG2SSP_AP_STATUS_RESUME)
		hub_data->ssp_data->uLastResumeState = buf[2];

	return ret;
}

static int ssp_sensorhub_send_instruction(struct ssp_sensorhub_data *hub_data,
					const char *buf, int count)
{
	unsigned char instruction = buf[0];

	if (buf[0] == MSG2SSP_INST_LIBRARY_REMOVE)
		instruction = REMOVE_LIBRARY;
	else if (buf[0] == MSG2SSP_INST_LIBRARY_ADD)
		instruction = ADD_LIBRARY;

	return send_instruction(hub_data->ssp_data, instruction,
		(unsigned char)buf[1], (unsigned char *)(buf+2),
		(unsigned short)(count-2));
}

static ssize_t ssp_sensorhub_write(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
{
	struct ssp_sensorhub_data *hub_data
		= container_of(file->private_data,
			struct ssp_sensorhub_data, sensorhub_device);
	int ret = 0;
	char *buffer;

	if (unlikely(count < 2)) {
		ssp_errf("library data length err(%d)", (int)count);
		return -EINVAL;
	}

	buffer = kzalloc(count * sizeof(char), GFP_KERNEL);
	if (unlikely(!buffer)) {
		ssp_errf("allocate memory for kernel buffer err");
		return -ENOMEM;
	}

	ret = copy_from_user(buffer, buf, count);
	if (unlikely(ret)) {
		ssp_errf("memcpy for kernel buffer err");
		ret = -EFAULT;
		goto exit;
	}

	ssp_sensorhub_log(__func__, buffer, count);

	if (unlikely(hub_data->ssp_data->bSspShutdown)) {
		ssp_errf("stop sending library data(shutdown)");
		ret = -EBUSY;
		goto exit;
	}

	if (buffer[0] == MSG2SSP_INST_LIB_NOTI)
		ret = ssp_sensorhub_send_cmd(hub_data, buffer, count);
	else
		ret = ssp_sensorhub_send_instruction(hub_data, buffer, count);

	if (unlikely(ret <= 0)) {
		ssp_errf("send library data err(%d)", ret);
		/* i2c transfer fail */
		if (ret == ERROR)
			ret = -EIO;
		/* i2c transfer done but no ack from MCU */
		else if (ret == FAIL)
			ret = -EAGAIN;

		goto exit;
	}

	ret = count;

exit:
	kfree(buffer);
	return ret;
}

static ssize_t ssp_sensorhub_read(struct file *file, char __user *buf,
				size_t count, loff_t *pos)
{
	struct ssp_sensorhub_data *hub_data
		= container_of(file->private_data,
			struct ssp_sensorhub_data, sensorhub_device);
	struct sensorhub_event *event;
	int retries = MAX_DATA_COPY_TRY;
	int length = 0;
	int ret = 0;

	spin_lock_bh(&hub_data->sensorhub_lock);
	if (unlikely(kfifo_is_empty(&hub_data->fifo))) {
		ssp_infof("no library data");
		goto err;
	}

	/* first in first out */
	ret = kfifo_out_peek(&hub_data->fifo, &event, sizeof(void *));
	if (unlikely(!ret)) {
		ssp_errf("kfifo out peek err(%d)", ret);
		ret = EIO;
		goto err;
	}

	length = event->library_length;

	while (retries--) {
		ret = copy_to_user(buf,
			event->library_data, event->library_length);
		if (likely(!ret))
			break;
	}
	if (unlikely(ret)) {
		ssp_errf("read library data err(%d)", ret);
		goto err;
	}

	ssp_sensorhub_log(__func__,
		event->library_data, event->library_length);

	/* remove first event from the list */
	ret = kfifo_out(&hub_data->fifo, &event, sizeof(void *));
	if (unlikely(ret != sizeof(void *))) {
		ssp_errf("kfifo out err(%d)", ret);
		ret = EIO;
		goto err;
	}

	complete(&hub_data->read_done);
	spin_unlock_bh(&hub_data->sensorhub_lock);

	return length;

err:
	spin_unlock_bh(&hub_data->sensorhub_lock);
	return ret ? -ret : 0;
}

static long ssp_sensorhub_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct ssp_sensorhub_data *hub_data
		= container_of(file->private_data,
			struct ssp_sensorhub_data, sensorhub_device);
	void __user *argp = (void __user *)arg;
	int retries = MAX_DATA_COPY_TRY;
	int length = 0;
	int ret = 0;

	switch (cmd) {
	case IOCTL_READ_BIG_CONTEXT_DATA:
		mutex_lock(&hub_data->big_events_lock);
		length = hub_data->big_events.library_length;
		if (unlikely(!hub_data->big_events.library_data
			|| !hub_data->big_events.library_length)) {
			ssp_infof("no big library data");
			mutex_unlock(&hub_data->big_events_lock);
			return 0;
		}

		while (retries--) {
			ret = copy_to_user(argp,
				hub_data->big_events.library_data,
				hub_data->big_events.library_length);
			if (likely(!ret))
				break;
		}
		if (unlikely(ret)) {
			ssp_errf("read big library data err(%d)", ret);
			mutex_unlock(&hub_data->big_events_lock);
			return -ret;
		}

		ssp_sensorhub_log(__func__,
			hub_data->big_events.library_data,
			hub_data->big_events.library_length);

		hub_data->is_big_event = false;
		kfree(hub_data->big_events.library_data);
		hub_data->big_events.library_data = NULL;
		hub_data->big_events.library_length = 0;
		complete(&hub_data->big_read_done);
		mutex_unlock(&hub_data->big_events_lock);
		break;

	default:
		ssp_errf("ioctl cmd err(%d)", cmd);
		return -EINVAL;
	}

	return length;
}

static struct file_operations ssp_sensorhub_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.write = ssp_sensorhub_write,
	.read = ssp_sensorhub_read,
	.unlocked_ioctl = ssp_sensorhub_ioctl,
};

void ssp_sensorhub_report_notice(struct ssp_data *ssp_data, char notice)
{
	struct ssp_sensorhub_data *hub_data = ssp_data->hub_data;

	input_report_rel(hub_data->sensorhub_input_dev, NOTICE, notice);
	input_sync(hub_data->sensorhub_input_dev);

	if (notice == MSG2SSP_AP_STATUS_WAKEUP)
		ssp_infof("wake up");
	else if (notice == MSG2SSP_AP_STATUS_SLEEP)
		ssp_infof("sleep");
	else if (notice == MSG2SSP_AP_STATUS_RESET)
		ssp_infof("reset");
	else
		ssp_errf("invalid notice(0x%x)", notice);
}

static void ssp_sensorhub_report_library(struct ssp_sensorhub_data *hub_data)
{
	input_report_rel(hub_data->sensorhub_input_dev, DATA, DATA);
	input_sync(hub_data->sensorhub_input_dev);
	wake_lock_timeout(&hub_data->sensorhub_wake_lock, WAKE_LOCK_TIMEOUT);
}

static void ssp_sensorhub_report_big_library(
			struct ssp_sensorhub_data *hub_data)
{
	input_report_rel(hub_data->sensorhub_input_dev, BIG_DATA, BIG_DATA);
	input_sync(hub_data->sensorhub_input_dev);
	wake_lock_timeout(&hub_data->sensorhub_wake_lock, WAKE_LOCK_TIMEOUT);
}

static int ssp_sensorhub_list(struct ssp_sensorhub_data *hub_data,
				char *dataframe, int length)
{
	struct sensorhub_event *event;
	int ret = 0;

	if (unlikely(length <= 0 || length >= PAGE_SIZE)) {
		ssp_errf("library length err(%d)", length);
		return -EINVAL;
	}

	ssp_sensorhub_log(__func__, dataframe, length);

	/* overwrite new event if list is full */
	if (unlikely(kfifo_is_full(&hub_data->fifo))) {
		ret = kfifo_out(&hub_data->fifo, &event, sizeof(void *));
		if (unlikely(ret != sizeof(void *))) {
			ssp_errf("kfifo out err(%d)", ret);
			return -EIO;
		}
		ssp_infof("overwrite event");
	}

	/* allocate memory for new event */
	kfree(hub_data->events[hub_data->event_number].library_data);
	hub_data->events[hub_data->event_number].library_data
		= kzalloc(length * sizeof(char), GFP_ATOMIC);
	if (unlikely(!hub_data->events[hub_data->event_number].library_data)) {
		ssp_errf("allocate memory for library err");
		return -ENOMEM;
	}

	/* copy new event into memory */
	memcpy(hub_data->events[hub_data->event_number].library_data,
		dataframe, length);
	hub_data->events[hub_data->event_number].library_length = length;

	/* add new event into the end of list */
	event = &hub_data->events[hub_data->event_number];
	ret = kfifo_in(&hub_data->fifo, &event, sizeof(void *));
	if (unlikely(ret != sizeof(void *))) {
		ssp_errf("kfifo in err(%d)", ret);
		return -EIO;
	}

	/* not to overflow max list capacity */
	if (hub_data->event_number++ >= LIST_SIZE - 1)
		hub_data->event_number = 0;

	return kfifo_len(&hub_data->fifo) / sizeof(void *);
}

int ssp_sensorhub_handle_data(struct ssp_data *ssp_data, char *dataframe,
				int start, int end)
{
	struct ssp_sensorhub_data *hub_data = ssp_data->hub_data;
	int ret = 0;

	/* add new sensorhub event into list */
	spin_lock_bh(&hub_data->sensorhub_lock);
	ret = ssp_sensorhub_list(hub_data, dataframe+start, end-start);
	spin_unlock_bh(&hub_data->sensorhub_lock);

	if (ret < 0)
		ssp_errf("sensorhub list err(%d)", ret);
	else
		wake_up(&hub_data->sensorhub_wq);

	return ret;
}

static int ssp_sensorhub_thread(void *arg)
{
	struct ssp_sensorhub_data *hub_data = (struct ssp_sensorhub_data *)arg;
	int ret = 0;

	while (likely(!kthread_should_stop())) {
		/* run thread if list is not empty */
		wait_event_interruptible(hub_data->sensorhub_wq,
				kthread_should_stop() ||
				!kfifo_is_empty(&hub_data->fifo) ||
				hub_data->is_big_event);

		/* exit thread if kthread should stop */
		if (unlikely(kthread_should_stop())) {
			ssp_infof("kthread_stop()");
			break;
		}

		if (likely(!kfifo_is_empty(&hub_data->fifo))) {
			/* report sensorhub event to user */
			ssp_sensorhub_report_library(hub_data);
			/* wait until transfer finished */
			ret = wait_for_completion_timeout(
				&hub_data->read_done, COMPLETION_TIMEOUT);
			if (unlikely(!ret))
				ssp_errf("wait for read timed out");
			else if (unlikely(ret < 0))
				ssp_errf("read completion err(%d)", ret);
		}

		if (unlikely(hub_data->is_big_event)) {
			/* report big sensorhub event to user */
			ssp_sensorhub_report_big_library(hub_data);
			/* wait until transfer finished */
			ret = wait_for_completion_timeout(
				&hub_data->big_read_done, COMPLETION_TIMEOUT);
			if (unlikely(!ret))
				ssp_errf("wait for big read timed out");
			else if (unlikely(ret < 0))
				ssp_errf("big read completion err(%d)",
					ret);
		}
	}

	return 0;
}

void ssp_read_big_library_task(struct work_struct *work)
{
	struct ssp_big *big = container_of(work, struct ssp_big, work);
	struct ssp_sensorhub_data *hub_data = big->data->hub_data;
	struct ssp_msg *msg;
	int buf_len, residue, ret = 0, index = 0, pos = 0;

	mutex_lock(&hub_data->big_events_lock);
	if (hub_data->big_events.library_data)
		kfree(hub_data->big_events.library_data);

	residue = big->length;
	hub_data->big_events.library_length = big->length;
	hub_data->big_events.library_data = kzalloc(big->length, GFP_ATOMIC);

	while (residue > 0) {
		buf_len = residue > DATA_PACKET_SIZE
			? DATA_PACKET_SIZE : residue;

		msg = kzalloc(sizeof(*msg), GFP_ATOMIC);
		msg->cmd = MSG2SSP_AP_GET_BIG_DATA;
		msg->length = buf_len;
		msg->options = AP2HUB_READ | (index++ << SSP_INDEX);
		msg->data = big->addr;
		msg->buffer = hub_data->big_events.library_data + pos;
		msg->free_buffer = 0;

		ret = ssp_spi_sync(big->data, msg, 1000);
		if (ret != SUCCESS) {
			ssp_errf("read big data err(%d)", ret);
			break;
		}

		pos += buf_len;
		residue -= buf_len;

		ssp_infof("read big data (%5d / %5d)", pos, big->length);
	}

	hub_data->is_big_event = true;
	wake_up(&hub_data->sensorhub_wq);
	kfree(big);
	mutex_unlock(&hub_data->big_events_lock);
}

int ssp_sensorhub_initialize(struct ssp_data *ssp_data)
{
	struct ssp_sensorhub_data *hub_data;
	int ret;

	/* allocate memory for sensorhub data */
	hub_data = kzalloc(sizeof(*hub_data), GFP_KERNEL);
	if (!hub_data) {
		ssp_errf("allocate memory for sensorhub data err");
		ret = -ENOMEM;
		goto exit;
	}
	hub_data->ssp_data = ssp_data;
	ssp_data->hub_data = hub_data;

	/* init wakelock, list, waitqueue, completion and spinlock */
	wake_lock_init(&hub_data->sensorhub_wake_lock, WAKE_LOCK_SUSPEND,
			"ssp_sensorhub_wake_lock");
	init_waitqueue_head(&hub_data->sensorhub_wq);
	init_completion(&hub_data->read_done);
	init_completion(&hub_data->big_read_done);
	spin_lock_init(&hub_data->sensorhub_lock);
	mutex_init(&hub_data->big_events_lock);

	/* allocate sensorhub input device */
	hub_data->sensorhub_input_dev = input_allocate_device();
	if (!hub_data->sensorhub_input_dev) {
		ssp_errf("allocate sensorhub input device err");
		ret = -ENOMEM;
		goto err_input_allocate_device_sensorhub;
	}

	/* set sensorhub input device */
	input_set_drvdata(hub_data->sensorhub_input_dev, hub_data);
	hub_data->sensorhub_input_dev->name = "ssp_context";
	input_set_capability(hub_data->sensorhub_input_dev, EV_REL, DATA);
	input_set_capability(hub_data->sensorhub_input_dev, EV_REL, BIG_DATA);
	input_set_capability(hub_data->sensorhub_input_dev, EV_REL, NOTICE);

	/* register sensorhub input device */
	ret = input_register_device(hub_data->sensorhub_input_dev);
	if (ret < 0) {
		ssp_errf("register sensorhub input device err(%d)", ret);
		input_free_device(hub_data->sensorhub_input_dev);
		goto err_input_register_device_sensorhub;
	}

	/* register sensorhub misc device */
	hub_data->sensorhub_device.minor = MISC_DYNAMIC_MINOR;
	hub_data->sensorhub_device.name = "ssp_sensorhub";
	hub_data->sensorhub_device.fops = &ssp_sensorhub_fops;

	ret = misc_register(&hub_data->sensorhub_device);
	if (ret < 0) {
		ssp_errf("register sensorhub misc device err(%d)", ret);
		goto err_misc_register;
	}

	/* allocate fifo */
	ret = kfifo_alloc(&hub_data->fifo,
		sizeof(void *) * LIST_SIZE, GFP_KERNEL);
	if (ret) {
		ssp_errf("kfifo allocate err(%d)", ret);
		goto err_kfifo_alloc;
	}

	/* create and run sensorhub thread */
	hub_data->sensorhub_task = kthread_run(ssp_sensorhub_thread,
				(void *)hub_data, "ssp_sensorhub_thread");
	if (IS_ERR(hub_data->sensorhub_task)) {
		ret = PTR_ERR(hub_data->sensorhub_task);
		goto err_kthread_run;
	}

	return 0;

err_kthread_run:
	kfifo_free(&hub_data->fifo);
err_kfifo_alloc:
	misc_deregister(&hub_data->sensorhub_device);
err_misc_register:
	input_unregister_device(hub_data->sensorhub_input_dev);
err_input_register_device_sensorhub:
err_input_allocate_device_sensorhub:
	complete_all(&hub_data->big_read_done);
	complete_all(&hub_data->read_done);
	wake_lock_destroy(&hub_data->sensorhub_wake_lock);
	kfree(hub_data);
exit:
	return ret;
}

void ssp_sensorhub_remove(struct ssp_data *ssp_data)
{
	struct ssp_sensorhub_data *hub_data = ssp_data->hub_data;

	ssp_sensorhub_fops.write = NULL;
	ssp_sensorhub_fops.read = NULL;
	ssp_sensorhub_fops.unlocked_ioctl = NULL;

	kthread_stop(hub_data->sensorhub_task);
	kfifo_free(&hub_data->fifo);
	misc_deregister(&hub_data->sensorhub_device);
	input_unregister_device(hub_data->sensorhub_input_dev);
	mutex_destroy(&hub_data->big_events_lock);
	complete_all(&hub_data->big_read_done);
	complete_all(&hub_data->read_done);
	wake_lock_destroy(&hub_data->sensorhub_wake_lock);
	kfree(hub_data);
}

MODULE_DESCRIPTION("Seamless Sensor Platform(SSP) sensorhub driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
