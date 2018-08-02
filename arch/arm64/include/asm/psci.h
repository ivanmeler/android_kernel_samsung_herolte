/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Copyright (C) 2013 ARM Limited
 */

#ifndef __ASM_PSCI_H
#define __ASM_PSCI_H

int psci_init(void);

#define PSCI_UNUSED_INDEX		128
#define PSCI_CLUSTER_SLEEP		(PSCI_UNUSED_INDEX)
#define PSCI_SYSTEM_IDLE		(PSCI_UNUSED_INDEX + 1)
#define PSCI_SYSTEM_IDLE_CLUSTER_SLEEP	(PSCI_UNUSED_INDEX + 2)
#define PSCI_SYSTEM_IDLE_AUDIO		(PSCI_UNUSED_INDEX + 3)
#define PSCI_SYSTEM_SLEEP		(PSCI_UNUSED_INDEX + 4)

#endif /* __ASM_PSCI_H */
