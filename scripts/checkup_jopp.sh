#!/bin/sh
# Examine vmlinux file if it has correct JOPP magic.
# It will check if a leading instruction of start_kernel() is JOPP magic.
elf_file=${1}
jopp_magic=${2}

if [ -z $elf_file ] || [ -z $jopp_magic ]; then
	echo "checkup_jopp: invalid argument"
	exit 0
fi

addr=`${OBJDUMP} -t ${elf_file} | grep -w start_kernel`
addr=${addr%% *}

# reduce size to treat as number
addr_n=$((0x${addr#ffffff}))

# subtract and make format ffffffxxxxxxxxxx
target_addr=`printf "ffffff%010x" $(($addr_n - 4))`

${OBJDUMP} --start-address=0x${target_addr} --stop-address=0x${addr} -d $elf_file | tail -n 1 | grep -qi $jopp_magic

if [ $? -ne "0" ]; then
	echo "Error: Can't find JOPP magic(${jopp_magic}) at address 0x${target_addr} in vmlinux symbol"
	exit 1
fi
