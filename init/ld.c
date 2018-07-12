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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <asm/cacheflush.h>
#include <asm/irqflags.h>
#include <linux/fs.h>
#include <asm/tlbflush.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>

#include "ld.h"
#include "elf.h"

//#define LD_DEBUG
#ifdef LD_DEBUG
#define ld_log(a, ...) printk(KERN_ALERT a, __VA_ARGS__)
#else
#define ld_log(a, ...)
#endif //LD_DEBUG

int ld_Elf_Ehdr_to_Elf_Shdr(_Elf_Ehdr *ehdr, _Elf_Shdr **shdr, size_t *size) {

	ld_log("%s\n", __FUNCTION__);

	if(ehdr == NULL) { return -1; }
	if(shdr == NULL) { return -1; }
	if(size == NULL) { return -1; }

	*shdr = (_Elf_Shdr *)((unsigned long)ehdr + (unsigned long)ehdr->e_shoff);

	*size = ehdr->e_shnum;

	return 0;
}

int ld_Elf_Ehdr_to_Elf_Phdr(_Elf_Ehdr *ehdr, _Elf_Phdr **phdr, size_t *size) {

	ld_log("%s\n", __FUNCTION__);

	if(ehdr == NULL) { return -1; }
	if(phdr == NULL) { return -1; }
	if(size == NULL) { return -1; }

	*phdr = (_Elf_Phdr *)((unsigned long)ehdr + (unsigned long)(ehdr->e_phoff));

	*size = ehdr->e_phnum;

	return 0;
}

int ld_binary_to_Elf_Ehdr(void *binary, _Elf_Ehdr **ehdr) {

	ld_log("%s\n", __FUNCTION__);

	if(ehdr == NULL) { return -1; }

	*ehdr = (_Elf_Ehdr *)binary;

	return 0;
}

int ld_get_entry(void *binary, void **entry) {

	_Elf_Ehdr *ehdr;
	void *base = NULL;

	ld_log("%s\n", __FUNCTION__);

	if(binary == NULL) { return -1; }
	if(entry == NULL) { return -1; }

	if(ld_binary_to_Elf_Ehdr(binary, &ehdr)) { return -1; }
	if(ld_get_base(binary, &base)) { return -1; }

	*entry = (void *)((unsigned long)base + (unsigned long)ehdr->e_entry);

	return 0;
}

int ld_get_name(void *binary, char **name) {

	_Elf_Dyn *dyn;
	char *strtab;
	size_t sz;
	unsigned int i;

	ld_log("%s\n", __FUNCTION__);

	if(binary == NULL) { return -1; }
	if(name == NULL) { return -1; }

	if(ld_get_sect(binary, ".dynamic", (void **)&dyn, &sz)) { return -1; }

	if(ld_get_dynamic_strtab(binary, &strtab, &sz)) { return -1; }

	*name = NULL;

	for(i = 0; i < (sz / sizeof(_Elf_Dyn)); i++) {
		switch(dyn[i].d_tag) {
			case _DT_SONAME:
				*name = &strtab[dyn[i].d_un.d_val];
				break;
		}
		if(*name != NULL) { break; }
	}

	return 0;
}

int ld_get_version(void *binary, char **version) {

	_Elf_Sym *symtab;
	char *strtab;
	size_t strtabsz;
	size_t symtabsz;
	char *name;
	char **string;
	_Elf_Sym *sym;
	void *base = NULL;
	unsigned int i;

	ld_log("%s\n", __FUNCTION__);

	if(ld_get_dynamic_strtab(binary, &strtab, &strtabsz)) { return -1; }

	if(ld_get_dynamic_symtab(binary, &symtab, &symtabsz)) { return -1; }

	for(i = 0; i < (symtabsz / sizeof(_Elf_Sym)); i++) {
		if(ld_get_symbol(symtab, i, &sym)) { return -1; }

		if(ld_get_string(strtab, sym->st_name, &name)) { return -1; }

		if(strcmp(name, "version") == 0) {
			if(ld_get_base(binary, &base)) { return -1; }
			string = (char **)((unsigned long)sym->st_value + (unsigned long)base);
			*version = *string;
			return 0;
		}
	}

	return -1;
}

