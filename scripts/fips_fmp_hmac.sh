#!/bin/bash

# fips_fmp_hmac.sh
#
# Author     : Rohit Kothari (r.kothari@samsung.com)
# Created on :  14 Feb 2014
# Copyright (c) Samsung Electronics 2014

# Given a vmlinux file and a System.map, this scripts finds bytes belonging to
# Kernel Crypto within vmlinux file.(Under section .text, .init.text, .exit.text and .rodata)
# After collecting all the bytes, it calculates a hmac(sha256) on those bytes.
# Generated hmac is put back into a fmp rodata variable within vmlinux file itself.
# This makes the build time hmac available at runtime, for integrity check.
#
# To find fmp bytes, this scripts heavily relies on output of arm-eabi-readelf.
# If the output of arm-eabi-readelf changes in future, this script might need changes.
#
# Pre-conditions : $READELF, $HOSTCC variables are set.
#
#

if test $# -ne 2; then
	echo "Usage: $0 vmlinux System.map"
	exit 1
fi

vmlinux_var=$1
system_map_var=$2

if [[ -z "$vmlinux_var" || -z "$system_map_var" || -z "$READELF" || -z "$HOSTCC" ]]; then
	echo "$0 : variables not set"
	exit 1
fi

if [[ ! -f $vmlinux_var || ! -f $system_map_var  ]]; then
	echo "$0 : files does not exist"
	exit 1
fi

rm -f vmlinux.elf
$READELF -S $vmlinux_var > vmlinux.elf

retval=$?
if [ $retval -ne 0 ]; then
	echo "$0 : $READELF returned error"
	exit 1
fi

declare -A array

# FOR GENERIC CRYPTO FILES                         #awk fields to cut
array[0]=".text first_fmp_text last_fmp_text \$5 \$6"
array[1]=".rodata first_fmp_rodata last_fmp_rodata \$5 \$6"
array[2]=".init.text first_fmp_init last_fmp_init \$4 \$5"

rm -f offsets_sizes.txt

#Addresses retrieved must be a valid hex
reg='^[0-9A-Fa-f]+$'

#Total bytes of all fmp sections scanned. Used later for error checking
total_bytes=0;

# For each type of Section :
# first_addr  = Address of first_fmp_text, first_fmp_rodata, etc.
# last_addr   = Address of last_fmp_text, last_fmp_rodata etc.
# start_addr  = Starting Address of a section within vmlinux
# offset      = Offset in vmlinux file where the section begins
# file_offset = Offset in vmlinux file where the fmp bytes begins.
# size        = size of fmp bytes.

# Output is offsets_sizes.txt, of the format
#   Section Name   fmp_bytes_offse     fmp_bytes_size
#                   (in decimal)        (in decimal)
#      .text           2531072            114576
#    .rodata           9289648             55388
#      :                  :                  :

