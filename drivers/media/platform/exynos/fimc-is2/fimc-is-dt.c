/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#include <media/exynos_mc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#include <exynos-fimc-is-module.h>
#include <exynos-fimc-is-sensor.h>
#include <exynos-fimc-is.h>
#include "fimc-is-config.h"
#include "fimc-is-dt.h"
#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"

#ifdef CONFIG_OF
static int get_pin_lookup_state(struct pinctrl *pinctrl,
	struct exynos_sensor_pin (*pin_ctrls)[GPIO_SCENARIO_MAX][GPIO_CTRL_MAX])
{
	int ret = 0;
	u32 i, j, k;
	char pin_name[30];
	struct pinctrl_state *s;

	for (i = 0; i < SENSOR_SCENARIO_MAX; ++i) {
		for (j = 0; j < GPIO_SCENARIO_MAX; ++j) {
			for (k = 0; k < GPIO_CTRL_MAX; ++k) {
				if (pin_ctrls[i][j][k].act == PIN_FUNCTION) {
					snprintf(pin_name, sizeof(pin_name), "%s%d",
						pin_ctrls[i][j][k].name,
						pin_ctrls[i][j][k].value);
					s = pinctrl_lookup_state(pinctrl, pin_name);
					if (IS_ERR_OR_NULL(s)) {
						err("pinctrl_lookup_state(%s) is failed", pin_name);
						ret = -EINVAL;
						goto p_err;
					}

					pin_ctrls[i][j][k].pin = (ulong)s;
				}
			}
		}
	}

p_err:
	return ret;
}

static int parse_gate_info(struct exynos_platform_fimc_is *pdata, struct device_node *np)
{
	int ret = 0;
	struct device_node *group_np = NULL;
	struct device_node *gate_info_np;
	struct property *prop;
	struct property *prop2;
	const __be32 *p;
	const char *s;
	u32 i = 0, u = 0;
	struct exynos_fimc_is_clk_gate_info *gate_info;

	/* get subip of fimc-is info */
	gate_info = kzalloc(sizeof(struct exynos_fimc_is_clk_gate_info), GFP_KERNEL);
	if (!gate_info) {
		printk(KERN_ERR "%s: no memory for fimc_is gate_info\n", __func__);
		return -EINVAL;
	}

	s = NULL;
	/* get gate register info */
	prop2 = of_find_property(np, "clk_gate_strs", NULL);
	of_property_for_each_u32(np, "clk_gate_enums", prop, p, u) {
		printk(KERN_INFO "int value: %d\n", u);
		s = of_prop_next_string(prop2, s);
		if (s != NULL) {
			printk(KERN_INFO "String value: %d-%s\n", u, s);
			gate_info->gate_str[u] = s;
		}
	}

	/* gate info */
	gate_info_np = of_find_node_by_name(np, "clk_gate_ctrl");
	if (!gate_info_np) {
		printk(KERN_ERR "%s: can't find fimc_is clk_gate_ctrl node\n", __func__);
		ret = -ENOENT;
		goto p_err;
	}
	i = 0;
	while ((group_np = of_get_next_child(gate_info_np, group_np))) {
		struct exynos_fimc_is_clk_gate_group *group =
				&gate_info->groups[i];
		of_property_for_each_u32(group_np, "mask_clk_on_org", prop, p, u) {
			printk(KERN_INFO "(%d) int1 value: %d\n", i, u);
			group->mask_clk_on_org |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_clk_off_self_org", prop, p, u) {
			printk(KERN_INFO "(%d) int2 value: %d\n", i, u);
			group->mask_clk_off_self_org |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_clk_off_depend", prop, p, u) {
			printk(KERN_INFO "(%d) int3 value: %d\n", i, u);
			group->mask_clk_off_depend |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_cond_for_depend", prop, p, u) {
			printk(KERN_INFO "(%d) int4 value: %d\n", i, u);
			group->mask_cond_for_depend |= (1 << u);
		}
		i++;
		printk(KERN_INFO "(%d) [0x%x , 0x%x, 0x%x, 0x%x\n", i,
			group->mask_clk_on_org,
			group->mask_clk_off_self_org,
			group->mask_clk_off_depend,
			group->mask_cond_for_depend
		);
	}

	pdata->gate_info = gate_info;
	pdata->gate_info->clk_on_off = exynos_fimc_is_clk_gate;

	return 0;
p_err:
	kfree(gate_info);
	return ret;
}

