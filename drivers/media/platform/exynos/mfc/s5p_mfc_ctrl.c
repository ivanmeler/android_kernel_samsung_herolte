/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_ctrl.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/delay.h>
#include <linux/jiffies.h>

#include <linux/err.h>
#include <linux/sched.h>

#include "s5p_mfc_ctrl.h"

#include "s5p_mfc_mem.h"
#include "s5p_mfc_intr.h"
#include "s5p_mfc_cmd.h"
#include "s5p_mfc_pm.h"

static inline int s5p_mfc_bus_reset(struct s5p_mfc_dev *dev)
{
	unsigned int status;
	unsigned long timeout;

	/* Reset */
	MFC_WRITEL(0x1, S5P_FIMV_MFC_BUS_RESET_CTRL);

	timeout = jiffies + msecs_to_jiffies(MFC_BW_TIMEOUT);
	/* Check bus status */
	do {
		if (time_after(jiffies, timeout)) {
			mfc_err_dev("Timeout while resetting MFC.\n");
			return -EIO;
		}
		status = MFC_READL(S5P_FIMV_MFC_BUS_RESET_CTRL);
	} while ((status & 0x2) == 0);

	return 0;
}

/* Reset the device */
static int s5p_mfc_reset(struct s5p_mfc_dev *dev)
{
	int i;
	unsigned int status;
	unsigned long timeout;

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	/* Stop procedure */
	/* Reset VI */
	/*
	MFC_WRITEL(0x3f7, S5P_FIMV_SW_RESET);
	*/

	if (IS_MFCV6(dev)) {
		/* Zero Initialization of MFC registers */
		MFC_WRITEL(0, S5P_FIMV_RISC2HOST_CMD);
		MFC_WRITEL(0, S5P_FIMV_HOST2RISC_CMD);
		MFC_WRITEL(0, S5P_FIMV_FW_VERSION);

		for (i = 0; i < S5P_FIMV_REG_CLEAR_COUNT; i++)
			MFC_WRITEL(0, S5P_FIMV_REG_CLEAR_BEGIN + (i*4));

		if (IS_MFCv6X(dev))
			if (s5p_mfc_bus_reset(dev))
				return -EIO;
		if (!IS_MFCV8(dev))
			MFC_WRITEL(0, S5P_FIMV_RISC_ON);
		MFC_WRITEL(0x1FFF, S5P_FIMV_MFC_RESET);
		MFC_WRITEL(0, S5P_FIMV_MFC_RESET);
	} else {
		MFC_WRITEL(0x3f6, S5P_FIMV_SW_RESET);	/*  reset RISC */
		MFC_WRITEL(0x3e2, S5P_FIMV_SW_RESET);	/*  All reset except for MC */
		mdelay(10);

		timeout = jiffies + msecs_to_jiffies(MFC_BW_TIMEOUT);

		/* Check MC status */
		do {
			if (time_after(jiffies, timeout)) {
				mfc_err_dev("Timeout while resetting MFC.\n");
				return -EIO;
			}

			status = MFC_READL(S5P_FIMV_MC_STATUS);

		} while (status & 0x3);

		MFC_WRITEL(0x0, S5P_FIMV_SW_RESET);
		MFC_WRITEL(0x3fe, S5P_FIMV_SW_RESET);
	}

	mfc_debug_leave();

	return 0;
}

