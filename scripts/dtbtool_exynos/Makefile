# scripts/dtbtool_exynos makefile

hostprogs-y	:= dtbtool
always		:= $(hostprogs-y)

dtbtool-objs	:= dtbtool.o
dtbtool-objs	+= ../dtc/libfdt/fdt.o ../dtc/libfdt/fdt_ro.o

CFLAGS_LIBFDT	:= -I$(src)/../dtc/libfdt

HOSTCFLAGS_dtbtool.o	:= $(CFLAGS_LIBFDT)
HOSTCFLAGS_fdt.o	:= $(CFLAGS_LIBFDT)
HOSTCFLAGS_fdt_ro.o	:= $(CFLAGS_LIBFDT)
