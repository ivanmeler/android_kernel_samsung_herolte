/*
 * Copyright (c) 2007 Ben Dooks
 * Copyright (c) 2008 Simtec Electronics
 *     Ben Dooks <ben@simtec.co.uk>, <ben-linux@fluff.org>
 * Copyright (c) 2013 Tomasz Figa <tomasz.figa@gmail.com>
 *
 * PWM driver for Samsung SoCs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/time.h>

/* For struct samsung_timer_variant and samsung_pwm_lock. */
#include <clocksource/samsung_pwm.h>
#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos-powermode.h>

#define REG_TCFG0			0x00
#define REG_TCFG1			0x04
#define REG_TCON			0x08

#define REG_TCNTB(chan)			(0x0c + ((chan) * 0xc))
#define REG_TCMPB(chan)			(0x10 + ((chan) * 0xc))

#define TCFG0_PRESCALER_MASK		0xff
#define TCFG0_PRESCALER1_SHIFT		8

#define TCFG1_MUX_MASK			0xf
#define TCFG1_SHIFT(chan)		(4 * (chan))

/*
 * Each channel occupies 4 bits in TCON register, but there is a gap of 4
 * bits (one channel) after channel 0, so channels have different numbering
 * when accessing TCON register. See to_tcon_channel() function.
 *
 * In addition, the location of autoreload bit for channel 4 (TCON channel 5)
 * in its set of bits is 2 as opposed to 3 for other channels.
 */
#define TCON_START(chan)		BIT(4 * (chan) + 0)
#define TCON_MANUALUPDATE(chan)		BIT(4 * (chan) + 1)
#define TCON_INVERT(chan)		BIT(4 * (chan) + 2)
#define _TCON_AUTORELOAD(chan)		BIT(4 * (chan) + 3)
#define _TCON_AUTORELOAD4(chan)		BIT(4 * (chan) + 2)
#define TCON_AUTORELOAD(chan)		\
	((chan < 5) ? _TCON_AUTORELOAD(chan) : _TCON_AUTORELOAD4(chan))

enum duty_cycle {
	DUTY_CYCLE_ZERO,
	DUTY_CYCLE_PULSE,
	DUTY_CYCLE_FULL,
};

/**
 * struct samsung_pwm_channel - private data of PWM channel
 * @period_ns:	current period in nanoseconds programmed to the hardware
 * @duty_ns:	current duty time in nanoseconds programmed to the hardware
 * @tin_ns:	time of one timer tick in nanoseconds with current timer rate
 */
struct samsung_pwm_channel {
	struct clk		*clk_div;
	struct clk		*clk_tin;

	u32 			period_ns;
	u32 			duty_ns;
	u32 			tin_ns;
	unsigned char	running;
	enum duty_cycle	duty_cycle;
};

/**
 * struct samsung_pwm_chip - private data of PWM chip
 * @chip:		generic PWM chip
 * @variant:		local copy of hardware variant data
 * @inverter_mask:	inverter status for all channels - one bit per channel
 * @base:		base address of mapped PWM registers
 * @base_clk:		base clock used to drive the timers
 */
struct samsung_pwm_chip {
	struct pwm_chip chip;
	struct samsung_pwm_variant variant;
	u8 inverter_mask;

	void __iomem *base;
	struct clk *base_clk;
	struct clk *sclk;
	unsigned int reg_tcfg0;
	int enable_cnt;
	unsigned int need_hw_init;
	unsigned int idle_ip_index;
	bool sclk_ctrl;
};

#ifndef CONFIG_CLKSRC_SAMSUNG_PWM
/*
 * PWM block is shared between pwm-samsung and samsung_pwm_timer drivers
 * and some registers need access synchronization. If both drivers are
 * compiled in, the spinlock is defined in the clocksource driver,
 * otherwise following definition is used.
 *
 * Currently we do not need any more complex synchronization method
 * because all the supported SoCs contain only one instance of the PWM
 * IP. Should this change, both drivers will need to be modified to
 * properly synchronize accesses to particular instances.
 */
static DEFINE_SPINLOCK(samsung_pwm_lock);
#endif
struct samsung_pwm_chip *g_samsung_pwm;

static void pwm_samsung_save(struct samsung_pwm_chip *chip);
static void pwm_samsung_restore(struct samsung_pwm_chip *chip);

