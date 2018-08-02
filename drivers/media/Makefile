#
# Makefile for the kernel multimedia device drivers.
#

media-objs	:= media-device.o media-devnode.o media-entity.o

#
# I2C drivers should come before other drivers, otherwise they'll fail
# when compiled as builtin drivers
#
obj-y += i2c/ tuners/
obj-$(CONFIG_DVB_CORE)  += dvb-frontends/

#
# Now, let's link-in the media core
#
ifeq ($(CONFIG_MEDIA_CONTROLLER),y)
  obj-$(CONFIG_MEDIA_SUPPORT) += media.o
endif

obj-$(CONFIG_VIDEO_DEV) += v4l2-core/
obj-$(CONFIG_DVB_CORE)  += dvb-core/

obj-$(CONFIG_MEDIA_M2M1SHOT) += m2m1shot.o m2m1shot-helper.o
obj-$(CONFIG_MEDIA_M2M1SHOT_TESTDEV) += m2m1shot-testdev.o
obj-$(CONFIG_MEDIA_M2M1SHOT2) += m2m1shot2.o
obj-$(CONFIG_MEDIA_M2M1SHOT2_TESTDEV) += m2m1shot2-testdev.o

# There are both core and drivers at RC subtree - merge before drivers
obj-y += rc/

#
# Finally, merge the drivers that require the core
#

obj-y += common/ platform/ pci/ usb/ mmc/ firewire/ parport/
obj-$(CONFIG_VIDEO_DEV) += radio/
obj-$(CONFIG_TDMB)  += tdmb/
