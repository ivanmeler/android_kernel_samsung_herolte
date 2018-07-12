#!/bin/bash

# fips_crypto_hmac.sh
#
# Author     : Rohit Kothari (r.kothari@samsung.com)
# Created on :  14 Feb 2014
# Modified on:  22 Jun  2015  by Jia Ma (jia.ma@samsung.com) /* To adapt to KASLR change */
# Modified on:  09 Jul  2015  by Wenbo Shen (wenbo.s@samsung.com) /* To reduce the hmac generation time */
# Copyright (c) Samsung Electronics 2014

# Given a vmlinux file and a System.map, this scripts finds bytes belonging to 
# Kernel Crypto within vmlinux file.(Under section .text, .init.text, .exit.text and .rodata)
# After collecting all the bytes, it calculates a hmac(sha256) on those bytes.
# Generated hmac is put back into a crypto rodata variable within vmlinux file itself.
# This makes the build time hmac available at runtime, for integrity check.
# 
# To find crypto bytes, this scripts heavily relies on output of arm-eabi-readelf.
# If the output of arm-eabi-readelf changes in future, this script might need changes.
# 
# Pre-conditions : $READELF, $HOSTCC variables are set.
#
# 

# Here we want to seperate the oneshot code from the repeated code 
################ the repeated code ####################
kaslr_patch () {
	index=$1
	./kaslr_fips $vmlinux_var $reloc_start_addr $reloc_end_addr $dynsym_addr $index $first_crypto_rodata $last_crypto_rodata $first_fmp_rodata $last_fmp_rodata $2
	retval=$?
	if [ $retval -ne 0 ]; then
		echo "$0 : kaslr_fips : unable to patch the vmlinux"
		exit 1
	fi
}

hmac_patch() {
	index=$1
	offsets_sizes_file=$2
	
	if [ "$3" == "crypto" ]; then
		fips_utils=./fips_crypto_utils
		hmac_offset=`expr $crypto_hmac_offset_base + $((32*$index)) `
	elif [ "$3" == "fmp" ]; then
		fips_utils=./fips_fmp_utils
		hmac_offset=`expr $fmp_hmac_offset_base + $((32*$index)) `
	fi

	rm -f builtime_bytes.txt #used for debugging
	rm -f builtime_bytes.bin #used for calculating hmac 
	
	date_var=`date`
	echo "Created on : " $date_var > builtime_bytes.txt
	
	
	#Using offsets_sizes.txt, dump crypto bytes from vmlinux file into builtime_bytes.bin
	#Also gather printf's into builtime_bytes.txt, for debugging if required
	while read args; do
		$fips_utils -g $vmlinux_var $args builtime_bytes.bin >> builtime_bytes.txt
		retval=$?
		if [ $retval -ne 0 ]; then
		    echo "$0 : $fips_utils : unable to gather $3 bytes from vmlinux"
		    exit 1
		fi
		echo "" >> builtime_bytes.txt
	done < $offsets_sizes_file  
	
	
	if [[ ! -f builtime_bytes.bin ]]; then
		echo "$0 : builtime_bytes.bin does not exist"
		exit 1
	fi
	
	#file_size=`cat builtime_bytes.bin| wc -c`
	
	# Make sure that file size of crypto_hmac.bin is as expected
	#if [ $total_bytes -ne $file_size ]; then
		#echo "$0: Bytes mismatch"
		#exit 1
	#fi
	
	
	key="The quick brown fox jumps over the lazy dog"
	
	# Now, generate the hmac.
	openssl dgst -sha256 -hmac "$key" -binary -out crypto_hmac.bin builtime_bytes.bin
	retval=$?
	if [ $retval -ne 0 ]; then
		echo "$0 : openssl dgst command returned error"
		exit 1
	fi
	
	# Just, for debugging, print the same hmac on console
	#openssl dgst -sha256 -hmac "$key" builtime_bytes.bin
	
	if [[ ! -f crypto_hmac.bin ]]; then
		echo "$0 : crypto_hmac.bin does not exist"
		exit 1
	fi
	
	file_size=`cat crypto_hmac.bin| wc -c`
	
	# hmac(sha256) produces 32 bytes of hmac 
	if [ $file_size -ne 32 ]; then
		echo "$0: Unexpected size of Hash file : " $file_size
		exit 1
	fi
	
	# Now that we have the hmac, update this hmac into an rodata "builtime_crypto_hmac" varialble
	# in vmlinux file.
	# This variable has a place holder 32 bytes that will be over-written with generated hmac.
	# This way, this build time hmac, will be available as a read-only variable at run-time.

	if  [[ $hmac_offset -le 0 ]]; then echo "$0 : hmac_offset invalid"; exit 1; fi
	
	# This does the actual update of hmac into vmlinux file, at given offset
	$fips_utils -u $vmlinux_var crypto_hmac.bin $hmac_offset
	retval=$?
	if [ $retval -ne 0 ]; then
		echo "$0 : fips_crypto_utils : unable to update hmac in vmlinux"
		exit 1
	fi
	
	rm -f crypto_hmac.bin    
	rm -f builtime_bytes.txt
	rm -f builtime_bytes.bin
}

