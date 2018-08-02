#
# Makefile for nfc devices
#

obj-$(CONFIG_SEC_NFC_SENN3AB)	+= secnfc/
obj-$(CONFIG_SEC_NFC)		+= sec_nfc.o
obj-$(CONFIG_NFC_PN547)		+= pn547.o
obj-$(CONFIG_NFC_P61)		+= p61.o
obj-$(CONFIG_ESE_P3_LSI)		+= ese_p3.o
obj-$(CONFIG_ESE_COFFEE)		+= sec_coffee.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
