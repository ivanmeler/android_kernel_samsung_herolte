# drivers/staging/samsung/Makefile
#
# Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

# Samsung Battery
obj-$(CONFIG_BATTERY_SAMSUNG)	+= sec_batt.o

# Samsung Logging Feature
obj-$(CONFIG_SEC_EXT)		+= sec_log.o
# Samsung sec sysfs Feature
obj-$(CONFIG_SEC_SYSFS)		+= sec_sysfs.o
# Samsung Reset Feature
obj-$(CONFIG_SEC_REBOOT)	+= sec_reboot.o

# Samsung TN Debugging Features
obj-$(CONFIG_SEC_DEBUG)			+= sec_debug.o sec_getlog.o sec_gaf.o sec_debug_test.o
obj-$(CONFIG_SEC_DEBUG)			+= sec_debug_hard_reset_hook.o
obj-$(CONFIG_SEC_DEBUG_RESET_REASON)	+= sec_debug_reset_reason.o
obj-$(CONFIG_SEC_DEBUG_EXTRA_INFO)	+= sec_debug_extra_info.o
obj-$(CONFIG_SEC_DEBUG_HW_PARAM)	+= sec_debug_hw_param.o
obj-$(CONFIG_SEC_DEBUG_AUTO_SUMMARY)	+= sec_debug_auto_summary.o
obj-$(CONFIG_SEC_DEBUG_LAST_KMSG)	+= sec_debug_last_kmsg.o
obj-$(CONFIG_SEC_UPLOAD)	+= sec_upload.o

# Samsung TN BSP Options
obj-$(CONFIG_SEC_PARAM)		+= sec_param.o

# Samsung Misc Feature
obj-$(CONFIG_SEC_MISC)         += sec_misc.o
# Samsung Argos
obj-$(CONFIG_ARGOS)		+= sec_argos.o
# Sentinel
obj-$(CONFIG_SENTINEL)		+= sentinel.o
# Samsung sec bsp Feature
obj-$(CONFIG_SEC_BSP)          += sec_bsp.o
obj-$(CONFIG_SEC_INITCALL_DEBUG)		+= sec_initcall_debug.o

obj-$(CONFIG_SEC_HOTPLUG)	+= sec_hotplug.o

# Samsung sec NAD Feature
obj-$(CONFIG_SEC_NAD)		+= sec_nad.o
