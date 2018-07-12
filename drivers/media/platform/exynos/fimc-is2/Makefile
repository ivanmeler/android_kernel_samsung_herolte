fimc-is-objs	:= fimc-is-core.o \
		fimc-is-spi.o \
		fimc-is-i2c.o \
		fimc-is-debug.o \
		fimc-is-mem.o \
		fimc-is-pipe.o \
		fimc-is-framemgr.o \
		fimc-is-groupmgr.o \
		fimc-is-resourcemgr.o \
		fimc-is-video.o \
		fimc-is-video-sensor.o \
		fimc-is-video-3aa.o \
		fimc-is-video-3aac.o \
		fimc-is-video-3aap.o \
		fimc-is-video-isp.o \
		fimc-is-video-ispc.o \
		fimc-is-video-ispp.o \
		fimc-is-video-dis.o \
		fimc-is-video-scc.o \
		fimc-is-video-scp.o \
		fimc-is-video-mcs.o \
		fimc-is-video-mcsp.o \
		fimc-is-video-vra.o \
		fimc-is-video-ssvc0.o \
		fimc-is-video-ssvc1.o \
		fimc-is-video-ssvc2.o \
		fimc-is-video-ssvc3.o \
		fimc-is-subdev-ctrl.o \
		fimc-is-device-flite.o \
		fimc-is-device-sensor.o \
		fimc-is-device-ischain.o \
		fimc-is-time.o \
		fimc-is-dvfs.o \
		fimc-is-dt.o \
		fimc-is-clk-gate.o \
		fimc-is-binary.o

obj-$(CONFIG_COMPANION_USE) += fimc-is-device-preprocessor.o \
			       fimc-is-video-preprocessor.o

ifeq ($(CONFIG_USE_DIRECT_IS_CONTROL),y)
fimc-is-objs += fimc-is-interface-wrap.o
else
fimc-is-objs += fimc-is-interface-wrap-fw.o
fimc-is-objs += fimc-is-interface.o
endif

obj-$(CONFIG_EXYNOS_DEVICE_MIPI_CSIS) += fimc-is-device-csi.o
obj-$(CONFIG_EXYNOS_DEVICE_MIPI_CSIS_VER2) += fimc-is-device-csi_v2.o

obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS2) += fimc-is.o
obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS2) += sensor/
obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS2) += ischain/
obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS2) += vendor/
obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS2) += hardware/
obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS2) += interface/
obj-$(CONFIG_USE_DIRECT_IS_CONTROL) += hardware/
obj-$(CONFIG_USE_DIRECT_IS_CONTROL) += interface/

subdir-ccflags-y := -Idrivers/media/platform/exynos/fimc-is2
subdir-ccflags-y += -Idrivers/media/platform/exynos/fimc-is2/include
subdir-ccflags-y += -Idrivers/media/platform/exynos/fimc-is2/interface
subdir-ccflags-y += -Idrivers/media/platform/exynos/fimc-is2/hardware
subdir-ccflags-y += -Idrivers/media/platform/exynos/fimc-is2/hardware/api
subdir-ccflags-y += -Idrivers/media/platform/exynos/fimc-is2/vendor
subdir-ccflags-y += -Idrivers/media/platform/exynos/fimc-is2/sensor/csi
subdir-ccflags-$(CONFIG_FIMC_IS_V3_1_1) += -Idrivers/media/platform/exynos/fimc-is2/ischain/fimc-is-v3_1_1
subdir-ccflags-$(CONFIG_FIMC_IS_V3_11_0) += -Idrivers/media/platform/exynos/fimc-is2/ischain/fimc-is-v3_11_0
subdir-ccflags-$(CONFIG_FIMC_IS_V4_0_0) += -Idrivers/media/platform/exynos/fimc-is2/ischain/fimc-is-v4_0_0
subdir-ccflags-$(CONFIG_FIMC_IS_V4_3_0) += -Idrivers/media/platform/exynos/fimc-is2/ischain/fimc-is-v4_3_0
ifeq ($(CONFIG_VENDER_MCD),y)
subdir-ccflags-y += -Idrivers/media/platform/exynos/fimc-is2/vendor/mcd
else
subdir-ccflags-y += -Idrivers/media/platform/exynos/fimc-is2/vendor/default
endif