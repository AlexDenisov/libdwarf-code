/*
Copyright 2018 David Anderson. All rights reserved.

Redistribution and use in source and binary forms, with
or without modification, are permitted provided that the
following conditions are met:

    Redistributions of source code must retain the above
    copyright notice, this list of conditions and the following
    disclaimer.

    Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials
    provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
This reads elf headers and creates generic-elf
structures containing the Elf headers.

These two enums used for type safety in passing
values.  See dwarf_elfread.h

enum RelocRela
enum RelocOffsetSize

dwarf_elfread.c
calls
    _dwarf_load_elf_relx(intfc,i,...,enum RelocRela,errcode)
        calls _dwarf_elf_load_a_relx_batch(ep,...enum RelocRela,
            enum RelocOffsetSize,errcode)
            which calls generic_rel_from_rela32(ep,gsh,relp,grel
            or    calls generic_rel_from_rela64(ep,gsh,relp,grel
            or    calls generic_rel_from_rel32(ep,gsh,relp,grel...
            or    calls generic_rel_from_rel64(ep,gsh,relp,grel...
*/


#include "config.h"
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h> /* For memcpy etc */
#endif /* HAVE_STRING_H */
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#ifdef HAVE_MALLOC_H
/* Useful include for some Windows compilers. */
#include <malloc.h>
#endif /* HAVE_MALLOC_H */
#include <stddef.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h> /* open(), off_t, size_t, ssize_t */
#endif /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>   /* for open() */
#endif /* HAVE_SYS_STAT_H */
#include <fcntl.h>   /* for open() */
#ifdef _WIN32
#include <io.h> /* lseek(), read(), close() */
#elif defined HAVE_UNISTD_H
#include <unistd.h> /* lseek(), read(), close() */
#endif /* _WIN32 */

/* Windows specific header files */
#if defined(_WIN32) && defined(HAVE_STDAFX_H)
#include "stdafx.h"
#endif /* HAVE_STDAFX_H */

#include "libdwarf_private.h"
#include "dwarf.h"
#include "libdwarf.h"
#include "dwarf_base_types.h"
#include "dwarf_opaque.h"
#include "memcpy_swap.h"
#include "dwarf_elfstructs.h"
#include "dwarf_reading.h"
#include "dwarf_elf_defines.h"
#include "dwarf_elfread.h"
#include "dwarf_object_detector.h"
#include "dwarf_object_read_common.h"
#include "dwarf_util.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif /* O_BINARY */

#if 0
/*  One example of calling this.
    place just before DW_DLE_SECTION_SIZE_OR_OFFSET_LARGE
    dumpsizes(__LINE__,strsectlength,strpsh->gh_offset,
    ep->f_filesize);
*/
static void
dumpsizes(int line,Dwarf_Unsigned s,
    Dwarf_Unsigned o,
    Dwarf_Unsigned fsz)
{
    Dwarf_Unsigned tlen = s + o;
    printf("Size Error DEBUGONLY sz 0x%llx off 0x%llx fsx 0x%llx "
        "sum 0x%llx line %d \n",
        s,o,fsz,tlen,line);
}
#endif

#ifdef WORDS_BIGENDIAN
#define ASNAR(func,t,s)                         \
    do {                                        \
        unsigned tbyte = sizeof(t) - sizeof(s); \
        t = 0;                                  \
        func(((char *)&t)+tbyte ,&s[0],sizeof(s));  \
    } while (0)
#else /* LITTLE ENDIAN */
#define ASNAR(func,t,s)                         \
    do {                                        \
        t = 0;                                  \
        func(&t,&s[0],sizeof(s));               \
    } while (0)
#endif /* end LITTLE- BIG-ENDIAN */

static int
_dwarf_load_elf_section_is_dwarf(const char *sname,
    int *is_rela,int *is_rel)
{
    *is_rel = FALSE;
    *is_rela = FALSE;
    if (_dwarf_ignorethissection(sname)) {
        return FALSE;
    }
    if (!strncmp(sname,".rel",4)) {
        if (!strncmp(sname,".rela.",6)) {
            *is_rela = TRUE;
            return TRUE;
        }
        if (!strncmp(sname,".rel.",5)) {
            *is_rela = TRUE;
            return TRUE;
        }
        /*  Else something is goofy/Impossible */
        return FALSE;
    }
    if (!strncmp(sname,".debug_",7)) {
        return TRUE;
    }
    if (!strncmp(sname,".zdebug_",8)) {
        return TRUE;
    }
    if (!strcmp(sname,".eh_frame")) {
        return TRUE;
    }
    if (!strncmp(sname,".gdb_index",10)) {
        return TRUE;
    }
    return FALSE;
}


static int
is_empty_section(Dwarf_Unsigned type)
{
    if (type == SHT_NOBITS) {
        return TRUE;
    }
    if (type == SHT_NULL) {
        return TRUE;
    }
    return FALSE;
}

#if 0
int
dwarf_construct_elf_access_path(const char *path,
    dwarf_elf_object_access_internals_t **mp,int *errcode)
{
    int fd = -1;
    int res = 0;
    dwarf_elf_object_access_internals_t *mymp = 0;

    fd = open(path, O_RDONLY|O_BINARY);
    if (fd < 0) {
        *errcode = DW_DLE_PATH_SIZE_TOO_SMALL;
        return DW_DLV_ERROR;
    }
    res = dwarf_construct_elf_access(fd,
        path,&mymp,errcode);
    if (res != DW_DLV_OK) {
        close(fd);
        return res;
    }
    mymp->f_destruct_close_fd = TRUE;
    *mp = mymp;
    return res;
}
#endif /* 0 */