################ the kaslr patch oneshot code ####################
# 1st parameter is the array, 2nd is the output file name
offsets_sizes_gen(){
	name=$1[@]
	array=("${!name}")

	outfile=$2
	rm -f $outfile
	
	#Addresses retrieved must be a valid hex
	reg='^[0-9A-Fa-f]+$'
	
	#Total bytes of all crypto sections scanned. Used later for error checking
	#total_bytes=0;
	
	# For each type of Section : 
	# first_addr  = Address of first_crypto_text, first_crypto_rodata, etc.
	# last_addr   = Address of last_crypto_text, last_crypto_rodata etc.
	# start_addr  = Starting Address of a section within vmlinux
	# offset      = Offset in vmlinux file where the section begins
	# file_offset = Offset in vmlinux file where the crypto bytes begins.
	# size        = size of crypto bytes.
	
	# Output is offsets_sizes.txt, of the format
	#   Section Name   crypto_bytes_offset crypto_bytes_size 
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
	
		start_addr=`cat vmlinux.elf |grep -w "$var1 "|grep PROGBITS| awk -F "]" '{print $2}' |awk '{print $3}'`
		if  [[ ! $start_addr =~ $reg ]]; then echo "$0 : start_addr invalid"; exit 1; fi
	
		offset=`cat vmlinux.elf |grep -w "$var1 "|grep PROGBITS| awk -F "]" '{print $2}' |awk '{print $4}'`
		if  [[ ! $offset =~ $reg ]]; then echo "$0 : offset invalid"; exit 1; fi
	
		if [[ $((16#$first_addr)) -lt $((16#$start_addr)) ]]; then echo "$0 : first_addr < start_addr"; exit 1; fi
	
		if [[ $((16#$last_addr)) -le $((16#$first_addr)) ]]; then echo "$0 : last_addr <= first_addr"; exit 1; fi
	
		file_offset=`expr $((16#$offset)) + $((16#$first_addr)) - $((16#$start_addr))`
		if  [[ $file_offset -le 0 ]]; then echo "$0 : file_offset invalid"; exit 1; fi
	
		size=`expr $((16#$last_addr)) - $((16#$first_addr))`
		if  [[ $size -le 0 ]]; then echo "$0 : crypto section size invalid"; exit 1; fi
	
		echo "$var1 " $file_offset " " $size >> $outfile
	
		#let "total_bytes += `expr $((16#$last_addr)) - $((16#$first_addr))`"
	done
	
	if [[ ! -f $outfile ]]; then
		echo "$0 : offset_sizes.txt does not exist"
		exit 1
	fi


}

cal_hmac_offset(){
	first_addr=`cat $system_map_var|grep -w $1|awk '{print $1}' `
	if  [[ ! $first_addr =~ $reg ]]; then echo "$0 : first_addr of hmac variable invalid"; exit 1; fi
	
	start_addr=`cat vmlinux.elf |grep -w $2|grep PROGBITS| awk -F "]" '{print $2}' |awk '{print $3}' `
	if  [[ ! $start_addr =~ $reg ]]; then echo "$0 : start_addr of .rodata invalid"; exit 1; fi
	
	offset=`cat vmlinux.elf |grep -w $2|grep PROGBITS| awk -F "]" '{print $2}' | awk '{print $4}' `
	if  [[ ! $offset =~ $reg ]]; then echo "$0 : offset of .rodata invalid"; exit 1; fi
	
	if [[ $((16#$first_addr)) -le $((16#$start_addr)) ]]; then echo "$0 : hmac var first_addr <= start_addr"; exit 1; fi
	
	if [ "$1" == "builtime_crypto_hmac" ]; then
		crypto_hmac_offset_base=`expr $((16#$offset)) + $((16#$first_addr)) - $((16#$start_addr)) `
	elif [ "$1" == "builtime_fmp_hmac" ]; then
		fmp_hmac_offset_base=`expr $((16#$offset)) + $((16#$first_addr)) - $((16#$start_addr)) `
	fi
}

################ Start Here ################

START_TIME=$SECONDS

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

#start patching vmlinux with static patcher, temporary put it here 
rm -f kaslr_fips
$HOSTCC -o kaslr_fips  $srctree/scripts/kaslr_fips.c
retval=$?
if [ $retval -ne 0 ]; then
	echo "$0 : $HOSTCC returned error"
	exit 1
fi

rm -f fips_crypto_utils
$HOSTCC -o fips_crypto_utils $srctree/scripts/fips_crypto_utils.c
retval=$?
if [ $retval -ne 0 ]; then
	echo "$0 : $HOSTCC returned error"
	exit 1
fi

rm -f fips_fmp_utils
$HOSTCC -o fips_fmp_utils $srctree/scripts/fips_fmp_utils.c
retval=$?
if [ $retval -ne 0 ]; then
	echo "$0 : $HOSTCC returned error"
	exit 1
fi

var="__reloc_start"
var1=`cat $system_map_var|grep -w $var|awk '{print $1}'`
var="__reloc_end"
var2=`cat $system_map_var|grep -w $var|awk '{print $1}'`
var="__dynsym_start"
var3=`cat $system_map_var|grep -w $var|awk '{print $1}'`

var="first_crypto_rodata"
var4=`cat $system_map_var|grep -w $var|awk '{print $1}'`
var="last_crypto_rodata"
var5=`cat $system_map_var|grep -w $var|awk '{print $1}'`

var="first_fmp_rodata"
var6=`cat $system_map_var|grep -w $var|awk '{print $1}'`
var="last_fmp_rodata"
var7=`cat $system_map_var|grep -w $var|awk '{print $1}'`

reloc_start_addr=`expr $((16#$var1))`
reloc_end_addr=`expr $((16#$var2))`
dynsym_addr=`expr $((16#$var3))`
first_crypto_rodata=`expr $((16#$var4))`
last_crypto_rodata=`expr $((16#$var5))`
first_fmp_rodata=`expr $((16#$var6))`
last_fmp_rodata=`expr $((16#$var7))`

################# CRYPTO #######################
# FOR GENERIC CRYPTO FILES                         #awk fields to cut
fips_crypto[0]=".text first_crypto_text last_crypto_text \$5 \$6"
fips_crypto[1]=".rodata first_crypto_rodata last_crypto_rodata \$5 \$6"
fips_crypto[2]=".init.text first_crypto_init last_crypto_init \$4 \$5"

# # FOR ASM CRYPTO FILES
fips_crypto[3]=".text first_crypto_asm_text last_crypto_asm_text \$5 \$6"
fips_crypto[4]=".rodata first_crypto_asm_rodata last_crypto_asm_rodata \$5 \$6"
fips_crypto[5]=".init.text first_crypto_asm_init last_crypto_asm_init \$4 \$5"

offsets_sizes_crypto="offsets_sizes_crypto.txt"
offsets_sizes_gen fips_crypto $offsets_sizes_crypto

#find hmac_offset in vmlinux for updating
cal_hmac_offset builtime_crypto_hmac .rodata


################# FMP #######################
fips_fmp[0]=".text first_fmp_text last_fmp_text \$5 \$6"
fips_fmp[1]=".rodata first_fmp_rodata last_fmp_rodata \$5 \$6"
fips_fmp[2]=".init.text first_fmp_init last_fmp_init \$4 \$5"

offsets_sizes_fmp="offsets_sizes_fmp.txt"
offsets_sizes_gen fips_fmp $offsets_sizes_fmp

#find hmac_offset in vmlinux for updating
cal_hmac_offset builtime_fmp_hmac .rodata

rodata_va_addr=`cat vmlinux.elf|grep -w '.rodata'| awk -F "]" '{print $2}' | awk '{print $3}'`
rodata_file_addr=`cat vmlinux.elf|grep -w '.rodata'| awk -F "]" '{print $2}' |awk '{print $4}'`
va_to_file=`expr $((16#$rodata_va_addr)) - $((16#$rodata_file_addr))`

# the loop
for index in {0..63}
do
	kaslr_patch $index $va_to_file
	hmac_patch $index $offsets_sizes_crypto crypto
	hmac_patch $index $offsets_sizes_fmp fmp
done

rm -f kaslr_fips
rm -f fips_crypto_utils
rm -f fips_fmp_utils
rm -f vmlinux.elf
rm -f offsets_sizes*

ELAPSED_TIME=$(($SECONDS - $START_TIME))
echo ">>>>> Time used for generated all hashes is $(($ELAPSED_TIME)) sec"

# And we are done...
