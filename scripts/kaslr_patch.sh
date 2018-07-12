#!/bin/sh
# Based on the relocatable vmlinux file and offset create the 
# new vmlinux and System.map file. New vmlinux and System.map is 
# intend to be used by debugging tools to retrieve the actual 
# addresses of symbols in the kernel.
#
# Usage
# mksysmap vmlinux-old  offset

# Author     : Jia Ma (jia.ma@samsung.com)
# Created on :  21 Sep 2015
# Copyright (c) Samsung Electronics 2015

if test $# -ne 2; then
	echo "Usage: $0 vmlinux  offset" 
	exit 1
fi


vmlinux=$1
offset=$2

if [[ -z "$offset" || -z "$vmlinux" ]]; then 
	echo  "$0 : variable not set"
	exit 1 
fi 

if [[ ! -f $vmlinux ]]; then 
	echo  "$vmlinux file not exist!"
	exit 1 
fi 

NM=$ARM_TOOLCHAIN-nm
OBJCOPY=$ARM_TOOLCHAIN-objcopy 

if [[ -z "$ARM_TOOLCHAIN" ]]; then 
	echo  "Please specify ARM toolchain" 
	exit 1 
fi 

echo  "+Patching System.map --> System.map.patched" 
### generate runtime  System.map file¡¡###
$OBJCOPY --adjust-vma $offset  $vmlinux vmlinux.tmp  2>/dev/null
$NM -n vmlinux.tmp | grep -v '\( [aNUw] \)\|\(__crc_\)\|\( \$[adt]\)' > System.map.patched 
rm -f  vmlinux.tmp 

echo  "+Patching $vmlinux -->vmlinux.patched"
# following simply change the vmlinux from DYN type to EXEC type 
# to avoid the JTag load the dyn symbol into the system: e.g. there will be 2 start_kernel in the JTag symbol list, 1 from SYMBOL table, 1 from RELO section 
if [[ ! -f "elfedit" ]]; then 
	echo "Can find elfedit"
	exit 1 
fi 

cp vmlinux vmlinux.patched
elfedit --output-type exec vmlinux.patched 

echo "+Done" 