#if defined(CONFIG_PM_DEVFREQ)
DECLARE_EXTERN_DVFS_DT(FIMC_IS_SN_END);
static int parse_dvfs_data(struct exynos_platform_fimc_is *pdata, struct device_node *np, int index)
{
	int i;
	u32 temp;
	char *pprop;
	char buf[64];

	for (i = 0; i < FIMC_IS_SN_END; i++) {
		sprintf(buf, "%s%s", fimc_is_dvfs_dt_arr[i].parse_scenario_nm, "int");
		DT_READ_U32(np, buf, pdata->dvfs_data[index][fimc_is_dvfs_dt_arr[i].scenario_id][FIMC_IS_DVFS_INT]);

		sprintf(buf, "%s%s", fimc_is_dvfs_dt_arr[i].parse_scenario_nm, "cam");
		DT_READ_U32(np, buf, pdata->dvfs_data[index][fimc_is_dvfs_dt_arr[i].scenario_id][FIMC_IS_DVFS_CAM]);

		sprintf(buf, "%s%s", fimc_is_dvfs_dt_arr[i].parse_scenario_nm, "mif");
		DT_READ_U32(np, buf, pdata->dvfs_data[index][fimc_is_dvfs_dt_arr[i].scenario_id][FIMC_IS_DVFS_MIF]);

		sprintf(buf, "%s%s", fimc_is_dvfs_dt_arr[i].parse_scenario_nm, "i2c");
		DT_READ_U32(np, buf, pdata->dvfs_data[index][fimc_is_dvfs_dt_arr[i].scenario_id][FIMC_IS_DVFS_I2C]);

		sprintf(buf, "%s%s", fimc_is_dvfs_dt_arr[i].parse_scenario_nm, "hpg");
		DT_READ_U32(np, buf, pdata->dvfs_data[index][fimc_is_dvfs_dt_arr[i].scenario_id][FIMC_IS_DVFS_HPG]);
	}

#ifdef DBG_DUMP_DVFS_DT
	for (i = 0; i < FIMC_IS_SN_END; i++) {
		probe_info("---- %s ----", fimc_is_dvfs_dt_arr[i].parse_scenario_nm);
		probe_info("[%d][%d][INT] = %d", index, i, pdata->dvfs_data[index][i][FIMC_IS_DVFS_INT]);
		probe_info("[%d][%d][CAM] = %d", index, i, pdata->dvfs_data[index][i][FIMC_IS_DVFS_CAM]);
		probe_info("[%d][%d][MIF] = %d", index, i, pdata->dvfs_data[index][i][FIMC_IS_DVFS_MIF]);
		probe_info("[%d][%d][I2C] = %d", index, i, pdata->dvfs_data[index][i][FIMC_IS_DVFS_I2C]);
		probe_info("[%d][%d][HPG] = %d", index, i, pdata->dvfs_data[index][i][FIMC_IS_DVFS_HPG]);
	}
#endif
	return 0;
}
#else
static int parse_dvfs_data(struct exynos_platform_fimc_is *pdata, struct device_node *np, int index)
{
	return 0;
}
#endif

static int parse_dvfs_table(struct fimc_is_dvfs_ctrl *dvfs,
	struct exynos_platform_fimc_is *pdata, struct device_node *np)
{
	int ret = 0;
	u32 table_cnt;
	struct device_node *table_np;
	const char *dvfs_table_desc;

	table_np = NULL;

	table_cnt = 0;
	while ((table_np = of_get_next_child(np, table_np)) &&
		(table_cnt < FIMC_IS_DVFS_TABLE_IDX_MAX)) {
		ret = of_property_read_string(table_np, "desc", &dvfs_table_desc);
		if (ret)
			dvfs_table_desc = "NOT defined";

		probe_info("dvfs table[%d] is %s", table_cnt, dvfs_table_desc);
		parse_dvfs_data(pdata, table_np, table_cnt);
		table_cnt++;
	}

	dvfs->dvfs_table_max = table_cnt;

	return ret;
}

