obj-$(CONFIG_USE_DIRECT_IS_CONTROL) += fimc-is-hw-control.o \
		fimc-is-hw-3aa.o \
		fimc-is-hw-isp.o \
		fimc-is-hw-tpu.o \
		fimc-is-hw-vra.o \
		fimc-is-hw-dm.o

obj-$(CONFIG_CAMERA_FIMC_SCALER_USE) += fimc-is-hw-scp.o
obj-$(CONFIG_CAMERA_MC_SCALER_USE) += fimc-is-hw-mcscaler.o

obj-$(CONFIG_USE_DIRECT_IS_CONTROL) += api/
obj-$(CONFIG_EXYNOS_FIMC_IS_HARDWARE) += api/

EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/include
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/interface
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/hardware
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/hardware/api
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/vendor
