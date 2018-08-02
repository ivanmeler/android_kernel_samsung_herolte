#
# Makefile for the Linux eCryptfs
#

ifneq "$(strip $(CONFIG_MMC_DW_FMP_ECRYPT_FS) $(CONFIG_UFS_FMP_ECRYPT_FS))" ""
ccflags-y += -Ifs/sdcardfs/m
endif

obj-$(CONFIG_ECRYPT_FS) += ecryptfs.o
obj-$(CONFIG_SDP) += ecryptfs_dek.o mm.o ecryptfs_sdp_chamber.o

ecryptfs-y := dentry.o file.o inode.o main.o super.o mmap.o read_write.o \
	      crypto.o keystore.o kthread.o debug.o

ecryptfs-$(CONFIG_ECRYPT_FS_MESSAGING) += messaging.o miscdev.o
