#ifndef FIMC_IS_DEVICE_CSI_H
#define FIMC_IS_DEVICE_CSI_H

#include <linux/interrupt.h>
#include <media/v4l2-device.h>
#include "fimc-is-hw.h"
#include "fimc-is-type.h"
#include "fimc-is-framemgr.h"
#include "fimc-is-subdev-ctrl.h"

#ifndef ENABLE_IS_CORE
#define CSI_NOTIFY_VSYNC	10
#define CSI_NOTIFY_VBLANK	11
#endif

enum fimc_is_csi_state {
	/* flite join ischain */
	CSIS_JOIN_ISCHAIN,
	/* one the fly output */
	CSIS_OTF_WITH_3AA,
	/* If it's dummy, H/W setting can't be applied */
	CSIS_DUMMY,
	/* WDMA flag */
	CSIS_DMA_ENABLE,
	CSIS_START_STREAM,
	/* runtime buffer done state for error */
	CSIS_BUF_ERR_VC0,
	CSIS_BUF_ERR_VC1,
	CSIS_BUF_ERR_VC2,
	CSIS_BUF_ERR_VC3,
};

struct fimc_is_device_csi {
	/* channel information */
	u32				instance;
	u32 __iomem			*base_reg;
	int				irq;

	/* for settle time */
	u32				sensor_cfgs;
	struct fimc_is_sensor_cfg	*sensor_cfg;

	/* for vci setting */
	u32				vcis;
	struct fimc_is_vci		*vci;

	/* image configuration */
	u32				mode;
	u32				lanes;
	u32				mipi_speed;
	struct fimc_is_image		image;

	unsigned long			state;

	/* for DMA feature */
	struct fimc_is_framemgr		*framemgr;
	u32				overflow_cnt;
	u32				sw_checker;
	atomic_t			fcount;
	struct tasklet_struct		tasklet_csis_str;
	struct tasklet_struct		tasklet_csis_end;
	struct tasklet_struct		tasklet_csis_dma[CSI_VIRTUAL_CH_MAX];
	int				pre_dma_enable[CSI_VIRTUAL_CH_MAX];

	/* subdev slots for dma */
	struct fimc_is_subdev		*dma_subdev[ENTRY_SEN_END];

	/* pointer address from device sensor */
	struct v4l2_subdev		**subdev;
	struct phy			*phy;

	u32				dbg_fcount;
#ifndef ENABLE_IS_CORE
	atomic_t                        vblank_count; /* Increase at CSI frame end */

	atomic_t			vvalid; /* set 1 while vvalid period */
#endif
};

int __must_check fimc_is_csi_probe(void *parent, u32 instance);
int __must_check fimc_is_csi_open(struct v4l2_subdev *subdev, struct fimc_is_framemgr *framemgr);
int __must_check fimc_is_csi_close(struct v4l2_subdev *subdev);

/* for DMA feature */
extern u32 __iomem *notify_fcount_sen0;
extern u32 __iomem *notify_fcount_sen1;
extern u32 __iomem *notify_fcount_sen2;
extern u32 __iomem *last_fcount0;
extern u32 __iomem *last_fcount1;
#endif
