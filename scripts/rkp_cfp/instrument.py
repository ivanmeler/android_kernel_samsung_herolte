#!/usr/bin/env python
#
# Instrument vmlinux STP, LDP and BLR instructions to protect RA and restrict jumpping
#
# Depends on:
# 1) a modified gcc that 
#    - outputs 2 nop's before stp x29, x30 instructions
#    - outputs 1 nop after ldp x29, x30 instructions
# 2) a kernel built using gcc command-line options to prevent allocation of registers x16, x17, and x18
#
# Copyright (c) 2015 Samsung Electronics Co., Ltd.
# Authors:      James Gleeson <jagleeso@gmail.com>
#		Wenbo Shen <wenbo.s@samsung.com>

# Set to False to have vmlinux instrumented during kernel build.
# OFF = True
OFF = False

# If true, skip instrumenting functions in hyperdrive/resource/debug/skip.txt
# (for debugging).
# SKIP_INSTRUMENTING = True
SKIP_INSTRUMENTING = False

import argparse
import subprocess
from common import pr, log
import common
import os
import fnmatch
import re
import cPickle
import sys
import mmap
import contextlib
import binascii
import pprint
import multiprocessing
import math
import tempfile
import pipes
import StringIO
import textwrap
import bisect
import itertools
# shutil has issues being re-imported during ipython's dreload.
# Just use subprocess instead.
#
# import shutil

import debug

# NOTE: must be kept in sync with macro definitions in init/hyperdrive.S
RRX_DEFAULT = 16
RRK_DEFAULT = 17
RRS_DEFAULT = 18

# Get some useful paths based on location of this file.

sys.path.append(os.path.dirname(__file__))
RKP_CFP = os.path.abspath(os.path.join( os.path.dirname(__file__), "..", ".."))
RESOURCE = os.path.abspath(os.path.join( RKP_CFP, "resource"))
DEBUG = os.path.abspath(os.path.join( RKP_CFP, "resource", "debug"))
SCRIPTS = os.path.abspath(os.path.join( RKP_CFP, "scripts"))
SRCTREE = os.path.abspath(os.path.join( RKP_CFP, ".."))

def bitmask(start_bit, end_bit):
    """
    e.g. start_bit = 8, end_bit = 2
    0b11111111  (2**(start_bit + 1) - 1)
    0b11111100  (2**(start_bit + 1) - 1) ^ (2**end_bit - 1)
    """
    return (2**(start_bit + 1) - 1) ^ (2**end_bit - 1);
def _zbits(x):
    """
    Return the number of low bits that are zero.
    e.g.
    >>> _zbits(0b11000000000000000000000000000000)
    30
    """
    n = 0
    while (x & 0x1) != 0x1 and x > 0:
        x >>= 1
        n += 1
    return n

# Use CROSS_COMPILE provided to kernel make command.
devnull = open('/dev/null', 'w')
def which(executable):
    return subprocess.Popen(['which', executable], stdout=devnull).wait() == 0
def guess_cross_compile(order):
    for prefix in order:
        if which("{prefix}gcc".format(**locals())):
            return prefix 
CROSS_COMPILE_DEFAULT = os.environ.get('CROSS_COMPILE', guess_cross_compile(order=[
    "aarch64-linux-android-",
    "aarch64-linux-gnu-",
    ]))
KERNEL_ARCH = 'arm64'
assert CROSS_COMPILE_DEFAULT is not None
def _cross(execname):
    name = "{CROSS_COMPILE_DEFAULT}{execname}".format(
            CROSS_COMPILE_DEFAULT=CROSS_COMPILE_DEFAULT, execname=execname)
    if not which(name):
        raise RuntimeError("Couldn't find {execname} on PATH\nPATH = {PATH}".format(
            execname=execname, PATH=os.environ['PATH']))
    return name
OBJDUMP = _cross("objdump")
READELF = _cross("readelf")
NM = _cross("nm")
#GDB = _cross("gdb")
GDB = None

hex_re = r'(?:[a-f0-9]+)'
virt_addr_re = re.compile(r'^(?P<virt_addr>{hex_re}):\s+'.format(hex_re=hex_re))

BL_OFFSET_MASK = 0x3ffffff
BLR_AND_RET_RN_MASK = 0b1111100000

ADRP_IMMLO_MASK = 0b1100000000000000000000000000000
ADRP_IMMHI_MASK =        0b111111111111111111100000
ADRP_RD_MASK    =                           0b11111
# _zbits
ADRP_IMMLO_ZBITS = _zbits(ADRP_IMMLO_MASK)
ADRP_IMMHI_ZBITS = _zbits(ADRP_IMMHI_MASK)
ADRP_RD_ZBITS = _zbits(ADRP_RD_MASK)


STP_OPC_MASK              = 0b11000000000000000000000000000000
STP_ADDRESSING_MODE_MASK  = 0b00111111110000000000000000000000
STP_IMM7_MASK             =           0b1111111000000000000000
STP_RT2_MASK              =                  0b111110000000000
STP_RN_MASK               =                       0b1111100000
STP_RT_MASK               =                            0b11111
# http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0489c/CIHGJHED.html
# op{type}{cond} Rt, [Rn {, #offset}]        ; immediate offset
# op{type}{cond} Rt, [Rn, #offset]!          ; pre-indexed
# op{type}{cond} Rt, [Rn], #offset           ; post-indexed
# opD{cond} Rt, Rt2, [Rn {, #offset}]        ; immediate offset, doubleword
# opD{cond} Rt, Rt2, [Rn, #offset]!          ; pre-indexed, doubleword
STP_PRE_INDEXED =  0b10100110
STP_POST_INDEXED = 0b10100010
STP_IMM_OFFSET =   0b10100100
# opD{cond} Rt, Rt2, [Rn], #offset           ; post-indexed, doubleword
# 00 for 32bit, 10 for 64bit
OPC_32 = 0b00
OPC_64 = 0b10

# Bits don't encode preindexed for str_imm_unsigned_preindex_insn
STR_IMM_OFFSET = 'preindexed'
STR_SIGN_UNSIGNED = 0b01
STR_SIZE_32 = 0b10
STR_SIZE_64 = 0b11

ADDIM_OPCODE_BITS = 0b10001
ADDIMM_SF_BIT_64 = 0b1
ADDIMM_SF_BIT_32 = 0b0
ADDIMM_OPCODE_MASK = bitmask(28, 24)
ADDIMM_SHIFT_MASK = bitmask(23, 22)
ADDIMM_IMM_MASK = bitmask(21, 10)
ADDIMM_RN_MASK = bitmask(9, 5)
ADDIMM_RD_MASK = bitmask(4, 0)
ADDIMM_SF_MASK = bitmask(31, 31)

DEFAULT_SKIP_ASM_FILE = os.path.join(RESOURCE, "skip_asm.txt")
DEFAULT_SKIP_BR = os.path.join(RESOURCE, "skip_br.txt")
DEFAULT_SKIP_SAVE_LR_TO_STACK = os.path.join(RESOURCE, "skip_save_lr_to_stack.txt")
DEFAULT_SKIP_STP_FILE = os.path.join(RESOURCE, "skip_stp.txt")
DEFAULT_SKIP_FILE = os.path.join(DEBUG, "skip.txt")

DEFAULT_THREADS = multiprocessing.cpu_count()
#DEFAULT_THREADS = 1

BYTES_PER_INSN = 4

REG_FP = 29
REG_LR = 30
REG_SP = 31
REG_XZR = 31

# x0, ..., x7
NUM_ARG_REGS = 8

def skip_func(func, skip, skip_asm):
    # Don't instrument the springboard itself.
    # Don't instrument functions in asm files we skip.
    # Don't instrument certain functions (for debugging).
    return func.startswith('jopp_springboard_') or \
           func in skip_asm or \
           func in skip

def parse_last_insn(objdump, i, n):
    return [objdump.parse_insn(j) if objdump.is_insn(j) else None for j in xrange(i-n, i)]