int ld_get_string(char *strtab, int index, char **string) {

	ld_log("%s\n", __FUNCTION__);

	if(strtab == NULL) { return -1; }
	if(string == NULL) { return -1; }

	*string = &(((char *)strtab)[index]);

	return 0;
}

int ld_get_symbol(_Elf_Sym *symtab, int index, _Elf_Sym **symbol) {

	ld_log("%s\n", __FUNCTION__);

	if(symtab == NULL) { return -1; }
	if(symbol == NULL) { return -1; }

	*symbol = &(symtab[index]);

	return 0;
}

int ld_get_base(void *binary, void **address) {

	_Elf_Ehdr *ehdr;
	_Elf_Phdr *phdr;
	int set;
	size_t size;
	unsigned int i;

	ld_log("%s\n", __FUNCTION__);

	if(address == NULL) { return -1; }

	if(ld_binary_to_Elf_Ehdr(binary, &ehdr)) { return -1; }

	if(ld_Elf_Ehdr_to_Elf_Phdr(ehdr, &phdr, &size)) { return -1; }

	set = 0;

	for(i = 0; i < ehdr->e_phnum; i++) {

		if(phdr[i].p_type == _PT_LOAD) {
			if((phdr[i].p_offset < (_Elf_Off)((unsigned long)*address)) || set == 0) {
				*address = (void *)(unsigned long)phdr[i].p_offset;
				set = 1;
			}
		}
	}

	*address = (void *)((long)*address + (long)binary);

	return 0;
}

int ld_get_size(void *binary, size_t *size) {

	_Elf_Ehdr *ehdr;
	_Elf_Phdr *phdr;
	int set;
	unsigned int i;

	ld_log("%s\n", __FUNCTION__);

	if(size == NULL) { return -1; }

	if(ld_binary_to_Elf_Ehdr(binary, &ehdr)) { return -1; }

	if(ld_Elf_Ehdr_to_Elf_Phdr(ehdr, &phdr, size)) { return -1; }

	set = 0;

	for(i = 0; i < ehdr->e_phnum; i++) {

		if(phdr[i].p_type == _PT_LOAD) {

			if((phdr[i].p_vaddr + phdr[i].p_memsz) > (_Elf_Off)*size) {
				*size = (size_t)phdr[i].p_vaddr + (size_t)phdr[i].p_memsz;
			}
		}
	}

	return 0;
}

int ld_get_sect(void *binary, char *name, void **section, size_t *size) {

	_Elf_Ehdr *ehdr;
	_Elf_Shdr *shdr;
	size_t sz;
	void *strtab;
	char *tmp;
	unsigned int i;

	ld_log("%s\n", __FUNCTION__);

	if(binary == NULL) { return -1; }
	if(name == NULL) { return -1; }
	if(section == NULL) { return -1; }
	if(size == NULL) { return -1; }

	if(ld_binary_to_Elf_Ehdr(binary, &ehdr)) { return -1; }

	if(ld_Elf_Ehdr_to_Elf_Shdr(ehdr, &shdr, &sz)) { return -1; }

	strtab = (void *)((unsigned long)binary + (unsigned long)shdr[ehdr->e_shstrndx].sh_offset);

	for(i = 0; i < sz; i++) {
		if(ld_get_string(strtab, shdr[i].sh_name, &tmp)) { return -1; }

		if(strcmp(name, tmp) == 0) {
			*section = (void *)((unsigned long)binary + (unsigned long)shdr[i].sh_offset);
			*size = shdr[i].sh_size;
			return 0;
		}
	}

	*section = NULL;

	return -1;
}

