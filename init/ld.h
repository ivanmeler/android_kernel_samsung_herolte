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


#ifndef __LD_H__
#define __LD_H__


#define __TARGET_64__

#include "elf.h"

typedef int (*ld_init_t)(void);
typedef int (*ld_fini_t)(void);

#ifdef __TARGET_64__
typedef int (*ld_resolve_t)(void *binary, _Elf_Sym *sym, _Elf_Sxword *value);
#else //__TARGET_32__
typedef int (*ld_resolve_t)(void *binary, _Elf_Sym *sym, _Elf_Sword *value);
#endif //__TARGET_64__ | __TARGET_32__

typedef int (*ld_translate_t)(void *binary, void *in, void **out);

int ld_Elf_Ehdr_to_Elf_Shdr(_Elf_Ehdr *ehdr, _Elf_Shdr **shdr, size_t *size);

int ld_Elf_Ehdr_to_Elf_Phdr(_Elf_Ehdr *ehdr, _Elf_Phdr **phdr, size_t *size);

int ld_binary_to_Elf_Ehdr(void *binary, _Elf_Ehdr **ehdr);

int ld_get_entry(void *binary, void **entry);

int ld_get_name(void *binary, char **name);

int ld_get_version(void *binary, char **version);

int ld_get_string(char *strtab, int index, char **string);

int ld_get_symbol(_Elf_Sym *symtab, int index, _Elf_Sym **symbol);

int ld_get_base(void *binary, void **address);

int ld_get_size(void *binary, size_t *size);

int ld_get_sect(void *binary, char *name, void **section, size_t *size);

#ifdef __TARGET_64__
int ld_get_dynamic_relatab(void *binary, _Elf_Rela **rela, size_t *size);
#else //__TARGET_32__
int ld_get_dynamic_reltab(void *binary, _Elf_Rel **rel, size_t *size);
#endif //__TARGET_64__ | __TARGET_32__

int ld_get_dynamic_symtab(void *binary, _Elf_Sym **symtab, size_t *size);

int ld_get_dynamic_strtab(void *binary, char **strtab, size_t *size);

#ifdef __TARGET_64__
int ld_fixup_dynamic_relatab(void *binary, ld_resolve_t resolve, ld_translate_t translate);
#else //__TARGET_32__
int ld_fixup_dynamic_reltab(void *binary, ld_resolve_t resolve, ld_translate_t translate);
#endif //__TARGET_64__ | __TARGET_32__

int ld_fixup_dynamic_plttab(void *binary, ld_resolve_t resolve, ld_translate_t translate);

#endif //__LD_H__
