#
# Makefile for the sensors drivers.
#

# Each configuration option enables a list of files.

ccflags-y := $(KBUILD_FP_SENSOR_CFLAGS)

obj-$(CONFIG_SENSORS_FINGERPRINT_SYSFS)		+= fingerprint_sysfs.o
obj-$(CONFIG_SENSORS_VFS7XXX)		+= vfs7xxx.o
obj-$(CONFIG_SENSORS_ET320)			+= et320-spi.o et320-spi_data_transfer.o
