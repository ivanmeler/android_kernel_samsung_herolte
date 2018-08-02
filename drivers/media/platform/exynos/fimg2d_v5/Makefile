#
# Copyright (c) 2015 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

obj-$(CONFIG_VIDEO_EXYNOS_FIMG2D_LEGACY)	+= fimg2d_drv.o fimg2d_clk.o \
					fimg2d_ctx.o fimg2d_cache.o fimg2d_helper.o \
					fimg2d5x_blt.o fimg2d5x_hw.o
obj-$(CONFIG_VIDEO_EXYNOS_FIMG2D_1SHOT2)	+= g2d1shot_drv.o g2d1shot_hw5x.o g2d1shot_helper.o

ccflags-y += -DDEBUG
ccflags-y += -DBLIT_WORKQUE
ccflags-$(CONFIG_FIMG2D_CCI_SNOOP) += -DCCI_SNOOP