int fimc_is_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_dvfs_ctrl *dvfs;
	struct exynos_platform_fimc_is *pdata;
	struct device *dev;
	struct device_node *dvfs_np = NULL;
	struct device_node *vender_np = NULL;
	struct device_node *np;

	BUG_ON(!pdev);

	dev = &pdev->dev;
	np = dev->of_node;

	core = dev_get_drvdata(&pdev->dev);
	if (!core) {
		probe_err("core is NULL");
		return -ENOMEM;
	}

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is), GFP_KERNEL);
	if (!pdata) {
		probe_err("no memory for platform data");
		return -ENOMEM;
	}

	dvfs = &core->resourcemgr.dvfs_ctrl;
	pdata->clk_get = exynos_fimc_is_clk_get;
	pdata->clk_cfg = exynos_fimc_is_clk_cfg;
	pdata->clk_on = exynos_fimc_is_clk_on;
	pdata->clk_off = exynos_fimc_is_clk_off;
	pdata->print_clk = exynos_fimc_is_print_clk;

	if (parse_gate_info(pdata, np) < 0)
		probe_err("can't parse clock gate info node");

	vender_np = of_find_node_by_name(np, "vender");
	if (vender_np) {
		ret = fimc_is_vender_dt(vender_np);
		if (ret)
			probe_err("fimc_is_vender_dt is fail(%d)", ret);
	}

	pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->pinctrl)) {
		probe_err("devm_pinctrl_get is fail");
		goto p_err;
	}

	dvfs_np = of_find_node_by_name(np, "fimc_is_dvfs");
	if (dvfs_np) {
		ret = parse_dvfs_table(dvfs, pdata, dvfs_np);
		if (ret)
			probe_err("parse_dvfs_table is fail(%d)", ret);
	}

	dev->platform_data = pdata;

	return 0;

p_err:
	kfree(pdata);
	return ret;
}

int fimc_is_sensor_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct device_node *dnode;
	struct device *dev;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_sensor), GFP_KERNEL);
	if (!pdata) {
		err("%s: no memory for platform data", __func__);
		return -ENOMEM;
	}

	pdata->iclk_cfg = exynos_fimc_is_sensor_iclk_cfg;
	pdata->iclk_on = exynos_fimc_is_sensor_iclk_on;
	pdata->iclk_off = exynos_fimc_is_sensor_iclk_off;
	pdata->mclk_on = exynos_fimc_is_sensor_mclk_on;
	pdata->mclk_off = exynos_fimc_is_sensor_mclk_off;

	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "scenario", &pdata->scenario);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "csi_ch", &pdata->csi_ch);
	if (ret) {
		err("csi_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "flite_ch", &pdata->flite_ch);
	if (ret) {
		err("flite_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "is_bns", &pdata->is_bns);
	if (ret) {
		err("is_bns read is fail(%d)", ret);
		goto p_err;
	}

	pdev->id = pdata->id;
	dev->platform_data = pdata;

	return ret;

p_err:
	kfree(pdata);
	return ret;
}

int fimc_is_preprocessor_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_platform_fimc_is_preproc *pdata;
	struct device_node *dnode;
	struct device *dev;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_preproc), GFP_KERNEL);
	if (!pdata) {
		probe_err("%s: no memory for platform data\n", __func__);
		return -ENOMEM;
	}

	ret = of_property_read_u32(dnode, "scenario", &pdata->scenario);
	if (ret) {
		probe_err("scenario read is fail(%d)", ret);
		ret = -EINVAL;
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		probe_err("mclk_ch read is fail(%d)", ret);
		ret = -EINVAL;
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		probe_err("csi_ch read is fail(%d)", ret);
		goto p_err;
	}

	pdata->iclk_cfg = exynos_fimc_is_preproc_iclk_cfg;
	pdata->iclk_on = exynos_fimc_is_preproc_iclk_on;
	pdata->iclk_off = exynos_fimc_is_preproc_iclk_off;
	pdata->mclk_on = exynos_fimc_is_preproc_mclk_on;
	pdata->mclk_off = exynos_fimc_is_preproc_mclk_off;

	dev->platform_data = pdata;

	return ret;

p_err:
	kfree(pdata);
	return ret;
}

static int parse_af_data(struct exynos_platform_fimc_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->af_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->af_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->af_i2c_ch);

	return 0;
}

static int parse_flash_data(struct exynos_platform_fimc_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->flash_product_name);
	DT_READ_U32(dnode, "flash_first_gpio", pdata->flash_first_gpio);
	DT_READ_U32(dnode, "flash_second_gpio", pdata->flash_second_gpio);

	return 0;
}

static int parse_preprocessor_data(struct exynos_platform_fimc_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->preprocessor_product_name);
	DT_READ_U32(dnode, "spi_channel", pdata->preprocessor_spi_channel);
	DT_READ_U32(dnode, "i2c_addr", pdata->preprocessor_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->preprocessor_i2c_ch);
	DT_READ_U32_DEFAULT(dnode, "dma_ch", pdata->preprocessor_dma_channel, DMA_CH_NOT_DEFINED);

	return 0;
}

