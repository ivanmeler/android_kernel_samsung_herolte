/*
 * Exynos power domain support for PWRCAL2.0 interface.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <soc/samsung/pm_domains-cal.h>
#include <soc/samsung/bts.h>
#include <linux/apm-exynos.h>

static int exynos_pd_status(struct exynos_pm_domain *pd)
{
	int status;

	if (unlikely(!pd))
		return -EINVAL;

	mutex_lock(&pd->access_lock);
	status = cal_pd_status(pd->cal_pdid);
	mutex_unlock(&pd->access_lock);

	return status;
}

/* Power domain on sequence.
 * on_pre, on_post functions are registered as notification handler at CAL code.
 */
static void exynos_genpd_power_on_pre(struct exynos_pm_domain *pd)
{
	exynos_update_ip_idle_status(pd->idle_ip_index, 0);

	if (!strcmp("pd-cam0", pd->name)) {
		exynos_devfreq_sync_voltage(DEVFREQ_CAM, true);
		exynos_bts_scitoken_setting(true);
	}
}

static void exynos_genpd_power_on_post(struct exynos_pm_domain *pd)
{
}

static void exynos_genpd_power_off_pre(struct exynos_pm_domain *pd)
{
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	if (!strcmp(pd->name, "pd-g3d")) {
		exynos_g3d_power_down_noti_apm();
	}
#endif /* CONFIG_EXYNOS_CL_DVFS_G3D */
}

static void exynos_genpd_power_off_post(struct exynos_pm_domain *pd)
{
	exynos_update_ip_idle_status(pd->idle_ip_index, 1);

	if (!strcmp("pd-cam0", pd->name)) {
		exynos_devfreq_sync_voltage(DEVFREQ_CAM, false);
		exynos_bts_scitoken_setting(false);
	}
}

static void prepare_forced_off(struct exynos_pm_domain *pd)
{
}

static int exynos_genpd_power_on(struct generic_pm_domain *genpd)
{
	struct exynos_pm_domain *pd = container_of(genpd, struct exynos_pm_domain, genpd);
	int ret;

	DEBUG_PRINT_INFO("%s(%s)+\n", __func__, pd->name);
	if (unlikely(!pd->pd_control)) {
		pr_debug(PM_DOMAIN_PREFIX "%s is Logical sub power domain, dose not have to power on control\n", pd->name);
		return 0;
	}

	mutex_lock(&pd->access_lock);

	exynos_genpd_power_on_pre(pd);

	ret = pd->pd_control(pd->cal_pdid, 1);
	if (ret) {
		pr_err(PM_DOMAIN_PREFIX "%s cannot be powered on\n", pd->name);
		exynos_genpd_power_off_post(pd);

		mutex_unlock(&pd->access_lock);
		return -EAGAIN;
	}

	exynos_genpd_power_on_post(pd);

	mutex_unlock(&pd->access_lock);

	DEBUG_PRINT_INFO("%s(%s)-\n", __func__, pd->name);
	return 0;
}

static int exynos_genpd_power_off(struct generic_pm_domain *genpd)
{
	struct exynos_pm_domain *pd = container_of(genpd, struct exynos_pm_domain, genpd);
	int ret;

	DEBUG_PRINT_INFO("%s(%s)+\n", __func__, pd->name);

	if (unlikely(!pd->pd_control)) {
		pr_debug(PM_DOMAIN_PREFIX "%s is Logical sub power domain, dose not have to power off control\n", genpd->name);
		return 0;
	}

	mutex_lock(&pd->access_lock);

	exynos_genpd_power_off_pre(pd);

	ret = pd->pd_control(pd->cal_pdid, 0);
	if (unlikely(ret)) {
		if (ret == -4) {
			pr_err(PM_DOMAIN_PREFIX "Timed out during %s  power off! -> forced power off\n", genpd->name);
			prepare_forced_off(pd);
			ret = pd->pd_control(pd->cal_pdid, 0);
			if (unlikely(ret)) {
				pr_err(PM_DOMAIN_PREFIX "%s occur error at power off!\n", genpd->name);
				goto acc_unlock;
			}
		} else {
			pr_err(PM_DOMAIN_PREFIX "%s occur error at power off!\n", genpd->name);
			goto acc_unlock;
		}
	}

	exynos_genpd_power_off_post(pd);

acc_unlock:
	mutex_unlock(&pd->access_lock);
	DEBUG_PRINT_INFO("%s(%s)-\n", __func__, pd->name);
	return ret;
}