int ld_get_dynamic_symtab(void *binary, _Elf_Sym **symtab, size_t *size) {

	_Elf_Dyn *dyn;
	size_t sz;
	size_t ent;
	unsigned int i;

	ld_log("%s\n", __FUNCTION__);

	if(binary == NULL) { return -1; }
	if(symtab == NULL) { return -1; }
	if(size == NULL) { return -1; }

	if(ld_get_sect(binary, ".dynamic", (void **)&dyn, &sz)) { return -1; }

	*symtab = NULL;
	*size = 0;
	ent = 0;

	for(i = 0; i < (sz / sizeof(_Elf_Dyn)); i++) {
		switch(dyn[i].d_tag) {
			case _DT_SYMTAB:
				*symtab = (void *)((unsigned long)dyn[i].d_un.d_ptr);
				break;
			case _DT_SYMENT:
				ent = dyn[i].d_un.d_val;
				break;
		}
		if((*symtab != NULL) && (*size != 0) && (ent != 0)) { break; }
	}

	if(ent != sizeof(_Elf_Sym)) { return -1; }

	*symtab = (void *)((long)*symtab + (long)binary);

	if(ld_get_sect(binary, ".dynsym", (void **)&dyn, &sz)) { return -1; }

	if(sz % sizeof(_Elf_Sym) != 0) { return -1; }

	*size = sz;

	return 0;
}

int ld_get_dynamic_strtab(void *binary, char **strtab, size_t *size) {

	_Elf_Dyn *dyn;
	size_t sz;
	unsigned int i;

	ld_log("%s\n", __FUNCTION__);

	if(binary == NULL) { return -1; }
	if(strtab == NULL) { return -1; }
	if(size == NULL) { return -1; }

	if(ld_get_sect(binary, ".dynamic", (void **)&dyn, &sz)) { return -1; }

	*strtab = NULL;
	*size = 0;

	for(i = 0; i < (sz / sizeof(_Elf_Dyn)); i++) {

		switch(dyn[i].d_tag) {
			case _DT_STRTAB:
				*strtab = (void *)((unsigned long)dyn[i].d_un.d_ptr);
				break;
			case _DT_STRSZ:
				*size = dyn[i].d_un.d_val;
				break;
		}
		if((*strtab != NULL) && (*size != 0)) { break; }
	}

	*strtab = (void *)((long)*strtab + (long)binary);

	if(ld_get_sect(binary, ".dynstr", (void **)&dyn, &sz)) { return -1; }

	if(*size != sz) { return -1; }

	return 0;
}

#ifdef __TARGET_64__
int ld_get_dynamic_relatab(void *binary, _Elf_Rela **relatab, size_t *size) {

	_Elf_Dyn *dyn;
	size_t sz;
	size_t ent;
	unsigned int i;

	//printk(KERN_ALERT "%s\n", __FUNCTION__);

	if(binary == NULL) { return -1; }
	if(relatab == NULL) { return -1; }
	if(size == NULL) { return -1; }

	if(ld_get_sect(binary, ".dynamic", (void **)&dyn, &sz)) { return -1; }

	*relatab = NULL;
	*size = 0;
	ent = 0;

	for(i = 0; i < (sz / sizeof(_Elf_Dyn)); i++) {
		switch(dyn[i].d_tag) {
			case _DT_RELA:
				*relatab = (void *)(unsigned long)dyn[i].d_un.d_ptr;
				break;
			case _DT_RELASZ:
				*size = dyn[i].d_un.d_val;
				break;
			case _DT_RELAENT:
				ent = dyn[i].d_un.d_val;
				break;
		}
		if((*relatab != NULL) && (*size != 0) && (ent != 0)) { break; }
	}

	ld_log("*relatab: %p, *size: %d, ent: %d\n", *relatab, (int)*size, (int)ent);

	if(ent != sizeof(_Elf_Rela)) { return -1; }

	*relatab = (void *)((long)*relatab + (long)binary);

	return 0;
}