static int pwm_samsung_clk_enable(struct samsung_pwm_chip *chip)
{
	int ret;

	exynos_update_ip_idle_status(chip->idle_ip_index, 0);
	ret = clk_enable(chip->base_clk);
	if (ret)
		goto base_clk_err;

	if (chip->sclk_ctrl) {
		ret = clk_enable(chip->sclk);
		if (ret)
			goto sclk_err;
	}

	return 0;
sclk_err:
	clk_disable(chip->base_clk);
base_clk_err:
	exynos_update_ip_idle_status(chip->idle_ip_index, 1);
	return ret;
}

static void pwm_samsung_clk_disable(struct samsung_pwm_chip *chip)
{
	if (chip->sclk_ctrl)
		clk_disable(chip->sclk);

	clk_disable(chip->base_clk);
	exynos_update_ip_idle_status(chip->idle_ip_index, 1);
}

static inline
struct samsung_pwm_chip *to_samsung_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct samsung_pwm_chip, chip);
}

static inline unsigned int to_tcon_channel(unsigned int channel)
{
	/* TCON register has a gap of 4 bits (1 channel) after channel 0 */
	return (channel == 0) ? 0 : (channel + 1);
}

static void pwm_samsung_set_divisor(struct samsung_pwm_chip *chip,
				    unsigned int chan, unsigned long rate)
{
	struct pwm_device *pwm = &chip->chip.pwms[chan];
	struct samsung_pwm_channel *channel = pwm_get_chip_data(pwm);

	clk_set_rate(channel->clk_div, rate);
}

static int pwm_samsung_is_tdiv(struct samsung_pwm_chip *chip, unsigned int chan)
{
	struct pwm_device *pwm = &chip->chip.pwms[chan];
	struct samsung_pwm_channel *channel = pwm_get_chip_data(pwm);

	return clk_get_parent(channel->clk_tin) == channel->clk_div;
}

static unsigned long pwm_samsung_get_tin_rate(struct samsung_pwm_chip *chip,
					      unsigned int chan)
{
	struct pwm_device *pwm = &chip->chip.pwms[chan];
	struct samsung_pwm_channel *channel = pwm_get_chip_data(pwm);
	unsigned long rate;

	rate = clk_get_rate(clk_get_parent(channel->clk_div));
	clk_set_rate(clk_get_parent(channel->clk_div), rate);

	return rate;
}

static unsigned long pwm_samsung_calc_tin(struct samsung_pwm_chip *chip,
					  unsigned int chan, unsigned long freq)
{
	struct pwm_device *pwm = &chip->chip.pwms[chan];
	struct samsung_pwm_channel *channel = pwm_get_chip_data(pwm);
	struct samsung_pwm_variant *variant = &chip->variant;
	unsigned long rate;
	u8 div;

	if (!pwm_samsung_is_tdiv(chip, chan)) {
		rate = clk_get_rate(channel->clk_tin);
		if (rate)
			return rate;

		dev_warn(chip->chip.dev,
			"tin of PWM %d is inoperational, using tdiv\n", chan);
	}

	rate = pwm_samsung_get_tin_rate(chip, chan);
	dev_dbg(chip->chip.dev, "tin parent at %lu\n", rate);

	/*
	 * Compare minimum PWM frequency that can be achieved with possible
	 * divider settings and choose the lowest divisor that can generate
	 * frequencies lower than requested.
	 */
	for (div = variant->div_base; div < 4; ++div)
		if ((rate >> (variant->bits + div)) < freq)
			break;

	pwm_samsung_set_divisor(chip, chan, rate >> div);
	dev_dbg(chip->chip.dev, "tdiv at %lu\n", clk_get_rate(channel->clk_div));

	return rate >> div;
}

static void pwm_samsung_init(struct samsung_pwm_chip *chip,
					struct pwm_device *pwm)
{
	unsigned int tcon_chan = to_tcon_channel(pwm->hwpwm);
	u32 tcon;

	__raw_writel(0, chip->base + REG_TCMPB(pwm->hwpwm));
	__raw_writel(0, chip->base + REG_TCNTB(pwm->hwpwm));

	tcon = __raw_readl(chip->base + REG_TCON);
	tcon |= TCON_INVERT(tcon_chan) | TCON_MANUALUPDATE(tcon_chan);
	tcon &= ~(TCON_AUTORELOAD(tcon_chan) | TCON_START(tcon_chan));
	__raw_writel(tcon, chip->base + REG_TCON);

