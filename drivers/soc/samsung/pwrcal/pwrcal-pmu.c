#include "pwrcal.h"
#include "pwrcal-env.h"
#include "pwrcal-rae.h"
#include "pwrcal-pmu.h"


static int blkpwr_enable(struct cal_pd *pd)
{
	unsigned int sfrvalue;
	unsigned int timeout;
	unsigned int ret = -1;

	if (!pd || !pd->offset)
		goto out;

	if (pd->prev)
		pd->prev(1);

	if (pd->config)
		pd->config(1);

	pwrcal_setf(pd->offset, pd->shift, pd->mask, pd->mask);
	ret = 0;
	timeout = 0;
	if (pd->status) {
		while (1) {
			sfrvalue = pwrcal_getf(pd->status, pd->status_shift, pd->status_mask);
			if (sfrvalue == pd->status_mask)
				break;
			timeout++;
			if (timeout > 10000) {
				ret = -1;
				break;
			}
		}
	}

	if (pd->post && !ret)
		pd->post(1);

out:
	if (pd && ret)
		pr_err("BLKPWR enable Fail (%s) STATUS : 0x%08X\n", pd->name, pwrcal_readl(pd->status));

	return 0;
}

static int blkpwr_disable(struct cal_pd *pd)
{
	unsigned int sfrvalue;
	unsigned int timeout;
	unsigned int ret = -1;

	if (!pd || !pd->offset)
		goto out;

	if (pd->prev)
		pd->prev(0);

	if (pd->config)
		pd->config(0);

	pwrcal_setf(pd->offset, pd->shift, pd->mask, 0);
	ret = 0;
	timeout = 0;
	if (pd->status) {
		while (1) {
			sfrvalue = pwrcal_getf(pd->status, pd->status_shift, pd->status_mask);
			if (sfrvalue == 0)
				break;
			timeout++;
			if (timeout > 10000) {
				ret = -1;
				break;
			}
		}
	}

	if (pd->post)
		pd->post(0);

out:
	if (pd && ret)
		pr_err("BLKPWR disable Fail (%s) STATUS : 0x%08X\n", pd->name, pwrcal_readl(pd->status));

	return 0;
}


int blkpwr_control(struct cal_pd *pd, int enable)
{
	if (enable)
		return blkpwr_enable(pd);
	return blkpwr_disable(pd);
}

int blkpwr_status(struct cal_pd *pd)
{
	unsigned int sfrvalue;

	if (!pd->status)
		return 1;

	sfrvalue = pwrcal_getf(pd->status, pd->status_shift, pd->status_mask);

	if (sfrvalue == 0)
		return 0;
	else if (sfrvalue == pd->status_mask)
		return 1;

	pr_err("blkpwr unknown state [%s] status [0x%08X]",
		pd->name, pwrcal_readl(pd->status));
	return -1;
}