def instrument(objdump, func=None, skip=set([]), skip_stp=set([]), skip_asm=set([]), skip_blr=set([]), threads=1):
    """
    Replace:
        BLR rX
    With:
        BL jopp_springboard_blr_rX

    Replace
        <assembled_c_function>:
          nop
          stp	x29, x30, [sp,#-<frame>]!
          (insns)
          mov	x29, sp
    With:
        <assembled_c_function>:
          eor RRX, x30, RRK
          stp x29, RRX, [sp,#-<frame>]!
          mov x29, sp
          (insns)

    Replace
        <assembled_c_function>:
          nop
          stp	x29, x30, [sp,#<offset>]
          (insns)
          add	x29, sp, #<offset>
    With:
        <assembled_c_function>:
          eor RRX, x30, RRK
          stp x29, RRX, [sp,#<offset>]
          add x29, sp, offset
          (insns)

    Replace:
        ldp x29, x30, ...
        nop
    With:
        ldp x29, RRX, ...
        eor x30, RRX, RRK


    """
    def __instrument(func=None, start_func=None, end_func=None, start_i=None, end_i=None,
            tid=None):
        def parse_insn_range(i, r):
            return [objdump.parse_insn(j) if objdump.is_insn(j) else None for j in xrange(i, r)]

        #
        # Instrumentation of function prologues.
        #
        #import pdb; pdb.set_trace()
        def instrument_stp(curfunc, func_i, i, stp_insn, new_stp_insn, add_x29_imm):
            """
            new_stp_insn(stp_insn, replaced_insn) -> new stp instruction to encode.
            """

            last_insn = parse_insn_range(i-1, i)
            if not are_nop_insns(last_insn):
                return

            offset = insn['args']['imm']

            # The last instruction was a nop spacer.
            # Move forward until we see "add x29, sp, #<...>".
            # (we expect ... = add_x29_imm)
            #mov_j, movx29_insn = find_add_x29_x30_imm(objdump, curfunc, func_i, i)
            #assert movx29_insn['args']['imm'] == add_x29_imm

            # Time to do prologue instrumentation.

            # eor RRX, x30, RRK
            eor = eor_insn(last_insn[0],
                    reg1=objdump.RRX, reg2=REG_LR, reg3=objdump.RRK)
            objdump.write(i-1, objdump.encode_insn(eor))
            # stp x29, RRX, ...
            stp = new_stp_insn(insn, insn)
            objdump.write(i, objdump.encode_insn(stp))
            # add x29, sp, #<add_x29_imm>
            #add = add_insn({},
                    #dst_reg=REG_FP, src_reg=REG_SP, imm12=add_x29_imm)
            #objdump.write(i, objdump.encode_insn(add))
            # nop
            #nop = nop_insn(movx29_insn)
            #objdump.write(mov_j, objdump.encode_insn(nop))

        def _skip_func(func):
            return skip_func(func, skip, skip_asm)

        last_func_i = [None]
        def each_insn():
            # Keep track of the last 2 instructions
            # (needed it for CONFIG_RKP_CFP_JOPP)
            for curfunc, func_i, i, insn, last_insns in objdump.each_insn(func=func, start_func=start_func, end_func=end_func, 
                    start_i=start_i, end_i=end_i, skip_func=_skip_func, num_last_insns=1):
                yield curfunc, func_i, i, insn, last_insns
                last_func_i[0] = func_i

        for curfunc, func_i, i, insn, last_insns in each_insn():
            if objdump.JOPP and func_i != last_func_i[0] and are_nop_insns(ins[1] for ins in last_insns):
                # Instrument the 2 noop spacers just before the function.
                #nargs_i, nargs_insn = last_insns[0]
                magic_i, magic_insn = last_insns[0]
                #nargs = objdump.get_nargs(func_i)
                #if nargs is None:
                    # nargs ought to be defined for everything eventually.  For symbol 
                    # we're not sure about yet, don't zero any argument registers.
                    #nargs = NUM_ARG_REGS
                #else:
                    # ARM64 calling standard dictates that only registers x0 ... x7 can 
                    # contain function parameters.
                    #
                    # If the C function takes more than these 8 registers (e.g. it takes 9 
                    # arguments), we only need to mark the first 8 as "don't clobber" (the 
                    # springboard code depends on this convention).
                    # 
                    #nargs = min(NUM_ARG_REGS, nargs)
                #objdump.write(nargs_i, nargs)
                objdump.write(magic_i, objdump.JOPP_MAGIC)

            if objdump.JOPP and insn['type'] == 'blr' and curfunc not in skip_blr :
                springboard_blr = 'jopp_springboard_blr_x{register}'.format(register=insn['args']['dst_reg'])
                insn = bl_insn(insn,
                        offset=objdump.func_offset(springboard_blr) - objdump.insn_offset(i))
                objdump.write(i, objdump.encode_insn(insn))
                continue
            elif objdump.ROPP_REARRANGE_PROLOGUE and insn['type'] == 'ldp' and \
                    insn['args']['reg1'] == REG_FP and \
                    insn['args']['reg2'] == REG_LR:
                forward_insn = parse_insn_range(i+1, i+2)
                if not are_nop_insns(forward_insn):
                    continue

                # stp x29, RRX, ...
                insn['args']['reg2'] = objdump.RRX
                stp = ((hexint(insn['binary']) >> 15) << 15) | \
                       (insn['args']['reg2'] << 10) | \
                       ((insn['args']['base_reg']) << 5) | \
                       (insn['args']['reg1'])
                objdump.write(i, stp)
                
                # eor x30, RRX, RRK
                eor = eor_insn(forward_insn[0],
                        reg1=REG_LR, reg2=objdump.RRX, reg3=objdump.RRK)
                objdump.write(i+1, objdump.encode_insn(eor))
                continue
            elif objdump.ROPP_REARRANGE_PROLOGUE and curfunc not in skip_stp and insn['type'] == 'stp' and \
                    insn['args']['reg1'] == REG_FP and \
                    insn['args']['reg2'] == REG_LR and \
                    insn['args']['imm'] > 0:
                def stp_x29_RRX_offset(insn, replaced_insn):
                    # stp x29, RRX, [sp,#<offset>]
                    offset = insn['args']['imm']
                    return stp_insn(replaced_insn,
                        reg1=REG_FP, reg2=objdump.RRX, base_reg=REG_SP, imm=offset, mode=STP_IMM_OFFSET)
                instrument_stp(curfunc, func_i, i, insn, stp_x29_RRX_offset, insn['args']['imm'])
                continue
            elif objdump.ROPP_REARRANGE_PROLOGUE and curfunc not in skip_stp and insn['type'] == 'stp' and \
                    insn['args']['reg1'] == REG_FP and \
                    insn['args']['reg2'] == REG_LR and \
                    insn['args']['imm'] < 0:
                def stp_x29_RRX_frame(insn, replaced_insn):
                    # stp x29, RRX, [sp,#-<frame>]!
                    frame = -1 * insn['args']['imm']
                    return stp_insn(replaced_insn,
                        reg1=REG_FP, reg2=objdump.RRX, base_reg=REG_SP, imm=-1 * frame, mode=STP_PRE_INDEXED)
                instrument_stp(curfunc, func_i, i, insn, stp_x29_RRX_frame, 0)
                continue
        objdump.flush()

    if threads == 1 or func:
        # Just use the current thread.
        if func:
            # Instrument a single function
            __instrument(func=func, tid=0)
        else: 
            # Instrument all functions
            __instrument(start_func=objdump.funcs()[0][0], tid=0)
        return

    objdump.each_insn_parallel(__instrument, threads)

def find_add_x29_x30_imm(objdump, curfunc, func_i, i):
    """
    Find the instruction that adjusts the frame pointer in the function prologue:

    add x29, x30, #...

    Start the search from i (inside curfunc) in the objdump.
    Throw an exception if we don't find it.
    """
    mov_j = None
    movx29_insn = None
    for j, ins in objdump.each_insn(end_func=curfunc, end_i=func_i, start_i=i+1, just_insns=True):
        if ins['type'] == 'add' and \
                'raw' not in ins['args'] and \
                ins['args']['dst_reg'] == REG_FP and \
                ins['args']['src_reg'] == REG_SP:
            mov_j = j
            movx29_insn = ins
            break
    if mov_j is None:
        raise RuntimeError("saw function prologue (nop, stp x29 x30) without mov x29 sp in {curfunc}".format(**locals()))
    return mov_j, movx29_insn

