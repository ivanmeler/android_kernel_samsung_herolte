# Switch Class Driver
obj-$(CONFIG_SWITCH)		+= switch_class.o
obj-$(CONFIG_SWITCH_GPIO)	+= switch_gpio.o

ifeq ($(CONFIG_SWITCH_PTT),y)
obj-$(CONFIG_SWITCH_ARIZONA)	+= switch-arizona_ptt.o
else
obj-$(CONFIG_SWITCH_ARIZONA)	+= switch-arizona.o
endif
