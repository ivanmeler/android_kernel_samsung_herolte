obj-$(CONFIG_VENDER_MCD) := fimc-is-vender.o \
				fimc-is-sec-define.o \
				crc32.o \
				fimc-is-sysfs.o \
				fimc-is-device-from.o

obj-$(CONFIG_COMPANION_DCDC_USE) += fimc-is-pmic.o

obj-$(CONFIG_COMPANION_C2_USE) += fimc-is-companion_c2.o
obj-$(CONFIG_COMPANION_C3_USE) += fimc-is-companion_c3.o

obj-$(CONFIG_OIS_USE)	+= fimc-is-device-ois_common.o
obj-$(CONFIG_AF_HOST_CONTROL)	+= fimc-is-device-af.o
obj-$(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)	+= fimc-is-device-eeprom.o
obj-$(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)	+= fimc-is-device-eeprom.o
obj-$(CONFIG_OIS_USE_RUMBA_S4) += fimc-is-device-ois_s4.o
obj-$(CONFIG_OIS_USE_RUMBA_SA) += fimc-is-device-ois_sa.o

EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/include