class Objdump(object):
    """
    Parse a vmlinux file, and apply instruction re-writes to a copy of it (or inplace).

    Makes heavy use of aarch64-linux-android-objdump output.
    i index in usage below is 0-based line number in aarch64-linux-android-objdump output.

    Usage:

    objdump = Objdump(vmlinux_filepath)
    objdump.parse()
    objdump.open()
    for i, insn in objdump.each_insn(func="stext"):
        if insn['type'] == 'bl':
            insn = bl_insn(insn, offset=32)
            objdump.write(i, objdump.encode_insn(insn))
    objdump.close()

    See "instrument" for implementation of actual hyperdrive instrumentations.
    """
    def __init__(self, vmlinux, kernel_src,
            config_file=None,
            RRK=RRK_DEFAULT, RRX=RRX_DEFAULT, RRS=RRS_DEFAULT,
            instr="{dirname}/{basename}.instr", inplace=False,
            make_copy=True, just_lines=False):
        self.vmlinux = vmlinux
        self.vmlinux_old = None
        self.kernel_src = kernel_src
        self.config_file = config_file
        self.conf = None
        self.nargs = None
        self.c_functions = set([])
        self.lines = []
        self.func_idx = {}
        self.func_addrs = set([])
        self._funcs = None
        self.sections = None
        self.make_copy = make_copy
        self.just_lines = just_lines

        #load config flags
        self._load_config()
        self.ROPP_REARRANGE_PROLOGUE = self.is_conf_set('CONFIG_RKP_CFP_ROPP')
        self.JOPP = self.is_conf_set('CONFIG_RKP_CFP_JOPP')
        self.JOPP_MAGIC = int(self.get_conf('CONFIG_RKP_CFP_JOPP_MAGIC'), 16)       

        self.RRK = RRK
        self.RRX = RRX
        self.RRS = RRS

        self.instr_copy = None
        if inplace:
            self.instr = self.vmlinux
        else:
            basename = os.path.basename(vmlinux)
            dirname = my_dirname(vmlinux)
            if dirname == '':
                dirname = '.'
            self.instr = instr.format(**locals())



    def _load_config(self):
        if self.config_file:
            self.conf = parse_config(self.config_file)

    def parse(self):
        """
        Read and save all lines from "aarch64-linux-android-objdump -d vmlinux".
        Read and save section information from "aarch64-linux-android-objdump -x".
        Keep track of where in the objdump output functions occur.
        """
        self.sections = parse_sections(self.vmlinux)
        fd, tmp = tempfile.mkstemp()
        os.close(fd)

        subprocess.check_call("{OBJDUMP} -d {vmlinux} > {tmp}".format(
            OBJDUMP=OBJDUMP, vmlinux=pipes.quote(self.vmlinux), tmp=pipes.quote(tmp)), shell=True)

        # NOTE: DON'T MOVE THIS.
        # We are adding to the objdump output symbols from the data section.
        symbols = parse_nm(self.vmlinux)
        for s in symbols.keys():
            sym = symbols[s]
            if sym[NE_TYPE] in ['t', 'T']:
                self.func_addrs.add(_int(sym[NE_ADDR]))
        """
        Find any assembly routines that appear inside the data section.
        
        1. List all symbols in vmlinux using nm
        2. Find all symbols in 1. that also both an "ENTRY(...)" and an "ENDPROC(...)" entry in 
           a .S Linux source file
        
        NOTE:
        The method we use isn't guaranteed to locate all such instances, but is a fast 
        approximation, and avoids having to instrument assembly .S files more than we 
        already do.
        
        e.g.
        In particular, there could be assembly code that doesn't use ENTRY/ENDPROC; 
        we rely on Linux coding standards for this.
        """
        symbols = parse_nm(self.vmlinux, symbols=parse_all_asm_functions(self.kernel_src))
        for s in sorted(symbols.keys(), key=lambda s: _int(symbols[s][NE_ADDR])):
            sym = symbols[s]
            if sym[NE_TYPE] in ['d', 'D']:
                # This symbol appears in the .data section but it's code since it's 
                # declared using both ENTRY and ENDPROC assembly routine markers
                start_address = '0x' + sym[NE_ADDR]
                stop_address = '0x' + _hex(_int(sym[NE_ADDR]) + sym[NE_SIZE]*BYTES_PER_INSN)
                subprocess.check_call(("{OBJDUMP} -D {vmlinux} "
                    "--start-address={start_address} --stop-address={stop_address} >> {tmp}").format(
                    OBJDUMP=OBJDUMP, vmlinux=pipes.quote(self.vmlinux), tmp=pipes.quote(tmp), 
                    start_address=start_address, stop_address=stop_address), shell=True)

        """
        Now process objdump output and extract information into self.lines:

        self.func_idx mapping from symbol name to a set of indicies into self.lines where 
        that symbol is defined (there can be multiple places with the same symbol / 
        function name in objdump).

        self.lines is tuple of:
        1. The line itself
        2. Which section each instructions occurs in
        3. Virtual addresses of instructions
        """
        section_idx = None
        with open(tmp, 'r') as f:
            for i, line in enumerate(f):
                virt_addr = None
                m = re.search(virt_addr_re, line)
                if m:
                    virt_addr = _int(m.group('virt_addr'))

                self.lines.append((line, section_idx, virt_addr))

                m = re.search(r'Disassembly of section (?P<name>.*):', line)
                if m:
                    section_idx = self.sections['section_idx'][m.group('name')]
                    continue

                m = re.search(common.fun_rec, line)
                if m:
                    if m.group('func_name') not in self.func_idx:
                        self.func_idx[m.group('func_name')] = set()
                    self.func_idx[m.group('func_name')].add(i)
                    continue
        # We have all the objdump lines read, we can delete the file now.
        self._copy_to_tmp(tmp, 'objdump.txt')
        os.remove(tmp)

    def _copy_to_tmp(self, from_path, to_basename):
        """
        Copy file to dirname(vmlinux)/tmp/basename(filename)
        (e.g. tmp directory inside where vmlinux is)
        """
        vfile = os.path.join(my_dirname(self.vmlinux), 'scripts/rkp_cfp/tmp', to_basename)
        subprocess.check_call(['mkdir', '-p', my_dirname(vfile)])
        subprocess.check_call(['cp', from_path, vfile])
        return vfile

    def save_instr_copy(self):
        """
        Copy vmlinux_instr to dirname(vmlinux)/tmp/vmlinux.instr
        (mostly for debugging)
        """
        self.instr_copy = self._copy_to_tmp(self.instr, 'vmlinux.instr')

    def open(self):
        """
        mmap vmlinux for reading and vmlinux.instr for writing instrumented instructions.
        """
        if os.path.abspath(self.vmlinux) != os.path.abspath(self.instr):
            subprocess.check_call(['cp', self.vmlinux, self.instr])

        if self.make_copy:
            # copy vmlinux to tmp/vmlinux.old (needed for validate_instrumentation)
            self.vmlinux_old = self._copy_to_tmp(os.path.join(my_dirname(self.vmlinux), 'vmlinux'), 'vmlinux')
            self._copy_to_tmp(os.path.join(my_dirname(self.vmlinux), '.config'), '.config')

        self.write_f = open(self.instr, 'r+b')
        self.write_f.flush()
        self.write_f.seek(0)
        self.write_mmap = mmap.mmap(self.write_f.fileno(), 0, access=mmap.ACCESS_WRITE)

        self.read_f = open(self.vmlinux, 'rb')
        self.read_f.flush()
        self.read_f.seek(0)
        self.read_mmap = mmap.mmap(self.read_f.fileno(), 0, access=mmap.ACCESS_READ)

    def __getstate__(self):
        """
        For debugging.  Don't pickle non-picklable attributes.
        """
        d = dict(self.__dict__)
        del d['write_f']
        del d['write_mmap']
        del d['read_f']
        del d['read_mmap']
        del d['_funcs']
        return d
    def __setstate__(self, d):
        self.__dict__.update(d)

    def insn_offset(self, i):
        """
        Virtual address of 
        """
        return self.parse_insn(i)['virt_addr']

    def _insn_idx(self, i):
        """
        Return the byte address into the file vmlinux.instr for the instruction at 
        self.line(i).

        (this is all the index into an mmap of the file).
        """
        virt_addr = self.virt_addr(i)
        section_file_offset = self._section(i)['offset']
        section_virt = self._section(i)['address']
        return section_file_offset + (virt_addr - section_virt)

    def read(self, i, size=4):
        """
        Read a 32-bit instruction into a list of chars in big-endian.
        """
        idx = self._insn_idx(i)
        insn = list(self.read_mmap[idx:idx+size])
        # ARM uses little-endian.  
        # Need to flip bytes around since we're reading individual chars.
        flip_endianness(insn)
        return insn

    def write(self, i, insn):
        """
        Write a 32-bit instruction back to vmlinux.instr.

        insn can be a list of 4 chars or an 32-bit integer in big-endian format.
        Converts back to little-endian (ARM's binary format) before writing.
        """
        size = 4
        idx = self._insn_idx(i)
        insn = list(byte_string(insn))
        flip_endianness(insn)
        self.write_mmap[idx:idx+size] = byte_string(insn)

    def write_raw(self, i, insn):
        self.write(i, insn['binary'], size=4)

    def read_raw(self, hexaddr, size):
        section = addr_to_section(hexaddr, self.sections['sections'])
        offset = _int(hexaddr) - section['address'] + section['offset']
        bytes = list(self.read_mmap[offset:offset+size])
        flip_endianness(bytes)
        return bytes

    def close(self):
        self.flush()
        
        self.write_mmap.close()
        self.write_f.close()
        self.read_mmap.close()
        self.read_f.close()

        self.write_mmap = None
        self.write_f = None
        self.read_mmap = None
        self.read_f = None

    def is_conf_set(self, var):
        if self.conf is None:
            return None
        return self.conf.get(var) == 'y'

    def get_conf(self, var):
        if self.conf is None:
            return None
        return self.conf.get(var) 

    def flush(self):
        self.write_mmap.flush()
        self.write_f.flush()

    def line(self, i):
        """
        Return the i-th (0-based) line of output from "aarch64-linux-android-objdump -d vmlinux".

        (no lines are filtered).
        """
        return self.lines[i][0]
    def _section_idx(self, i):
        return self.lines[i][1]
    def virt_addr(self, i):
        return self.lines[i][2]

    def section(self, section_name):
        """
        >>> self.section('.text')
        {'address': 18446743798847711608L,
         'align': 1,
         'lma': 18446743798847711608L,
         'name': '.text',
         'number': 23,
         'offset': 15608184,
         'size': '0017aae8',
         'type': 'ALLOC'},
        """
        return self.sections['sections'][self.sections['section_idx'][section_name]]

    def _section(self, i):
        return self.sections['sections'][self._section_idx(i)]

    def is_func(self, i):
        return bool(self.get_func(i))
    def is_func_addr(self, func_addr):
        return func_addr in self.func_addrs
    def get_func(self, i):
        return re.search(common.fun_rec, self.line(i))

    def is_insn(self, i):
        """
        Returns True if self.line(i) is an instruction
        (i.e. not a function label line, blank line, etc.)
        """
        return not self.is_func(i) and self.virt_addr(i) is not None

    FUNC_OFFSET_RE = re.compile(r'^(?P<virt_addr>{hex_re})'.format(hex_re=hex_re))
    def get_func_idx(self, func, i=None):
        i_set = self.func_idx[func]
        if len(i_set) != 1 and i is None:
            raise RuntimeError("{func} occurs multiple times in vmlinux, specify which line from objdump you want ({i_set})".format(**locals()))
        elif i is None:
            i = iter(i_set).next()
        else:
            assert i in i_set
        return i
    def get_func_end_idx(self, func, start_i=None):
        i = self.get_func_idx(func, start_i)
        while i < len(self.lines) and ( self.is_func(i) or self.is_insn(i) ):
            i += 1
        return i - 1
    def func_offset(self, func, i=None):
        i = self.get_func_idx(func, i)
        m = re.search(Objdump.FUNC_OFFSET_RE, self.line(i))
        return _int(m.group('virt_addr'))

    PARSE_INSN_RE = re.compile((
        r'(?P<virt_addr>{hex_re}):\s+'
        r'(?P<hex_insn>{hex_re})\s+'
        r'(?P<type>[^\s]+)\s*'
        ).format(hex_re=hex_re))
    def parse_insn(self, i):
        """
        Parse the i-th line of objdump output into a python dict.

        e.g.
        >>> self.line(...)
        [2802364][97 DB 48 D0] :: ffffffc0014ee5b4:     97db48d0        bl      ffffffc000bc08f4 <rmnet_vnd_exit>

        >>> self.parse_insn(2802364)
        {'args': {'offset': -9624768},             # 'args' field varies based on instruction type.
         'binary': ['\x97', '\xdb', 'H', '\xd0'],  # The remaining fields are always present.
         'hex_insn': '97db48d0',
         'type': 'bl',
         'virt_addr': 18446743798853592500L}
        """
        line = self.line(i)
        m = re.search(Objdump.PARSE_INSN_RE, line)
        insn = m.groupdict()
        insn['virt_addr'] = _int(insn['virt_addr'])
        insn['binary'] = self.read(i)

        insn['args'] = {}
        if insn['type'] == 'bl':
            # imm26 (bits 0..25)
            insn['args']['offset'] = from_twos_compl((hexint(insn['binary']) & BL_OFFSET_MASK) << 2, nbits=26 + 2)
        elif insn['type'] in set(['blr', 'ret']):
            arg = {
            'blr':'dst_reg',
            'ret':'target_reg',
            }[insn['type']]
            insn['args'][arg] = (hexint(insn['binary']) & BLR_AND_RET_RN_MASK) >> 5
        elif insn['type'] == 'stp':
            insn['args']['reg1']     = mask_shift(insn , STP_RT_MASK              , 0)
            insn['args']['base_reg'] = mask_shift(insn , STP_RN_MASK              , 5)
            insn['args']['reg2']     = mask_shift(insn , STP_RT2_MASK             , 10)
            insn['args']['opc']      = mask_shift(insn , STP_OPC_MASK             , 30)
            insn['args']['mode']     = mask_shift(insn , STP_ADDRESSING_MODE_MASK , 22)
            lsl_bits = stp_lsl_bits(insn)
            insn['args']['imm'] = from_twos_compl(
                    ((hexint(insn['binary']) & STP_IMM7_MASK) >> 15) << lsl_bits, 
                    nbits=7 + lsl_bits)
        elif mask_shift(insn, ADDIMM_OPCODE_MASK, 24) == ADDIM_OPCODE_BITS \
                and insn['type'] in set(['add', 'mov']):
            insn['type'] = 'add'
            insn['args']['sf'] = mask_shift(insn, ADDIMM_SF_MASK, 31)
            insn['args']['shift'] = mask_shift(insn, ADDIMM_SHIFT_MASK, 22)
            insn['args']['imm'] = mask_shift(insn, ADDIMM_IMM_MASK, 10)
            insn['args']['src_reg'] = mask_shift(insn, ADDIMM_RN_MASK, 5)
            insn['args']['dst_reg'] = mask_shift(insn, ADDIMM_RD_MASK, 0)
            insn['args']['opcode_bits'] = mask_shift(insn, ADDIMM_OPCODE_MASK, 24)
        elif insn['type'] == 'adrp':
            immlo = mask_shift(insn, ADRP_IMMLO_MASK, ADRP_IMMLO_ZBITS)
            immhi = mask_shift(insn, ADRP_IMMHI_MASK, ADRP_IMMHI_ZBITS)
            insn['args']['dst_reg'] = mask_shift(insn, ADRP_RD_MASK, ADRP_RD_ZBITS)
            insn['args']['imm'] = from_twos_compl((immhi << (2 + 12)) | (immlo << 12), nbits=2 + 19 + 12)
        elif insn['type'] == 'ldp':
            insn['args']['reg1']     = mask_shift(insn , STP_RT_MASK              , 0)
            insn['args']['base_reg'] = mask_shift(insn , STP_RN_MASK              , 5)
            insn['args']['reg2']     = mask_shift(insn , STP_RT2_MASK             , 10)
        elif mask_shift(insn, ADDIMM_OPCODE_MASK, 24) == ADDIM_OPCODE_BITS \
                and insn['type'] in set(['add', 'mov']):
                insn['type'] = 'add'
                insn['args']['sf'] = mask_shift(insn, ADDIMM_SF_MASK, 31)
                insn['args']['shift'] = mask_shift(insn, ADDIMM_SHIFT_MASK, 22)
                insn['args']['imm'] = mask_shift(insn, ADDIMM_IMM_MASK, 10)
                insn['args']['src_reg'] = mask_shift(insn, ADDIMM_RN_MASK, 5)
                insn['args']['dst_reg'] = mask_shift(insn, ADDIMM_RD_MASK, 0)
                insn['args']['opcode_bits'] = mask_shift(insn, ADDIMM_OPCODE_MASK, 24)
        else:
            insn['args']['raw'] = line[m.end():]
        return insn

    def encode_insn(self, insn):
        """
        Given a python dict representation of an instruction (see parse_insn), write its 
        binary to vmlinux.instr.

        TODO:
        stp x29, xzr, [sp,#<frame>]
        str x30, [sp,#<frame - 8>]
        add x29, sp, offset
        """
        if insn['type'] == 'eor':
            upper_11_bits =0b11001010000
            return (upper_11_bits << 21) | (insn['args']['reg3'] << 16) | (0b000000<<10) | \
                    (insn['args']['reg2'] << 5) |(insn['args']['reg1'])
        elif insn['type'] == 'ldp':
            return (0b1010100111 << 22) | (insn['args']['reg2'] << 10) | \
                    (insn['args']['base_reg'] << 5) | (insn['args']['reg1'])
        elif insn['type'] in ['bl', 'b']:
            # BL: 1 0 0 1 0 1 [ imm26 ]
            #  B: 0 0 0 1 0 1 [ imm26 ]
            upper_6_bits = {
                'bl':0b100101,
                 'b':0b000101,
            }[insn['type']]
            assert 128*1024*1024 >= insn['args']['offset'] >= -128*1024*1024
            return ( upper_6_bits << 26 ) | (to_twos_compl(insn['args']['offset'], nbits=26 + 2) >> 2)
        elif insn['type'] in ['blr', 'ret']:
            #      1 1 0 1 0 1 1 0 0  [ op ] 1 1 1 1 1 0 0 0 0 0 0 [   Rn   ] 0 0 0 0 0
            # BLR:                     0  1                                            
            # RET:                     1  0                                            
            op = {
                'blr':0b01,
                'ret':0b10,
            }[insn['type']]
            assert 0 <= insn['args']['dst_reg'] <= 2**5 - 1
            return (0b110101100 << 25) | \
                   (op << 21) | \
                   (0b11111000000 << 10) | \
                   (insn['args']['dst_reg'] << 5)
        elif insn['type'] == 'ret':
            # 1 1 0 1 0 1 1 0 0 0 1 1 1 1 1 1 0 0 0 0 0 0 [  Rn   ] 0 0 0 0 0
            assert 0 <= insn['args']['dst_reg'] <= 2**5 - 1
            return (0b1101011000111111000000 << 9) | (insn['args']['dst_reg'] << 4)
        elif insn['type'] == 'stp':
            assert insn['args']['opc'] == OPC_64
            return (insn['args']['opc'] << 30) | \
                   (insn['args']['mode'] << 22) | \
                   (to_twos_compl(insn['args']['imm'] >> stp_lsl_bits(insn), nbits=7) << 15) | \
                   (insn['args']['reg2'] << 10) | \
                   (insn['args']['base_reg'] << 5) | \
                   insn['args']['reg1']
        elif insn['type'] == 'str' and \
                insn['args']['mode'] == STR_IMM_OFFSET and \
                insn['args']['sign'] == STR_SIGN_UNSIGNED:
            assert insn['args']['imm'] >= 0
            return (insn['args']['size'] << 30) | \
                   (0b111001 << 24) | \
                   (insn['args']['opc'] << 22) | \
                   (to_twos_compl(insn['args']['imm'] >> str_lsl_bits(insn), nbits=12) << 10) | \
                   (insn['args']['base_reg'] << 5) | \
                   (insn['args']['reg1'] << 0)
        elif insn['type'] == 'add' and insn['args']['opcode_bits'] == ADDIM_OPCODE_BITS:
            assert insn['args']['sf'] == ADDIMM_SF_BIT_64
            return \
                (insn['args']['sf'] << 31) | \
                (insn['args']['shift'] << 22) | \
                (insn['args']['imm'] << 10) | \
                (insn['args']['src_reg'] << 5) | \
                (insn['args']['dst_reg'] << 0) | \
                (insn['args']['opcode_bits'] << 24)
        elif insn['type'] in ['mov', 'movk', 'movn']:
            opc = {
                'mov':0b10,
                'movk':0b11,
                'movn':0b00,
            }[insn['type']]
            hw = {
                0:0b00,
                16:0b01,
                32:0b11,
            }[insn['args']['shift']]
            sf = 0b1 # 64-bit registers
            return (sf << 31) | \
                   (opc << 29) | \
                   (0b100101 << 23) | \
                   (hw << 21) | \
                   (insn['args']['imm16'] << 5) | \
                   (insn['args']['dst_reg'])
        elif insn['type'] == 'nop':
            return 0xd503201f
        raise NotImplementedError

    def _print_func(self, func_name,
            parse_insn=True):
        """
        Output parse objdump lines the internal indices (i) of those lines.
        Always ouput python dict representations of instructions.
        Useful for debugging / learning how this class works.

        >>> self.print_func('rmnet_exit')
        [2802357][A9 BF 7B FD] :: ffffffc0014ee598:     a9bf7bfd        stp     x29, x30, [sp,#-16]!
        [2802358][91 00 03 FD] :: ffffffc0014ee59c:     910003fd        mov     x29, sp
        ...

        Parsed instructions:
        {'args': {'raw': 'x29, x30, [sp,#-16]!'},
         'binary': ['\xa9', '\xbf', '{', '\xfd'],
         'hex_insn': 'a9bf7bfd',
         'id': 2802357,
         'type': 'stp',
         'virt_addr': 18446743798853592472L}
        {'args': {'raw': 'x29, sp'},
         'binary': ['\x91', '\x00', '\x03', '\xfd'],
         'hex_insn': '910003fd',
         'id': 2802358,
         'type': 'mov',
         'virt_addr': 18446743798853592476L}
         ...
        """
        def byte_to_hex(byteStr):
            return ''.join(["%02X " % ord(x) for x in byteStr]).strip()
        parsed_insns = []
        for i, insn in self.each_insn(func=func_name):
            objdump_line = self.line(i)
            hex_insn = byte_to_hex(insn['binary'])
            log("[{i}][{hex_insn}] :: {objdump_line}".format(**locals()))
        if parse_insn:
            log() 
            log("Parsed instructions:")
            for i, insn in self.each_insn(func=func_name):
                insn['id'] = i
                pr(insn)

    def num_funcs(self):
        return len(self.func_idx)

    def funcs(self):
        """
        [ ("func_1", 0), ("func_2", 1), ... ]
        """
        def __funcs():
            for func, i_set in self.func_idx.iteritems():
                for i in i_set:
                    yield func, i
        funcs = list(__funcs())
        funcs.sort(key=lambda func_i: func_i[1])
        return funcs

    def _idx_to_func(self, i):
        if self._funcs is None:
            self._funcs = self.funcs()
        lo = 0
        hi = len(self._funcs) - 1
        mi = None
        def func(i):
            return self._funcs[i][0]
        def idx(i):
            return self._funcs[i][1]
        while lo <= hi:
            mi = (hi + lo)/2
            if i < idx(mi):
                hi = mi-1
            elif i > idx(mi):
                lo = mi+1
            else:
                return i
        assert lo == hi + 1
        return idx(hi)

    def each_func_lines(self, 
            # Number of past instruction lines to yield with the current one.
            num_last_insns=None,
            with_func_i=False):

        last_insns = [] if num_last_insns is None else [None] * num_last_insns

        def is_insn(line):
            return not re.search(common.fun_rec, line) and re.search(virt_addr_re, line)
        def each_line():
            for i, line in enumerate(self.lines):
                line = line[0]
                yield i, line
                if not num_last_insns or not is_insn(line):
                    continue
                last_insns.pop(0)
                last_insns.append(line)
        last_func_insns = list(last_insns)

        func_lines = []
        func = None
        func_i = None
        def _tuple(func_i, func, func_lines, last_func_insns):
            func_tup = None
            if with_func_i:
                func_tup = (func_i, func)
            else:
                func_tup = (func,)
            if num_last_insns:
                return func_tup + (func_lines, last_func_insns)
            return func_tup + (func_lines,)
        for i, line in each_line():
            m = re.search(common.fun_rec, line)
            if m:
                if func is not None:
                    yield _tuple(func_i, func, func_lines, last_func_insns)
                if num_last_insns:
                    last_func_insns = list(last_insns)
                func_lines = []
                func = m.group('func_name')
                func_i = i
            elif func is None:
                continue
            func_lines.append(line)
        if func is not None:
            yield _tuple(func_i, func, func_lines, last_func_insns)

    def each_insn(self, 
            # Instrument a single function.
            func=None, 
            # Instrument a range of functions.
            start_func=None, end_func=None, 
            # Start index into objdump of function (NEED this to disambiguate duplicate symbols)
            start_i=None, end_i=None,
            # If skip_func(func_name), skip it.
            skip_func=None,
            just_insns=False,
            # Don't parse instruction, just give raw objdump line.
            raw_line=False,
            # Number of past instruction lines to yield with the current one.
            num_last_insns=None,
            debug=False):
        """
        Iterate over instructions (i.e. line indices and their parsed python dicts).

        Default is entire file, but can be limited to just a function.
        """
        if func:
            start_func = func
            end_func = func

        i = 0
        if start_func is not None:
            i = self.get_func_idx(start_func, start_i)
        elif start_i is not None:
            i = start_i
            start_func = self._idx_to_func(i)
        else:
            # The first function
            start_func, i = self.funcs()[0]
        func_i = i
        curfunc = start_func

        end = len(self.lines) - 1
        if end_func is not None:
            end = self.get_func_end_idx(end_func, end_i)
        assert not( end_func is None and end_i is not None )
        
        def should_skip_func(func):
            return skip_func is not None and skip_func(func)
        assert start_func is not None

        last_insns = None
        num_before_start = 0
        if num_last_insns is not None:
            last_insns = [(None, None)] * num_last_insns
            assert len(last_insns) == num_last_insns
            # Walk backwards from the start until we see num_last_insns instructions.
            # j is the index of the instruction.
            # That new starting point (i) will be that many instructions back.
            n = num_last_insns
            j = i - 1
            while n > 0 and j > 0:
                if self.is_insn(j):
                    n -= 1
                j -= 1
            new_i = j + 1
            num_before_start = i - new_i + 1
            i = new_i
        def shift_insns_left(last_insns, i, to_yield):
            last_insns.pop(0)
            last_insns.append((i, to_yield))

        do_skip_func = should_skip_func(start_func)
        def _tup(i, curfunc, func_i, to_yield):
            if just_insns:
                return i, to_yield
            else:
                return curfunc, func_i, i, to_yield

        def _parse(to_yield, i):
            if to_yield is None:
                return self.line(i) if raw_line else self.parse_insn(i)
            return to_yield

        for i in xrange(i, min(end, len(self.lines) - 1) + 1):

            to_yield = None

            if num_last_insns is not None and num_before_start != 0:
                if self.is_insn(i):
                    to_yield = _parse(to_yield, i)
                    shift_insns_left(last_insns, i, to_yield)
                num_before_start -= 1
                continue

            if self.is_insn(i):
                to_yield = _parse(to_yield, i)
                if not do_skip_func:
                    t = _tup(i, curfunc, func_i, to_yield)
                    if num_last_insns is not None:
                        yield t + (last_insns,)
                    else:
                        yield t
                if num_last_insns is not None:
                    shift_insns_left(last_insns, i, to_yield)
            else:
                m = self.get_func(i)
                if m:
                    curfunc = m.group('func_name')
                    func_i = i
                    do_skip_func = should_skip_func(curfunc)

    def each_insn_parallel(self, each_insn, threads=1, **kwargs):
        """
        each_insn(start_func=None, end_func=None, start_i=None, end_i=None)
        """
        # Spawn a bunch of threads to instrument in parallel.
        procs = []
        i = 0
        funcs = self.funcs()
        chunk = int(math.ceil(len(funcs)/float(threads)))
        for n in xrange(threads):
            start_func_idx = i
            end_func_idx = min(i+chunk-1, len(funcs)-1)
            start_i = funcs[start_func_idx][1]
            end_i = funcs[end_func_idx][1]
            kwargs.update({
                'start_func':funcs[start_func_idx][0],
                'end_func':funcs[end_func_idx][0],
                'start_i':start_i,
                'end_i':end_i,
                'tid':n,
                })
            if threads == 1:
                each_insn(**kwargs)
                return
            proc = multiprocessing.Process(target=each_insn, kwargs=kwargs)

            # pr(kwargs)
            # log("{start_i} {end_i} [start_i, end_i]".format(**locals()))

            i = end_func_idx + 1
            proc.start()
            procs.append(proc)
        for proc in procs:
            proc.join()