/* Initialize hardware */
static int _s5p_mfc_init_hw(struct s5p_mfc_dev *dev, enum mfc_buf_usage_type buf_type)
{
	char fimv_info;
	int fw_ver;
	int ret = 0;
	int curr_ctx_backup;

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	curr_ctx_backup = dev->curr_ctx_drm;
	dev->sys_init_status = 0;
	/* RMVME: */
	if (!dev->fw_info.alloc)
		return -EINVAL;

	/* 0. MFC reset */
	mfc_debug(2, "MFC reset...\n");

	/* At init time, do not call secure API */
	if (buf_type == MFCBUF_NORMAL)
		dev->curr_ctx_drm = 0;
	else if (buf_type == MFCBUF_DRM)
		dev->curr_ctx_drm = 1;

	ret = s5p_mfc_clock_on(dev);
	if (ret) {
		mfc_err_dev("Failed to enable clock before reset(%d)\n", ret);
		dev->curr_ctx_drm = curr_ctx_backup;
		return ret;
	}

	dev->sys_init_status = 1;
	ret = s5p_mfc_reset(dev);
	if (ret) {
		mfc_err_dev("Failed to reset MFC - timeout.\n");
		goto err_init_hw;
	}
	mfc_debug(2, "Done MFC reset...\n");

	/* 1. Set DRAM base Addr */
	s5p_mfc_init_memctrl(dev, buf_type);

	/* 2. Initialize registers of channel I/F */
	s5p_mfc_clear_cmds(dev);
	s5p_mfc_clean_dev_int_flags(dev);

	/* 3. Release reset signal to the RISC */
	if (IS_MFCV6(dev))
		MFC_WRITEL(0x1, S5P_FIMV_RISC_ON);
	else
		MFC_WRITEL(0x3ff, S5P_FIMV_SW_RESET);

	if (IS_MFCv10X(dev))
		MFC_WRITEL(0x0, S5P_FIMV_MFC_CLOCK_OFF);

	mfc_debug(2, "Will now wait for completion of firmware transfer.\n");
	if (s5p_mfc_wait_for_done_dev(dev, S5P_FIMV_R2H_CMD_FW_STATUS_RET)) {
		mfc_err_dev("Failed to load firmware.\n");
		s5p_mfc_clean_dev_int_flags(dev);
		ret = -EIO;
		goto err_init_hw;
	}

	s5p_mfc_clean_dev_int_flags(dev);
	/* 4. Initialize firmware */
	ret = s5p_mfc_sys_init_cmd(dev, buf_type);
	if (ret) {
		mfc_err_dev("Failed to send command to MFC - timeout.\n");
		goto err_init_hw;
	}
	mfc_debug(2, "Ok, now will write a command to init the system\n");
	if (s5p_mfc_wait_for_done_dev(dev, S5P_FIMV_R2H_CMD_SYS_INIT_RET)) {
		mfc_err_dev("Failed to load firmware\n");
		ret = -EIO;
		goto err_init_hw;
	}

	dev->int_cond = 0;
	if (dev->int_err != 0 || dev->int_type !=
						S5P_FIMV_R2H_CMD_SYS_INIT_RET) {
		/* Failure. */
		mfc_err_dev("Failed to init firmware - error: %d"
				" int: %d.\n", dev->int_err, dev->int_type);
		ret = -EIO;
		goto err_init_hw;
	}

	fimv_info = s5p_mfc_get_fimv_info();
	if (fimv_info != 'D' && fimv_info != 'E')
		fimv_info = 'N';

	mfc_info_dev("MFC v%d.%x, F/W: %02xyy, %02xmm, %02xdd (%c)\n",
		 MFC_VER_MAJOR(dev),
		 MFC_VER_MINOR(dev),
		 s5p_mfc_get_fw_ver_year(),
		 s5p_mfc_get_fw_ver_month(),
		 s5p_mfc_get_fw_ver_date(),
		 fimv_info);

	dev->fw.date = s5p_mfc_get_fw_ver_all();
	/* Check MFC version and F/W version */
	if (IS_MFCV6(dev) && FW_HAS_VER_INFO(dev)) {
		fw_ver = s5p_mfc_get_mfc_version();
		if (fw_ver != mfc_version(dev)) {
			mfc_err_dev("Invalid F/W version(0x%x) for MFC H/W(0x%x)\n",
					fw_ver, mfc_version(dev));
			ret = -EIO;
			goto err_init_hw;
		}
	}

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	/* Cache flush for base address change */
	if (FW_HAS_BASE_CHANGE(dev)) {
		s5p_mfc_clean_dev_int_flags(dev);
		s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_CACHE_FLUSH, NULL);
		if (s5p_mfc_wait_for_done_dev(dev, S5P_FIMV_R2H_CMD_CACHE_FLUSH_RET)) {
			mfc_err_dev("Failed to flush cache\n");
			ret = -EIO;
			goto err_init_hw;
		}

		if (buf_type == MFCBUF_DRM && !curr_ctx_backup) {
			s5p_mfc_clock_off(dev);
			dev->curr_ctx_drm = curr_ctx_backup;
			s5p_mfc_clock_on_with_base(dev, MFCBUF_NORMAL);
		} else if (buf_type == MFCBUF_NORMAL && curr_ctx_backup) {
			s5p_mfc_init_memctrl(dev, MFCBUF_DRM);
		}
	}
