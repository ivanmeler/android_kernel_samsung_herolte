/* linux/driver/mailbox/mailbox-exynos.h
 *
 * Copyright (c) 2014-2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Mailbox-exynos - Mailbox register definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_DRIVER_REGS_MAILBOX_H
#define __ASM_DRIVER_REGS_MAILBOX_H __FILE__

#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>

#define MAX_LINKS	1
#define POLL_PERIOD	(100)
#define G3D_STATUS	(4)
#define INT_STATUS	(5)
#define MAILBOX_REG_CNT (4)
#define CM3_COUNT_MAX	(10000)

#define EXYNOS_PMU_G3D_STATUS			(0x4064)
#define G3D_STATUS_MASK				(0xF)

#define EXYNOS_MAILBOX_TX_CON			(0x00000)
#define EXYNOS_MAILBOX_TX_ADDR			(0x00004)
#define EXYNOS_MAILBOX_TX_DATA			(0x00008)
#define EXYNOS_MAILBOX_TX_INT			(0x0000C)
#define EXYNOS_MAILBOX_TX(i)			((i) * (0x4))
#define EXYNOS_MAILBOX_MRX_SEM                  (0x00010)
#define EXYNOS_MAILBOX_RX_CON			(0x00040)
#define EXYNOS_MAILBOX_RX_ADDR			(0x00044)
#define EXYNOS_MAILBOX_RX_DATA			(0x00048)
#define EXYNOS_MAILBOX_RX_INT			(0x0004C)
#define EXYNOS_MAILBOX_RX(i)			(0x0040 + ((i) * 0x4))

#define RX_INT_CLEAR				(0xFF << 0)
#define MRX_SEM_ENABLE				(0x1 << 0)
#define NOTI_INT_CLEAR				(0x1 << 0)
#define CM3_STATUS1_SHIFT			(31)
#define CM3_STATUS1_MASK			(0x1)
#define CM3_INTERRPUT_SHIFT			(0x1)
#define CM3_POLLING_SHIFT			(0x1)
#define SRAM_UNSTABLE				(0x0)
#define SRAM_STABLE				(0x1)

/* Exynos8890 EVT1 Register */
#define EXYNOS_MAILBOX_TX_MSG0			(0x00000)
#define EXYNOS_MAILBOX_TX_MSG1			(0x00004)
#define EXYNOS_MAILBOX_TX_MSG2			(0x00008)
#define EXYNOS_MAILBOX_TX_MSG3			(0x0000C)
#define EXYNOS_MAILBOX_TX_MSG4			(0x00010)
#define EXYNOS_MAILBOX_TX_MSG5			(0x00014)
#define EXYNOS_MAILBOX_TX_MSG6			(0x00018)
#define EXYNOS_MAILBOX_TX_MSG7			(0x0001C)
#define EXYNOS_MAILBOX_TX_MSG(i)		((i) * (0x4))

#define EXYNOS_MAILBOX_RX_MSG0			(0x00080)
#define EXYNOS_MAILBOX_RX_MSG1			(0x00084)
#define EXYNOS_MAILBOX_RX_MSG2			(0x00088)
#define EXYNOS_MAILBOX_RX_MSG3			(0x0008C)
#define EXYNOS_MAILBOX_RX_MSG4			(0x00090)
#define EXYNOS_MAILBOX_RX_MSG5			(0x00094)
#define EXYNOS_MAILBOX_RX_MSG6			(0x00098)
#define EXYNOS_MAILBOX_RX_MSG7			(0x0009C)
#define EXYNOS_MAILBOX_RX_MSG(i)		(0x00080 + ((i) * 0x4))

#define EXYNOS_MAILBOX_TX_IRQ0			(0x00040)
#define EXYNOS_MAILBOX_TX_IRQ1			(0x00044)
#define EXYNOS_MAILBOX_TX_IRQ2			(0x00048)
#define EXYNOS_MAILBOX_TX_IRQ3			(0x0004C)
#define EXYNOS_MAILBOX_TX_IRQ4			(0x00050)
#define EXYNOS_MAILBOX_TX_IRQ5			(0x00054)
#define EXYNOS_MAILBOX_TX_IRQ6			(0x00058)
#define EXYNOS_MAILBOX_TX_IRQ7			(0x0005C)

#define EXYNOS_MAILBOX_RX_IRQ0			(0x000C0)
#define EXYNOS_MAILBOX_RX_IRQ1			(0x000C4)
#define EXYNOS_MAILBOX_RX_IRQ2			(0x000C8)
#define EXYNOS_MAILBOX_RX_IRQ3			(0x000CC)
#define EXYNOS_MAILBOX_RX_IRQ4			(0x000D0)
#define EXYNOS_MAILBOX_RX_IRQ5			(0x000D4)
#define EXYNOS_MAILBOX_RX_IRQ6			(0x000D8)
#define EXYNOS_MAILBOX_RX_IRQ7			(0x000DC)

/* Debug */
#define ATL_VOL_SHIFT				(24)
#define APO_VOL_SHIFT				(16)
#define G3D_VOL_SHIFT				(8)
#define MIF_VOL_SHIFT				(0)
#define VOL_MASK				(0xFF)
#define PMIC_STEP				(6250)
#define MIN_VOL					(300000)

extern int data_history(void);
extern void samsung_mbox_enable_irq(void);
extern void samsung_mbox_disable_irq(void);

struct samsung_mlink {
	const char *name;
	struct mbox_chan chan;
	struct samsung_mbox *smc;
	int irq;
	int exception_irq;
	void *data;
};

struct samsung_mbox {
	struct device *dev;
	void __iomem *mbox_base;
	struct mbox_controller mbox_con;
	struct mutex lock;
	struct samsung_mlink samsung_link[MAX_LINKS];
};

void exynos_mbox_client_init(struct device *dev);
#endif /* __ASM_DRIVER_REGS_CLOCK_H */