def each_procline(proc):
    """
    Iterate over the stdout lines of subprocess.Popen(...).
    """
    while True:
        line = proc.stdout.readline()
        if line != '':
            yield line.rstrip()
        else:
            break

"""
Replace an instruction with a new one.

These functions modify the python dict's returned by Objdump.each_insn.
Instructions can then be written to vmlinux.instr using Objdump.write(i, Objdump.encode_insn(insn)).
"""
def bl_insn(insn, offset):
    return _jmp_offset_insn(insn, 'bl', offset)
def b_insn(insn, offset):
    return _jmp_offset_insn(insn, 'b', offset)
def _jmp_offset_insn(insn, typ, offset):
    insn['type'] = typ
    insn['args'] = { 'offset': offset }
    return insn
def _mov_insn(insn, typ, dst_reg, imm16, shift):
    insn['type'] = typ
    insn['args'] = { 'dst_reg': dst_reg, 'imm16': imm16, 'shift': shift }
    return insn
def mov_insn(insn, dst_reg, imm16, shift=0):
    return _mov_insn(insn, 'mov', dst_reg, imm16, shift)
def movk_insn(insn, dst_reg, imm16, shift=0):
    return _mov_insn(insn, 'movk', dst_reg, imm16, shift)
def movn_insn(insn, dst_reg, imm16, shift=0):
    return _mov_insn(insn, 'movn', dst_reg, imm16, shift)
