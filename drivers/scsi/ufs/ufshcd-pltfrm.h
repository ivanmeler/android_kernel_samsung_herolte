/*
 * Universal Flash Storage Host controller Platform bus based glue driver
 *
 * Copyright (C) 2013, Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * at your option) any later version.
 */

#ifndef _UFSHCD_PLTFRM_H_
#define _UFSHCD_PLTFRM_H_

extern int ufshcd_pltfrm_init(struct platform_device *pdev,
			      const struct ufs_hba_variant *var);
extern void ufshcd_pltfrm_exit(struct platform_device *pdev);

#endif /* _UFSHCD_PLTFRM_H_ */
