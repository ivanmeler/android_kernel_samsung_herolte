/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */


#ifndef __ELF_H__
#define __ELF_H__

#define __TARGET_64__

#define _SHN_UNDEF	0

#ifdef __TARGET_64__
#define _DT_NULL      0
#define _DT_NEEDED    1
#define _DT_PLTRELSZ  2
#define _DT_PLTGOT    3
#define _DT_STRTAB	  5
#define _DT_SYMTAB	  6
#define _DT_RELA      7
#define _DT_RELASZ    8
#define _DT_RELAENT   9
#define _DT_STRSZ     10
#define _DT_SYMENT    11
#define _DT_INIT      12
#define _DT_FINI      13
#define _DT_SONAME    14
#define _DT_REL       17
#define _DT_PLTREL    20
#define _DT_JMPREL    23

#define _R_AARCH64_NONE            0
#define _R_AARCH64_ABS64         257
#define _R_AARCH64_RELATIVE     1027
#define _R_AARCH64_IRELATIVE	1032
#define _R_AARCH64_GLOB_DAT     1025
#define _R_AARCH64_JUMP_SLOT    1026
#define _R_AARCH64_RELATIVE     1027

#else //__TARGET_32__
#define _DT_INIT       12
#define _DT_FINI       13
#define _DT_SONAME      1
#define _DT_STRTAB      5
#define _DT_SYMTAB      6
#define _DT_RELA        7
#define _DT_RELASZ      8
#define _DT_RELAENT     9
#define _DT_STRSZ      10
#define _DT_SYMENT     11
#define _DT_REL        17
#define _DT_RELSZ      18
#define _DT_RELENT     19

#define _DT_JMPREL     23
#define _DT_PLTRELSZ    2
#define _DT_PLTREL     20

#define _R_ARM_NONE       0
#define _R_ARM_ABS32      2
#define _R_ARM_GLOB_DAT  21
#define _R_ARM_JUMP_SLOT 22
#define _R_ARM_RELATIVE  23
#endif

#define _PT_LOAD          1

typedef unsigned short _Elf_Half;

typedef unsigned int _Elf_Word;
typedef	int _Elf_Sword;

typedef unsigned long long _Elf_Xword;
typedef	long long  _Elf_Sxword;

typedef unsigned long long _Elf_Addr;

typedef unsigned long long _Elf_Off;

typedef unsigned short _Elf_Section;

#define _EI_NIDENT (16)

typedef struct
{
  unsigned char	e_ident[_EI_NIDENT];
  _Elf_Half	e_type;
  _Elf_Half	e_machine;
  _Elf_Word	e_version;
  _Elf_Addr	e_entry;
  _Elf_Off	e_phoff;
  _Elf_Off	e_shoff;
  _Elf_Word	e_flags;
  _Elf_Half	e_ehsize;
  _Elf_Half	e_phentsize;
  _Elf_Half	e_phnum;
  _Elf_Half	e_shentsize;
  _Elf_Half	e_shnum;
  _Elf_Half	e_shstrndx;
} _Elf_Ehdr;

#ifdef __TARGET_64__

typedef struct
{
  _Elf_Word	sh_name;
  _Elf_Word	sh_type;
  _Elf_Xword	sh_flags;
  _Elf_Addr	sh_addr;
  _Elf_Off	sh_offset;
  _Elf_Xword	sh_size;
  _Elf_Word	sh_link;
  _Elf_Word	sh_info;
  _Elf_Xword	sh_addralign;
  _Elf_Xword	sh_entsize;
} _Elf_Shdr;

#else //__TARGET_32__
typedef struct
{
  _Elf_Word	sh_name;
  _Elf_Word	sh_type;
  _Elf_Word	sh_flags;
  _Elf_Addr	sh_addr;
  _Elf_Off	sh_offset;
  _Elf_Word	sh_size;
  _Elf_Word	sh_link;
  _Elf_Word	sh_info;
  _Elf_Word	sh_addralign;
  _Elf_Word	sh_entsize;
} _Elf_Shdr;

#endif //__TARGET_64__ | __TARGET_32__

#ifdef __TARGET_64__
typedef struct
{
  _Elf_Word	st_name;
  unsigned char	st_info;
  unsigned char st_other;
  _Elf_Section	st_shndx;
  _Elf_Addr	st_value;
  _Elf_Xword	st_size;
} _Elf_Sym;

#else //__TARGET_32__
typedef struct
{
  _Elf_Word	st_name;
  _Elf_Addr	st_value;
  _Elf_Word	st_size;
  unsigned char	st_info;
  unsigned char	st_other;
  _Elf_Section	st_shndx;
} _Elf_Sym;

#endif //__TARGET_64__ | __TARGET_32__

#define _ELF_ST_BIND(val)		(((unsigned char) (val)) >> 4)
#define _ELF_ST_TYPE(val)		((val) & 0xf)
#define _ELF_ST_INFO(bind, type)	(((bind) << 4) + ((type) & 0xf))

#ifdef __TARGET_64__
typedef struct
{
  _Elf_Addr	r_offset;		/* Address */
  _Elf_Xword	r_info;			/* Relocation type and symbol index */
  _Elf_Sxword	r_addend;		/* Addend */
} _Elf_Rela;

#define _ELF_R_SYM(i)			((i) >> 32)
#define _ELF_R_TYPE(i)			((i) & 0xffffffff)

#else //__TARGET_32__
typedef struct
{
  _Elf_Addr	r_offset;		/* Address */
  _Elf_Word	r_info;			/* Relocation type and symbol index */
} _Elf_Rel;

#define _ELF_R_SYM(val)		((val) >> 8)
#define _ELF_R_TYPE(val)	((val) & 0xff)

#endif //__TARGET_64__ | __TARGET_32__

#ifdef __TARGET_64__
typedef struct
{
	_Elf_Word	p_type;			/* Segment type */
	_Elf_Word	p_flags;		/* Segment flags */
	_Elf_Off	p_offset;		/* Segment file offset */
	_Elf_Addr	p_vaddr;		/* Segment virtual address */
	_Elf_Addr	p_paddr;		/* Segment physical address */
	_Elf_Xword	p_filesz;		/* Segment size in file */
	_Elf_Xword	p_memsz;		/* Segment size in memory */
	_Elf_Xword	p_align;		/* Segment alignment */
} _Elf_Phdr;

#else //__TARGET_32__
typedef struct
{
  _Elf_Word	p_type;			/* Segment type */
  _Elf_Off	p_offset;		/* Segment file offset */
  _Elf_Addr	p_vaddr;		/* Segment virtual address */
  _Elf_Addr	p_paddr;		/* Segment physical address */
  _Elf_Word	p_filesz;		/* Segment size in file */
  _Elf_Word	p_memsz;		/* Segment size in memory */
  _Elf_Word	p_flags;		/* Segment flags */
  _Elf_Word	p_align;		/* Segment alignment */
} _Elf_Phdr;

#endif //__TARGET_64__ | __TARGET_32__

#ifdef __TARGET_64__
typedef struct
{
  _Elf_Sxword	d_tag;			/* Dynamic entry type */
  union
    {
      _Elf_Xword d_val;		/* Integer value */
      _Elf_Addr d_ptr;			/* Address value */
    } d_un;
} _Elf_Dyn;

#else //__TARGET_32__
typedef struct
{
  _Elf_Sword	d_tag;			/* Dynamic entry type */
  union
    {
      _Elf_Word d_val;			/* Integer value */
      _Elf_Addr d_ptr;			/* Address value */
    } d_un;
} _Elf_Dyn;

#endif //__TARGET_64__ | __TARGET_32__

#endif //__ELF_H__