	tcon &= ~TCON_MANUALUPDATE(tcon_chan);
	__raw_writel(tcon, chip->base + REG_TCON);
}

static int pwm_samsung_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct samsung_pwm_chip *our_chip = to_samsung_pwm_chip(chip);
	struct samsung_pwm_channel *our_chan;
	unsigned char clk_tin_name[16];
	unsigned char clk_tdiv_name[16];
	unsigned long flags;

	if (!(our_chip->variant.output_mask & BIT(pwm->hwpwm))) {
		dev_warn(chip->dev,
			"tried to request PWM channel %d without output\n",
			pwm->hwpwm);
		return -EINVAL;
	}

	our_chan = devm_kzalloc(chip->dev, sizeof(*our_chan), GFP_KERNEL);
	if (!our_chan)
		return -ENOMEM;

	pwm_set_chip_data(pwm, our_chan);

	snprintf(clk_tin_name, sizeof(clk_tin_name), "pwm-tin%d", pwm->hwpwm);
	our_chan->clk_tin = devm_clk_get(chip->dev, clk_tin_name);
	if (IS_ERR(our_chan->clk_tin)) {
		dev_err(chip->dev, "failed to get pwm tin clk\n");
		return PTR_ERR(our_chan->clk_tin);
	}

	snprintf(clk_tdiv_name, sizeof(clk_tdiv_name), "pwm-tdiv%d", pwm->hwpwm);
	our_chan->clk_div = devm_clk_get(chip->dev, clk_tdiv_name);
	if (IS_ERR(our_chan->clk_div)) {
		dev_err(chip->dev, "failed to get pwm tdiv clk\n");
		return PTR_ERR(our_chan->clk_div);
	}

	spin_lock_irqsave(&samsung_pwm_lock, flags);
	pwm_samsung_clk_enable(our_chip);
	pwm_samsung_init(our_chip, pwm);
	pwm_samsung_clk_disable(our_chip);
	spin_unlock_irqrestore(&samsung_pwm_lock, flags);

	return 0;
}

static void pwm_samsung_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	devm_kfree(chip->dev, pwm_get_chip_data(pwm));
	pwm_set_chip_data(pwm, NULL);
}

static int pwm_samsung_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct samsung_pwm_chip *our_chip = to_samsung_pwm_chip(chip);
	unsigned int tcon_chan = to_tcon_channel(pwm->hwpwm);
	struct samsung_pwm_channel *channel = pwm_get_chip_data(pwm);
	unsigned long flags;
	u32 tcon;

	spin_lock_irqsave(&samsung_pwm_lock, flags);

	if (!our_chip->enable_cnt && !our_chip->need_hw_init)
		pwm_samsung_clk_enable(our_chip);

	if (our_chip->need_hw_init)
		pwm_samsung_restore(our_chip);

	tcon = __raw_readl(our_chip->base + REG_TCON);
	if (!(tcon & TCON_START(tcon_chan))) {
		tcon |= TCON_MANUALUPDATE(tcon_chan);
		__raw_writel(tcon, our_chip->base + REG_TCON);

		tcon &= ~TCON_MANUALUPDATE(tcon_chan);
		if (channel->duty_cycle == DUTY_CYCLE_ZERO)
			tcon &= ~TCON_AUTORELOAD(tcon_chan);
		else
			tcon |= TCON_AUTORELOAD(tcon_chan);

		tcon |= TCON_START(tcon_chan);
		__raw_writel(tcon, our_chip->base + REG_TCON);
	} else if (!(tcon & TCON_AUTORELOAD(tcon_chan)) &&
		   channel->duty_cycle != DUTY_CYCLE_ZERO) {
		tcon |= TCON_MANUALUPDATE(tcon_chan);
		__raw_writel(tcon, our_chip->base + REG_TCON);

		tcon &= ~TCON_MANUALUPDATE(tcon_chan);
		tcon |= TCON_AUTORELOAD(tcon_chan);
		__raw_writel(tcon, our_chip->base + REG_TCON);
	}

	channel->running = 1;
	our_chip->enable_cnt++;

	spin_unlock_irqrestore(&samsung_pwm_lock, flags);
	return 0;
}