def movreg_insn(insn, dst_reg, src_reg):
    return add_insn(insn, dst_reg, src_reg, 0)
def add_insn(insn, dst_reg, src_reg, imm12):
    insn['type'] = 'add'
    insn['args'] = {
                'sf':ADDIMM_SF_BIT_64,
                'shift':0,
                'imm':imm12,
                'src_reg':src_reg,
                'dst_reg':dst_reg,
                'opcode_bits':ADDIM_OPCODE_BITS,
            }
    return insn
def nop_insn(insn):
    insn['type'] = 'nop'
    insn['args'] = {}
    return insn
def eor_insn(insn, reg1, reg2, reg3):
    insn['type'] = 'eor'
    insn['args'] = {
        'reg1':reg1,
        'reg2':reg2,
        'reg3':reg3,
    }
    return insn

def str_imm_unsigned_preindex_insn(insn, reg1, base_reg, imm):
    """
    STR (immediate)
    Unsigned offset
    11    111   0  01    00    000000000111 11111 11110

    31:30 29:27 26 25:24 23:22        21:10   9:5   4:0
    size           sign  opc          imm12    Rn    Rt

    size = 11 (64-bit)
           10 (32-bit)

    mode = 01

    sign = 01 (unsigned offset)

    ffffffc000097a8c:	f9001fbe 	str	x30, [x29,#56]
    11 111 0 01 00 0 00000 000 1 11 11101 11110
    """
    insn['type'] = 'str'
    insn['args'] = {
        'mode':STR_IMM_OFFSET,
        'size':STR_SIZE_64,
        'sign':STR_SIGN_UNSIGNED,
        'reg1':reg1,
        'base_reg':base_reg,
        # 0b00 for all STR (imm) forms
        'opc':0b00,
        'imm':imm,
    }
    return insn
