#
# Makefile for the TUI_Module
#
# add TUI_Module

ifeq ($(CONFIG_SOC_EXYNOS5433),y)
PLATFORM := exynos5433
else ifeq ($(CONFIG_SOC_EXYNOS7420),y)
PLATFORM := exynos7420
else ifeq ($(CONFIG_SOC_EXYNOS8890),y)
PLATFORM := exynos8890
endif

obj-y += TlcTui.o

TlcTui-objs := main.o \
	tlcTui.o \
	trustedui.o \
	platforms/$(PLATFORM)/tui-hal.o

# Release mode by default
ccflags-y := -DNDEBUG
ccflags-y += -I$(KERNEL_API_MODULE) -I$(M) -I$(M)/inc
ccflags-y += -Idrivers/gud/gud-$(PLATFORM)/MobiCoreDriver/public
#ccflags-y += -Idrivers/gud/gud-$(PLATFORM)/MobiCoreDriver/include
ccflags-y += -Idrivers/video/fbdev/exynos/decon_8890
ccflags-y += -Idrivers/gud/TlcTui/
ccflags-y += -Idrivers/gud/TlcTui/inc
ccflags-y += -Idrivers/gud/TlcTui/public

ifdef MOBICORE_CFLAGS
    ccflags-y += $(MOBICORE_CFLAGS)
endif

# Linker script
# KBUILD_LDFLAGS_MODULE += -T $(SUBDIRS)/mcApiTest.lds
# Extra symbols from the API module
KBUILD_EXTRA_SYMBOLS="$(KERNEL_API_MODULE)/Module.symvers"
ccflags-y += -Wno-declaration-after-statement -Wno-missing-noreturn -Wno-return-type

clean:
	@echo $(COMP_PATH_MobiCoreDriverKernelApi)
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions \
		Module.markers Module.symvers modules.order

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
    include .depend
endif