static void pwm_samsung_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct samsung_pwm_chip *our_chip = to_samsung_pwm_chip(chip);
	unsigned int tcon_chan = to_tcon_channel(pwm->hwpwm);
	struct samsung_pwm_channel *channel = pwm_get_chip_data(pwm);
	unsigned long flags;
	u32 tcon;

	spin_lock_irqsave(&samsung_pwm_lock, flags);

	tcon = readl(our_chip->base + REG_TCON);
	tcon &= ~TCON_AUTORELOAD(tcon_chan);
	writel(tcon, our_chip->base + REG_TCON);

	channel->running = 0;
	our_chip->enable_cnt--;
	if (!our_chip->enable_cnt && !our_chip->need_hw_init)
		pwm_samsung_clk_disable(our_chip);

	spin_unlock_irqrestore(&samsung_pwm_lock, flags);
}

static int pwm_samsung_config(struct pwm_chip *chip, struct pwm_device *pwm,
			      int duty_ns, int period_ns)
{
	struct samsung_pwm_chip *our_chip = to_samsung_pwm_chip(chip);
	unsigned int tcon_chan = to_tcon_channel(pwm->hwpwm);
	struct samsung_pwm_channel *chan = pwm_get_chip_data(pwm);
	enum duty_cycle duty_cycle;
	u32 tin_ns = chan->tin_ns, tcnt, tcmp, tcon;
	unsigned long flags;
	int ret = 0;

	/*
	 * We currently avoid using 64bit arithmetic by using the
	 * fact that anything faster than 1Hz is easily representable
	 * by 32bits.
	 */
	if (period_ns > NSEC_PER_SEC)
		return -ERANGE;

	if (duty_ns > period_ns)
		return -EINVAL;

	if (period_ns == chan->period_ns && duty_ns == chan->duty_ns)
		return 0;

	pwm_samsung_clk_enable(our_chip);

	dev_dbg(our_chip->chip.dev, "base_clk at %lu\n",
			clk_get_rate(our_chip->base_clk));
	if (our_chip->sclk_ctrl)
		dev_dbg(our_chip->chip.dev, "sclk at %lu\n",
				clk_get_rate(our_chip->base_clk));

	/* Check to see if we are changing the clock rate of the PWM. */
	if (chan->period_ns != period_ns) {
		unsigned long tin_rate;
		u32 period;

		period = NSEC_PER_SEC / period_ns;
		dev_dbg(our_chip->chip.dev, "duty_ns=%d, period_ns=%d (%u)\n",
						duty_ns, period_ns, period);

		tin_rate = pwm_samsung_calc_tin(our_chip, pwm->hwpwm, period);
		if(!tin_rate) {
			ret = -EINVAL;
			goto out;
		}

		tin_ns = NSEC_PER_SEC / tin_rate;
	}

	/* Note that counters count down. */
	tcnt = DIV_ROUND_CLOSEST(period_ns, tin_ns);
	tcmp = DIV_ROUND_CLOSEST(duty_ns, tin_ns);

	/* Period is too short. */
	if (tcnt <= 1) {
		ret = -ERANGE;
		goto out;
	}

	if (tcmp == 0)
		duty_cycle = DUTY_CYCLE_ZERO;
	else if (tcmp == tcnt)
		duty_cycle = DUTY_CYCLE_FULL;
	else
		duty_cycle = DUTY_CYCLE_PULSE;

	tcmp = tcnt - tcmp;
	/* the pwm hw only checks the compare register after a decrement,
	   so the pin never toggles if tcmp = tcnt */
	if (tcmp == tcnt)
		tcmp--;

	/* PWM counts 1 hidden tick at the end of each period on S3C64XX and
	 * EXYNOS series, so tcmp and tcnt should be subtracted 1.
	 */
	/* Decrement to get tick numbers, instead of tick counts. */
	--tcnt;
	/* -1UL will give 100% duty. */
	--tcmp;

	dev_dbg(our_chip->chip.dev,
				"tin_ns=%u, tcmp=%u/%u\n", tin_ns, tcmp, tcnt);

	/* Update PWM registers. */
	spin_lock_irqsave(&samsung_pwm_lock, flags);
	if (our_chip->need_hw_init)
		pwm_samsung_restore(our_chip);

	writel(tcnt, our_chip->base + REG_TCNTB(pwm->hwpwm));
	writel(tcmp, our_chip->base + REG_TCMPB(pwm->hwpwm));

	tcon = __raw_readl(our_chip->base + REG_TCON);
	if (chan->running == 1 && tcon & TCON_START(tcon_chan) &&
	    chan->duty_cycle != duty_cycle) {
		if (duty_cycle == DUTY_CYCLE_ZERO) {
			tcon |= TCON_MANUALUPDATE(tcon_chan);
			__raw_writel(tcon, our_chip->base + REG_TCON);
			tcon &= ~TCON_MANUALUPDATE(tcon_chan);
			tcon &= ~TCON_AUTORELOAD(tcon_chan);
		} else {
			tcon |= TCON_AUTORELOAD(tcon_chan);
		}
		__raw_writel(tcon, our_chip->base + REG_TCON);
	}

	chan->period_ns = period_ns;
	chan->tin_ns = tin_ns;
	chan->duty_ns = duty_ns;
	chan->duty_cycle = duty_cycle;

	spin_unlock_irqrestore(&samsung_pwm_lock, flags);
out:
	pwm_samsung_clk_disable(our_chip);

	return ret;
}

