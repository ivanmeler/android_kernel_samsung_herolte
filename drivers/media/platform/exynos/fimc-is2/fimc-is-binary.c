/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/firmware.h>

#include "fimc-is-binary.h"

static noinline_for_stack long get_file_size(struct file *file)
{
	struct kstat st;

	if (vfs_getattr(&file->f_path, &st))
		return -1;
	if (!S_ISREG(st.mode))
		return -1;
	if (st.size != (long)st.size)
		return -1;

	return st.size;
}

static int read_file_contents(struct file *fp, struct fimc_is_binary *bin)
{
	long size;
	char *buf;
	int ret;

	size = get_file_size(fp);
	if (size <= 0)
		return -EBADF;

	buf = bin->alloc(size);
	if (!buf)
		return -ENOMEM;

	ret = kernel_read(fp, 0, buf, size);
	if (ret != size) {
		bin->free(buf);
		return ret;
	}

	bin->data = buf;
	bin->size = size;

	return 0;
}

static int get_filesystem_binary(const char *filename, struct fimc_is_binary *bin)
{
	struct file *fp;
	int ret = 0;

	fp = filp_open(filename, O_RDONLY, 0);
	if (!IS_ERR_OR_NULL(fp)) {
		ret = read_file_contents(fp, bin);
		fput(fp);
	} else {
		ret = PTR_ERR(fp);
	}

	return ret;
}

/**
  * setup_binary_loader: customize an instance of the binary loader
  * @bin: pointer to fimc_is_binary structure
  * @retry_cnt: retry count of request_firmware
  * @retry_err: specific error number for retry of request_firmware
  * @alloc: custom allocation function for file system loading
  * @free: custom free function for file system loading
  **/
void setup_binary_loader(struct fimc_is_binary *bin,
				unsigned int retry_cnt, int retry_err,
				void *(*alloc)(unsigned long size),
				void (*free)(const void *buf))
{
	bin->retry_cnt = retry_cnt;
	bin->retry_err = retry_err;

	if (alloc && free) {
		bin->alloc = alloc;
		bin->free = free;
	} else {
		/* set vmalloc/vfree as a default */
		bin->alloc = &vmalloc;
		bin->free =  &vfree;
	}

	bin->customized = (unsigned long)bin;
}

 /**
  * request_binary: send loading request to the loader
  * @bin: pointer to fimc_is_binary structure
  * @path: path of binary file
  * @name: name of binary file
  * @device: device for which binary is being loaded
  **/
int request_binary(struct fimc_is_binary *bin, const char *path,
				const char *name, struct device *device)
{
	char *filename;
	unsigned int retry_cnt = 0;
	int retry_err = 0;
	int ret;

	bin->data = NULL;
	bin->size = 0;
	bin->fw = NULL;

	/* whether the loader is customized or not */
	if (bin->customized != (unsigned long)bin) {
		bin->alloc = &vmalloc;
		bin->free =  &vfree;
	} else {
		retry_cnt = bin->retry_cnt;
		retry_err = bin->retry_err;
	}

	/* read the requested binary from file system directly */
	if (path) {
		filename = __getname();
		if (unlikely(!filename))
			return -ENOMEM;

		snprintf(filename, PATH_MAX, "%s%s", path, name);
		ret = get_filesystem_binary(filename, bin);
		__putname(filename);
		/* read successfully or don't want to go further more */
		if (!ret || !device)
			return ret;
	}

	/* ask to 'request_firmware' */
	do {
		ret = request_firmware(&bin->fw, name, device);

		if (!ret) {
			bin->data = (void *)bin->fw->data;
			bin->size = bin->fw->size;

			break;
		}

		/*
		 * if no specific error for retry is given;
		 * whatever any error is occurred, we should retry
		 */
		if (!bin->retry_err)
			retry_err = ret;
	} while (retry_cnt-- && (retry_err == ret));

	return ret;
}

/**
 * release_firmware: release the resource related to a binary
 * @bin: binary resource to release
**/
void release_binary(struct fimc_is_binary *bin)
{
	if (bin->fw)
		release_firmware(bin->fw);
	else if (bin->data)
		bin->free(bin->data);
}

/**
 * was_loaded_by: how was a binary loaded
 * @bin: pointer to fimc_is_binary structure
**/
int was_loaded_by(struct fimc_is_binary *bin)
{
	if (bin->fw)
		return 1;	/* by request_firmware */
	else if (bin->data)
		return 0;	/* from file system */
	else
		return -EINVAL;
}
