/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_cmd.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __S5P_MFC_CMD_H
#define __S5P_MFC_CMD_H __FILE__


#include "s5p_mfc_common.h"

#define MAX_H2R_ARG		4

struct s5p_mfc_cmd_args {
	unsigned int	arg[MAX_H2R_ARG];
};

int s5p_mfc_cmd_host2risc(struct s5p_mfc_dev *dev, int cmd, struct s5p_mfc_cmd_args *args);
int s5p_mfc_sys_init_cmd(struct s5p_mfc_dev *dev,
				enum mfc_buf_usage_type buf_type);
int s5p_mfc_sleep_cmd(struct s5p_mfc_dev *dev);
int s5p_mfc_wakeup_cmd(struct s5p_mfc_dev *dev);
int s5p_mfc_open_inst_cmd(struct s5p_mfc_ctx *ctx);
int s5p_mfc_close_inst_cmd(struct s5p_mfc_ctx *ctx);

void s5p_mfc_init_memctrl(struct s5p_mfc_dev *dev,
					enum mfc_buf_usage_type buf_type);
int s5p_mfc_check_int_cmd(struct s5p_mfc_dev *dev);

static inline void s5p_mfc_clear_cmds(struct s5p_mfc_dev *dev)
{
	if (IS_MFCV6(dev)) {
		/* Zero initialization should be done before RESET.
		 * Nothing to do here. */
	} else {
		MFC_WRITEL(0xffffffff, S5P_FIMV_SI_CH0_INST_ID);
		MFC_WRITEL(0xffffffff, S5P_FIMV_SI_CH1_INST_ID);

		MFC_WRITEL(0, S5P_FIMV_RISC2HOST_CMD);
		MFC_WRITEL(0, S5P_FIMV_HOST2RISC_CMD);
	}
}

#define s5p_mfc_clear_int_flags()				\
	do {							\
		MFC_WRITEL(0, S5P_FIMV_RISC2HOST_CMD);	\
		MFC_WRITEL(0, S5P_FIMV_RISC2HOST_INT);	\
	} while (0)

#endif /* __S5P_MFC_CMD_H */