#else //__TARGET_32__
int ld_get_dynamic_reltab(void *binary, _Elf_Rel **reltab, size_t *size) {

	_Elf_Dyn *dyn;
	size_t sz;
	size_t ent;
	unsigned int i;

	ld_log("%s\n", __FUNCTION__);

	if(binary == NULL) { return -1; }
	if(reltab == NULL) { return -1; }
	if(size == NULL) { return -1; }

	if(ld_get_sect(binary, ".dynamic", (void **)&dyn, &sz)) { return -1; }

	*reltab = NULL;
	*size = 0;
	ent = 0;

	for(i = 0; i < (sz / sizeof(_Elf_Dyn)); i++) {
		switch(dyn[i].d_tag) {
			case _DT_REL:
				*reltab = (void *)dyn[i].d_un.d_ptr;
				break;
			case _DT_RELSZ:
				*size = dyn[i].d_un.d_val;
				break;
			case _DT_RELENT:
				ent = dyn[i].d_un.d_val;
				break;
		}
		if((*reltab != NULL) && (*size != 0) && (ent != 0)) { break; }
	}

	if(ent != sizeof(_Elf_Rel)) { return -1; }

	*reltab = (void *)((long)*reltab + (long)binary);

	return 0;
}
#endif //__TARGET_64__ | __TARGET_32__