#endif

err_init_hw:
	s5p_mfc_clock_off(dev);
	dev->curr_ctx_drm = curr_ctx_backup;
	mfc_debug_leave();

	return ret;
}

/* Wrapper : Initialize hardware */
int s5p_mfc_init_hw(struct s5p_mfc_dev *dev)
{
	int ret;

	ret = _s5p_mfc_init_hw(dev, MFCBUF_NORMAL);
	if (ret)
		return ret;

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	if (dev->drm_fw_status) {
		ret = _s5p_mfc_init_hw(dev, MFCBUF_DRM);
		if (ret)
			return ret;
	}
#endif

	return ret;
}

/* Deinitialize hardware */
void s5p_mfc_deinit_hw(struct s5p_mfc_dev *dev)
{
	mfc_debug(2, "mfc deinit start\n");

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	if (!IS_MFCv7X(dev) && !IS_MFCV8(dev)) {
		s5p_mfc_clock_on(dev);
		s5p_mfc_reset(dev);
		s5p_mfc_clock_off(dev);
	} else if (IS_MFCv10X(dev)) {
		mfc_info_dev("MFC h/w state: %d\n",
				MFC_READL(S5P_FIMV_MFC_STATE));
		MFC_WRITEL(0x1, S5P_FIMV_MFC_CLOCK_OFF);
	}

	mfc_debug(2, "mfc deinit completed\n");
}

int s5p_mfc_sleep(struct s5p_mfc_dev *dev)
{
	struct s5p_mfc_ctx *ctx;
	int ret;
	int old_state, i;
	int need_cache_flush = 0;

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	ctx = dev->ctx[dev->curr_ctx];
	if (!ctx) {
		for (i = 0; i < MFC_NUM_CONTEXTS; i++) {
			if (dev->ctx[i]) {
				ctx = dev->ctx[i];
				break;
			}
		}
		if (!ctx) {
			mfc_err("no mfc context to run\n");
			return -EINVAL;
		} else {
			mfc_info_dev("ctx is changed %d -> %d\n",
					dev->curr_ctx, ctx->num);
			dev->curr_ctx = ctx->num;
			if (dev->curr_ctx_drm != ctx->is_drm) {
				need_cache_flush = 1;
				mfc_info_dev("DRM attribute is changed %d->%d\n",
						dev->curr_ctx_drm, ctx->is_drm);
			}
		}
	}
	old_state = ctx->state;
	s5p_mfc_change_state(ctx, MFCINST_ABORT);
	ret = wait_event_interruptible_timeout(ctx->queue,
			(dev->hw_lock == 0),
			msecs_to_jiffies(MFC_INT_TIMEOUT));
	if (ret == 0) {
		mfc_err_dev("Waiting for hardware to finish timed out\n");
		ret = -EIO;
		return ret;
	}

	spin_lock_irq(&dev->condlock);
	mfc_info_dev("curr_ctx_drm:%d, hw_lock:%lu\n", dev->curr_ctx_drm, dev->hw_lock);
	set_bit(ctx->num, &dev->hw_lock);
	spin_unlock_irq(&dev->condlock);

	s5p_mfc_change_state(ctx, old_state);
	s5p_mfc_clock_on(dev);
	s5p_mfc_clean_dev_int_flags(dev);

	if (need_cache_flush) {
		s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_CACHE_FLUSH, NULL);
		if (s5p_mfc_wait_for_done_dev(dev, S5P_FIMV_R2H_CMD_CACHE_FLUSH_RET)) {
			mfc_err_ctx("Failed to flush cache\n");
			ret = -EINVAL;
			goto err_mfc_sleep;
		}

		s5p_mfc_init_memctrl(dev, (ctx->is_drm ? MFCBUF_DRM : MFCBUF_NORMAL));
		s5p_mfc_clock_off(dev);

		dev->curr_ctx_drm = ctx->is_drm;
		s5p_mfc_clock_on(dev);
	}

	ret = s5p_mfc_sleep_cmd(dev);
	if (ret) {
		mfc_err_dev("Failed to send command to MFC - timeout.\n");
		goto err_mfc_sleep;
	}
	if (s5p_mfc_wait_for_done_dev(dev, S5P_FIMV_R2H_CMD_SLEEP_RET)) {
		mfc_err_dev("Failed to sleep\n");
		ret = -EIO;
		goto err_mfc_sleep;
	}

	dev->int_cond = 0;
	if (dev->int_err != 0 || dev->int_type !=
						S5P_FIMV_R2H_CMD_SLEEP_RET) {
		/* Failure. */
		mfc_err_dev("Failed to sleep - error: %d"
				" int: %d.\n", dev->int_err, dev->int_type);
		ret = -EIO;
		goto err_mfc_sleep;
	}

