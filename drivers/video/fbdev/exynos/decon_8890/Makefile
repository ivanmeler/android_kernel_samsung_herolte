#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

obj-$(CONFIG_EXYNOS_VPP) += vpp/
obj-$(CONFIG_EXYNOS_MIPI_DSI) += dsim.o
dsim-y += dsim_drv.o dsim_reg_8890.o
obj-$(CONFIG_EXYNOS_DECON_8890) += decon.o
decon-y += decon_core.o decon_f.o decon_s.o decon_t.o decon_dsi.o decon_helper.o decon_bts.o
obj-$(CONFIG_SOC_EXYNOS8890) += decon_reg_8890.o
obj-y += panels/