static int parse_ois_data(struct exynos_platform_fimc_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->ois_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->ois_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->ois_i2c_ch);

	return 0;
}

/* Deprecated. Use  fimc_is_module_parse_dt */ 
int fimc_is_sensor_module_parse_dt(struct platform_device *pdev,
	fimc_is_moudle_dt_callback module_callback)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *pdata;
	struct device_node *dnode;
	struct device_node *af_np;
	struct device_node *flash_np;
	struct device_node *preprocessor_np;
	struct device_node *ois_np;
	struct device *dev;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);
	BUG_ON(!module_callback);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_module), GFP_KERNEL);
	if (!pdata) {
		probe_err("%s: no memory for platform data", __func__);
		return -ENOMEM;
	}

	pdata->gpio_cfg = exynos_fimc_is_module_pins_cfg;
	pdata->gpio_dbg = exynos_fimc_is_module_pins_dbg;

	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		probe_err("id read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		probe_err("mclk_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "sensor_i2c_ch", &pdata->sensor_i2c_ch);
	if (ret) {
		probe_err("i2c_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "sensor_i2c_addr", &pdata->sensor_i2c_addr);
	if (ret) {
		probe_err("i2c_addr read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "position", &pdata->position);
	if (ret) {
		probe_err("id read is fail(%d)", ret);
		goto p_err;
	}

	af_np = of_find_node_by_name(dnode, "af");
	if (!af_np) {
		pdata->af_product_name = ACTUATOR_NAME_NOTHING;
	} else {
		parse_af_data(pdata, af_np);
	}

	flash_np = of_find_node_by_name(dnode, "flash");
	if (!flash_np) {
		pdata->flash_product_name = FLADRV_NAME_NOTHING;
	} else {
		parse_flash_data(pdata, flash_np);
	}

	preprocessor_np = of_find_node_by_name(dnode, "preprocessor");
	if (!preprocessor_np) {
		pdata->preprocessor_product_name = PREPROCESSOR_NAME_NOTHING;
	} else {
		parse_preprocessor_data(pdata, preprocessor_np);
	}

	ois_np = of_find_node_by_name(dnode, "ois");
	if (!ois_np) {
		pdata->ois_product_name = OIS_NAME_NOTHING;
	} else {
		parse_ois_data(pdata, ois_np);
	}

	ret = module_callback(pdev, pdata);
	if (ret) {
		probe_err("sensor dt callback is fail(%d)", ret);
		goto p_err;
	}

	pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->pinctrl)) {
		probe_err("devm_pinctrl_get is fail");
		goto p_err;
	}

	ret = get_pin_lookup_state(pdata->pinctrl, pdata->pin_ctrls);
	if (ret) {
		probe_err("get_pin_lookup_state is fail(%d)", ret);
		goto p_err;
	}

	dev->platform_data = pdata;

	return ret;

p_err:
	kfree(pdata);
	return ret;
}

/* New function for module parse dt. Use this instead of fimc_is_sensor_module_parse_dt */ 
int fimc_is_module_parse_dt(struct device *dev,
	fimc_is_moudle_callback module_callback)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *pdata;
	struct device_node *dnode;
	struct device_node *af_np;
	struct device_node *flash_np;
	struct device_node *preprocessor_np;
	struct device_node *ois_np;

	BUG_ON(!dev);
	BUG_ON(!dev->of_node);
	BUG_ON(!module_callback);

	dnode = dev->of_node;
	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_module), GFP_KERNEL);
	if (!pdata) {
		probe_err("%s: no memory for platform data", __func__);
		return -ENOMEM;
	}

	pdata->gpio_cfg = exynos_fimc_is_module_pins_cfg;
	pdata->gpio_dbg = exynos_fimc_is_module_pins_dbg;

	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		probe_err("id read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		probe_err("mclk_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "sensor_i2c_ch", &pdata->sensor_i2c_ch);
	if (ret) {
		probe_err("i2c_ch read is fail(%d)", ret);
	}

	ret = of_property_read_u32(dnode, "sensor_i2c_addr", &pdata->sensor_i2c_addr);
	if (ret) {
		probe_err("i2c_addr read is fail(%d)", ret);
	}

	ret = of_property_read_u32(dnode, "position", &pdata->position);
	if (ret) {
		probe_err("id read is fail(%d)", ret);
		goto p_err;
	}

	af_np = of_find_node_by_name(dnode, "af");
	if (!af_np) {
		pdata->af_product_name = ACTUATOR_NAME_NOTHING;
	} else {
		parse_af_data(pdata, af_np);
	}

	flash_np = of_find_node_by_name(dnode, "flash");
	if (!flash_np) {
		pdata->flash_product_name = FLADRV_NAME_NOTHING;
	} else {
		parse_flash_data(pdata, flash_np);
	}

	preprocessor_np = of_find_node_by_name(dnode, "preprocessor");
	if (!preprocessor_np) {
		pdata->preprocessor_product_name = PREPROCESSOR_NAME_NOTHING;
	} else {
		parse_preprocessor_data(pdata, preprocessor_np);
	}

	ois_np = of_find_node_by_name(dnode, "ois");
	if (!ois_np) {
		pdata->ois_product_name = OIS_NAME_NOTHING;
	} else {
		parse_ois_data(pdata, ois_np);
	}

	ret = module_callback(dev, pdata);
	if (ret) {
		probe_err("sensor dt callback is fail(%d)", ret);
		goto p_err;
	}

	pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->pinctrl)) {
		probe_err("devm_pinctrl_get is fail");
		goto p_err;
	}

	ret = get_pin_lookup_state(pdata->pinctrl, pdata->pin_ctrls);
	if (ret) {
		probe_err("get_pin_lookup_state is fail(%d)", ret);
		goto p_err;
	}

	dev->platform_data = pdata;

	return ret;