#ifdef __TARGET_64__
int ld_get_dynamic_plttab(void *binary, _Elf_Rela **plttab, size_t *size) {
#else //__TARGET_32__
int ld_get_dynamic_plttab(void *binary, _Elf_Rel **plttab, size_t *size) {
#endif //__TARGET_64__ | __TARGET_32__

	_Elf_Dyn *dyn;
	size_t sz;
	int type;
	unsigned int i;

	ld_log("%s\n", __FUNCTION__);

	if(binary == NULL) { return -1; }
	if(plttab == NULL) { return -1; }
	if(size == NULL) { return -1; }

	if(ld_get_sect(binary, ".dynamic", (void **)&dyn, &sz)) { return -1; }

	*plttab = NULL;
	*size = 0;
	type = 0;

	for(i = 0; i < (sz / sizeof(_Elf_Dyn)); i++) {
		switch(dyn[i].d_tag) {
			case _DT_JMPREL:
				*plttab = (void *)((unsigned long)dyn[i].d_un.d_ptr);
				break;
			case _DT_PLTREL:
				type = dyn[i].d_un.d_val;
				break;
			case _DT_PLTRELSZ:
				*size = dyn[i].d_un.d_val;
				break;
		}
		if((*plttab != NULL) && (*size != 0) && (type != 0)) { break; }
	}

#ifdef __TARGET_64__
	if((type != _DT_RELA) || ((*size % sizeof(_Elf_Rela)) != 0)) { return -1; }
#else //__TARGET_32_
	if((type != _DT_REL) || ((*size % sizeof(_Elf_Rel)) != 0)) { return -1; }
#endif //__TARGET_64__ | __TARGET_32__

	*plttab = (void *)((long)*plttab + (long)binary);

	return 0;
}

#ifdef __TARGET_64__
int ld_fixup_dynamic_relatab(void *binary, ld_resolve_t resolve, ld_translate_t translate) {

	_Elf_Rela *relatab;
	_Elf_Sym *symtab;
	size_t relatabsz;
	size_t symtabsz;
	_Elf_Sym *sym;
	unsigned int i;
	void *base = NULL;
	void *runtime;
	_Elf_Sxword *pointer;
	_Elf_Sxword value;

	//printk(KERN_ALERT "%s\n", __FUNCTION__);

	if(ld_get_base(binary, &base)) { return -1; }

	if(ld_get_dynamic_relatab(binary, &relatab, &relatabsz)) { return 0; }
	ld_log("relatab %p, relatabsz: %d\n", relatab, (int)relatabsz);
	if(ld_get_dynamic_symtab(binary, &symtab, &symtabsz)) { return -1; }
	ld_log("symtab %p, symtabsz: %d\n", symtab, (int)symtabsz);

	if(translate(binary, base, &runtime)) { return -1; }

	ld_log("base: %p, runtime: %p\n", base, runtime);

	for(i = 0; i < (relatabsz / sizeof(_Elf_Rela)); i++) {
		switch(_ELF_R_TYPE(relatab[i].r_info)) {
			case _R_AARCH64_RELATIVE:
				pointer = (_Elf_Sxword *)((_Elf_Sxword)base + ((unsigned long)relatab[i].r_offset));
				value = (_Elf_Sxword)runtime + ((unsigned long)relatab[i].r_addend);
				*((_Elf_Sxword *)pointer) = value;

				ld_log("fixed up _R_AARCH64_RELATIVE: %p, %p\n", (void *)pointer, (void *)value);
				break;
			case _R_AARCH64_GLOB_DAT:
				if(ld_get_symbol(symtab, _ELF_R_SYM(relatab[i].r_info), &sym)) { return -1; }
				pointer = (_Elf_Sxword *)((_Elf_Sxword)base + ((unsigned long)relatab[i].r_offset));
				ld_log("fixed up R_ARM_GLOB_DAT: %p, %p\n", (void *)pointer, (void *)value);
				if(sym->st_shndx == _SHN_UNDEF) {
					if(resolve(binary, sym, &value) != 0) { return -1; }
				}
				else { value = (_Elf_Sxword)runtime + (_Elf_Sword)sym->st_value; }

				*((_Elf_Sxword *)pointer) = value;

				ld_log("fixed up R_ARM_GLOB_DAT: %p, %p\n", (void *)pointer, (void *)value);
				break;
			default:
				ld_log("unsupported relocate: %d\n", (int)_ELF_R_TYPE(relatab[i].r_info));
				return -1;
		}
	}

	return 0;
}

#else //__TARGET_32__
int ld_fixup_dynamic_reltab(void *binary, ld_resolve_t resolve, ld_translate_t translate) {

	_Elf_Rel *reltab;
	_Elf_Sym *symtab;
	size_t reltabsz;
	size_t symtabsz;
	_Elf_Sym *sym;
	unsigned int i;
	void *base = NULL;
	void *runtime;
	_Elf_Sword *pointer;
	_Elf_Sword value;

	ld_log("%s\n", __FUNCTION__);

	if(ld_get_base(binary, &base)) { return -1; }

	if(ld_get_dynamic_reltab(binary, &reltab, &reltabsz)) { return 0; }
	ld_log("reltab 0x%x, reltabsz: %d\n", reltab, (int)reltabsz);
	if(ld_get_dynamic_symtab(binary, &symtab, &symtabsz)) { return -1; }
	ld_log("symtab 0x%x, symtabsz: %d\n", symtab, (int)symtabsz);

	if(translate(binary, base, &runtime)) { return -1; }

	ld_log("base: %p, runtime: %p\n", base, runtime);

	for(i = 0; i < (reltabsz / sizeof(_Elf_Rel)); i++) {
		switch(_ELF_R_TYPE(reltab[i].r_info)) {
		case _R_ARM_RELATIVE:
			pointer = (_Elf_Sword *)((_Elf_Sword)base + (_Elf_Sword)reltab[i].r_offset);
			value = (_Elf_Sword)runtime + *pointer;
			*((_Elf_Sword *)pointer) = value;

			ld_log("fixed up _R_ARM_RELATIVE: 0x%x, 0x%x\n", pointer, (void *)value);
			break;
			case _R_ARM_ABS32:
				if(ld_get_symbol(symtab, _ELF_R_SYM(reltab[i].r_info), &sym)) { return -1; }
				pointer = (_Elf_Sword *)((_Elf_Sword)runtime + (_Elf_Sword)reltab[i].r_offset);

				if(sym->st_shndx == _SHN_UNDEF) {
					if(resolve(binary, sym, &value) != 0) { return -1; }
				}
				else { value = (_Elf_Sword)runtime + (_Elf_Sword)sym->st_value; }

				*((_Elf_Sword *)pointer) = value;

				ld_log("fixed up _R_ARM_ABS32: 0x%x, 0x%x\n", pointer, (void *)value);
				break;
			case _R_ARM_GLOB_DAT:
				if(ld_get_symbol(symtab, _ELF_R_SYM(reltab[i].r_info), &sym)) { return -1; }
				pointer = (_Elf_Sword *)((_Elf_Sword)base + (_Elf_Sword)reltab[i].r_offset);

				if(sym->st_shndx == _SHN_UNDEF) {
					if(resolve(binary, sym, &value) != 0) { return -1; }
				}
				else { value = (_Elf_Sword)runtime + (_Elf_Sword)sym->st_value; }

				*((_Elf_Sword *)pointer) = value;

				ld_log("fixed up _R_ARM_GLOB_DAT: 0x%x, 0x%x\n", pointer, (void *)value);
				break;

			default:
				ld_log("unsupported relocate: %d\n", (int)_ELF_R_TYPE(reltab[i].r_info));
				return -1;
		}
	}

	return 0;
}

#endif //__TARGET_64__ | __TARGET_32__

int ld_fixup_dynamic_plttab(void *binary, ld_resolve_t resolve, ld_translate_t translate) {

#ifdef __TARGET_64__
	_Elf_Rela *plttab;
#else //__TARGET_64__
	_Elf_Rel *plttab;
#endif //__TARGET_64__ | __TARGET_32__

	_Elf_Sym *symtab;
	_Elf_Sym *sym;
	size_t plttabsz;
	size_t symtabsz;
	unsigned int i;
	void *base = NULL;
	void *runtime;

#ifdef __TARGET_64__
	_Elf_Sxword *pointer;
	_Elf_Sxword value;
#else //__TARGET_32__
	_Elf_Sword *pointer;
	_Elf_Sword value;
#endif //__TARGET_64__ | __TARGET_32__

	ld_log("%s\n", __FUNCTION__);

	if(ld_get_base(binary, &base)) { return -1; }
	if(ld_get_dynamic_plttab(binary, &plttab, &plttabsz)) { return 0; }
	ld_log("plttab: %p, plttabsz: %d\n", plttab, (int)plttabsz);

	if(ld_get_dynamic_symtab(binary, &symtab, &symtabsz)) { return -1; }
	ld_log("symtab: %p, symtabsz: %d\n", symtab, (int)symtabsz);

	if(translate(binary, base, &runtime)) { return -1; }

	ld_log("base: %p, runtime: %p\n", base, runtime);

#ifdef __TARGET_64__
	for(i = 0; i < (plttabsz / sizeof(_Elf_Rela)); i++) {
#else //__TARGET_32__
	for(i = 0; i < (plttabsz / sizeof(_Elf_Rel)); i++) {
#endif //__TARGET_64__ | __TARGET_32__

		switch(_ELF_R_TYPE(plttab[i].r_info)) {
#ifdef __TARGET_64__
			case _R_AARCH64_JUMP_SLOT:
				if(ld_get_symbol(symtab, _ELF_R_SYM(plttab[i].r_info), &sym)) { return -1; }
				pointer = (_Elf_Sxword *)((_Elf_Sxword)base + ((unsigned long)plttab[i].r_offset));
				if(sym->st_shndx == _SHN_UNDEF) {
					if(resolve(binary, sym, &value) != 0) { return -1; }
				}
				else { value = (_Elf_Sxword)runtime + (_Elf_Sword)sym->st_value; }

				*((_Elf_Sxword *)pointer) = value;
				ld_log("fixed up _R_AARCH64_JUMP_SLOT: %p, %p\n", (void *)pointer, (void *)value);
				break;
#else //__TARGET_32__
			case _R_ARM_JUMP_SLOT:
				if(ld_get_symbol(symtab, _ELF_R_SYM(plttab[i].r_info), &sym)) { return -1; }
				pointer = (_Elf_Sword *)((_Elf_Sword)base + (_Elf_Sword)plttab[i].r_offset);
				if(sym->st_shndx == _SHN_UNDEF) {
					if(resolve(binary, sym, &value) != 0) { return -1; }
				}
				else { value = (_Elf_Sword)runtime + (_Elf_Sword)sym->st_value; }

				*((_Elf_Sword *)pointer) = value;
				ld_log("fixed up _R_ARM_JUMP_SLOT: 0x%x, 0x%x\n", pointer, (void *)value);
				break;
#endif //__TARGET_64__ | __TARGET_32__
			default:
				ld_log("unsupported relocation: %d\n", (int)_ELF_R_TYPE(plttab[i].r_info));
				return -1;
		}
	}

	return 0;
}