def stp_insn(insn, reg1, reg2, base_reg, imm, mode):
    insn['type'] = 'stp'
    insn['args'] = {
        'reg1':reg1,
        'base_reg':base_reg,
        'reg2':reg2,
        'opc':OPC_64,
        'mode':mode,
        'imm':imm,
    }
    return insn

def print_bl_offsets(objdump):
    for curfunc, func_i, i, insn in objdump.each_insn():
        if insn['type'] == 'bl':
            log(objdump.line(i))
            log("  instr-bits = 0x{offset:x}".format(offset=hexint(insn['binary']) & BL_OFFSET_MASK))
            log("  actual-offset = {offset}".format(offset=insn['args']['offset']))

# For tests, limit instrumentation to a known function in the source code that we can 
# trigger once the kernel is up and running.
DEFAULT_TEST_FUNC = 'tima_read'
def test_instrument(objdump):
    """
    1. read the elf file header, to determine where text section is mapped to in vmlinux
    2. mmap the text section
    3. create a mapping between:
       objdump output lines from vmlinux and mmap'ed text words
    4. iterate until we find tima_read
       for func in functions:
         if func == 'tima_read':
           for insn in instructions:
             if insn['type'] = 'bl':
               # need to do calculation of offset from this bl instruction to print_something function
               dump[idx] = bl print_something
         
    5. instrument the first bl instruction to jump to the springboard
    """
    for curfunc, func_i, i, insn in objdump.each_insn(func=DEFAULT_TEST_FUNC):
        if insn['type'] == 'bl':
            insn['args']['offset'] = objdump.func_offset('print_something') - objdump.insn_offset(i)
            objdump.write(i, objdump.encode_insn(insn))
            break

class GDBError(Exception):
    pass
