obj-y += fimc-is-helper-i2c.o
obj-$(CONFIG_EXYNOS_MIPI_CSIS) += module/
obj-$(CONFIG_EXYNOS_DEVICE_MIPI_CSIS) += csi/
obj-$(CONFIG_EXYNOS_DEVICE_MIPI_CSIS_VER2) += csi2/
obj-$(CONFIG_EXYNOS_FIMC_BNS) += flite/

obj-$(CONFIG_USE_DIRECT_IS_CONTROL) += module_framework/ \
					module_framework/modules/ \
					module_framework/cis/ \
					module_framework/actuator/ \
					module_framework/flash/

obj-$(CONFIG_CAMERA_FPGA_DPHY) += module_framework/dphy/

EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/include
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/vendor
