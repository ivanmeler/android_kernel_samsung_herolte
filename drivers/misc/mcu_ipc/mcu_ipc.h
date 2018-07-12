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

extern int mbox_request_irq(u32 int_num,
			void (*handler)(void *), void *data);
extern int mcu_ipc_unregister_handler(u32 int_num,
				void (*handler)(void *));
extern void mbox_set_interrupt(u32 int_num);
extern void mcu_ipc_send_command(u32 int_num, u16 cmd);
extern u32 mbox_get_value(u32 mbx_num);
extern void mbox_set_value(u32 mbx_num, u32 msg);

struct mcu_ipc_ipc_handler {
void *data;
void (*handler)(void *);
};

struct mcu_ipc_drv_data {
	char *name;

	void __iomem *ioaddr;
	u32 registered_irq;

#ifdef CONFIG_ARGOS
	/**
	 * irq affinity cpu mask
	 */
	cpumask_var_t dmask;	/* default cpu mask */
	cpumask_var_t imask;	/* irq affinity cpu mask */
#endif

	struct device *mcu_ipc_dev;
	struct mcu_ipc_ipc_handler hd[16];

	u32 mr0;	/* Interrupt mask register */
};

static struct mcu_ipc_drv_data mcu_dat;

static inline void mcu_ipc_writel(u32 val, long reg)
{
	writel(val, mcu_dat.ioaddr + reg);
}

static inline u32 mcu_ipc_readl(long reg)
{
	return readl(mcu_dat.ioaddr + reg);
}

#define mcu_dt_read_enum(np, prop, dest) \
	do { \
		u32 val; \
		if (of_property_read_u32(np, prop, &val)) \
			return -EINVAL; \
		dest = (__typeof__(dest))(val); \
	} while (0)

#define mcu_dt_read_bool(np, prop, dest) \
	do { \
		u32 val; \
		if (of_property_read_u32(np, prop, &val)) \
			return -EINVAL; \
		dest = val ? true : false; \
	} while (0)

#define mcu_dt_read_string(np, prop, dest) \
	do { \
		if (of_property_read_string(np, prop, \
				(const char **)&dest)) \
			return -EINVAL; \
	} while (0)

#define mcu_dt_read_u32(np, prop, dest) \
	do { \
		u32 val; \
		if (of_property_read_u32(np, prop, &val)) \
			return -EINVAL; \
		dest = val; \
	} while (0)

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


#endif