err_mfc_sleep:
	if (IS_MFCv10X(dev))
		MFC_WRITEL(0x1, S5P_FIMV_MFC_CLOCK_OFF);
	s5p_mfc_clock_off(dev);
	mfc_debug_leave();

	return ret;
}

int s5p_mfc_wakeup(struct s5p_mfc_dev *dev)
{
	enum mfc_buf_usage_type buf_type;
	int ret;

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	mfc_info_dev("curr_ctx_drm:%d\n", dev->curr_ctx_drm);
	dev->wakeup_status = 1;

	/* 0. MFC reset */
	mfc_debug(2, "MFC reset...\n");

	s5p_mfc_clock_on(dev);

	dev->wakeup_status = 0;

	ret = s5p_mfc_reset(dev);
	if (ret) {
		mfc_err_dev("Failed to reset MFC - timeout.\n");
		goto err_mfc_wakeup;
	}
	mfc_debug(2, "Done MFC reset...\n");
	if (dev->curr_ctx_drm)
		buf_type = MFCBUF_DRM;
	else
		buf_type = MFCBUF_NORMAL;

	/* 1. Set DRAM base Addr */
	s5p_mfc_init_memctrl(dev, buf_type);

	/* 2. Initialize registers of channel I/F */
	s5p_mfc_clear_cmds(dev);

	s5p_mfc_clean_dev_int_flags(dev);
	/* 3. Initialize firmware */
	if (!FW_WAKEUP_AFTER_RISC_ON(dev))
		s5p_mfc_wakeup_cmd(dev);

	/* 4. Release reset signal to the RISC */
	if (IS_MFCV6(dev))
		MFC_WRITEL(0x1, S5P_FIMV_RISC_ON);
	else
		MFC_WRITEL(0x3ff, S5P_FIMV_SW_RESET);

	if (IS_MFCv10X(dev))
		MFC_WRITEL(0x0, S5P_FIMV_MFC_CLOCK_OFF);

	mfc_debug(2, "Will now wait for completion of firmware transfer.\n");
	if (FW_WAKEUP_AFTER_RISC_ON(dev)) {
		if (s5p_mfc_wait_for_done_dev(dev, S5P_FIMV_R2H_CMD_FW_STATUS_RET)) {
			mfc_err_dev("Failed to load firmware.\n");
			s5p_mfc_clean_dev_int_flags(dev);
			ret = -EIO;
			goto err_mfc_wakeup;
		}
	}

	if (FW_WAKEUP_AFTER_RISC_ON(dev))
		s5p_mfc_wakeup_cmd(dev);
	mfc_debug(2, "Ok, now will write a command to wakeup the system\n");
	if (s5p_mfc_wait_for_done_dev(dev, S5P_FIMV_R2H_CMD_WAKEUP_RET)) {
		mfc_err_dev("Failed to load firmware\n");
		ret = -EIO;
		goto err_mfc_wakeup;
	}

	dev->int_cond = 0;
	if (dev->int_err != 0 || dev->int_type !=
						S5P_FIMV_R2H_CMD_WAKEUP_RET) {
		/* Failure. */
		mfc_err_dev("Failed to wakeup - error: %d"
				" int: %d.\n", dev->int_err, dev->int_type);
		ret = -EIO;
		goto err_mfc_wakeup;
	}

err_mfc_wakeup:
	s5p_mfc_clock_off(dev);
	mfc_debug_leave();

	return 0;
}
