/*
 * Copyright (C) 2014 Samsung Electronics Co.Ltd
 * http://www.samsung.com
 *
 * MCU IPC driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
*/
#ifndef MCU_IPC_H
#define MCU_IPC_H

#define MCU_IPC_INT0    (0)
#define MCU_IPC_INT1    (1)
#define MCU_IPC_INT2    (2)
#define MCU_IPC_INT3    (3)
#define MCU_IPC_INT4    (4)
#define MCU_IPC_INT5    (5)
#define MCU_IPC_INT6    (6)
#define MCU_IPC_INT7    (7)
#define MCU_IPC_INT8    (8)
#define MCU_IPC_INT9    (9)
#define MCU_IPC_INT10   (10)
#define MCU_IPC_INT11   (11)
#define MCU_IPC_INT12   (12)
#define MCU_IPC_INT13   (13)
#define MCU_IPC_INT14   (14)
#define MCU_IPC_INT15   (15)

/* Shared register with 64 * 32 words */
#define MAX_MBOX_NUM	64

struct mcu_ipc_ipc_handler {
	void *data;
	void (*handler)(void *);
};

struct mcu_ipc_drv_data {
	char *name;
	void __iomem *ioaddr;
	u32 registered_irq;

	/**
	 * irq affinity cpu mask
	 */
	cpumask_var_t dmask;	/* default cpu mask */
	cpumask_var_t imask;	/* irq affinity cpu mask */

	struct device *mcu_ipc_dev;
	struct mcu_ipc_ipc_handler hd[16];
};

#ifdef CONFIG_ARGOS
/* kernel team needs to provide argos header file. !!!
 * As of now, there's nothing to use. */
#ifdef CONFIG_SCHED_HMP
extern struct cpumask hmp_slow_cpu_mask;
extern struct cpumask hmp_fast_cpu_mask;

static inline struct cpumask *get_default_cpu_mask(void)
{
	return &hmp_slow_cpu_mask;
}
#else
static inline struct cpumask *get_default_cpu_mask(void)
{
	return cpu_all_mask;
}
#endif

int argos_irq_affinity_setup_label(unsigned int irq, const char *label,
		struct cpumask *affinity_cpu_mask,
		struct cpumask *default_cpu_mask);
int argos_task_affinity_setup_label(struct task_struct *p, const char *label,
		struct cpumask *affinity_cpu_mask,
		struct cpumask *default_cpu_mask);
#endif

int mbox_request_irq(u32 int_num, void (*handler)(void *), void *data);
int mcu_ipc_unregister_handler(u32 int_num, void (*handler)(void *));
void mbox_set_interrupt(u32 int_num);
void mcu_ipc_send_command(u32 int_num, u16 cmd);
u32 mbox_get_value(u32 mbx_num);
void mbox_set_value(u32 mbx_num, u32 msg);

#ifdef CONFIG_MCU_IPC_LOG
void mbox_check_mcu_irq(int irq);
#endif
#endif