static void pwm_samsung_set_invert(struct samsung_pwm_chip *chip,
				   unsigned int channel, bool invert)
{
	unsigned int tcon_chan = to_tcon_channel(channel);
	unsigned long flags;
	u32 tcon;

	spin_lock_irqsave(&samsung_pwm_lock, flags);
	if (!chip->enable_cnt)
		pwm_samsung_clk_enable(chip);

	tcon = readl(chip->base + REG_TCON);
	if (invert) {
		chip->inverter_mask |= BIT(channel);
		tcon |= TCON_INVERT(tcon_chan);
	} else {
		chip->inverter_mask &= ~BIT(channel);
		tcon &= ~TCON_INVERT(tcon_chan);
	}
	writel(tcon, chip->base + REG_TCON);

	if (!chip->enable_cnt)
		pwm_samsung_clk_disable(chip);

	spin_unlock_irqrestore(&samsung_pwm_lock, flags);
}

static int pwm_samsung_set_polarity(struct pwm_chip *chip,
				    struct pwm_device *pwm,
				    enum pwm_polarity polarity)
{
	struct samsung_pwm_chip *our_chip = to_samsung_pwm_chip(chip);
	bool invert = (polarity == PWM_POLARITY_NORMAL);

	/* Inverted means normal in the hardware. */
	pwm_samsung_set_invert(our_chip, pwm->hwpwm, invert);

	return 0;
}

static const struct pwm_ops pwm_samsung_ops = {
	.request	= pwm_samsung_request,
	.free		= pwm_samsung_free,
	.enable		= pwm_samsung_enable,
	.disable	= pwm_samsung_disable,
	.config		= pwm_samsung_config,
	.set_polarity	= pwm_samsung_set_polarity,
	.owner		= THIS_MODULE,
};

#ifdef CONFIG_OF
static const struct samsung_pwm_variant s3c24xx_variant = {
	.bits		= 16,
	.div_base	= 1,
	.has_tint_cstat	= false,
	.tclk_mask	= BIT(4),
};

static const struct samsung_pwm_variant s3c64xx_variant = {
	.bits		= 16,
	.div_base	= 1,
	.has_tint_cstat	= true,
	.tclk_mask	= BIT(7) | BIT(6) | BIT(5),
};

static const struct samsung_pwm_variant s5p64x0_variant = {
	.bits		= 32,
	.div_base	= 0,
	.has_tint_cstat	= true,
	.tclk_mask	= 0,
};

static const struct samsung_pwm_variant s5pc100_variant = {
	.bits		= 32,
	.div_base	= 0,
	.has_tint_cstat	= true,
	.tclk_mask	= BIT(5),
};

static const struct of_device_id samsung_pwm_matches[] = {
	{ .compatible = "samsung,s3c2410-pwm", .data = &s3c24xx_variant },
	{ .compatible = "samsung,s3c6400-pwm", .data = &s3c64xx_variant },
	{ .compatible = "samsung,s5p6440-pwm", .data = &s5p64x0_variant },
	{ .compatible = "samsung,s5pc100-pwm", .data = &s5pc100_variant },
	{ .compatible = "samsung,exynos4210-pwm", .data = &s5p64x0_variant },
	{},
};

