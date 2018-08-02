/* drivers/rtc/exynos_persistent_clock.c
 *
 * Based on rtc-s3c.c, which is:
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Copyright (c) 2004,2006 Simtec Electronics
 *	Ben Dooks, <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/of.h>

#include <asm/io.h>
#include <asm/time.h>
#include "rtc-s3c.h"

static struct clk *rtc_clk;
static void __iomem *exynos_rtc_base;

static void exynos_persistent_clock_read(struct rtc_time *rtc_tm)
{
	unsigned int have_retried = 0;
	void __iomem *base = exynos_rtc_base;
	int ret;

	ret = clk_enable(rtc_clk);
	if (ret < 0) {
		pr_err("%s: fail to enable rtc_clk (%d)\n", __func__, ret);
		return;
	}
retry_get_time:
	rtc_tm->tm_min  = bcd2bin(readb(base + S3C2410_RTCMIN));
	rtc_tm->tm_hour = bcd2bin(readb(base + S3C2410_RTCHOUR));
	rtc_tm->tm_mday = bcd2bin(readb(base + S3C2410_RTCDATE));
	rtc_tm->tm_mon  = bcd2bin(readb(base + S3C2410_RTCMON));
	rtc_tm->tm_year = bcd2bin(readb(base + S3C2410_RTCYEAR));
	rtc_tm->tm_sec  = bcd2bin(readb(base + S3C2410_RTCSEC));

	if (rtc_tm->tm_sec == 0 && !have_retried) {
		have_retried = 1;
		goto retry_get_time;
	}

	rtc_tm->tm_year += 100;
	rtc_tm->tm_mon -= 1;
	clk_disable(rtc_clk);
	return;
}

/**
 * read_persistent_clock() -  Return time from a persistent clock.
 * @ts:		timespec returns a relative timestamp, not a wall time
 *
 * Reads a clock that keeps relative time across suspend/resume on Exynos
 * platforms.  The clock is accurate to 1 second.
 */
void exynos_read_persistent_clock(struct timespec *ts)
{
	struct rtc_time rtc_tm;
	unsigned long time;

	if (!exynos_rtc_base) {
		ts->tv_sec = 0;
		ts->tv_nsec = 0;
		return;
	}

	exynos_persistent_clock_read(&rtc_tm);
	rtc_tm_to_time(&rtc_tm, &time);
	ts->tv_sec = time;
	ts->tv_nsec = 0;
}

static int __devexit exynos_persistent_clock_remove(struct platform_device *dev)
{
	clk_unprepare(rtc_clk);
	return 0;
}

static int __devinit exynos_persistent_clock_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct rtc_time tm;
	unsigned int tmp;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENOENT;

	exynos_rtc_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(exynos_rtc_base))
		return PTR_ERR(exynos_rtc_base);

	rtc_clk = devm_clk_get(&pdev->dev, "gate_rtc");
	if (IS_ERR(rtc_clk)) {
		int ret;
		dev_err(&pdev->dev, "failed to find rtc clock source\n");
		ret = PTR_ERR(rtc_clk);
		rtc_clk = NULL;
		return ret;
	}

	clk_prepare_enable(rtc_clk);

	/*
	 * initializing rtc with valid time values because of
	 * undefined reset values
	 */
	tm.tm_year	= 100;
	tm.tm_mon	= 1;
	tm.tm_mday	= 1;
	tm.tm_hour	= 0;
	tm.tm_min	= 0;
	tm.tm_sec	= 0;

	/* enable rtc before writing bcd values */
	tmp = readw(exynos_rtc_base + S3C2410_RTCCON);
	writew(tmp | S3C2410_RTCCON_RTCEN,
		exynos_rtc_base + S3C2410_RTCCON);
	tmp = readw(exynos_rtc_base + S3C2410_RTCCON);
	writew(tmp & ~S3C2410_RTCCON_CNTSEL,
		exynos_rtc_base + S3C2410_RTCCON);
	tmp = readw(exynos_rtc_base + S3C2410_RTCCON);
	writew(tmp & ~S3C2410_RTCCON_CLKRST,
		exynos_rtc_base + S3C2410_RTCCON);

	writeb(bin2bcd(tm.tm_sec), exynos_rtc_base + S3C2410_RTCSEC);
	writeb(bin2bcd(tm.tm_min), exynos_rtc_base + S3C2410_RTCMIN);
	writeb(bin2bcd(tm.tm_hour), exynos_rtc_base + S3C2410_RTCHOUR);
	writeb(bin2bcd(tm.tm_mday), exynos_rtc_base + S3C2410_RTCDATE);
	writeb(bin2bcd(tm.tm_mon), exynos_rtc_base + S3C2410_RTCMON);
	writeb(bin2bcd(tm.tm_year - 100), exynos_rtc_base + S3C2410_RTCYEAR);

	pr_debug("%s: set time %04d.%02d.%02d %02d:%02d:%02d\n", __func__,
			1900 + tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour,
			tm.tm_min, tm.tm_sec);

	/* checking rtc time values are valid or not */
	exynos_persistent_clock_read(&tm);
	BUG_ON(rtc_valid_tm(&tm)) ;

	tmp = readw(exynos_rtc_base + S3C2410_RTCCON);
	tmp &= ~S3C2410_RTCCON_RTCEN;
	writew(tmp, exynos_rtc_base + S3C2410_RTCCON);
	clk_disable(rtc_clk);

	register_persistent_clock(NULL, exynos_read_persistent_clock);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos_persistent_clock_dt_match[] = {
	{
		.compatible = "samsung,exynos_persistent_clock",
		.data = NULL,
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_persistent_clock_dt_match);
#else
static const struct of_device_id exynos_persistent_clock_dt_match[] = {};
#endif

static struct platform_driver exynos_persistent_clock_driver = {
	.probe		= exynos_persistent_clock_probe,
	.remove		= __devexit_p(exynos_persistent_clock_remove),
	.driver		= {
		.name	= "exynos_persistent_clock",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(exynos_persistent_clock_dt_match),
	},
};

module_platform_driver(exynos_persistent_clock_driver);

MODULE_DESCRIPTION("Samsung EXYNOS RTC persistent clock only driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:persistent_clock");
