# Makefile of modem_v1

EXTRA_CFLAGS += -Idrivers/misc/modem_v1 \
		-DCONFIG_OPTION_REGION=\"$(PROJECT_REGION)\"

obj-y += modem_main.o modem_variation.o modem_io_device.o modem_utils.o

obj-$(CONFIG_LINK_DEVICE_MEMORY) += link_device_memory_main.o \
				    link_device_memory_snapshot.o \
				    link_device_memory_command.o \
				    link_device_memory_flow_control.o \
				    link_device_memory_debug.o
obj-$(CONFIG_LINK_DEVICE_SHMEM) += link_device_shmem.o \
				   link_device_memory_snapshot.o \
				   link_device_memory_flow_control.o \
				   link_device_memory_debug.o \
				   link_device_memory_sbd.o
obj-$(CONFIG_UMTS_MODEM_SS310AP) += modem_ctrl_ss310ap.o pmu-cp.o

obj-$(CONFIG_LINK_CONTROL_MSG_IOSM) += link_ctrlmsg_iosm.o
obj-$(CONFIG_LINK_CONTROL_MSG_COMMAND) += link_ctrlmsg_command.o
obj-$(CONFIG_DEBUG_PKTLOG) += modem_pktlog.o