for i in "${array[@]}"; do

	var1=var2=var3=var4=var5=""
	first_addr=last_addr=start_addr=offset=file_offset=size=""
	k=1
        #This loop creates var1, var2 etc and set them to individual strings of a row in array
	for j in $i; do
		export var$k=$j
		let k+=1
	done

	first_addr=`cat $system_map_var|grep -w $var2|awk '{print $1}'`
	if  [[ ! $first_addr =~ $reg ]]; then echo "$0 : first_addr invalid"; exit 1; fi

	last_addr=`cat $system_map_var|grep -w $var3|awk '{print $1}'`
	if  [[ ! $last_addr =~ $reg ]]; then echo "$0 : last_addr invalid"; exit 1; fi

	start_addr=`cat vmlinux.elf |grep -w "$var1 "|grep PROGBITS|awk '{print '$var4'}'`
	if  [[ ! $start_addr =~ $reg ]]; then echo "$0 : start_addr invalid"; exit 1; fi

	offset=`cat vmlinux.elf |grep -w "$var1 "|grep PROGBITS|awk '{print '$var5'}'`
	if  [[ ! $offset =~ $reg ]]; then echo "$0 : offset invalid"; exit 1; fi

	if [[ $((16#$first_addr)) -lt $((16#$start_addr)) ]]; then echo "$0 : first_addr < start_addr"; exit 1; fi

	if [[ $((16#$last_addr)) -le $((16#$first_addr)) ]]; then echo "$0 : last_addr <= first_addr"; exit 1; fi

	file_offset=`expr $((16#$offset)) + $((16#$first_addr)) - $((16#$start_addr))`
	if  [[ $file_offset -le 0 ]]; then echo "$0 : file_offset invalid"; exit 1; fi

	size=`expr $((16#$last_addr)) - $((16#$first_addr))`
	if  [[ $size -le 0 ]]; then echo "$0 : fmp section size invalid"; exit 1; fi

	echo "$var1 " $file_offset " " $size >> offsets_sizes.txt

	let "total_bytes += `expr $((16#$last_addr)) - $((16#$first_addr))`"
done

if [[ ! -f offsets_sizes.txt ]]; then
	echo "$0 : offset_sizes.txt does not exist"
	exit 1
fi

rm -f fips_fmp_utils
$HOSTCC -o fips_fmp_utils $srctree/scripts/fips_fmp_utils.c
retval=$?
if [ $retval -ne 0 ]; then
	echo "$0 : $HOSTCC returned error"
	exit 1
fi

rm -f builtime_bytes.txt #used for debugging
rm -f builtime_bytes.bin #used for calculating hmac

date_var=`date`
echo "Created on : " $date_var > builtime_bytes.txt

#Using offsets_sizes.txt, dump fmp bytes from vmlinux file into builtime_bytes.bin
#Also gather printf's into builtime_bytes.txt, for debugging if required
while read args; do
	./fips_fmp_utils -g $vmlinux_var $args builtime_bytes.bin >> builtime_bytes.txt
	retval=$?
	if [ $retval -ne 0 ]; then
	    echo "$0 : fips_fmp_utils : unable to gather fmp bytes from vmlinux"
	    exit 1
	fi
	echo "" >> builtime_bytes.txt
done < offsets_sizes.txt   # <================== offsets_sizes.txt

if [[ ! -f builtime_bytes.bin ]]; then
	echo "$0 : builtime_bytes.bin does not exist"
	exit 1
fi

file_size=`cat builtime_bytes.bin| wc -c`

# Make sure that file size of fmp_hmac.bin is as expected
if [ $total_bytes -ne $file_size ]; then
	echo "$0: Bytes mismatch"
	exit 1
fi

key="The quick brown fox jumps over the lazy dog"

# Now, generate the hmac.
openssl dgst -sha256 -hmac "$key" -binary -out fmp_hmac.bin builtime_bytes.bin
retval=$?
if [ $retval -ne 0 ]; then
	echo "$0 : openssl dgst command returned error"
	exit 1
fi

# Just, for debugging, print the same hmac on console
openssl dgst -sha256 -hmac "$key" builtime_bytes.bin
retval=$?
if [ $retval -ne 0 ]; then
	echo "$0 : openssl dgst command returned error"
	exit 1
fi

if [[ ! -f fmp_hmac.bin ]]; then
	echo "$0 : fmp_hmac.bin does not exist"
	exit 1
fi

file_size=`cat fmp_hmac.bin| wc -c`

# hmac(sha256) produces 32 bytes of hmac
if [ $file_size -ne 32 ]; then
	echo "$0: Unexpected size of Hash file : " $file_size
	exit 1
fi


# Now that we have the hmac, update this hmac into an rodata "builtime_fmp_hmac" varialble
# in vmlinux file.
# This variable has a place holder 32 bytes that will be over-written with generated hmac.
# This way, this build time hmac, will be available as a read-only variable at run-time.

first_addr=`cat $system_map_var|grep -w "builtime_fmp_hmac"|awk '{print $1}' `
if  [[ ! $first_addr =~ $reg ]]; then echo "$0 : first_addr of hmac variable invalid"; exit 1; fi

start_addr=`cat vmlinux.elf |grep -w ".rodata"|grep PROGBITS|awk '{print $5}' `
if  [[ ! $start_addr =~ $reg ]]; then echo "$0 : start_addr of .rodata invalid"; exit 1; fi

offset=`cat vmlinux.elf |grep -w ".rodata"|grep PROGBITS| awk '{print $6}' `
if  [[ ! $offset =~ $reg ]]; then echo "$0 : offset of .rodata invalid"; exit 1; fi

if [[ $((16#$first_addr)) -le $((16#$start_addr)) ]]; then echo "$0 : hmac var first_addr <= start_addr"; exit 1; fi

hmac_offset=`expr $((16#$offset)) + $((16#$first_addr)) - $((16#$start_addr))`
if  [[ $hmac_offset -le 0 ]]; then echo "$0 : hmac_offset invalid"; exit 1; fi

# This does the actual update of hmac into vmlinux file, at given offset
./fips_fmp_utils -u $vmlinux_var fmp_hmac.bin $hmac_offset
retval=$?
if [ $retval -ne 0 ]; then
	echo "$0 : fips_fmp_utils : unable to update hmac in vmlinux"
	exit 1
fi

# rm -f fmp_hmac.bin
# rm -f builtime_bytes.txt
# rm -f builtime_bytes.bin
# rm -f fips_fmp_utils
# rm -f vmlinux.elf
# rm -f offsets_sizes.txt

# And we are done...
