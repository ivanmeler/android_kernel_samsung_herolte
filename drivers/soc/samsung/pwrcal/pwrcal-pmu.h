#ifndef __PWRCAL_PMU_H__
#define __PWRCAL_PMU_H__

#ifdef __cplusplus
extern "C" {
#endif

struct cal_pd {
	void *offset;
	void *status;
	unsigned int shift;
	unsigned int mask;
	unsigned int status_shift;
	unsigned int status_mask;
	void (*config)(int enable);
	void (*prev)(int enable);
	void (*post)(int enable);
	char *name;
};
struct cal_pd_ops {
	int (*pd_control)(struct cal_pd *pd, int on);
	int (*pd_status)(struct cal_pd *pd);
	int (*pd_init)(void);
};
extern struct cal_pd_ops cal_pd_ops;

/* Macro to define BLKPWR */
#define BLKPWR(_name, _offset, _shift, _mask, _status, _status_shift, _status_mask, _config, _prev, _post)	\
struct cal_pd blkpwr_##_name __attribute__((unused, aligned(8), section(".blkpwr."))) = {	\
	.offset		= _offset,	\
	.shift		= _shift,		\
	.mask		= _mask,	\
	.status		= _status,	\
	.status_shift	= _status_shift,	\
	.status_mask	= _status_mask,	\
	.config		= _config,	\
	.prev		= _prev,		\
	.post		= _post,		\
	.name		= #_name,	\
}

extern struct cal_pd *pwrcal_blkpwr_list[];
extern unsigned int pwrcal_blkpwr_size;

extern int blkpwr_control(struct cal_pd *pd, int enable);
extern int blkpwr_status(struct cal_pd *pd);



struct cal_pm_ops {
	void (*pm_enter)(int mode);
	void (*pm_exit)(int mode);
	void (*pm_earlywakeup)(int mode);
	void (*pm_init)(void);
};
extern struct cal_pm_ops cal_pm_ops;

#ifdef __cplusplus
}
#endif


#endif
