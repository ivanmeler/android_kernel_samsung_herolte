#
# Makefile for trace.
#

ifeq ($(CONFIG_SOC_EXYNOS8890),y)
obj-$(CONFIG_EXYNOS_BUSMONITOR) += exynos8890-busmon.o
else
obj-$(CONFIG_EXYNOS_BUSMONITOR) += exynos-busmon.o
endif
obj-$(CONFIG_EXYNOS_SNAPSHOT) += exynos-ss.o
obj-$(CONFIG_EXYNOS_CORESIGHT) += exynos-coresight.o
obj-$(CONFIG_EXYNOS_CORESIGHT_ETM) += exynos-coresight-etm.o
obj-$(CONFIG_EXYNOS_CORESIGHT_STM) += of_coresight.o coresight.o coresight-stm.o
obj-$(CONFIG_EXYNOS_DRAMTEST) += dramtest.o
