#
# Makefile for muic devices
#

obj-y					+= muic-core.o
obj-$(CONFIG_MUIC_NOTIFIER)		+= muic_notifier.o
obj-$(CONFIG_MUIC_MAX77804)		+= max77804-muic.o
obj-$(CONFIG_MUIC_MAX77804K)	+= max77804k-muic.o
obj-$(CONFIG_MUIC_MAX77828)		+= max77828-muic.o
obj-$(CONFIG_MUIC_MAX77843)		+= max77843-muic.o
obj-$(CONFIG_HV_MUIC_MAX77843_AFC)	+= max77843-muic-afc.o
obj-$(CONFIG_MUIC_MAX77833)		+= max77833-muic.o
obj-$(CONFIG_HV_MUIC_MAX77833_AFC)  += max77833-muic-afc.o
obj-$(CONFIG_MUIC_MAX77854)		+= max77854-muic.o
obj-$(CONFIG_HV_MUIC_MAX77854_AFC)  += max77854-muic-afc.o
obj-$(CONFIG_MUIC_MAX77888)		+= max77888-muic.o
obj-$(CONFIG_MUIC_FSA9480)		+= fsa9480.o
obj-$(CONFIG_MUIC_S2MM001)		+= s2mm001.o
obj-$(CONFIG_MUIC_TSU6721)		+= tsu6721-muic.o

# MUIC_UNIVERSAL
obj-$(CONFIG_MUIC_SUPPORT_CCIC)	+= universal/muic_ccic.o
obj-$(CONFIG_MUIC_UNIVERSAL)	+= universal/muic_usb.o
obj-$(CONFIG_MUIC_UNIVERSAL)	+= universal/muic_coagent.o
obj-$(CONFIG_MUIC_UNIVERSAL)	+= universal/muic_task.o
obj-$(CONFIG_MUIC_UNIVERSAL)	+= universal/muic_state.o
obj-$(CONFIG_MUIC_UNIVERSAL)	+= universal/muic_apis.o
obj-$(CONFIG_MUIC_UNIVERSAL)	+= universal/muic_sysfs.o
obj-$(CONFIG_MUIC_UNIVERSAL)	+= universal/muic_debug.o
obj-$(CONFIG_MUIC_UNIVERSAL)	+= universal/muic_dt.o
obj-$(CONFIG_MUIC_UNIVERSAL)	+= universal/muic_i2c.o
obj-$(CONFIG_MUIC_UNIVERSAL)	+= universal/muic_regmap.o
obj-$(CONFIG_MUIC_UNIVERSAL)	+= universal/muic_vps.o
obj-$(CONFIG_MUIC_UNIVERSAL_SM5703)		+= universal/muic_regmap_sm5703.o
obj-$(CONFIG_MUIC_UNIVERSAL_S2MM001)	+= universal/muic_regmap_s2mm001b.o
obj-$(CONFIG_MUIC_UNIVERSAL_MAX77849)	+= universal/muic_regmap_max77849.o
obj-$(CONFIG_MUIC_UNIVERSAL_MAX77854)	+= universal/muic_regmap_max77854.o

obj-$(CONFIG_MUIC_HV)	+= universal/muic_hv.o