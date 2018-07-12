#
# Makefile for the sensor drivers.
#

# Each configuration option enables a list of files.
obj-$(CONFIG_SENSORS_SSP)		+= ssp_dev.o ssp_i2c.o ssp_data.o ssp_sysfs.o\
						ssp_input.o ssp_firmware.o ssp_debug.o
obj-$(CONFIG_SENSORS_SYSFS)		+= sensors_core.o
obj-$(CONFIG_IIO)			+= ssp_iio_ring.o ssp_iio_trigger.o
obj-$(CONFIG_SENSORS_SSP_AK8963C)	+= ssp_ak8963c.o
obj-$(CONFIG_SENSORS_SSP_SENSORHUB)	+= ssp_sensorhub.o

obj-$(CONFIG_SENSORS_SSP_C12SD)		+= ssp_c12sd.o
obj-$(CONFIG_SENSORS_MAX86900)		+= max86900.o
obj-$(CONFIG_SENSORS_SSP_FACTORY)	+= factory/