/* Here path is not essential. Pass in with "" if unknown. */
int
dwarf_construct_elf_access(int fd,
    const char *path,
    dwarf_elf_object_access_internals_t **mp,int *errcode)
{
    unsigned ftype = 0;
    unsigned endian = 0;
    unsigned offsetsize = 0;
    Dwarf_Unsigned filesize = 0;
    dwarf_elf_object_access_internals_t *mfp = 0;
    int      res = 0;

    res = dwarf_object_detector_fd(fd,
        &ftype,&endian,&offsetsize, &filesize, errcode);
    if (res != DW_DLV_OK) {
        return res;
    }

    mfp = calloc(1,sizeof(dwarf_elf_object_access_internals_t));
    if (!mfp) {
        *errcode = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    /* For non-libelf Elf, call it 'F'. Libelf Elf uses 'E' */
    mfp->f_ident[0] = 'F';
    mfp->f_ident[1] = 1;
    mfp->f_fd = fd;
    mfp->f_destruct_close_fd = FALSE;
    mfp->f_is_64bit =  ((offsetsize==64)?TRUE:FALSE);
    mfp->f_filesize = filesize;
    mfp->f_offsetsize = offsetsize;
    mfp->f_pointersize = offsetsize;
    mfp->f_endian = endian;
    mfp->f_ftype = ftype;
    mfp->f_path = strdup(path);

    *mp = mfp;
    return DW_DLV_OK;
}

/*  Caller must zero the passed in pointer
    after this returns to remind
    the caller to avoid use of the pointer. */
int
dwarf_destruct_elf_access(dwarf_elf_object_access_internals_t* ep,
    int *errcode UNUSEDARG)
{
    struct generic_shdr *shp = 0;
    Dwarf_Unsigned shcount = 0;
    Dwarf_Unsigned i = 0;

    free(ep->f_ehdr);
    shp = ep->f_shdr;
    shcount = ep->f_loc_shdr.g_count;
    for (i = 0; i < shcount; ++i,++shp) {
        free(shp->gh_rels);
        shp->gh_rels = 0;
        free(shp->gh_content);
        shp->gh_content = 0;
        free(shp->gh_sht_group_array);
        shp->gh_sht_group_array = 0;
        shp->gh_sht_group_array_count = 0;
    }
    free(ep->f_shdr);
    free(ep->f_phdr);
    free(ep->f_elf_shstrings_data);
    free(ep->f_dynamic);
    free(ep->f_symtab_sect_strings);
    free(ep->f_dynsym_sect_strings);
    free(ep->f_symtab);
    free(ep->f_dynsym);

    /* if TRUE close f_fd on destruct.*/
    if (ep->f_destruct_close_fd) {
        close(ep->f_fd);
    }
    ep->f_ident[0] = 'X';
    free(ep->f_path);
    free(ep);
    return DW_DLV_OK;
}




static int
generic_ehdr_from_32(dwarf_elf_object_access_internals_t *ep,
    struct generic_ehdr *ehdr, dw_elf32_ehdr *e,
    int *errcode UNUSEDARG)
{
    int i = 0;

    for (i = 0; i < EI_NIDENT; ++i) {
        ehdr->ge_ident[i] = e->e_ident[i];
    }
    ASNAR(ep->f_copy_word,ehdr->ge_type,e->e_type);
    ASNAR(ep->f_copy_word,ehdr->ge_machine,e->e_machine);
    ASNAR(ep->f_copy_word,ehdr->ge_version,e->e_version);
    ASNAR(ep->f_copy_word,ehdr->ge_entry,e->e_entry);
    ASNAR(ep->f_copy_word,ehdr->ge_phoff,e->e_phoff);
    ASNAR(ep->f_copy_word,ehdr->ge_shoff,e->e_shoff);
    ASNAR(ep->f_copy_word,ehdr->ge_flags,e->e_flags);
    ASNAR(ep->f_copy_word,ehdr->ge_ehsize,e->e_ehsize);
    ASNAR(ep->f_copy_word,ehdr->ge_phentsize,e->e_phentsize);
    ASNAR(ep->f_copy_word,ehdr->ge_phnum,e->e_phnum);
    ASNAR(ep->f_copy_word,ehdr->ge_shentsize,e->e_shentsize);
    ASNAR(ep->f_copy_word,ehdr->ge_shnum,e->e_shnum);
    ASNAR(ep->f_copy_word,ehdr->ge_shstrndx,e->e_shstrndx);
    ep->f_machine = ehdr->ge_machine;
    ep->f_ehdr = ehdr;
    ep->f_loc_ehdr.g_name = "Elf File Header";
    ep->f_loc_ehdr.g_offset = 0;
    ep->f_loc_ehdr.g_count = 1;
    ep->f_loc_ehdr.g_entrysize = sizeof(dw_elf32_ehdr);
    ep->f_loc_ehdr.g_totalsize = sizeof(dw_elf32_ehdr);
    return DW_DLV_OK;
}

static int
generic_ehdr_from_64(dwarf_elf_object_access_internals_t* ep,
    struct generic_ehdr *ehdr, dw_elf64_ehdr *e,
    int *errcode UNUSEDARG)
{
    int i = 0;

    for (i = 0; i < EI_NIDENT; ++i) {
        ehdr->ge_ident[i] = e->e_ident[i];
    }
    ASNAR(ep->f_copy_word,ehdr->ge_type,e->e_type);
    ASNAR(ep->f_copy_word,ehdr->ge_machine,e->e_machine);
    ASNAR(ep->f_copy_word,ehdr->ge_version,e->e_version);
    ASNAR(ep->f_copy_word,ehdr->ge_entry,e->e_entry);
    ASNAR(ep->f_copy_word,ehdr->ge_phoff,e->e_phoff);
    ASNAR(ep->f_copy_word,ehdr->ge_shoff,e->e_shoff);
    ASNAR(ep->f_copy_word,ehdr->ge_flags,e->e_flags);
    ASNAR(ep->f_copy_word,ehdr->ge_ehsize,e->e_ehsize);
    ASNAR(ep->f_copy_word,ehdr->ge_phentsize,e->e_phentsize);
    ASNAR(ep->f_copy_word,ehdr->ge_phnum,e->e_phnum);
    ASNAR(ep->f_copy_word,ehdr->ge_shentsize,e->e_shentsize);
    ASNAR(ep->f_copy_word,ehdr->ge_shnum,e->e_shnum);
    ASNAR(ep->f_copy_word,ehdr->ge_shstrndx,e->e_shstrndx);
    ep->f_machine = ehdr->ge_machine;
    ep->f_ehdr = ehdr;
    ep->f_loc_ehdr.g_name = "Elf File Header";
    ep->f_loc_ehdr.g_offset = 0;
    ep->f_loc_ehdr.g_count = 1;
    ep->f_loc_ehdr.g_entrysize = sizeof(dw_elf64_ehdr);
    ep->f_loc_ehdr.g_totalsize = sizeof(dw_elf64_ehdr);
    return DW_DLV_OK;
}


#if 0 /* not used */
static int
generic_phdr_from_phdr32(dwarf_elf_object_access_internals_t* ep,
    struct generic_phdr **phdr_out,
    Dwarf_Unsigned * count_out,
    Dwarf_Unsigned offset,
    Dwarf_Unsigned entsize,
    Dwarf_Unsigned count,
    int *errcode)
{
    dw_elf32_phdr *pph =0;
    dw_elf32_phdr *orig_pph =0;
    struct generic_phdr *gphdr =0;
    struct generic_phdr *orig_gphdr =0;
    Dwarf_Unsigned i = 0;
    int res = 0;

    *count_out = 0;
    pph = (dw_elf32_phdr *)calloc(count , entsize);
    if (pph == 0) {
        *errcode =  DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    gphdr = (struct generic_phdr *)calloc(count,sizeof(*gphdr));
    if (gphdr == 0) {
        free(pph);
        *errcode =  DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }

    orig_pph = pph;
    orig_gphdr = gphdr;
    res = RRMOA(ep->f_fd,pph,offset,count*entsize,
        ep->f_filesize,errcode);
    if (res != DW_DLV_OK) {
        free(pph);
        free(gphdr);
        return res;
    }
    for ( i = 0; i < count;
        ++i,  pph++,gphdr++) {
        ASNAR(ep->f_copy_word,gphdr->gp_type,pph->p_type);
        ASNAR(ep->f_copy_word,gphdr->gp_offset,pph->p_offset);
        ASNAR(ep->f_copy_word,gphdr->gp_vaddr,pph->p_vaddr);
        ASNAR(ep->f_copy_word,gphdr->gp_paddr,pph->p_paddr);
        ASNAR(ep->f_copy_word,gphdr->gp_filesz,pph->p_filesz);
        ASNAR(ep->f_copy_word,gphdr->gp_memsz,pph->p_memsz);
        ASNAR(ep->f_copy_word,gphdr->gp_flags,pph->p_flags);
        ASNAR(ep->f_copy_word,gphdr->gp_align,pph->p_align);
    }
    free(orig_pph);
    *phdr_out = orig_gphdr;
    *count_out = count;
    ep->f_phdr = orig_gphdr;
    ep->f_loc_phdr.g_name = "Program Header";
    ep->f_loc_phdr.g_offset = offset;
    ep->f_loc_phdr.g_count = count;
    ep->f_loc_phdr.g_entrysize = sizeof(dw_elf32_phdr);
    ep->f_loc_phdr.g_totalsize = sizeof(dw_elf32_phdr)*count;
    return DW_DLV_OK;
}

static int
generic_phdr_from_phdr64(dwarf_elf_object_access_internals_t* ep,
    struct generic_phdr **phdr_out,
    Dwarf_Unsigned * count_out,
    Dwarf_Unsigned offset,
    Dwarf_Unsigned entsize,
    Dwarf_Unsigned count,
    int *errcode)
{
    dw_elf64_phdr *pph =0;
    dw_elf64_phdr *orig_pph =0;
    struct generic_phdr *gphdr =0;
    struct generic_phdr *orig_gphdr =0;
    int res = 0;
    Dwarf_Unsigned i = 0;

    *count_out = 0;
    pph = (dw_elf64_phdr *)calloc(count , entsize);
    if (pph == 0) {
        *errcode =  DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    gphdr = (struct generic_phdr *)calloc(count,sizeof(*gphdr));
    if (gphdr == 0) {
        free(pph);
        *errcode =  DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }

    orig_pph = pph;
    orig_gphdr = gphdr;
    res = RRMOA(ep->f_fd,pph,offset,count*entsize,
        ep->f_filesize,errcode);
    if (res != DW_DLV_OK) {
        free(pph);
        free(gphdr);
        return res;
    }
    for ( i = 0; i < count;
        ++i,  pph++,gphdr++) {
        ASNAR(ep->f_copy_word,gphdr->gp_type,pph->p_type);
        ASNAR(ep->f_copy_word,gphdr->gp_offset,pph->p_offset);
        ASNAR(ep->f_copy_word,gphdr->gp_vaddr,pph->p_vaddr);
        ASNAR(ep->f_copy_word,gphdr->gp_paddr,pph->p_paddr);
        ASNAR(ep->f_copy_word,gphdr->gp_filesz,pph->p_filesz);
        ASNAR(ep->f_copy_word,gphdr->gp_memsz,pph->p_memsz);
        ASNAR(ep->f_copy_word,gphdr->gp_flags,pph->p_flags);
        ASNAR(ep->f_copy_word,gphdr->gp_align,pph->p_align);
    }
    free(orig_pph);
    *phdr_out = orig_gphdr;
    *count_out = count;
    ep->f_phdr = orig_gphdr;
    ep->f_loc_phdr.g_name = "Program Header";
    ep->f_loc_phdr.g_offset = offset;
    ep->f_loc_phdr.g_count = count;
    ep->f_loc_phdr.g_entrysize = sizeof(dw_elf64_phdr);
    ep->f_loc_phdr.g_totalsize = sizeof(dw_elf64_phdr)*count;
    return DW_DLV_OK;
}
#endif /* not used */

static int
generic_shdr_from_shdr32(dwarf_elf_object_access_internals_t *ep,
    Dwarf_Unsigned * count_out,
    Dwarf_Unsigned offset,
    Dwarf_Unsigned entsize,
    Dwarf_Unsigned count,
    int *errcode)
{
    dw_elf32_shdr          *psh =0;
    dw_elf32_shdr          *orig_psh =0;
    struct generic_shdr *gshdr =0;
    struct generic_shdr *orig_gshdr =0;
    Dwarf_Unsigned i = 0;
    int res = 0;

    *count_out = 0;
    psh = (dw_elf32_shdr *)calloc(count , entsize);
    if (!psh) {
        *errcode = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    gshdr = (struct generic_shdr *)calloc(count,sizeof(*gshdr));
    if (!gshdr) {
        free(psh);
        *errcode = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }

    orig_psh = psh;
    orig_gshdr = gshdr;
    res = RRMOA(ep->f_fd,psh,offset,count*entsize,
        ep->f_filesize,errcode);
    if (res != DW_DLV_OK) {
        free(psh);
        free(gshdr);
        return res;
    }
    for (i = 0; i < count;
        ++i,  psh++,gshdr++) {
        gshdr->gh_secnum = i;
        ASNAR(ep->f_copy_word,gshdr->gh_name,psh->sh_name);
        ASNAR(ep->f_copy_word,gshdr->gh_type,psh->sh_type);
        ASNAR(ep->f_copy_word,gshdr->gh_flags,psh->sh_flags);
        ASNAR(ep->f_copy_word,gshdr->gh_addr,psh->sh_addr);
        ASNAR(ep->f_copy_word,gshdr->gh_offset,psh->sh_offset);
        ASNAR(ep->f_copy_word,gshdr->gh_size,psh->sh_size);
        ASNAR(ep->f_copy_word,gshdr->gh_link,psh->sh_link);
        ASNAR(ep->f_copy_word,gshdr->gh_info,psh->sh_info);
        ASNAR(ep->f_copy_word,gshdr->gh_addralign,psh->sh_addralign);
        ASNAR(ep->f_copy_word,gshdr->gh_entsize,psh->sh_entsize);
        if (gshdr->gh_type == SHT_REL || gshdr->gh_type == SHT_RELA){
            gshdr->gh_reloc_target_secnum = gshdr->gh_info;
        }
    }
    free(orig_psh);
    *count_out = count;
    ep->f_shdr = orig_gshdr;
    ep->f_loc_shdr.g_name = "Section Header";
    ep->f_loc_shdr.g_count = count;
    ep->f_loc_shdr.g_offset = offset;
    ep->f_loc_shdr.g_entrysize = sizeof(dw_elf32_shdr);
    ep->f_loc_shdr.g_totalsize = sizeof(dw_elf32_shdr)*count;
    return DW_DLV_OK;
}

static int
generic_shdr_from_shdr64(dwarf_elf_object_access_internals_t *ep,
    Dwarf_Unsigned * count_out,
    Dwarf_Unsigned offset,
    Dwarf_Unsigned entsize,
    Dwarf_Unsigned count,
    int *errcode)
{
    dw_elf64_shdr          *psh =0;
    dw_elf64_shdr          *orig_psh =0;
    struct generic_shdr *gshdr =0;
    struct generic_shdr *orig_gshdr =0;
    Dwarf_Unsigned i = 0;
    int res = 0;

    *count_out = 0;
    psh = (dw_elf64_shdr *)calloc(count , entsize);
    if (!psh) {
        *errcode = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    gshdr = (struct generic_shdr *)calloc(count,sizeof(*gshdr));
    if (gshdr == 0) {
        free(psh);
        *errcode = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }

    orig_psh = psh;
    orig_gshdr = gshdr;
    res = RRMOA(ep->f_fd,psh,offset,count*entsize,
        ep->f_filesize,errcode);
    if (res != DW_DLV_OK) {
        free(psh);
        free(gshdr);
        return res;
    }
    for ( i = 0; i < count;
        ++i,  psh++,gshdr++) {
        gshdr->gh_secnum = i;
        ASNAR(ep->f_copy_word,gshdr->gh_name,psh->sh_name);
        ASNAR(ep->f_copy_word,gshdr->gh_type,psh->sh_type);
        ASNAR(ep->f_copy_word,gshdr->gh_flags,psh->sh_flags);
        ASNAR(ep->f_copy_word,gshdr->gh_addr,psh->sh_addr);
        ASNAR(ep->f_copy_word,gshdr->gh_offset,psh->sh_offset);
        ASNAR(ep->f_copy_word,gshdr->gh_size,psh->sh_size);
        ASNAR(ep->f_copy_word,gshdr->gh_link,psh->sh_link);
        ASNAR(ep->f_copy_word,gshdr->gh_info,psh->sh_info);
        ASNAR(ep->f_copy_word,gshdr->gh_addralign,psh->sh_addralign);
        ASNAR(ep->f_copy_word,gshdr->gh_entsize,psh->sh_entsize);
        if (gshdr->gh_type == SHT_REL ||
            gshdr->gh_type == SHT_RELA){
            gshdr->gh_reloc_target_secnum = gshdr->gh_info;
        }
    }
    free(orig_psh);
    *count_out = count;
    ep->f_shdr = orig_gshdr;
    ep->f_loc_shdr.g_name = "Section Header";
    ep->f_loc_shdr.g_count = count;
    ep->f_loc_shdr.g_offset = offset;
    ep->f_loc_shdr.g_entrysize = sizeof(dw_elf64_shdr);
    ep->f_loc_shdr.g_totalsize = sizeof(dw_elf64_shdr)*count;
    return DW_DLV_OK;
}



static int
dwarf_generic_elf_load_symbols32(
    dwarf_elf_object_access_internals_t *ep,
    struct generic_symentry **gsym_out,
    Dwarf_Unsigned offset,Dwarf_Unsigned size,
    Dwarf_Unsigned *count_out,int *errcode)
{
    Dwarf_Unsigned ecount = 0;
    Dwarf_Unsigned size2 = 0;
    Dwarf_Unsigned i = 0;
    dw_elf32_sym *psym = 0;
    dw_elf32_sym *orig_psym = 0;
    struct generic_symentry * gsym = 0;
    struct generic_symentry * orig_gsym = 0;
    int res = 0;

    ecount = (long)(size/sizeof(dw_elf32_sym));
    size2 = ecount * sizeof(dw_elf32_sym);
    if (size != size2) {
        *errcode = DW_DLE_SECTION_SIZE_ERROR;
        return DW_DLV_ERROR;
    }
    psym = calloc(ecount,sizeof(dw_elf32_sym));
    if (!psym) {
        *errcode = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    gsym = calloc(ecount,sizeof(struct generic_symentry));
    if (!gsym) {
        free(psym);
        *errcode = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    res = RRMOA(ep->f_fd,psym,offset,size,
        ep->f_filesize,errcode);
    if (res!= DW_DLV_OK) {
        free(psym);
        free(gsym);
        return res;
    }
    orig_psym = psym;
    orig_gsym = gsym;
    for ( i = 0; i < ecount; ++i,++psym,++gsym) {
        Dwarf_Unsigned bind = 0;
        Dwarf_Unsigned type = 0;

        ASNAR(ep->f_copy_word,gsym->gs_name,psym->st_name);
        ASNAR(ep->f_copy_word,gsym->gs_value,psym->st_value);
        ASNAR(ep->f_copy_word,gsym->gs_size,psym->st_size);
        ASNAR(ep->f_copy_word,gsym->gs_info,psym->st_info);
        ASNAR(ep->f_copy_word,gsym->gs_other,psym->st_other);
        ASNAR(ep->f_copy_word,gsym->gs_shndx,psym->st_shndx);
        bind = gsym->gs_info >> 4;
        type = gsym->gs_info & 0xf;
        gsym->gs_bind = bind;
        gsym->gs_type = type;
    }
    *count_out = ecount;
    *gsym_out = orig_gsym;
    free(orig_psym);
    return DW_DLV_OK;
}


static int
dwarf_generic_elf_load_symbols64(
    dwarf_elf_object_access_internals_t *ep,
    struct generic_symentry **gsym_out,
    Dwarf_Unsigned offset,Dwarf_Unsigned size,
    Dwarf_Unsigned *count_out,int *errcode)
{
    Dwarf_Unsigned ecount = 0;
    Dwarf_Unsigned size2 = 0;
    Dwarf_Unsigned i = 0;
    dw_elf64_sym *psym = 0;
    dw_elf64_sym *orig_psym = 0;
    struct generic_symentry * gsym = 0;
    struct generic_symentry * orig_gsym = 0;
    int res = 0;

    ecount = (long)(size/sizeof(dw_elf64_sym));
    size2 = ecount * sizeof(dw_elf64_sym);
    if (size != size2) {
        *errcode = DW_DLE_SECTION_SIZE_ERROR;
        return DW_DLV_ERROR;
    }
    psym = calloc(ecount,sizeof(dw_elf64_sym));
    if (!psym) {
        *errcode = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    gsym = calloc(ecount,sizeof(struct generic_symentry));
    if (!gsym) {
        free(psym);
        *errcode = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    res = RRMOA(ep->f_fd,psym,offset,size,
        ep->f_filesize,errcode);
    if (res!= DW_DLV_OK) {
        free(psym);
        free(gsym);
        *errcode = DW_DLE_ALLOC_FAIL;
        return res;
    }
    orig_psym = psym;
    orig_gsym = gsym;
    for ( i = 0; i < ecount; ++i,++psym,++gsym) {
        Dwarf_Unsigned bind = 0;
        Dwarf_Unsigned type = 0;

        ASNAR(ep->f_copy_word,gsym->gs_name,psym->st_name);
        ASNAR(ep->f_copy_word,gsym->gs_value,psym->st_value);
        ASNAR(ep->f_copy_word,gsym->gs_size,psym->st_size);
        ASNAR(ep->f_copy_word,gsym->gs_info,psym->st_info);
        ASNAR(ep->f_copy_word,gsym->gs_other,psym->st_other);
        ASNAR(ep->f_copy_word,gsym->gs_shndx,psym->st_shndx);
        bind = gsym->gs_info >> 4;
        type = gsym->gs_info & 0xf;
        gsym->gs_bind = bind;
        gsym->gs_type = type;
    }
    *count_out = ecount;
    *gsym_out = orig_gsym;
    free(orig_psym);
    return DW_DLV_OK;
}

static int
dwarf_generic_elf_load_symbols(
    dwarf_elf_object_access_internals_t *ep,
    int secnum,
    struct generic_shdr *psh,
    struct generic_symentry **gsym_out,
    Dwarf_Unsigned *count_out,int *errcode)
{
    int res = 0;
    struct generic_symentry *gsym = 0;
    Dwarf_Unsigned count = 0;

    if (!secnum) {
        return DW_DLV_NO_ENTRY;
    }
    if (psh->gh_size > ep->f_filesize) {
        *errcode = DW_DLE_SECTION_SIZE_ERROR;
        return DW_DLV_ERROR;
    }
    if (ep->f_offsetsize == 32) {
        res = dwarf_generic_elf_load_symbols32(ep,
            &gsym,
            psh->gh_offset,psh->gh_size,
            &count,errcode);
    } else if (ep->f_offsetsize == 64) {
        res = dwarf_generic_elf_load_symbols64(ep,
            &gsym,
            psh->gh_offset,psh->gh_size,
            &count,errcode);
    } else {
        *errcode = DW_DLE_OFFSET_SIZE;
        return DW_DLV_ERROR;
    }
    if (res == DW_DLV_OK) {
        *gsym_out = gsym;
        *count_out = count;
    }
    return res;
}
#if 0
int
dwarf_load_elf_dynsym_symbols(
    dwarf_elf_object_access_internals_t *ep, int*errcode)
{
    int res = 0;
    struct generic_symentry *gsym = 0;
    Dwarf_Unsigned count = 0;
    Dwarf_Unsigned secnum = ep->f_dynsym_sect_index;
    struct generic_shdr * psh = 0;

    if (!secnum) {
        return DW_DLV_NO_ENTRY;
    }
    psh = ep->f_shdr + secnum;
    if we ever use this... gh_size big?
    res = dwarf_generic_elf_load_symbols(ep,
        secnum,
        psh,
        &gsym,
        &count,errcode);
    if (res == DW_DLV_OK) {
        ep->f_dynsym = gsym;
        ep->f_loc_dynsym.g_count = count;
    }
    return res;
}
#endif /* 0 */

int
_dwarf_load_elf_symtab_symbols(
    dwarf_elf_object_access_internals_t *ep, int*errcode)
{
    int res = 0;
    struct generic_symentry *gsym = 0;
    Dwarf_Unsigned count = 0;
    Dwarf_Unsigned secnum = ep->f_symtab_sect_index;
    struct generic_shdr * psh = 0;

    if (!secnum) {
        return DW_DLV_NO_ENTRY;
    }
    psh = ep->f_shdr + secnum;
    if (psh->gh_size > ep->f_filesize) {
        *errcode = DW_DLE_SECTION_SIZE_ERROR;
        return DW_DLV_ERROR;
    }
    res = dwarf_generic_elf_load_symbols(ep,
        secnum,
        psh,
        &gsym,
        &count,errcode);
    if (res == DW_DLV_OK) {
        ep->f_symtab = gsym;
        ep->f_loc_symtab.g_count = count;
    }
    return res;
}

static int
generic_rel_from_rela32(
    dwarf_elf_object_access_internals_t *ep,
    struct generic_shdr * gsh,
    dw_elf32_rela *relp,
    struct generic_rela *grel,
    int *errcode)
{
    Dwarf_Unsigned ecount = 0;
    Dwarf_Unsigned size = gsh->gh_size;
    Dwarf_Unsigned size2 = 0;
    Dwarf_Unsigned i = 0;

    ecount = size/sizeof(dw_elf32_rela);
    size2 = ecount * sizeof(dw_elf32_rela);
    if (size != size2) {
        *errcode = DW_DLE_SECTION_SIZE_ERROR;
        return  DW_DLV_ERROR;
    }
    for ( i = 0; i < ecount; ++i,++relp,++grel) {
        ASNAR(ep->f_copy_word,grel->gr_offset,relp->r_offset);
        ASNAR(ep->f_copy_word,grel->gr_info,relp->r_info);
        /* addend signed */
        ASNAR(ep->f_copy_word,grel->gr_addend,relp->r_addend);
        SIGN_EXTEND(grel->gr_addend,sizeof(relp->r_addend));
        grel->gr_sym  = grel->gr_info>>8; /* ELF32_R_SYM */
        grel->gr_type = grel->gr_info & 0xff;
        grel->gr_is_rela = TRUE;
    }
    return DW_DLV_OK;
}

static int
generic_rel_from_rela64(
    dwarf_elf_object_access_internals_t *ep,
    struct generic_shdr * gsh,
    dw_elf64_rela *relp,
    struct generic_rela *grel, int *errcode)
{
    Dwarf_Unsigned ecount = 0;
    Dwarf_Unsigned size = gsh->gh_size;
    Dwarf_Unsigned size2 = 0;
    Dwarf_Unsigned i = 0;
    int objlittleendian = (ep->f_endian == DW_OBJECT_LSB);
    int ismips64 = (ep->f_machine == EM_MIPS);
    int issparcv9 = (ep->f_machine == EM_SPARCV9);

    ecount = size/sizeof(dw_elf64_rela);
    size2 = ecount * sizeof(dw_elf64_rela);
    if (size != size2) {
        *errcode = DW_DLE_SECTION_SIZE_ERROR;
        return  DW_DLV_ERROR;
    }
    for ( i = 0; i < ecount; ++i,++relp,++grel) {
        ASNAR(ep->f_copy_word,grel->gr_offset,relp->r_offset);
        ASNAR(ep->f_copy_word,grel->gr_info,relp->r_info);
        ASNAR(ep->f_copy_word,grel->gr_addend,relp->r_addend);
        SIGN_EXTEND(grel->gr_addend,sizeof(relp->r_addend));
        if (ismips64 && objlittleendian ) {
            char realsym[4];

            memcpy(realsym,&relp->r_info,sizeof(realsym));
            ASNAR(ep->f_copy_word,grel->gr_sym,realsym);
            grel->gr_type  = relp->r_info[7];
            grel->gr_type2 = relp->r_info[6];
            grel->gr_type3 = relp->r_info[5];
        } else if (issparcv9) {
            /*  Always Big Endian?  */
            char realsym[4];

            memcpy(realsym,&relp->r_info,sizeof(realsym));
            ASNAR(ep->f_copy_word,grel->gr_sym,realsym);
            grel->gr_type  = relp->r_info[7];
        } else {
            grel->gr_sym  = grel->gr_info >> 32;
            grel->gr_type = grel->gr_info & 0xffffffff;
        }
        grel->gr_is_rela = TRUE;
    }
    return DW_DLV_OK;
}

static int
generic_rel_from_rel32(
    dwarf_elf_object_access_internals_t *ep,
    struct generic_shdr * gsh,
    dw_elf32_rel *relp,
    struct generic_rela *grel,int *errcode)
{
    Dwarf_Unsigned ecount = 0;
    Dwarf_Unsigned size = gsh->gh_size;
    Dwarf_Unsigned size2 = 0;
    Dwarf_Unsigned i = 0;

    ecount = size/sizeof(dw_elf32_rel);
    size2 = ecount * sizeof(dw_elf32_rel);
    if (size != size2) {
        *errcode = DW_DLE_SECTION_SIZE_ERROR;
        return  DW_DLV_ERROR;
    }
    for ( i = 0; i < ecount; ++i,++relp,++grel) {
        ASNAR(ep->f_copy_word,grel->gr_offset,relp->r_offset);
        ASNAR(ep->f_copy_word,grel->gr_info,relp->r_info);
        grel->gr_addend  = 0; /* Unused for plain .rel */
        grel->gr_sym  = grel->gr_info >>8; /* ELF32_R_SYM */
        grel->gr_is_rela = FALSE;
        grel->gr_type = grel->gr_info & 0xff;
    }
    return DW_DLV_OK;
}

static int
generic_rel_from_rel64(
    dwarf_elf_object_access_internals_t *ep,
    struct generic_shdr * gsh,
    dw_elf64_rel *relp,
    struct generic_rela *grel,int *errcode)
{
    Dwarf_Unsigned ecount = 0;
    Dwarf_Unsigned size = gsh->gh_size;
    Dwarf_Unsigned size2 = 0;
    Dwarf_Unsigned i = 0;
    int objlittleendian = (ep->f_endian == DW_OBJECT_LSB);
    int ismips64 = (ep->f_machine == EM_MIPS);
    int issparcv9 = (ep->f_machine == EM_SPARCV9);

    ecount = size/sizeof(dw_elf64_rel);
    size2 = ecount * sizeof(dw_elf64_rel);
    if (size != size2) {
        *errcode = DW_DLE_SECTION_SIZE_ERROR;
        return DW_DLV_ERROR;
    }
    for ( i = 0; i < ecount; ++i,++relp,++grel) {
        ASNAR(ep->f_copy_word,grel->gr_offset,relp->r_offset);
        ASNAR(ep->f_copy_word,grel->gr_info,relp->r_info);
        grel->gr_addend  = 0; /* Unused for plain .rel */
        if (ismips64 && objlittleendian ) {
            char realsym[4];

            memcpy(realsym,&relp->r_info,sizeof(realsym));
            ASNAR(ep->f_copy_word,grel->gr_sym,realsym);
            grel->gr_type  = relp->r_info[7];
            grel->gr_type2 = relp->r_info[6];
            grel->gr_type3 = relp->r_info[5];
        } else if (issparcv9) {
            /*  Always Big Endian?  */
            char realsym[4];

            memcpy(realsym,&relp->r_info,sizeof(realsym));
            ASNAR(ep->f_copy_word,grel->gr_sym,realsym);
            grel->gr_type  = relp->r_info[7];
        } else {
            grel->gr_sym  = grel->gr_info >>32;
            grel->gr_type = grel->gr_info & 0xffffffff;
        }
        grel->gr_is_rela = FALSE;
    }
    return DW_DLV_OK;
}

#if 0
int
dwarf_load_elf_dynstr(
    dwarf_elf_object_access_internals_t *ep, int *errcode)
{
    struct generic_shdr *strpsh = 0;
    int res = 0;
    Dwarf_Unsigned strsectindex  =0;
    Dwarf_Unsigned strsectlength = 0;

        if (!ep->f_dynsym_sect_strings_sect_index) {
            return DW_DLV_NO_ENTRY;
        }
        strsectindex = ep->f_dynsym_sect_strings_sect_index;
        strsectlength = ep->f_dynsym_sect_strings_max;
        strpsh = ep->f_shdr + strsectindex;
        /*  Alloc an extra byte as a guaranteed NUL byte
            at the end of the strings in case the section
            is corrupted and lacks a NUL at end. */
        ep->f_dynsym_sect_strings = calloc(1,strsectlength+1);
        if (!ep->f_dynsym_sect_strings) {
            ep->f_dynsym_sect_strings = 0;
            ep->f_dynsym_sect_strings_max = 0;
            ep->f_dynsym_sect_strings_sect_index = 0;
            *errcode = DW_DLE_ALLOC_FAIL;
            return DW_DLV_ERROR;
        }
        res = RRMOA(ep->f_fd,ep->f_dynsym_sect_strings,
            strpsh->gh_offset,
            strsectlength,
            ep->f_filesize,errcode);
        if (res != DW_DLV_OK) {
            ep->f_dynsym_sect_strings = 0;
            ep->f_dynsym_sect_strings_max = 0;
            ep->f_dynsym_sect_strings_sect_index = 0;
            return res;
        }
    return DW_DLV_OK;
}
#endif /* 0 */

int
_dwarf_load_elf_symstr(
    dwarf_elf_object_access_internals_t *ep, int *errcode)
{
    struct generic_shdr *strpsh = 0;
    int res = 0;
    Dwarf_Unsigned strsectindex  =0;
    Dwarf_Unsigned strsectlength = 0;

    if (!ep->f_symtab_sect_strings_sect_index) {
        return DW_DLV_NO_ENTRY;
    }
    strsectindex = ep->f_symtab_sect_strings_sect_index;
    strsectlength = ep->f_symtab_sect_strings_max;
    strpsh = ep->f_shdr + strsectindex;
    /*  Alloc an extra byte as a guaranteed NUL byte
        at the end of the strings in case the section
        is corrupted and lacks a NUL at end. */
    if (strsectlength > ep->f_filesize ||
        strpsh->gh_offset >ep->f_filesize ||
        (strsectlength + strpsh->gh_offset) >
            ep->f_filesize) {
        *errcode = DW_DLE_SECTION_SIZE_OR_OFFSET_LARGE;
        return DW_DLV_ERROR;
    }
    ep->f_symtab_sect_strings = calloc(1,strsectlength+1);
    if (!ep->f_symtab_sect_strings) {
        ep->f_symtab_sect_strings = 0;
        ep->f_symtab_sect_strings_max = 0;
        ep->f_symtab_sect_strings_sect_index = 0;
        *errcode = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    res = RRMOA(ep->f_fd,ep->f_symtab_sect_strings,
        strpsh->gh_offset,
        strsectlength,
        ep->f_filesize,errcode);
    if (res != DW_DLV_OK) {
        free(ep->f_symtab_sect_strings);
        ep->f_symtab_sect_strings = 0;
        ep->f_symtab_sect_strings_max = 0;
        ep->f_symtab_sect_strings_sect_index = 0;
        return res;
    }
    return DW_DLV_OK;
}


static int
_dwarf_elf_load_sectstrings(
    dwarf_elf_object_access_internals_t *ep,
    Dwarf_Unsigned stringsection,
    int *errcode)
{
    int res = 0;
    struct generic_shdr *psh = 0;
    Dwarf_Unsigned secoffset = 0;

    ep->f_elf_shstrings_length = 0;
    if (stringsection >= ep->f_ehdr->ge_shnum) {
        *errcode = DW_DLE_SECTION_INDEX_BAD;
        return DW_DLV_ERROR;
    }
    psh = ep->f_shdr + stringsection;
    secoffset = psh->gh_offset;
    if (is_empty_section(psh->gh_type)) {
        *errcode = DW_DLE_ELF_STRING_SECTION_MISSING;
        return DW_DLV_ERROR;
    }
    if (secoffset    >=    ep->f_filesize ||
        psh->gh_size > ep->f_filesize ||
        (secoffset + psh->gh_size) >
            ep->f_filesize) {
        *errcode = DW_DLE_SECTION_SIZE_OR_OFFSET_LARGE;
        return DW_DLV_ERROR;
    }
    if (psh->gh_size > ep->f_elf_shstrings_max) {
        free(ep->f_elf_shstrings_data);
        ep->f_elf_shstrings_data = (char *)malloc(psh->gh_size);
        ep->f_elf_shstrings_max = psh->gh_size;
        if (!ep->f_elf_shstrings_data) {
            ep->f_elf_shstrings_max = 0;
            *errcode = DW_DLE_ALLOC_FAIL;
            return DW_DLV_ERROR;
        }
    }
    ep->f_elf_shstrings_length = psh->gh_size;
    res = RRMOA(ep->f_fd,ep->f_elf_shstrings_data,secoffset,
        psh->gh_size,
        ep->f_filesize,errcode);
    return res;
}

static int
elf_load_sectheaders32(
    dwarf_elf_object_access_internals_t *ep,
    Dwarf_Unsigned offset,Dwarf_Unsigned entsize,
    Dwarf_Unsigned count,int *errcode)
{
    Dwarf_Unsigned generic_count = 0;
    int res = 0;


    if (count == 0) {
        return DW_DLV_NO_ENTRY;
    }
    if (entsize < sizeof(dw_elf32_shdr)) {
        *errcode = DW_DLE_SECTION_SIZE_ERROR;
        return DW_DLV_ERROR;
    }
    if ((offset > ep->f_filesize)||
        (entsize > 200)||
        (count > ep->f_filesize) ||
        ((count *entsize +offset) > ep->f_filesize)) {
        *errcode = DW_DLE_SECTION_SIZE_OR_OFFSET_LARGE;
        return DW_DLV_ERROR;
    }
    res = generic_shdr_from_shdr32(ep,&generic_count,
        offset,entsize,count,errcode);
    if (res != DW_DLV_OK) {
        return res;
    }
    if (generic_count != count) {
        *errcode = DW_DLE_ELF_SECTION_COUNT_MISMATCH;
        return DW_DLV_ERROR;
    }
    return DW_DLV_OK;
}

static int
elf_load_sectheaders64(
    dwarf_elf_object_access_internals_t *ep,
    Dwarf_Unsigned offset,Dwarf_Unsigned entsize,
    Dwarf_Unsigned count,int*errcode)
{
    Dwarf_Unsigned generic_count = 0;
    int res = 0;


    if (count == 0) {
        return DW_DLV_NO_ENTRY;
    }
    if (entsize < sizeof(dw_elf64_shdr)) {
        *errcode = DW_DLE_SECTION_SIZE_ERROR;
        return DW_DLV_ERROR;
    }
    if ((offset > ep->f_filesize)||
        (entsize > 200)||
        (count > ep->f_filesize) ||
        ((count *entsize +offset) > ep->f_filesize)) {
        *errcode = DW_DLE_SECTION_SIZE_OR_OFFSET_LARGE;
        return DW_DLV_ERROR;
    }
    res = generic_shdr_from_shdr64(ep,&generic_count,
        offset,entsize,count,errcode);
    if (res != DW_DLV_OK) {
        return res;
    }
    if (generic_count != count) {
        *errcode = DW_DLE_ELF_SECTION_COUNT_MISMATCH;
        return DW_DLV_ERROR;
    }
    return DW_DLV_OK;
}


static int
_dwarf_elf_load_a_relx_batch(
    dwarf_elf_object_access_internals_t *ep,
    struct generic_shdr * gsh,
    struct generic_rela ** grel_out,
    Dwarf_Unsigned *count_out,
    enum RelocRela localrela,
    enum RelocOffsetSize localoffsize,
    int *errcode)
{
    Dwarf_Unsigned count = 0;
    Dwarf_Unsigned size = 0;
    Dwarf_Unsigned size2 = 0;
    Dwarf_Unsigned sizeg = 0;
    Dwarf_Unsigned offset = 0;
    int res = 0;
    Dwarf_Unsigned object_reclen = 0;
    struct generic_rela *grel = 0;

    /*  ASSERT: Caller guarantees localoffsetsize
        is a valid 4 or 8. */
    /*  ASSERT: Caller guarantees localrela is one
        of the 2 valid values 1 or 2 */

    offset = gsh->gh_offset;
    size = gsh->gh_size;
    if (size == 0) {
        return DW_DLV_NO_ENTRY;
    }
    if ((offset > ep->f_filesize)||
        (size > ep->f_filesize) ||
        ((size +offset) > ep->f_filesize)) {
        *errcode = DW_DLE_SECTION_SIZE_OR_OFFSET_LARGE;
        return DW_DLV_ERROR;
    }
    if (localoffsize == RelocOffset32) {
        if (localrela ==  RelocIsRela) {
            object_reclen = sizeof(dw_elf32_rela);
            count = (long)(size/object_reclen);
            size2 = count * object_reclen;
            if (size != size2) {
                *errcode = DW_DLE_SECTION_SIZE_ERROR;
                return DW_DLV_ERROR;
            }
        } else {
            object_reclen = sizeof(dw_elf32_rel);
            count = (long)(size/object_reclen);
            size2 = count * object_reclen;
            if (size != size2) {
                *errcode = DW_DLE_SECTION_SIZE_ERROR;
                return DW_DLV_ERROR;
            }
        }
    } else {
        if (localrela ==  RelocIsRela) {
            object_reclen = sizeof(dw_elf64_rela);
            count = (long)(size/object_reclen);
            size2 = count * object_reclen;
            if (size != size2) {
                *errcode = DW_DLE_SECTION_SIZE_ERROR;
                return DW_DLV_ERROR;
            }
        } else {
            object_reclen = sizeof(dw_elf64_rel);
            count = (long)(size/object_reclen);
            size2 = count * object_reclen;
            if (size != size2) {
                *errcode = DW_DLE_SECTION_SIZE_ERROR;
                return DW_DLV_ERROR;
            }
        }
    }
    sizeg = count*sizeof(struct generic_rela);
    grel = (struct generic_rela *)malloc(sizeg);
    if (!grel) {
        *errcode = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    if (localoffsize == RelocOffset32) {
        if (localrela ==  RelocIsRela) {
            dw_elf32_rela *relp = 0;
            relp = (dw_elf32_rela *)malloc(size);
            if (!relp) {
                free(grel);
                *errcode = DW_DLE_ALLOC_FAIL;
                return DW_DLV_ERROR;
            }
            res = RRMOA(ep->f_fd,relp,offset,size,
                ep->f_filesize,errcode);
            if (res != DW_DLV_OK) {
                free(relp);
                free(grel);
                return res;
            }
            res = generic_rel_from_rela32(ep,gsh,relp,grel,errcode);
            free(relp);
        } else {
            dw_elf32_rel *relp = 0;
            relp = (dw_elf32_rel *)malloc(size);
            if (!relp) {
                free(grel);
                *errcode = DW_DLE_ALLOC_FAIL;
                return DW_DLV_ERROR;
            }
            res = RRMOA(ep->f_fd,relp,offset,size,
                ep->f_filesize,errcode);
            if (res != DW_DLV_OK) {
                free(relp);
                free(grel);
                return res;
            }
            res = generic_rel_from_rel32(ep,gsh,relp,grel,errcode);
            free(relp);
        }
    } else {
        if (localrela ==  RelocIsRela) {
            dw_elf64_rela *relp = 0;
            relp = (dw_elf64_rela *)malloc(size);
            if (!relp) {
                free(grel);
                *errcode = DW_DLE_ALLOC_FAIL;
                return DW_DLV_ERROR;
            }
            res = RRMOA(ep->f_fd,relp,offset,size,
                ep->f_filesize,errcode);
            if (res != DW_DLV_OK) {
                free(relp);
                free(grel);
                return res;
            }
            res = generic_rel_from_rela64(ep,gsh,relp,grel,errcode);
            free(relp);
        } else {
            dw_elf64_rel *relp = 0;
            relp = (dw_elf64_rel *)malloc(size);
            if (!relp) {
                free(grel);
                *errcode = DW_DLE_ALLOC_FAIL;
                return DW_DLV_ERROR;
            }
            res = RRMOA(ep->f_fd,relp,offset,size,
                ep->f_filesize,errcode);
            if (res != DW_DLV_OK) {
                free(relp);
                free(grel);
                return res;
            }
            res = generic_rel_from_rel64(ep,gsh,relp,grel,errcode);
            free(relp);
        }
    }
    if (res == DW_DLV_OK) {
        gsh->gh_relcount = count;
        gsh->gh_rels = grel;
        *count_out = count;
        *grel_out = grel;
        return res;
    }
    /* Some sort of issue */
    count_out = 0;
    free(grel);
    return res;
}


/*  Is this rel/rela section related to dwarf at all?
    set oksecnum zero if not. Else set targ secnum.
    Never returns DW_DLV_NO_ENTRY. */
static int
this_rel_is_a_section_dwarf_related(
    dwarf_elf_object_access_internals_t *ep,
    struct generic_shdr *gshdr,
    unsigned *oksecnum_out,
    int *errcode)
{
    unsigned oksecnum = 0;
    struct generic_shdr *gstarg = 0;

    if (gshdr->gh_type != SHT_RELA &&
        gshdr->gh_type != SHT_REL)  {
        *oksecnum_out = 0;
        return DW_DLV_OK;
    }
    oksecnum = gshdr->gh_reloc_target_secnum;
    if (oksecnum >= ep->f_loc_shdr.g_count) {
        *oksecnum_out = 0;
        *errcode = DW_DLE_ELF_SECTION_ERROR;
        return DW_DLV_ERROR;
    }
    gstarg = ep->f_shdr+oksecnum;
    if (!gstarg->gh_is_dwarf) {
        *oksecnum_out = 0; /* no reloc needed. */
        return DW_DLV_OK;
    }
    *oksecnum_out = oksecnum;
    return DW_DLV_OK;
}
/*  Secnum here is the secnum of rela. Not
    the target of the relocations.
    This also loads .rel. */
int
_dwarf_load_elf_relx(
    dwarf_elf_object_access_internals_t *ep,
    Dwarf_Unsigned secnum,
    enum RelocRela localr,
    int *errcode)
{
    struct generic_shdr *gshdr = 0;
    Dwarf_Unsigned seccount = 0;
    unsigned offsetsize = 0;
    struct generic_rela *grp = 0;
    Dwarf_Unsigned count_read = 0;
    int res = 0;
    unsigned oksec = 0;
    enum RelocOffsetSize localoffsize = RelocOffset32;

    /*  ASSERT: Caller guarantees localr is
        a valid RelocRela  */
    if (!ep) {
        *errcode = DW_DLE_INTERNAL_NULL_POINTER;
        return DW_DLV_ERROR;
    }
    offsetsize = ep->f_offsetsize;
    seccount = ep->f_loc_shdr.g_count;
    if (secnum >= seccount) {
        *errcode = DW_DLE_ELF_SECTION_ERROR;
        return DW_DLV_ERROR;
    }
    gshdr = ep->f_shdr +secnum;
    if (is_empty_section(gshdr->gh_type)) {

        return DW_DLV_NO_ENTRY;
    }

    res = this_rel_is_a_section_dwarf_related(ep,gshdr,
        &oksec,errcode);
    if (res == DW_DLV_ERROR) {
        return res;
    }
    if (!oksec) {
        return DW_DLV_OK;
    }
    /*  We will actually read these relocations. */
    if (offsetsize == 64) {
        localoffsize = RelocOffset64;
    } else if (offsetsize == 32) {
        localoffsize = RelocOffset32;
    } else {
        *errcode = DW_DLE_OFFSET_SIZE;
        return DW_DLV_ERROR;
    }
    /*  ASSERT: localoffsize is now a valid enum value,
        one of the two defined. */
    res = _dwarf_elf_load_a_relx_batch(ep,
        gshdr,&grp,&count_read,localr,localoffsize,errcode);
    if (res == DW_DLV_ERROR) {
        return res;
    }
    if (res == DW_DLV_NO_ENTRY) {
        return res;
    }
    gshdr->gh_rels = grp;
    gshdr->gh_relcount = count_read;
    return DW_DLV_OK;
}
static int
validate_section_name_string(Dwarf_Unsigned section_length,
    Dwarf_Unsigned string_loc_index,
    const char * strings_start,
    int  * errcode)
{
    const char *endpoint = strings_start + section_length;
    const char *cur = 0;

    if (section_length <= string_loc_index) {
        *errcode = DW_DLE_SECTION_STRING_OFFSET_BAD;
        return DW_DLV_ERROR;
    }
    cur = string_loc_index+strings_start;
    for ( ; cur < endpoint;++cur) {
        if (!*cur) {
            return DW_DLV_OK;
        }
    }
    *errcode = DW_DLE_SECTION_STRING_OFFSET_BAD;
    return DW_DLV_ERROR;
}

static int
_dwarf_elf_load_sect_namestring(
    dwarf_elf_object_access_internals_t *ep,
    int *errcode)
{
    struct generic_shdr *gshdr = 0;
    Dwarf_Unsigned generic_count = 0;
    Dwarf_Unsigned i = 1;
    const char *stringsecbase = 0;

    stringsecbase = ep->f_elf_shstrings_data;
    gshdr = ep->f_shdr;
    generic_count = ep->f_loc_shdr.g_count;
    for (i = 0; i < generic_count; i++, ++gshdr) {
        const char *namestr =
            "<Invalid sh_name value. Corrupt Elf.>";
        int res = 0;

        res = validate_section_name_string(ep->f_elf_shstrings_length,
            gshdr->gh_name, stringsecbase,
            errcode);
        if (res != DW_DLV_OK) {
            gshdr->gh_namestring = namestr;
            return res;
        }
        gshdr->gh_namestring = stringsecbase + gshdr->gh_name;
    }
    return DW_DLV_OK;
}


static int
elf_load_elf_header32(
    dwarf_elf_object_access_internals_t *ep,int *errcode)
{
    int res = 0;
    dw_elf32_ehdr ehdr32;
    struct generic_ehdr *ehdr = 0;

    res = RRMOA(ep->f_fd,&ehdr32,0,sizeof(ehdr32),
        ep->f_filesize,errcode);
    if (res != DW_DLV_OK) {
        return res;
    }
    ehdr = (struct generic_ehdr *)calloc(1,
        sizeof(struct generic_ehdr));
    if (!ehdr) {
        *errcode = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    res  = generic_ehdr_from_32(ep,ehdr,&ehdr32,errcode);
    return res;
}
static int
elf_load_elf_header64(
    dwarf_elf_object_access_internals_t *ep,int *errcode)
{
    int res = 0;
    dw_elf64_ehdr ehdr64;
    struct generic_ehdr *ehdr = 0;

    res = RRMOA(ep->f_fd,&ehdr64,0,sizeof(ehdr64),
        ep->f_filesize,errcode);
    if (res != DW_DLV_OK) {
        return res;
    }
    ehdr = (struct generic_ehdr *)calloc(1,
        sizeof(struct generic_ehdr));
    if (!ehdr) {
        *errcode = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    res  = generic_ehdr_from_64(ep,ehdr,&ehdr64,errcode);
    return res;
}

static int
validate_struct_sizes(
#ifdef HAVE_ELF_H
    int*errcode
#else
    int*errcode UNUSEDARG
#endif
    )
{
#ifdef HAVE_ELF_H
    /*  This is a sanity check when we have an elf.h
        to check against. */
    if (sizeof(Elf32_Ehdr) != sizeof(dw_elf32_ehdr)) {
        *errcode = DW_DLE_BAD_TYPE_SIZE;
        return DW_DLV_ERROR;
    }
    if (sizeof(Elf64_Ehdr) != sizeof(dw_elf64_ehdr)) {
        *errcode = DW_DLE_BAD_TYPE_SIZE;
        return DW_DLV_ERROR;
    }
    if (sizeof(Elf32_Shdr) != sizeof(dw_elf32_shdr)) {
        *errcode = DW_DLE_BAD_TYPE_SIZE;
        return DW_DLV_ERROR;
    }
    if (sizeof(Elf64_Shdr) != sizeof(dw_elf64_shdr)) {
        *errcode = DW_DLE_BAD_TYPE_SIZE;
        return DW_DLV_ERROR;
    }
    if (sizeof(Elf32_Phdr) != sizeof(dw_elf32_phdr)) {
        *errcode = DW_DLE_BAD_TYPE_SIZE;
        return DW_DLV_ERROR;
    }
    if (sizeof(Elf64_Phdr) != sizeof(dw_elf64_phdr)) {
        *errcode = DW_DLE_BAD_TYPE_SIZE;
        return DW_DLV_ERROR;
    }
    if (sizeof(Elf32_Rel) != sizeof(dw_elf32_rel)) {
        *errcode = DW_DLE_BAD_TYPE_SIZE;
        return DW_DLV_ERROR;
    }
    if (sizeof(Elf64_Rel) != sizeof(dw_elf64_rel)) {
        *errcode = DW_DLE_BAD_TYPE_SIZE;
        return DW_DLV_ERROR;
    }
    if (sizeof(Elf32_Rela) != sizeof(dw_elf32_rela)) {
        *errcode = DW_DLE_BAD_TYPE_SIZE;
        return DW_DLV_ERROR;
    }
    if (sizeof(Elf64_Rela) != sizeof(dw_elf64_rela)) {
        *errcode = DW_DLE_BAD_TYPE_SIZE;
        return DW_DLV_ERROR;
    }
    if (sizeof(Elf32_Sym) != sizeof(dw_elf32_sym)) {
        *errcode = DW_DLE_BAD_TYPE_SIZE;
        return DW_DLV_ERROR;
    }
    if (sizeof(Elf64_Sym) != sizeof(dw_elf64_sym)) {
        *errcode = DW_DLE_BAD_TYPE_SIZE;
        return DW_DLV_ERROR;
    }
#endif /* HAVE_ELF_H */
    return DW_DLV_OK;
}

int
_dwarf_load_elf_header(
    dwarf_elf_object_access_internals_t *ep,int*errcode)
{
    unsigned offsetsize = ep->f_offsetsize;
    int res = 0;

    res = validate_struct_sizes(errcode);
    if (res != DW_DLV_OK) {
        return res;
    }

    if (offsetsize == 32) {
        res = elf_load_elf_header32(ep,errcode);
    } else if (offsetsize == 64) {
        if (sizeof(Dwarf_Unsigned) < 8) {
            *errcode =  DW_DLE_INTEGER_TOO_SMALL;
            return DW_DLV_ERROR;
        }
        res = elf_load_elf_header64(ep,errcode);
    } else {
        *errcode = DW_DLE_OFFSET_SIZE;
        return DW_DLV_ERROR;
    }
    return res;
}

static int
validate_links(
    dwarf_elf_object_access_internals_t *ep,
    Dwarf_Unsigned knownsect,
    Dwarf_Unsigned string_sect,
    int *errcode)
{
    struct generic_shdr* pshk = 0;

    if (!knownsect) {
        return DW_DLV_OK;
    }
    if (!string_sect) {
        *errcode = DW_DLE_ELF_STRING_SECTION_ERROR;
        return DW_DLV_ERROR;
    }
    pshk = ep->f_shdr + knownsect;
    if (string_sect != pshk->gh_link) {
        *errcode = DW_DLE_ELF_SECTION_LINK_ERROR;
        return DW_DLV_ERROR;
    }
    return DW_DLV_OK;
}


static int
string_endswith(const char *n,const char *q)
{
    unsigned long len = strlen(n);
    unsigned long qlen = strlen(q);
    const char *startpt = 0;

    if ( len < qlen) {
        return FALSE;
    }
    startpt = n + (len-qlen);
    if (strcmp(startpt,q)) {
        return FALSE;
    }
    return TRUE;
}

/*  We are allowing either SHT_GROUP or .group to indicate
    a group section, but really one should have both
    or neither! */
static int
elf_sht_groupsec(Dwarf_Unsigned type, const char *sname)
{
    /*  ARM compilers name SHT group "__ARM_grp<long name here>"
        not .group */
    if ((type == SHT_GROUP) || (!strcmp(sname,".group"))){
        return TRUE;
    }
    return FALSE;
}

static int
elf_flagmatches(Dwarf_Unsigned flagsword,Dwarf_Unsigned flag)
{
    if ((flagsword&flag) == flag) {
        return TRUE;
    }
    return FALSE;
}

/*  For SHT_GROUP sections. */
static int
read_gs_section_group(
    dwarf_elf_object_access_internals_t *ep,
    struct generic_shdr* psh,
    int *errcode)
{
    Dwarf_Unsigned i = 0;
    int res = 0;

    if (!psh->gh_sht_group_array) {
        Dwarf_Unsigned seclen = psh->gh_size;
        char *data = 0;
        char *dp = 0;
        Dwarf_Unsigned* grouparray = 0;
        char dblock[4];
        Dwarf_Unsigned va = 0;
        Dwarf_Unsigned count = 0;
        int foundone = 0;

        if (seclen < DWARF_32BIT_SIZE) {
            *errcode = DW_DLE_ELF_SECTION_GROUP_ERROR;
            return DW_DLV_ERROR;
        }
        data = malloc(seclen);
        if (!data) {
            *errcode = DW_DLE_ALLOC_FAIL;
            return DW_DLV_ERROR;
        }
        dp = data;
        if (psh->gh_entsize != DWARF_32BIT_SIZE) {
            *errcode = DW_DLE_ELF_SECTION_GROUP_ERROR;
            free(data);
            return DW_DLV_ERROR;
        }
        if (!psh->gh_entsize) {
            free(data);
            *errcode = DW_DLE_ELF_SECTION_GROUP_ERROR;
            return DW_DLV_ERROR;
        }
        count = seclen/psh->gh_entsize;
        if (count > ep->f_loc_shdr.g_count) {
            /* Impossible */
            free(data);
            *errcode = DW_DLE_ELF_SECTION_GROUP_ERROR;
            return DW_DLV_ERROR;
        }
        res = RRMOA(ep->f_fd,data,psh->gh_offset,seclen,
            ep->f_filesize,errcode);
        if (res != DW_DLV_OK) {
            free(data);
            return res;
        }
        grouparray = malloc(count * sizeof(Dwarf_Unsigned));
        if (!grouparray) {
            free(data);
            *errcode = DW_DLE_ALLOC_FAIL;
            return DW_DLV_ERROR;
        }

        memcpy(dblock,dp,DWARF_32BIT_SIZE);
        ASNAR(memcpy,va,dblock);
        /* There is ambiguity on the endianness of this stuff. */
        if (va != 1 && va != 0x1000000) {
            /*  Could be corrupted elf object. */
            *errcode = DW_DLE_ELF_SECTION_GROUP_ERROR;
            free(data);
            free(grouparray);
            return DW_DLV_ERROR;
        }
        grouparray[0] = 1;
        dp = dp + DWARF_32BIT_SIZE;
        for ( i = 1; i < count; ++i,dp += DWARF_32BIT_SIZE) {
            Dwarf_Unsigned gseca = 0;
            Dwarf_Unsigned gsecb = 0;
            struct generic_shdr* targpsh = 0;

            memcpy(dblock,dp,DWARF_32BIT_SIZE);
            ASNAR(memcpy,gseca,dblock);
            ASNAR(_dwarf_memcpy_swap_bytes,gsecb,dblock);
            if (!gseca) {
                free(data);
                free(grouparray);
                *errcode = DW_DLE_ELF_SECTION_GROUP_ERROR;
                return DW_DLV_ERROR;
            }
            grouparray[i] = gseca;
            if (gseca > ep->f_loc_shdr.g_count) {
                /*  Might be confused endianness by
                    the compiler generating the SHT_GROUP.
                    This is pretty horrible. */

                if (gsecb > ep->f_loc_shdr.g_count) {
                    *errcode = DW_DLE_ELF_SECTION_GROUP_ERROR;
                    free(data);
                    free(grouparray);
                    return DW_DLV_ERROR;
                }
                /* Ok. Yes, ugly. */
                gseca = gsecb;
                grouparray[i] = gseca;
            }
            targpsh = ep->f_shdr + gseca;
            if (targpsh->gh_section_group_number) {
                /* multi-assignment to groups. Oops. */
                free(data);
                free(grouparray);
                *errcode = DW_DLE_ELF_SECTION_GROUP_ERROR;
                return DW_DLV_ERROR;
            }
            targpsh->gh_section_group_number =
                ep->f_sg_next_group_number;
            foundone = 1;
        }
        if (foundone) {
            ++ep->f_sg_next_group_number;
            ++ep->f_sht_group_type_section_count;
        }
        free(data);
        psh->gh_sht_group_array = grouparray;
        psh->gh_sht_group_array_count = count;
    }
    return DW_DLV_OK;
}
/*  Does related things.
    A)  Counts the number of SHT_GROUP
        and for each builds an array of the sections in the group
        (which we expect are all DWARF-related)
        and sets the group number in each mentioned section.
    B)  Counts the number of SHF_GROUP flags.
    C)  If gnu groups:
        ensure all the DWARF sections marked with right group
        based on A(we will mark unmarked as group 1,
        DW_GROUPNUMBER_BASE).
    D)  If arm groups (SHT_GROUP zero, SHF_GROUP non-zero):
        Check the relocations of all SHF_GROUP section
        FIXME: algorithm needed.


    If SHT_GROUP and SHF_GROUP this is GNU groups.
    If no SHT_GROUP and have SHF_GROUP this is
    arm cc groups and we must use relocation information
    to identify the group members.

    It seems(?) impossible for an object to have both
    dwo sections and (SHF_GROUP or SHT_GROUP), but
    we do not rule that out here.  */
static int
_dwarf_elf_setup_all_section_groups(
    dwarf_elf_object_access_internals_t *ep,
    int *errcode)
{
    struct generic_shdr* psh = 0;
    Dwarf_Unsigned i = 0;
    Dwarf_Unsigned count = 0;
    int res = 0;

    count = ep->f_loc_shdr.g_count;
    psh = ep->f_shdr;

    /* Does step A and step B */
    for (i = 0; i < count; ++psh,++i) {
        const char *name = psh->gh_namestring;
        if (is_empty_section(psh->gh_type)) {
            /*  No data here. */
            continue;
        }
        if (!elf_sht_groupsec(psh->gh_type,name)) {
            /* Step B */
            if (elf_flagmatches(psh->gh_flags,SHF_GROUP)) {
                ep->f_shf_group_flag_section_count++;
            }
            continue;
        }
        /* Looks like a section group. Do Step A. */
        res  =read_gs_section_group(ep,psh,errcode);
        if (res != DW_DLV_OK) {
            return res;
        }
    }
    /*  Any sections not marked above or here are in
        grep DW_GROUPNUMBER_BASE (1).
        Section C. */
    psh = ep->f_shdr;
    for (i = 0; i < count; ++psh,++i) {
        const char *name = psh->gh_namestring;
        int is_rel = FALSE;
        int is_rela = FALSE;

        if (is_empty_section(psh->gh_type)) {
            /*  No data here. */
            continue;
        }
        if (elf_sht_groupsec(psh->gh_type,name)) {
            continue;
        }
        /* Not a section group */
        if (string_endswith(name,".dwo")) {
            if (psh->gh_section_group_number) {
                /* multi-assignment to groups. Oops. */
                *errcode = DW_DLE_ELF_SECTION_GROUP_ERROR;
                return DW_DLV_ERROR;
            }
            psh->gh_is_dwarf = TRUE;
            psh->gh_section_group_number = DW_GROUPNUMBER_DWO;
            ep->f_dwo_group_section_count++;
        } else if (_dwarf_load_elf_section_is_dwarf(name,
            &is_rela,&is_rel)) {
            if (!psh->gh_section_group_number) {
                psh->gh_section_group_number = DW_GROUPNUMBER_BASE;
            }
            psh->gh_is_dwarf = TRUE;
        } else {
            /* Do nothing. */
        }
    }
    if (ep->f_sht_group_type_section_count) {
        /*  Not ARM. Done. */
    }
    if (!ep->f_shf_group_flag_section_count) {
        /*  Nothing more to do. */
        return DW_DLV_OK;
    }
    return DW_DLV_OK;
}

static int
_dwarf_elf_find_sym_sections(
    dwarf_elf_object_access_internals_t *ep,
    int *errcode)
{
    struct generic_shdr* psh = 0;
    Dwarf_Unsigned i = 0;
    Dwarf_Unsigned count = 0;
    int res = 0;

    count = ep->f_loc_shdr.g_count;
    psh = ep->f_shdr;
    for (i = 0; i < count; ++psh,++i) {
        const char *name = psh->gh_namestring;
        if (is_empty_section(psh->gh_type)) {
            /*  No data here. */
            continue;
        }
        if (!strcmp(name,".dynsym")) {
            ep->f_dynsym_sect_index = i;
            ep->f_loc_dynsym.g_offset = psh->gh_offset;
        } else if (!strcmp(name,".dynstr")) {
            ep->f_dynsym_sect_strings_sect_index = i;
            ep->f_dynsym_sect_strings_max = psh->gh_size;
        } else if (!strcmp(name,".symtab")) {
            ep->f_symtab_sect_index = i;
            ep->f_loc_symtab.g_offset = psh->gh_offset;
        } else if (!strcmp(name,".strtab")) {
            ep->f_symtab_sect_strings_sect_index = i;
            ep->f_symtab_sect_strings_max = psh->gh_size;
        } else if (!strcmp(name,".dynamic")) {
            ep->f_dynamic_sect_index = i;
            ep->f_loc_dynamic.g_offset = psh->gh_offset;
        }
    }

#if 0
    res = validate_links(ep,ep->f_dynsym_sect_index,
        ep->f_dynsym_sect_strings_sect_index,errcode);
    if (res!= DW_DLV_OK) {
        return res;
    }
#endif /* 0 */
    res = validate_links(ep,ep->f_symtab_sect_index,
        ep->f_symtab_sect_strings_sect_index,errcode);
    if (res!= DW_DLV_OK) {
        return res;
    }
    return DW_DLV_OK;
}


int
_dwarf_load_elf_sectheaders(
    dwarf_elf_object_access_internals_t *ep,int*errcode)
{
    int res = 0;

    if (ep->f_offsetsize == 32) {
        res  = elf_load_sectheaders32(ep,ep->f_ehdr->ge_shoff,
            ep->f_ehdr->ge_shentsize,
            ep->f_ehdr->ge_shnum,errcode);
    } else  if (ep->f_offsetsize == 64) {
        res  = elf_load_sectheaders64(ep,ep->f_ehdr->ge_shoff,
            ep->f_ehdr->ge_shentsize,
            ep->f_ehdr->ge_shnum,errcode);
    } else {
        *errcode = DW_DLE_OFFSET_SIZE;
        return DW_DLV_ERROR;
    }
    if (res != DW_DLV_OK) {
        return res;
    }
    res  = _dwarf_elf_load_sectstrings(ep,
        ep->f_ehdr->ge_shstrndx,errcode);
    if (res != DW_DLV_OK) {
        return res;
    }
    res  = _dwarf_elf_load_sect_namestring(ep,errcode);
    if (res != DW_DLV_OK) {
        return res;
    }
    res  = _dwarf_elf_find_sym_sections(ep,errcode);
    if (res != DW_DLV_OK) {
        return res;
    }
    res = _dwarf_elf_setup_all_section_groups(ep,errcode);
    return res;
}
