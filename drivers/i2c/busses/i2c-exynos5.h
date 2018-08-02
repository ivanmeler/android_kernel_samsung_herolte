/**
 * i2c-exynos5.h - Samsung Exynos5 I2C Controller Driver Header file
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __I2C_EXYNOS5_H
#define __I2C_EXYNOS5_H

struct exynos5_i2c {
	struct list_head	node;
	struct i2c_adapter	adap;
	unsigned int		need_hw_init;
	unsigned int		suspended:1;

	struct i2c_msg		*msg;
	struct completion	msg_complete;
	unsigned int		msg_ptr;
	unsigned int		msg_len;

	unsigned int		irq;

	void __iomem		*regs;
	void __iomem		*regs_mailbox;
	struct clk		*clk;
	struct clk		*rate_clk;
	struct device		*dev;
	int			state;

	/*
	 * Since the TRANS_DONE bit is cleared on read, and we may read it
	 * either during an IRQ or after a transaction, keep track of its
	 * state here.
	 */
	int			trans_done;

	/* Controller operating frequency */
	unsigned int		fs_clock;
	unsigned int		hs_clock;

	/*
	 * HSI2C Controller can operate in
	 * 1. High speed upto 3.4Mbps
	 * 2. Fast speed upto 1Mbps
	 */
	int			speed_mode;
	int			operation_mode;
	int			bus_id;
	int			scl_clk_stretch;
	int			stop_after_trans;
	int			use_old_timing_values;
	unsigned int		transfer_delay;
#ifdef CONFIG_EXYNOS_APM
	int			use_apm_mode;
#endif
	/* HSI2C Batcher can automatically handle HSI2C operation */
	int			support_hsi2c_batcher;

	unsigned int		cmd_buffer;
	unsigned int		cmd_index;
	unsigned int		cmd_pointer;
	unsigned int		desc_pointer;
	unsigned int		batcher_read_addr;
	int			need_cs_enb;
	int			idle_ip_index;
	int			reset_before_trans;
	unsigned int		secure_mode;
};
#endif /*__I2C_EXYNOS5_H */