def list_symbol_locations(hex_func_pairs, obj, gdb=GDB):
    """
    Use GDB to figure out where a symbol is in the source.
    symbols can be hex addresses or symbol names.
    """
    fd, tmp = tempfile.mkstemp()
    os.close(fd)
    gdbcmds = open(tmp)
    if os.path.exists('.gdbinit') and os.getcwd() != os.path.expandvars('$HOME'):
        raise RuntimeError("WARNING: .gdbinit is in this directory, run from another place to avoid sourcing it.")
    with open(tmp, 'w') as gdbcmds:
        def write(string, **kwargs):
            gdbcmds.write(textwrap.dedent(string.format(**kwargs)))
        write("""
        set pagination off
        set listsize 1
        """)
        # This is slow...
        # info function ^{func}$
        for hexaddr, func in hex_func_pairs:
            write("""
            list *(0x{hexaddr})
            echo > SYMBOL\\n
            """, hexaddr=strip_hex_prefix(hexaddr), func=func)
        write("quit")

    # 0x28ca40 is in set_in_fips_err (crypto/testmgr.c:163).
    skip_until_match = r'^0x[0-9a-f]+ is in '
    matched = [False]
    def should_skip(line):
        if matched[0] or re.search(skip_until_match, line):
            matched[0] = True
        return not matched[0]

    def should_error(line):
        return re.search(r'Reading symbols from .*\(no debugging symbols found\)', line)

    proc = subprocess.Popen([gdb, obj, '-x', tmp], 
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    def _tuple(m):
        symbol = strip_hex_prefix(m.group('symbol'))
        return symbol, m.group('filepath'), int(m.group('lineno'))
    tup = (None, None, None)
    possible_files = set([])
    for line in iter(proc.stdout.readline, ''):
        if should_error(line):
            raise GDBError("ERROR: " + line)
        elif should_skip(line):
            continue
        elif re.search(r'^> SYMBOL', line):
            list_sym, list_path, list_lineno = tup
            path = None
            if (list_path is None or list_path.endswith('.h')) and len(possible_files) == 1:
                path = iter(possible_files).next()
            else:
                path = list_path
            yield list_sym, path, list_lineno
            tup = (None, None, None)
            possible_files = set([])
            continue


        # File net/sunrpc/cache.c:
        m = re.search(r'^File (?P<filepath>[^:]+):$', line)
        if m:
            possible_files.add(m.group('filepath'))

        # print line
        # 0xffffffc000bf7f20 is in boot_alloc_snapshot (kernel/trace/trace.c:208).
        m = re.search(r'^(?P<symbol>[^\s]+) is in [^\s]+ \((?P<filepath>.*):(?P<lineno>\d+)\)\.', line)
        if m:
            tup = _tuple(m)
            continue
        # 0xffffffc000080000 is at arch/arm64/kernel/head.bl.S:129.
        # 129             add     x13, x18, #0x16
        m = re.search(r'^(?P<symbol>.*) is at (?P<filepath>.*):(?P<lineno>\d+)\.', line)
        if m:
            tup = _tuple(m)
            continue

NM_RE = re.compile(r'(?P<addr>.{16}) (?P<symbol_type>.) (?P<symbol>.*)')
NE_TYPE = 0
NE_ADDR = 1
NE_SIZE = 2
def parse_nm(vmlinux, symbols=None):
    """
    MAJOR TODO:
    Must handle functions (symbols) that occur more than once!
    e.g. 
    add_dirent_to_buf is a static function defined in both:
    - fs/ext3/namei.c
    - fs/ext4/namei.c

    ffffffc0000935b0 T cpu_resume_mmu
    ffffffc0000935c0 t cpu_resume_after_mmu
    ...
    ffffffc0000935f0 D cpu_resume
    ...
    ffffffc000093680 T __cpu_suspend_save
    ...
    ffffffc000c60000 B _end
                     U el
                     U lr

    {
      'cpu_resume':('D', 'ffffffc0000935f0', 36)
    }
    """
    proc = subprocess.Popen(["{NM} {vmlinux} | sort".format(NM=NM, vmlinux=vmlinux)], shell=True, stdout=subprocess.PIPE)
    f = each_procline(proc)
    nm = {}
    last_symbol = None
    last_name = None
    for line in f:
        m = re.search(NM_RE, line)
        if m:
            if last_symbol is not None and ( symbols is None or last_name in symbols ):
                last_symbol[NE_SIZE] = ( _int(m.group('addr')) - _int(last_symbol[NE_ADDR]) ) / BYTES_PER_INSN \
                            if \
                                re.match(hex_re, last_symbol[NE_ADDR]) and \
                                re.match(hex_re, m.group('addr')) \
                            else None
            last_symbol = [m.group('symbol_type'), m.group('addr'), None]
            last_name = m.group('symbol')
            if symbols is None or m.group('symbol') in symbols:
                nm[m.group('symbol')] = last_symbol
    return nm

def parse_plain_labels(asm_file):
    labels = set([])
    for line in each_line(asm_file):
        m = re.search(r'^\s*(?:(?P<label>{ident_re}):)'.format(ident_re=common.ident_re), line)
        if m:
            labels.add(m.group('label'))
            continue
    return labels
def parse_asm(asm_file, 
        # If true, just parse functions
        functions=None, 
        # If true, just parse nargs
        nargs=None):
    """
    Find all "functions" in an assembly file.
    We consider a function to be something like:

    ENTRY(func):
      ...
    ENDPROC(func)

    NOTE: we don't consider this a function:
    func:
       ...
    """
    endproc = set([])
    entry = set([])
    func_entry = set([])
    vector_entry = set([])
    narg = {}
    if not functions and not nargs:
        functions = True
        nargs = True 
    def add_narg(m):
        symbol, n = re.split(r', ', m.group('symbol'))
        if nargs:
            narg[symbol] = (asm_file, int(n))
        return symbol
    def asm_pattern(macroname):
        return r'^\s*' + macroname + r'\((?P<symbol>[^)\\]+)\)'
    for line in each_line(asm_file):
        m = re.search(asm_pattern('FUNC_ENTRY'), line)
        if m:
            symbol = add_narg(m)
            func_entry.add(symbol)
            continue
        m = re.search(asm_pattern('FUNC_NARGS'), line)
        if m:
            add_narg(m)
            continue
        m = re.search(asm_pattern('ENTRY'), line)
        if m:
            entry.add(m.group('symbol'))
            continue
        m = re.search(asm_pattern('VECTOR_ENTRY'), line)
        if m:
            vector_entry.add(m.group('symbol'))
            continue
        m = re.search(asm_pattern('ENDPROC'), line)
        if m:
            endproc.add(m.group('symbol'))
            continue
    funcs = endproc.intersection(
            (entry.union(func_entry)).difference(
                vector_entry))
    if functions and not nargs:
        return funcs
    elif not functions and nargs:
        return narg
    return funcs, narg
def kernel_files(root, glob):
    for root, dirnames, filenames in os.walk(root):
        for filename in fnmatch.filter(filenames, glob):
            yield os.path.join(root, filename)
def asm_files(kernel_src):
    return kernel_files(os.path.join(kernel_src, 'arch', KERNEL_ARCH), '*.S')
def nargs_files(kernel_src):
    return kernel_files(kernel_src, '*.o.nargs')
def parse_all_asm_functions(kernel_src):
    s = set([])
    for asm_file in asm_files(kernel_src):
        for symbol in parse_asm(asm_file, functions=True):
            s.add(symbol)
    return s
def parse_all_asm_labels(kernel_src):
    s = set([])
    for asm_file in asm_files(kernel_src):
        for symbol in parse_plain_labels(asm_file):
            s.add(symbol)
    return s

def offset_to_section(offset, sections):
    for section in sections:
        if section['offset'] <= offset < section['offset'] + section['size']:
            return section
def addr_to_section(hexaddr, sections):
    addr = _int(hexaddr)
    for section in sections:
        if section['address'] <= addr < section['address'] + section['size']:
            return section

def parse_sections(vmlinux):
    """
    [Nr] Name              Type             Address           Offset
         Size              EntSize          Flags  Link  Info  Align
    [ 0]                   NULL             0000000000000000  00000000
         0000000000000000  0000000000000000           0     0     0
    [ 1] .head.text        PROGBITS         ffffffc000205000  00005000
         0000000000000500  0000000000000000  AX       0     0     64 
    {
      'name': '.head.text',
      'size': 0,
      'type': PROGBITS,
      ...
    }
    """
    proc = subprocess.Popen([OBJDUMP, '--section-headers', vmlinux], stdout=subprocess.PIPE)
    f = each_procline(proc)
    d = {
        'sections': [],
        'section_idx': {},
    }
    it = iter(f)
    section_idx = 0
    while True:
        try:
            line = it.next()
        except StopIteration:
            break

        m = re.search(r'^Sections:', line)
        if m:
            # first section
            it.next()
            continue

        m = re.search((
            # [Nr] Name              Type             Address           Offset
            r'^\s*(?P<number>\d+)'
            r'\s+(?P<name>[^\s]*)'
            r'\s+(?P<size>{hex_re})'
            r'\s+(?P<address>{hex_re})'
            r'\s+(?P<lma>{hex_re})'
            r'\s+(?P<offset>{hex_re})'
            r'\s+(?P<align>[^\s]+)'
            ).format(hex_re=hex_re), line)
        if m:
            section = {}

            d['section_idx'][m.group('name')] = int(m.group('number'))

            def parse_power(x):
                m = re.match(r'(?P<base>\d+)\*\*(?P<exponent>\d+)', x)
                return int(m.group('base'))**int(m.group('exponent'))
            section.update(coerce(m.groupdict(), [
                [_int, ['size', 'address', 'offset', 'lma']],
                [int, ['number']],
                [parse_power, ['align']]]))

            line = it.next()
            # CONTENTS, ALLOC, LOAD, READONLY, CODE
            m = re.search((
            r'\s+(?P<type>.*)'
            ).format(hex_re=hex_re), line)
            section.update(m.groupdict())

            d['sections'].append(section)

    return d
            
def coerce(dic, funcs, default=lambda x: x):
    field_to_func = {}
    for row in funcs:
        f, fields = row
        for field in fields:
            field_to_func[field] = f

    fields = dic.keys()
    for field in fields:
        if field not in field_to_func:
            continue
        dic[field] = field_to_func[field](dic[field])

    return dic


BYTES_PER_INSN = 4
def insn_addr():
    try:
        return _int(env['insertion_addr']) + (env['line_number'] - 1)*BYTES_PER_INSN
    except Exception, e:
        raise e

def _int(hex_string):
    """
    Convert a string of hex characters into an integer 

    >>> _int("ffffffc000206028")
    18446743798833766440L
    """
    return int(hex_string, 16)
def _hex(integer):
    return re.sub('^0x', '', hex(integer)).rstrip('L')

def strip_hex_prefix(string):
    return re.sub('^0x', '', string)

def main():
    if OFF:
        return

    parser = argparse.ArgumentParser("Instrument vmlinux to protect against ROP attacks")
    parser.add_argument("--vmlinux", required=True,
            help="vmlinux file to run objdump on")
    parser.add_argument("--config",
            help="kernel .config file; default = .config in location of vmlinux if it exists")
    args = parser.parse_known_args()[0]
    config_file = None
    if args.config:
        config_file = args.config
    if not args.config:
        config_file = guess_config_file(args.vmlinux)
    if config_file is None:
        parser.error("Cannot find kernel .config file in directory of vmlinux; specify one with --config")
    # Some default arguments depend on configuration options.
    conf = parse_config(config_file)
    parser.add_argument("--threads", type=int, default=DEFAULT_THREADS,
            help="Number of threads to instrument with (default = # of CPUs on machine)")
    parser.add_argument("--instrument-func",
            help=\
                    "We will instrument the first BL instruction inside this function. " \
                    "We calculate B offsets based on this")
    parser.add_argument("--vmlinux-dump",
            help="vmlinux objdump")
    parser.add_argument("--insertion-addr", default='0xffffffc002000000',
            help="virtual address that we insert the code at (should follow text)")
    parser.add_argument("--print-insertion-addr", action='store_true')
    parser.add_argument("--test-instrument", action='store_true')
    parser.add_argument("--test-func", action='store_true',
            help="Only instrument the {DEFAULT_TEST_FUNC} function".format(
                DEFAULT_TEST_FUNC=DEFAULT_TEST_FUNC))
    _reg_note = " (NOTE: must be kept in sync with macro definitions in init/hyperdrive.S)"
    parser.add_argument("--RRK", default=RRK_DEFAULT,
            help="ARM64 register reserved for storing the return-address key" + _reg_note)
    parser.add_argument("--RRX", default=RRX_DEFAULT,
            help="ARM64 register reserved for scratch during BL/BLR instrumentation" + _reg_note)
    parser.add_argument("--RRS", default=RRS_DEFAULT,
            help="ARM64 register reserved for storing the springboard base address" + _reg_note)

    parser.add_argument("--debug", action='store_true')
    parser.add_argument("--inplace", action='store_true',
            help="instrument the vmlinux file inplace")
    parser.add_argument("--log",
            help="in addition to standard out, write any output to this file (default: kernel-src/scripts/rkp_cfp/cfp_log.txt)")
    parser.add_argument("--kernel-src",
            help="kernel source top directory; default = directory containing vmlinux")
    parser.add_argument("--print-bl-offsets", action='store_true')

    args = parser.parse_known_args()[0]
    if args.print_insertion_addr:
        log(args.insertion_addr)
        sys.exit(0) 

    args = parser.parse_args()

    if not os.path.exists(args.vmlinux):
        parser.error("--vmlinux ({vmlinux}) doesn't exist".format(vmlinux=args.vmlinux))

    kernel_src = args.kernel_src if args.kernel_src else guess_kernel_src(args.vmlinux)
    if kernel_src is None:
        parser.error("Need top directory of kernel source for --kernel-src")

    common.LOG = common.Log('cfp_log.txt')

    with common.LOG:

        test_func = None
        if args.test_func:
            test_func = DEFAULT_TEST_FUNC

        def _load_objdump():
            return contextlib.closing(load_and_cache_objdump(args.vmlinux, debug=args.debug, 
                kernel_src=kernel_src, config_file=config_file, RRK=args.RRK, RRX=args.RRX, RRS=args.RRS, inplace=args.inplace))

        if args.test_instrument:
            with _load_objdump() as objdump:
                test_instrument(objdump)
            return

        if args.print_bl_offsets:
            with _load_objdump() as objdump:
                print_bl_offsets(objdump)
            return

        # instrument and validate
        with _load_objdump() as objdump:
            instrument(objdump, func=test_func, skip=common.skip, skip_stp=common.skip_stp, 
                    skip_asm=common.skip_asm, skip_blr=common.skip_blr, threads=args.threads)
            objdump.save_instr_copy()
            return

def each_line(fname):
    with open(fname) as f:
        for line in f:
            line = line.rstrip()
            yield line

def each_func_lines(objdump_path):
    lines = []
    func = None
    for line in each_line(objdump_path):
        m = re.search(common.fun_rec, line)
        if m:
            yield func, lines
            lines = []
            func = m.group('func_name')
        elif func is None:
            continue
        lines.append(line)
    if func is not None:
        yield func, lines

def guess_kernel_src(vmlinux):
    kernel_src = my_dirname(vmlinux)
    if os.path.exists(kernel_src):
        return kernel_src

def guess_config_file(vmlinux):
    config_file = os.path.join(my_dirname(vmlinux), '.config')
    if os.path.exists(config_file):
        return config_file
    return None

def parse_config(config_file):
    """
    Parse kernel .config 
    
    Apparently, even if this script gets run from the Kbuild system, it's not seeing CONFIG_ variables in its environment.
    At least CROSS_COMPILE is there though.
    """
    conf = {}
    for line in each_line(config_file):
        m = re.search(r'^\s*(?P<var>[A-Z0-9_]+)=(?P<value>[^\s#]+)', line)
        if m:
            conf[m.group('var')] = m.group('value')
    return conf

def first_n(xs, n):
    it = iter(xs)
    i = 0
    for x in xs:
        if i == n:
            break
        yield x
        i += 1

def parse_skip(skip_file):
    """
    Parse a file that looks like this:
    '
    # this is a comment
    1st_entry # another commment
    2nd_entry

    '
    >>> set(['1st_entry', '2nd_entry'])
    """
    skip = set([])
    with open(skip_file, 'r') as f:
        for line in f:
            m = re.search(r'^\s*(?P<entry>[^\s#]+)', line)
            if m:
                skip.add(m.group('entry'))
    return skip

DEFAULT_PICKLE_FNAME="{dirname}/.{basename}.pickle"
def pickle_name(fname, pickle_fname=DEFAULT_PICKLE_FNAME):
    basename = os.path.basename(fname)
    dirname = my_dirname(fname)
    return pickle_fname.format(**locals())

def load_and_cache_objdump(vmlinux, debug=False, use_stale_pickle=False, pickle_fname=DEFAULT_PICKLE_FNAME, 
        *objdump_args, **objdump_kwargs):
    """
    Parse vmlinux into an Objdump.

    If debug mode, load it from a cached python pickle file to speed things up.
    """

    def should_use_pickle(pickle_file, vmlinux):
        return os.path.exists(pickle_file) and ( 
                     # Pickle is newer than vmlinux and this script
                    (os.path.getmtime(pickle_file) > os.path.getmtime(vmlinux) or use_stale_pickle) and
                    (os.path.getmtime(pickle_file) > os.path.getmtime(__file__) or debug)
                )
    def should_save_pickle():
        return debug

    objdump = None
    #needs_update = True
    #pickle_file = pickle_name(vmlinux)
    #if should_use_pickle(pickle_file, vmlinux):
        #with open(pickle_file, 'rb') as f:
            #objdump = cPickle.load(f)
            #objdump.open()
        #needs_update = False
    #else:
    objdump = Objdump(vmlinux, *objdump_args, **objdump_kwargs)
    objdump.parse()
    objdump.open()

    ## Update the pickle file.
    #if needs_update and should_save_pickle():
        #with open(pickle_file, 'wb') as f:
            #cPickle.dump(objdump, f)

    return objdump

def flip_endianness(word):
    assert len(word) == 4
    def swap(i, j):
        tmp = word[i]
        word[i] = word[j]
        word[j] = tmp
    swap(0, 3)
    swap(1, 2)

def from_twos_compl(x, nbits):
    """
    Convert nbit two's compliment into native decimal.
    """
    # Truely <= nbits long?
    assert x == x & ((2**nbits) - 1)
    if x & (1 << (nbits - 1)):
        # sign bit is set; it's negative
        flip = -( (x ^ (2**nbits) - 1) + 1 )
        # twiddle = ~x + 1
        return flip
    return x

def to_twos_compl(x, nbits):
    """
    Convert native decimal into nbit two's complement
    """
    if x < 0:
        flip = (( -x ) - 1) ^ ((2**nbits) - 1)
        assert flip == flip & ((2**nbits) - 1)
        return flip
    return x

def byte_string(xs):
    if type(xs) == list:
        return ''.join(xs)
    elif type(xs) in [int, long]:
        return ''.join([chr((xs >> 8*i) & 0xff) for i in xrange(3, -1, 0-1)])
    return xs
def hexint(b):
    return int(binascii.hexlify(byte_string(b)), 16)
def mask_shift(insn, mask, shift):
    return (hexint(insn['binary']) & mask) >> shift
def mask(insn, mask):
    return hexint(insn['binary']) & mask

def my_dirname(fname):
    """
    If file is in current directory, return '.'.
    """
    dirname = os.path.dirname(fname)
    if dirname == '':
        dirname = '.'
    return dirname

def are_nop_insns(insns):
    return all(ins is not None and ins['type'] == 'nop' for ins in insns)

def stp_lsl_bits(insn):
    return (2 + (insn['args']['opc'] >> 1))
def str_lsl_bits(insn):
    """
    ARMv8 Manual:
    integer scale = UInt(size);
    bits(64) offset = LSL(ZeroExtend(imm12, 64), scale);
    """
    return insn['args']['size']

def shuffle_insns(objdump, mov_j, i):
    # This doesn't work on the emulator. oh well.
    # Only reason i can think of would be that there are branch instructions between stp and add...
    # But then it would branch over adjusting x29...
    #
    # We CANNOT guarantee that "add x29, sp, offset" immediately follows "stp x29, RRX, ...".
    # We need to shuffle instructions down like so:
    #
    # nop                            ->   eor RRX, x30, RRK
    # stp x29, x30, [sp,#<offset>]   ->   stp x29, RRX, [sp,#<offset>]
    # insns[0]                       ->   add x29, sp, #<offset>
    # ...                            ->   insns[0]
    # insns[n]                       ->   ... 
    # add x29, sp, #<offset>         ->   insns[n]
    for j in xrange(mov_j, i+1, -1):
        objdump.write(j, objdump.read(j-1))

def _p(path):
    return os.path.expandvars(path)

if common.run_from_ipython():
    """
    Iterative development is done using ipython REPL.  
    This code only runs when importing this module from ipython.
    We use this to conveniently define variables and load a sample vmlinux file.

    Set sample_vmlinux_file to a vmlinux file on your path.
    Instrumentation will be created in a copy of that file (with a .instr suffix).

    ==== How to test ====

    >>> ... # means to type this at the ipython terminal prompt

    # To reload your code after making changes, do:
    change the DEFAULT_THREADS to 1 before debugging
    >>> import instrument; dreload(instrument)

    # To instrument vmlinux, do:
    >>> instrument._instrument()
    """
    # Define some useful stuff for debugging via ipython.

    # Set this to a vmlinux file we want to copy then instrument.
    sample_vmlinux_file = _p("./vmlinux")
    sample_kernel_src = _p("$k6")

    sample_config_file = guess_config_file(sample_vmlinux_file)

    # Set this to True if you changed data members in Objdump
    # reload_and_save_pickle = True
    reload_and_save_pickle = False
    num_threads = DEFAULT_THREADS
    # num_threads = 1

    
    #import pdb; pdb.set_trace()
    o = load_and_cache_objdump(sample_vmlinux_file, debug=True,
                    kernel_src=sample_kernel_src, config_file=sample_config_file)

    print "in function common.run_from_ipython()"
    common.LOG = common.Log('cfp_log.txt')
    log("first line, for test")

    def _instrument(func=None, skip=common.skip, validate=True, threads=num_threads):
        instrument(o, func=func, skip=common.skip, skip_stp=common.skip_stp, skip_asm=common.skip_asm, threads=threads)
        o.flush()
        if validate and (sample_config_file) and func is None:
            return debug.validate_instrumentation(o, common.skip, common.skip_stp, \
                    common.skip_asm, common.skip_save_lr_to_stack, common.skip_br, threads=num_threads)

if __name__ == '__main__':
    main()