static int pwm_samsung_parse_dt(struct samsung_pwm_chip *chip)
{
	struct device_node *np = chip->chip.dev->of_node;
	const struct of_device_id *match;
	struct property *prop;
	const __be32 *cur;
	u32 val;

	match = of_match_node(samsung_pwm_matches, np);
	if (!match)
		return -ENODEV;

	memcpy(&chip->variant, match->data, sizeof(chip->variant));

	of_property_for_each_u32(np, "samsung,pwm-outputs", prop, cur, val) {
		if (val >= SAMSUNG_PWM_NUM) {
			dev_err(chip->chip.dev,
				"%s: invalid channel index in samsung,pwm-outputs property\n",
								__func__);
			continue;
		}
		chip->variant.output_mask |= BIT(val);
	}

	if (of_find_property(np, "samsung,pwm-sclk-ctrl", NULL))
		chip->sclk_ctrl = true;
	else
		chip->sclk_ctrl = false;

	return 0;
}
#else
static int pwm_samsung_parse_dt(struct samsung_pwm_chip *chip)
{
	return -ENODEV;
}
#endif

#ifdef CONFIG_CPU_IDLE
static int pwm_samsung_notifier(struct notifier_block *nb,
				unsigned long cmd, void *v)
{
	switch (cmd) {
	case LPA_ENTER:
		if (!g_samsung_pwm->need_hw_init && !g_samsung_pwm->enable_cnt)
			pwm_samsung_save(g_samsung_pwm);
		break;
	case LPA_EXIT:
		if (!g_samsung_pwm->need_hw_init && !g_samsung_pwm->enable_cnt)
			g_samsung_pwm->need_hw_init = 1;
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block pwm_samsung_notifier_block = {
	.notifier_call = pwm_samsung_notifier,
};
#endif

static int pwm_samsung_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct samsung_pwm_chip *chip;
	struct resource *res;
	int ret;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;

	chip->chip.dev = &pdev->dev;
	chip->chip.ops = &pwm_samsung_ops;
	chip->chip.base = -1;
	chip->chip.npwm = SAMSUNG_PWM_NUM;
	chip->inverter_mask = BIT(SAMSUNG_PWM_NUM) - 1;
	chip->idle_ip_index = exynos_get_idle_ip_index(dev_name(&pdev->dev));
	g_samsung_pwm = chip;

	if (IS_ENABLED(CONFIG_OF) && pdev->dev.of_node) {
		ret = pwm_samsung_parse_dt(chip);
		if (ret)
			return ret;

		chip->chip.of_xlate = of_pwm_xlate_with_flags;
		chip->chip.of_pwm_n_cells = 3;
	} else {
		if (!pdev->dev.platform_data) {
			dev_err(&pdev->dev, "no platform data specified\n");
			return -EINVAL;
		}

		memcpy(&chip->variant, pdev->dev.platform_data,
							sizeof(chip->variant));
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	chip->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(chip->base))
		return PTR_ERR(chip->base);

	chip->base_clk = devm_clk_get(&pdev->dev, "gate_timers");
	if (IS_ERR(chip->base_clk)) {
		dev_err(dev, "failed to get timer base clk\n");
		return PTR_ERR(chip->base_clk);
	}

	if (chip->sclk_ctrl) {
		chip->sclk = devm_clk_get(&pdev->dev, "sclk_pwm");
		if (IS_ERR(chip->sclk)) {
			dev_err(dev, "failed to get timer sclk-pwm\n");
			return PTR_ERR(chip->sclk);
		}
	}

	exynos_update_ip_idle_status(chip->idle_ip_index, 0);
	ret = clk_prepare_enable(chip->base_clk);
	if (ret)
		goto base_clk_err;

	if (chip->sclk_ctrl) {
		ret = clk_prepare_enable(chip->sclk);
		if (ret)
			goto sclk_err;
	}

	dev_info(dev, "base_clk at %lu", clk_get_rate(chip->base_clk));
	if (chip->sclk_ctrl)
		dev_info(dev, "sclk at %lu", clk_get_rate(chip->sclk));

	platform_set_drvdata(pdev, chip);
	ret = pwmchip_add(&chip->chip);
	if (ret < 0) {
		dev_err(dev, "failed to register PWM chip\n");
		goto chip_add_err;
	}

	pwm_samsung_clk_disable(chip);
#ifdef CONFIG_CPU_IDLE
	exynos_pm_register_notifier(&pwm_samsung_notifier_block);
#endif
	return 0;
chip_add_err:
	if (chip->sclk_ctrl)
		clk_disable_unprepare(chip->sclk);
sclk_err:
	clk_disable_unprepare(chip->base_clk);
base_clk_err:
	exynos_update_ip_idle_status(chip->idle_ip_index, 1);

	return ret;
}

static int pwm_samsung_remove(struct platform_device *pdev)
{
	struct samsung_pwm_chip *chip = platform_get_drvdata(pdev);
	int ret;

	ret = pwmchip_remove(&chip->chip);
	if (ret < 0)
		return ret;

	if (chip->sclk_ctrl)
		clk_disable_unprepare(chip->sclk);

	clk_disable_unprepare(chip->base_clk);

	return 0;
}

static void pwm_samsung_save(struct samsung_pwm_chip *chip)
{
	u32 tcon;
	unsigned int i;

	if (!chip->enable_cnt && !chip->need_hw_init)
		pwm_samsung_clk_enable(chip);

	for (i = 0; i < SAMSUNG_PWM_NUM; ++i) {
		struct pwm_device *pwm = &chip->chip.pwms[i];
		struct samsung_pwm_channel *chan = pwm_get_chip_data(pwm);
		unsigned int tcon_chan = to_tcon_channel(pwm->hwpwm);

		if (!chan)
			continue;

		if (chan->running == 0) {
			tcon = __raw_readl(chip->base + REG_TCON);
			if (chan->duty_cycle == DUTY_CYCLE_ZERO) {
				tcon |= TCON_MANUALUPDATE(tcon_chan);
			} else if (chan->duty_cycle == DUTY_CYCLE_FULL) {
				tcon &= TCON_INVERT(tcon_chan);
				tcon |= TCON_MANUALUPDATE(tcon_chan);
			}
			tcon &= ~TCON_START(tcon_chan);
			__raw_writel(tcon, chip->base + REG_TCON);
		}

		chan->period_ns = -1;
		chan->duty_ns = -1;
	}
	/* Save pwm registers*/
	if (chip->enable_cnt)
		chip->reg_tcfg0 = __raw_readl(chip->base + REG_TCFG0);

	pwm_samsung_clk_disable(chip);
}

static void pwm_samsung_restore(struct samsung_pwm_chip *chip)
{
	unsigned int chan;

	pwm_samsung_clk_enable(chip);

	chip->need_hw_init = 0;
	/* Restore pwm registers*/
	if (chip->enable_cnt)
		__raw_writel(chip->reg_tcfg0, chip->base + REG_TCFG0);

	for (chan = 0; chan < SAMSUNG_PWM_NUM; ++chan) {
		if (chip->variant.output_mask & BIT(chan)) {
			struct pwm_device *pwm = &chip->chip.pwms[chan];

			if (pwm == NULL)
				continue;
			pwm_samsung_init(chip, pwm);
		}
	}
}

#ifdef CONFIG_PM_SLEEP
static int pwm_samsung_suspend(struct device *dev)
{
	struct samsung_pwm_chip *chip = dev_get_drvdata(dev);

	if(!chip->need_hw_init)
		pwm_samsung_save(chip);

	return 0;
}

static int pwm_samsung_resume(struct device *dev)
{
	struct samsung_pwm_chip *chip = dev_get_drvdata(dev);

	pwm_samsung_restore(chip);

	if (!chip->enable_cnt)
		pwm_samsung_clk_disable(chip);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(pwm_samsung_pm_ops, pwm_samsung_suspend,
			 pwm_samsung_resume);

static struct platform_driver pwm_samsung_driver = {
	.driver		= {
		.name	= "samsung-pwm",
		.owner	= THIS_MODULE,
		.pm	= &pwm_samsung_pm_ops,
		.of_match_table = of_match_ptr(samsung_pwm_matches),
	},
	.probe		= pwm_samsung_probe,
	.remove		= pwm_samsung_remove,
};
module_platform_driver(pwm_samsung_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tomasz Figa <tomasz.figa@gmail.com>");
MODULE_ALIAS("platform:samsung-pwm");