#ifdef CONFIG_OF
static void exynos_genpd_init(struct exynos_pm_domain *pd, int state)
{
	pd->genpd.name = pd->name;
	pd->genpd.power_off = exynos_genpd_power_off;
	pd->genpd.power_on = exynos_genpd_power_on;

	/* pd power on/off latency is less than 1ms */
	pd->genpd.power_on_latency_ns = 1000000;
	pd->genpd.power_off_latency_ns = 1000000;

	pm_genpd_init(&pd->genpd, NULL, state ? false : true);
}

/* show_power_domain - show current power domain status.
 *
 * read the status of power domain and show it.
 */
static void show_power_domain(void)
{
	struct device_node *np;
	for_each_compatible_node(np, NULL, "samsung,exynos-pd") {
		struct platform_device *pdev;
		struct exynos_pm_domain *pd;

		if (of_device_is_available(np)) {
			pdev = of_find_device_by_node(np);
			if (!pdev)
				continue;
			pd = platform_get_drvdata(pdev);
			pr_info("   %-9s - %-3s\n", pd->genpd.name,
					cal_pd_status(pd->cal_pdid) ? "on" : "off");
		} else
			pr_info("   %-9s - %s\n", np->name, "on,  always");
	}

	return;
}

static __init int exynos_pm_dt_parse_domains(void)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;
	int ret = 0;

	for_each_compatible_node(np, NULL, "samsung,exynos-pd") {
		struct exynos_pm_domain *pd;
		struct device_node *children;
		int initial_state;

		/* skip unmanaged power domain */
		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);

		pd = kzalloc(sizeof(*pd), GFP_KERNEL);
		if (!pd) {
			pr_err(PM_DOMAIN_PREFIX "%s: failed to allocate memory for domain\n",
					__func__);
			return -ENOMEM;
		}

		/* init exynos_pm_domain's members  */
		pd->name = kstrdup(np->name, GFP_KERNEL);
		ret = of_property_read_u32(np, "cal_id", (u32 *)&pd->cal_pdid);
		if (ret) {
			pr_err(PM_DOMAIN_PREFIX "%s: failed to get cal_pdid  from of %s\n",
					__func__, pd->name);
			return -ENODEV;
		}
		pd->base = of_iomap(np, 0);
		pd->of_node = np;
		pd->pd_control = cal_pd_control;
		pd->check_status = exynos_pd_status;
		initial_state = cal_pd_status(pd->cal_pdid);
		if (initial_state == -1) {
			pr_err(PM_DOMAIN_PREFIX "%s: %s is in unknown state\n",
					__func__, pd->name);
			return -EINVAL;
		}

		pd->idle_ip_index = exynos_get_idle_ip_index(pd->name);

		mutex_init(&pd->access_lock);
		platform_set_drvdata(pdev, pd);

		__of_genpd_add_provider(np, __of_genpd_xlate_simple, &pd->genpd);

		exynos_genpd_init(pd, initial_state);

		/* add LOGICAL sub-domain
		 * It is not assumed that there is REAL sub-domain.
		 * Power on/off functions are not defined here.
		 */
		for_each_child_of_node(np, children) {
			struct exynos_pm_domain *sub_pd;
			struct platform_device *sub_pdev;

			sub_pd = kzalloc(sizeof(*sub_pd), GFP_KERNEL);
			if (!sub_pd) {
				pr_err("%s: failed to allocate memory for power domain\n",
						__func__);
				return -ENOMEM;
			}

			sub_pd->name = kstrdup(children->name, GFP_KERNEL);
			sub_pd->of_node = children;

			/* Logical sub-domain does not have to power on/off control*/
			sub_pd->pd_control = NULL;

			/* kernel does not create sub-domain pdev. */
			sub_pdev = of_find_device_by_node(children);
			if (!sub_pdev)
				sub_pdev = of_platform_device_create(children, NULL, &pdev->dev);
			if (!sub_pdev) {
				pr_err(PM_DOMAIN_PREFIX "sub domain allocation failed: %s\n",
							kstrdup(children->name, GFP_KERNEL));
				continue;
			}

			mutex_init(&sub_pd->access_lock);
			platform_set_drvdata(sub_pdev, sub_pd);

			__of_genpd_add_provider(children, __of_genpd_xlate_simple, &sub_pd->genpd);
			exynos_genpd_init(sub_pd, initial_state);

			if (pm_genpd_add_subdomain(&pd->genpd, &sub_pd->genpd))
				pr_err("PM DOMAIN: %s can't add subdomain %s\n", pd->genpd.name, sub_pd->genpd.name);
			else
				pr_info(PM_DOMAIN_PREFIX "%s has a new logical child %s.\n",
						pd->genpd.name, sub_pd->genpd.name);
		}
	}

	/* EXCEPTION: add physical sub-pd to master pd using device tree */
	for_each_compatible_node(np, NULL, "samsung,exynos-pd") {
		struct exynos_pm_domain *parent_pd, *child_pd;
		struct device_node *parent;
		struct platform_device *parent_pd_pdev, *child_pd_pdev;
		int i;

		/* skip unmanaged power domain */
		if (!of_device_is_available(np))
			continue;

		/* child_pd_pdev should have value. */
		child_pd_pdev = of_find_device_by_node(np);
		child_pd = platform_get_drvdata(child_pd_pdev);

		/* search parents in device tree */
		for (i = 0; i < MAX_PARENT_POWER_DOMAIN; i++) {
			/* find parent node if available */
			parent = of_parse_phandle(np, "parent", i);
			if (!parent)
				break;

			/* display error when parent is unmanaged. */
			if (!of_device_is_available(parent)) {
				pr_err(PM_DOMAIN_PREFIX "%s is not managed by runtime pm.\n",
						kstrdup(parent->name, GFP_KERNEL));
				continue;
			}

			/* parent_pd_pdev should have value. */
			parent_pd_pdev = of_find_device_by_node(parent);
			parent_pd = platform_get_drvdata(parent_pd_pdev);

			if (pm_genpd_add_subdomain(&parent_pd->genpd, &child_pd->genpd))
				pr_err(PM_DOMAIN_PREFIX "%s cannot add subdomain %s\n",
						parent_pd->name, child_pd->name);
			else
				pr_info(PM_DOMAIN_PREFIX "%s has a new child %s.\n",
						parent_pd->name, child_pd->name);
		}
	}

	return 0;
}
#endif /* CONFIG_OF */

static int __init exynos_pm_domain_init(void)
{
	int ret;
#ifdef CONFIG_OF
	if (of_have_populated_dt()) {
		ret = exynos_pm_dt_parse_domains();
		if (ret) {
			pr_err(PM_DOMAIN_PREFIX "dt parse failed.\n");
			return ret;
		}

		pr_info("EXYNOS: PM Domain Initialize\n");
		/* show information of power domain registration */
		show_power_domain();

		return 0;
	}
#endif
	pr_err(PM_DOMAIN_PREFIX "PM Domain works along with Device Tree\n");
	return -EPERM;
}
subsys_initcall(exynos_pm_domain_init);