p_err:
	kfree(pdata);
	return ret;
}

int fimc_is_spi_parse_dt(struct fimc_is_spi *spi)
{
	int ret = 0;
	struct device_node *np;
	struct device *dev;
	struct pinctrl_state *s;

	BUG_ON(!spi);

	dev = &spi->device->dev;

	np = of_find_compatible_node(NULL,NULL, spi->node);
	if(np == NULL) {
		probe_err("compatible: fail to read, spi_parse_dt");
		ret = -ENODEV;
		goto p_err;
	}

	spi->use_spi_pinctrl = of_property_read_bool(np, "use_spi_pinctrl");
	if (!spi->use_spi_pinctrl) {
		probe_info("%s: spi dt parsing skipped\n", __func__);
		goto p_err;
	}

	spi->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(spi->pinctrl)) {
		probe_err("devm_pinctrl_get is fail");
		goto p_err;
	}

	s = pinctrl_lookup_state(spi->pinctrl, "ssn_out");
	if (IS_ERR_OR_NULL(s)) {
		probe_info("pinctrl_lookup_state(%s) is not found", "ssn_out");
		spi->pin_ssn_out = NULL;
	} else {
		spi->pin_ssn_out = s;
	}

	s = pinctrl_lookup_state(spi->pinctrl, "ssn_fn");
	if (IS_ERR_OR_NULL(s)) {
		probe_info("pinctrl_lookup_state(%s) is not found", "ssn_fn");
		spi->pin_ssn_fn = NULL;
	} else {
		spi->pin_ssn_fn = s;
	}

	s = pinctrl_lookup_state(spi->pinctrl, "ssn_inpd");
	if (IS_ERR_OR_NULL(s)) {
		probe_info("pinctrl_lookup_state(%s) is not found", "ssn_inpd");
		spi->pin_ssn_inpd = NULL;
	} else {
		spi->pin_ssn_inpd = s;
	}

	s = pinctrl_lookup_state(spi->pinctrl, "ssn_inpu");
	if (IS_ERR_OR_NULL(s)) {
		probe_info("pinctrl_lookup_state(%s) is not found", "ssn_inpu");
		spi->pin_ssn_inpu = NULL;
	} else {
		spi->pin_ssn_inpu = s;
	}

	spi->parent_pinctrl = devm_pinctrl_get(spi->device->dev.parent->parent);

	s = pinctrl_lookup_state(spi->parent_pinctrl, "spi_out");
	if (IS_ERR_OR_NULL(s)) {
		err("pinctrl_lookup_state(%s) is failed", "spi_out");
		ret = -EINVAL;
		goto p_err;
	}

	spi->parent_pin_out = s;

	s = pinctrl_lookup_state(spi->parent_pinctrl, "spi_fn");
	if (IS_ERR_OR_NULL(s)) {
		err("pinctrl_lookup_state(%s) is failed", "spi_fn");
		ret = -EINVAL;
		goto p_err;
	}

	spi->parent_pin_fn = s;

p_err:
	return ret;
}
#else
struct exynos_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	return ERR_PTR(-EINVAL);
}
#endif
