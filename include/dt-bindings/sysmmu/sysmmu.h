/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for Exynos System MMU.
 */

#ifndef _DT_BINDINGS_EXYNOS_SYSTEM_MMU_H
#define _DT_BINDINGS_EXYNOS_SYSTEM_MMU_H

/* MAX AXI ID: 65535 (MAX AW_AXI_ID + AW Marker(=0x8000))
 * NAID: Not Available AXI ID that exceeds MAX AXI ID.
 * AR: AR AXI ID.
 * AW: AW AXI ID.
 */
#define AXI_ID_MARKER	0x8000

#define NAID		65536

#define AR(x)	(x)
#define AW(x)	((x) + AXI_ID_MARKER)

#define AXIID_MASK	(AXI_ID_MARKER - 1)

#define is_axi_id(id)		((id) != NAID)
#define is_ar_axi_id(id)	(!((id) & AXI_ID_MARKER))


#endif /* _DT_BINDINGS_EXYNOS_SYSTEM_MMU_H */
