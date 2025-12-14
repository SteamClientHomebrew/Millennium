/* -*- indent-tabs-mode: nil -*-
 *
 * plthook_elf.c -- implementation of plthook for ELF format
 *
 * URL: https://github.com/kubo/plthook
 *
 * ------------------------------------------------------
 *
 * Copyright 2013-2019 Kubo Takehiro <kubo@jiubao.org>
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of the authors.
 *
 */
#if defined(__sun) && defined(_XOPEN_SOURCE) && !defined(__EXTENSIONS__)
#define __EXTENSIONS__
#endif
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/mman.h>
#include <errno.h>
#include <dlfcn.h>
#ifdef __sun
#include <sys/auxv.h>
#include <procfs.h>
#define ELF_TARGET_ALL
#endif /* __sun */
#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/user.h>
#include <libutil.h>
#endif
#include <elf.h>
#include <link.h>
#include "plthook.h"

#if defined __UCLIBC__ && !defined RTLD_NOLOAD
#define RTLD_NOLOAD 0
#endif

#ifndef __GNUC__
#define __attribute__(arg)
#endif

#if defined __FreeBSD__ && defined __i386__ && __ELF_WORD_SIZE == 64
#error 32-bit application on 64-bit OS is not supported.
#endif

#if !defined(R_X86_64_JUMP_SLOT) && defined(R_X86_64_JMP_SLOT)
#define R_X86_64_JUMP_SLOT R_X86_64_JMP_SLOT
#endif

#if defined __x86_64__ || defined __x86_64
#define R_JUMP_SLOT R_X86_64_JUMP_SLOT
#define R_GLOBAL_DATA R_X86_64_GLOB_DAT
#elif defined __i386__ || defined __i386
#define R_JUMP_SLOT R_386_JMP_SLOT
#define R_GLOBAL_DATA R_386_GLOB_DAT
#define USE_REL
#elif defined __arm__ || defined __arm
#define R_JUMP_SLOT R_ARM_JUMP_SLOT
#define R_GLOBAL_DATA R_ARM_GLOB_DAT
#define USE_REL
#elif defined __aarch64__ || defined __aarch64 /* ARM64 */
#define R_JUMP_SLOT R_AARCH64_JUMP_SLOT
#define R_GLOBAL_DATA R_AARCH64_GLOB_DAT
#elif defined __powerpc64__
#define R_JUMP_SLOT R_PPC64_JMP_SLOT
#define R_GLOBAL_DATA R_PPC64_GLOB_DAT
#elif defined __powerpc__
#define R_JUMP_SLOT R_PPC_JMP_SLOT
#define R_GLOBAL_DATA R_PPC_GLOB_DAT
#elif defined __riscv
#define R_JUMP_SLOT R_RISCV_JUMP_SLOT
#if __riscv_xlen == 32
#define R_GLOBAL_DATA R_RISCV_32
#elif __riscv_xlen == 64
#define R_GLOBAL_DATA R_RISCV_64
#else
#error unsupported RISCV implementation
#endif
#elif 0 /* disabled because not tested */ && (defined __sparcv9 || defined __sparc_v9__)
#define R_JUMP_SLOT R_SPARC_JMP_SLOT
#elif 0 /* disabled because not tested */ && (defined __sparc || defined __sparc__)
#define R_JUMP_SLOT R_SPARC_JMP_SLOT
#elif 0 /* disabled because not tested */ && (defined __ia64 || defined __ia64__)
#define R_JUMP_SLOT R_IA64_IPLTMSB
#else
#error unsupported OS
#endif

#ifdef USE_REL
#define Elf_Plt_Rel Elf_Rel
#define PLT_DT_REL DT_REL
#define PLT_DT_RELSZ DT_RELSZ
#define PLT_DT_RELENT DT_RELENT
#else
#define Elf_Plt_Rel Elf_Rela
#define PLT_DT_REL DT_RELA
#define PLT_DT_RELSZ DT_RELASZ
#define PLT_DT_RELENT DT_RELAENT
#endif

#if defined __LP64__
#ifndef ELF_CLASS
#define ELF_CLASS ELFCLASS64
#endif
#define SIZE_T_FMT "lu"
#define ELF_WORD_FMT "u"
#ifdef __ANDROID__
#define ELF_XWORD_FMT "llu"
#else
#define ELF_XWORD_FMT "lu"
#endif
#define ELF_SXWORD_FMT "ld"
#define Elf_Half Elf64_Half
#define Elf_Xword Elf64_Xword
#define Elf_Sxword Elf64_Sxword
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Sym Elf64_Sym
#define Elf_Dyn Elf64_Dyn
#define Elf_Rel Elf64_Rel
#define Elf_Rela Elf64_Rela
#ifndef ELF_R_SYM
#define ELF_R_SYM ELF64_R_SYM
#endif
#ifndef ELF_R_TYPE
#define ELF_R_TYPE ELF64_R_TYPE
#endif
#else /* __LP64__ */
#ifndef ELF_CLASS
#define ELF_CLASS ELFCLASS32
#endif
#define SIZE_T_FMT "u"
#ifdef __sun
#define ELF_WORD_FMT "lu"
#define ELF_XWORD_FMT "lu"
#define ELF_SXWORD_FMT "ld"
#else
#define ELF_WORD_FMT "u"
#define ELF_XWORD_FMT "u"
#define ELF_SXWORD_FMT "d"
#endif
#define Elf_Half Elf32_Half
#define Elf_Xword Elf32_Word
#define Elf_Sxword Elf32_Sword
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Phdr Elf32_Phdr
#define Elf_Sym Elf32_Sym
#define Elf_Dyn Elf32_Dyn
#define Elf_Rel Elf32_Rel
#define Elf_Rela Elf32_Rela
#ifndef ELF_R_SYM
#define ELF_R_SYM ELF32_R_SYM
#endif
#ifndef ELF_R_TYPE
#define ELF_R_TYPE ELF32_R_TYPE
#endif
#endif /* __LP64__ */

typedef struct mem_prot
{
    size_t start;
    size_t end;
    int prot;
} mem_prot_t;

#define NUM_MEM_PROT 20

struct plthook
{
    const Elf_Sym* dynsym;
    const char* dynstr;
    size_t dynstr_size;
    const char* plt_addr_base;
    const Elf_Plt_Rel* rela_plt;
    size_t rela_plt_cnt;
#ifdef R_GLOBAL_DATA
    const Elf_Plt_Rel* rela_dyn;
    size_t rela_dyn_cnt;
#endif
    mem_prot_t mem_prot[NUM_MEM_PROT];
};

static char errmsg[512];
static size_t page_size;
#define ALIGN_ADDR(addr) ((void*)((size_t)(addr) & ~(page_size - 1)))

static int plthook_open_executable(plthook_t** plthook_out);
static int plthook_open_shared_library(plthook_t** plthook_out, const char* filename);
static const Elf_Dyn* find_dyn_by_tag(const Elf_Dyn* dyn, Elf_Sxword tag);

typedef struct mem_prot_iter mem_prot_iter_t;
static int mem_prot_begin(mem_prot_iter_t* iter);
static int mem_prot_next(mem_prot_iter_t* iter, mem_prot_t* mem_prot);
static void mem_prot_end(mem_prot_iter_t* iter);

static int plthook_open_real(plthook_t** plthook_out, struct link_map* lmap);
static int plthook_set_mem_prot(plthook_t* plthook);
static int plthook_get_mem_prot(plthook_t* plthook, void* addr);
#if defined __FreeBSD__ || defined __sun
static int check_elf_header(const Elf_Ehdr* ehdr);
#endif
static void set_errmsg(const char* fmt, ...) __attribute__((__format__(__printf__, 1, 2)));

#if defined __ANDROID__ || defined __UCLIBC__
struct dl_iterate_data
{
    char* addr;
    struct link_map lmap;
};

static int dl_iterate_cb(struct dl_phdr_info* info, size_t size, void* cb_data)
{
    struct dl_iterate_data* data = (struct dl_iterate_data*)cb_data;
    Elf_Half idx = 0;

    for (idx = 0; idx < info->dlpi_phnum; ++idx) {
        const Elf_Phdr* phdr = &info->dlpi_phdr[idx];
        char* base = (char*)info->dlpi_addr + phdr->p_vaddr;
        if (base <= data->addr && data->addr < base + phdr->p_memsz) {
            break;
        }
    }
    if (idx == info->dlpi_phnum) {
        return 0;
    }
    for (idx = 0; idx < info->dlpi_phnum; ++idx) {
        const Elf_Phdr* phdr = &info->dlpi_phdr[idx];
        if (phdr->p_type == PT_DYNAMIC) {
            data->lmap.l_addr = info->dlpi_addr;
            data->lmap.l_ld = (Elf_Dyn*)(info->dlpi_addr + phdr->p_vaddr);
            return 1;
        }
    }
    return 0;
}
#endif

int plthook_open(plthook_t** plthook_out, const char* filename)
{
    *plthook_out = NULL;
    if (filename == NULL) {
        return plthook_open_executable(plthook_out);
    } else {
        return plthook_open_shared_library(plthook_out, filename);
    }
}

int plthook_open_by_handle(plthook_t** plthook_out, void* hndl)
{
#if defined __ANDROID__ || defined __UCLIBC__
    const static char* symbols[] = { "__INIT_ARRAY__", "_end", "_start" };
    size_t i;

    if (hndl == NULL) {
        set_errmsg("NULL handle");
        return PLTHOOK_FILE_NOT_FOUND;
    }
    for (i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
        char* addr = dlsym(hndl, symbols[i]);
        if (addr != NULL) {
            return plthook_open_by_address(plthook_out, addr - 1);
        }
    }
    set_errmsg("Could not find an address in the specified handle.");
    return PLTHOOK_INTERNAL_ERROR;
#else
    struct link_map* lmap = NULL;

    if (hndl == NULL) {
        set_errmsg("NULL handle");
        return PLTHOOK_FILE_NOT_FOUND;
    }
    if (dlinfo(hndl, RTLD_DI_LINKMAP, &lmap) != 0) {
        set_errmsg("dlinfo error");
        return PLTHOOK_FILE_NOT_FOUND;
    }
    return plthook_open_real(plthook_out, lmap);
#endif
}

int plthook_open_by_address(plthook_t** plthook_out, void* address)
{
#if defined __FreeBSD__
    return PLTHOOK_NOT_IMPLEMENTED;
#elif defined __ANDROID__ || defined __UCLIBC__
    struct dl_iterate_data data = {
        0,
    };
    data.addr = address;
    dl_iterate_phdr(dl_iterate_cb, &data);
    if (data.lmap.l_ld == NULL) {
        set_errmsg("Could not find memory region containing address %p", address);
        return PLTHOOK_INTERNAL_ERROR;
    }
    return plthook_open_real(plthook_out, &data.lmap);
#else
    Dl_info info;
    struct link_map* lmap = NULL;

    *plthook_out = NULL;
    if (dladdr1(address, &info, (void**)&lmap, RTLD_DL_LINKMAP) == 0) {
        set_errmsg("dladdr error");
        return PLTHOOK_FILE_NOT_FOUND;
    }
    return plthook_open_real(plthook_out, lmap);
#endif
}

static int plthook_open_executable(plthook_t** plthook_out)
{
#if defined __ANDROID__ || defined __UCLIBC__
    return plthook_open_shared_library(plthook_out, NULL);
#elif defined __linux__
    return plthook_open_real(plthook_out, _r_debug.r_map);
#elif defined __sun
    const char* auxv_file = "/proc/self/auxv";
#define NUM_AUXV_CNT 10
    FILE* fp = fopen(auxv_file, "r");
    auxv_t auxv;
    struct r_debug* r_debug = NULL;

    if (fp == NULL) {
        set_errmsg("Could not open %s: %s", auxv_file, strerror(errno));
        return PLTHOOK_INTERNAL_ERROR;
    }
    while (fread(&auxv, sizeof(auxv_t), 1, fp) == 1) {
        if (auxv.a_type == AT_SUN_LDDATA) {
            r_debug = (struct r_debug*)auxv.a_un.a_ptr;
            break;
        }
    }
    fclose(fp);
    if (r_debug == NULL) {
        set_errmsg("Could not find r_debug");
        return PLTHOOK_INTERNAL_ERROR;
    }
    return plthook_open_real(plthook_out, r_debug->r_map);
#elif defined __FreeBSD__
    return plthook_open_shared_library(plthook_out, NULL);
#else
    set_errmsg("Opening the main program is not supported on this platform.");
    return PLTHOOK_NOT_IMPLEMENTED;
#endif
}

static int plthook_open_shared_library(plthook_t** plthook_out, const char* filename)
{
    void* hndl = dlopen(filename, RTLD_LAZY | RTLD_NOLOAD);
#if defined __ANDROID__ || defined __UCLIBC__
    int rv;
#else
    struct link_map* lmap = NULL;
#endif

    if (hndl == NULL) {
        set_errmsg("dlopen error: %s", dlerror());
        return PLTHOOK_FILE_NOT_FOUND;
    }
#if defined __ANDROID__ || defined __UCLIBC__
    rv = plthook_open_by_handle(plthook_out, hndl);
    dlclose(hndl);
    return rv;
#else
    if (dlinfo(hndl, RTLD_DI_LINKMAP, &lmap) != 0) {
        set_errmsg("dlinfo error");
        dlclose(hndl);
        return PLTHOOK_FILE_NOT_FOUND;
    }
    dlclose(hndl);
    return plthook_open_real(plthook_out, lmap);
#endif
}

static const Elf_Dyn* find_dyn_by_tag(const Elf_Dyn* dyn, Elf_Sxword tag)
{
    while (dyn->d_tag != DT_NULL) {
        if (dyn->d_tag == tag) {
            return dyn;
        }
        dyn++;
    }
    return NULL;
}

#ifdef __linux__
struct mem_prot_iter
{
    FILE* fp;
};

static int mem_prot_begin(mem_prot_iter_t* iter)
{
    iter->fp = fopen("/proc/self/maps", "r");
    if (iter->fp == NULL) {
        set_errmsg("failed to open /proc/self/maps");
        return -1;
    }
    return 0;
}

static int mem_prot_next(mem_prot_iter_t* iter, mem_prot_t* mem_prot)
{
    char buf[PATH_MAX];
    char perms[5];
    int bol = 1; /* beginnng of line */

    while (fgets(buf, PATH_MAX, iter->fp) != NULL) {
        unsigned long start, end;
        int eol = (strchr(buf, '\n') != NULL); /* end of line */
        if (bol) {
            /* The fgets reads from the beginning of a line. */
            if (!eol) {
                /* The next fgets reads from the middle of the same line. */
                bol = 0;
            }
        } else {
            /* The fgets reads from the middle of a line. */
            if (eol) {
                /* The next fgets reads from the beginning of a line. */
                bol = 1;
            }
            continue;
        }

        if (sscanf(buf, "%lx-%lx %4s", &start, &end, perms) != 3) {
            continue;
        }
        mem_prot->start = start;
        mem_prot->end = end;
        mem_prot->prot = 0;
        if (perms[0] == 'r') {
            mem_prot->prot |= PROT_READ;
        }
        if (perms[1] == 'w') {
            mem_prot->prot |= PROT_WRITE;
        }
        if (perms[2] == 'x') {
            mem_prot->prot |= PROT_EXEC;
        }
        return 0;
    }
    return -1;
}

static void mem_prot_end(mem_prot_iter_t* iter)
{
    if (iter->fp != NULL) {
        fclose(iter->fp);
    }
}
#elif defined __FreeBSD__
struct mem_prot_iter
{
    struct kinfo_vmentry* kve;
    int idx;
    int num;
};

static int mem_prot_begin(mem_prot_iter_t* iter)
{
    iter->kve = kinfo_getvmmap(getpid(), &iter->num);
    if (iter->kve == NULL) {
        set_errmsg("failed to call kinfo_getvmmap()\n");
        return -1;
    }
    iter->idx = 0;
    return 0;
}

static int mem_prot_next(mem_prot_iter_t* iter, mem_prot_t* mem_prot)
{
    if (iter->idx >= iter->num) {
        return -1;
    }
    struct kinfo_vmentry* kve = &iter->kve[iter->idx++];
    mem_prot->start = kve->kve_start;
    mem_prot->end = kve->kve_end;
    mem_prot->prot = 0;
    if (kve->kve_protection & KVME_PROT_READ) {
        mem_prot->prot |= PROT_READ;
    }
    if (kve->kve_protection & KVME_PROT_WRITE) {
        mem_prot->prot |= PROT_WRITE;
    }
    if (kve->kve_protection & KVME_PROT_EXEC) {
        mem_prot->prot |= PROT_EXEC;
    }
    return 0;
}

static void mem_prot_end(mem_prot_iter_t* iter)
{
    if (iter->kve != NULL) {
        free(iter->kve);
    }
}
#elif defined(__sun)
struct mem_prot_iter
{
    FILE* fp;
    prmap_t maps[20];
    size_t idx;
    size_t num;
};

static int mem_prot_begin(mem_prot_iter_t* iter)
{
    iter->fp = fopen("/proc/self/map", "r");
    if (iter->fp == NULL) {
        set_errmsg("failed to open /proc/self/map");
        return -1;
    }
    iter->idx = iter->num = 0;
    return 0;
}

static int mem_prot_next(mem_prot_iter_t* iter, mem_prot_t* mem_prot)
{
    prmap_t* map;

    if (iter->idx == iter->num) {
        iter->num = fread(iter->maps, sizeof(iter->maps[0]), sizeof(iter->maps) / sizeof(iter->maps[0]), iter->fp);
        if (iter->num == 0) {
            return -1;
        }
        iter->idx = 0;
    }
    map = &iter->maps[iter->idx++];
    mem_prot->start = map->pr_vaddr;
    mem_prot->end = map->pr_vaddr + map->pr_size;
    mem_prot->prot = 0;
    if (map->pr_mflags & MA_READ) {
        mem_prot->prot |= PROT_READ;
    }
    if (map->pr_mflags & MA_WRITE) {
        mem_prot->prot |= PROT_WRITE;
    }
    if (map->pr_mflags & MA_EXEC) {
        mem_prot->prot |= PROT_EXEC;
    }
    return 0;
}

static void mem_prot_end(mem_prot_iter_t* iter)
{
    if (iter->fp != NULL) {
        fclose(iter->fp);
    }
}
#else
#error Unsupported platform
#endif

static int plthook_open_real(plthook_t** plthook_out, struct link_map* lmap)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    plthook_t plthook = {
        NULL,
    };
#pragma GCC diagnostic pop

    const Elf_Dyn* dyn;
    const char* dyn_addr_base = NULL;

    if (page_size == 0) {
        page_size = sysconf(_SC_PAGESIZE);
    }

#if defined __linux__
    plthook.plt_addr_base = (char*)lmap->l_addr;
#if defined __riscv
    const Elf_Ehdr* ehdr = (const Elf_Ehdr*)lmap->l_addr;
    if (ehdr->e_type == ET_DYN) {
        dyn_addr_base = (const char*)lmap->l_addr;
    }
#endif
#if defined __ANDROID__ || defined __UCLIBC__
    dyn_addr_base = (const char*)lmap->l_addr;
#endif
#elif defined __FreeBSD__ || defined __sun
#if __FreeBSD__ >= 13
    const Elf_Ehdr* ehdr = (const Elf_Ehdr*)lmap->l_base;
#else
    const Elf_Ehdr* ehdr = (const Elf_Ehdr*)lmap->l_addr;
#endif
    int rv_ = check_elf_header(ehdr);
    if (rv_ != 0) {
        return rv_;
    }
    if (ehdr->e_type == ET_DYN) {
        dyn_addr_base = (const char*)lmap->l_addr;
        plthook.plt_addr_base = (const char*)lmap->l_addr;
    }
#else
#error unsupported OS
#endif

    /* get .dynsym section */
    dyn = find_dyn_by_tag(lmap->l_ld, DT_SYMTAB);
    if (dyn == NULL) {
        set_errmsg("failed to find DT_SYMTAB");
        return PLTHOOK_INTERNAL_ERROR;
    }
    plthook.dynsym = (const Elf_Sym*)(dyn_addr_base + dyn->d_un.d_ptr);

    /* Check sizeof(Elf_Sym) */
    dyn = find_dyn_by_tag(lmap->l_ld, DT_SYMENT);
    if (dyn == NULL) {
        set_errmsg("failed to find DT_SYMTAB");
        return PLTHOOK_INTERNAL_ERROR;
    }
    if (dyn->d_un.d_val != sizeof(Elf_Sym)) {
        set_errmsg("DT_SYMENT size %" ELF_XWORD_FMT " != %" SIZE_T_FMT, dyn->d_un.d_val, sizeof(Elf_Sym));
        return PLTHOOK_INTERNAL_ERROR;
    }

    /* get .dynstr section */
    dyn = find_dyn_by_tag(lmap->l_ld, DT_STRTAB);
    if (dyn == NULL) {
        set_errmsg("failed to find DT_STRTAB");
        return PLTHOOK_INTERNAL_ERROR;
    }
    plthook.dynstr = dyn_addr_base + dyn->d_un.d_ptr;

    /* get .dynstr size */
    dyn = find_dyn_by_tag(lmap->l_ld, DT_STRSZ);
    if (dyn == NULL) {
        set_errmsg("failed to find DT_STRSZ");
        return PLTHOOK_INTERNAL_ERROR;
    }
    plthook.dynstr_size = dyn->d_un.d_val;

    /* get .rela.plt or .rel.plt section */
    dyn = find_dyn_by_tag(lmap->l_ld, DT_JMPREL);
    if (dyn != NULL) {
        plthook.rela_plt = (const Elf_Plt_Rel*)(dyn_addr_base + dyn->d_un.d_ptr);
        dyn = find_dyn_by_tag(lmap->l_ld, DT_PLTRELSZ);
        if (dyn == NULL) {
            set_errmsg("failed to find DT_PLTRELSZ");
            return PLTHOOK_INTERNAL_ERROR;
        }
        plthook.rela_plt_cnt = dyn->d_un.d_val / sizeof(Elf_Plt_Rel);
    }
#ifdef R_GLOBAL_DATA
    /* get .rela.dyn or .rel.dyn section */
    dyn = find_dyn_by_tag(lmap->l_ld, PLT_DT_REL);
    if (dyn != NULL) {
        size_t total_size, elem_size;

        plthook.rela_dyn = (const Elf_Plt_Rel*)(dyn_addr_base + dyn->d_un.d_ptr);
        dyn = find_dyn_by_tag(lmap->l_ld, PLT_DT_RELSZ);
        if (dyn == NULL) {
            set_errmsg("failed to find PLT_DT_RELSZ");
            return PLTHOOK_INTERNAL_ERROR;
        }
        total_size = dyn->d_un.d_ptr;

        dyn = find_dyn_by_tag(lmap->l_ld, PLT_DT_RELENT);
        if (dyn == NULL) {
            set_errmsg("failed to find PLT_DT_RELENT");
            return PLTHOOK_INTERNAL_ERROR;
        }
        elem_size = dyn->d_un.d_ptr;
        plthook.rela_dyn_cnt = total_size / elem_size;
    }
#endif

#ifdef R_GLOBAL_DATA
    if (plthook.rela_plt == NULL && plthook.rela_dyn == NULL) {
        set_errmsg("failed to find either of DT_JMPREL and DT_REL");
        return PLTHOOK_INTERNAL_ERROR;
    }
#else
    if (plthook.rela_plt == NULL) {
        set_errmsg("failed to find DT_JMPREL");
        return PLTHOOK_INTERNAL_ERROR;
    }
#endif
    if (plthook_set_mem_prot(&plthook)) {
        return PLTHOOK_INTERNAL_ERROR;
    }

    *plthook_out = malloc(sizeof(plthook_t));
    if (*plthook_out == NULL) {
        set_errmsg("failed to allocate memory: %" SIZE_T_FMT " bytes", sizeof(plthook_t));
        return PLTHOOK_OUT_OF_MEMORY;
    }
    **plthook_out = plthook;
    return 0;
}

static int plthook_set_mem_prot(plthook_t* plthook)
{
    unsigned int pos = 0;
    const char* name;
    void** addr;
    size_t start = (size_t)-1;
    size_t end = 0;
    mem_prot_iter_t iter;
    mem_prot_t mem_prot;
    int idx = 0;

    while (plthook_enum(plthook, &pos, &name, &addr) == 0) {
        if (start > (size_t)addr) {
            start = (size_t)addr;
        }
        if (end < (size_t)addr) {
            end = (size_t)addr;
        }
    }
    end++;

    if (mem_prot_begin(&iter) != 0) {
        return PLTHOOK_INTERNAL_ERROR;
    }
    while (mem_prot_next(&iter, &mem_prot) == 0 && idx < NUM_MEM_PROT) {
        if (mem_prot.prot != 0 && mem_prot.start < end && start < mem_prot.end) {
            plthook->mem_prot[idx++] = mem_prot;
        }
    }
    mem_prot_end(&iter);
    return 0;
}

static int plthook_get_mem_prot(plthook_t* plthook, void* addr)
{
    mem_prot_t* ptr = plthook->mem_prot;
    mem_prot_t* end = ptr + NUM_MEM_PROT;

    while (ptr < end && ptr->prot != 0) {
        if (ptr->start <= (size_t)addr && (size_t)addr < ptr->end) {
            return ptr->prot;
        }
        ++ptr;
    }
    return 0;
}

#if defined __FreeBSD__ || defined __sun
static int check_elf_header(const Elf_Ehdr* ehdr)
{
    static const unsigned short s = 1;
    /* Check endianness at runtime. */
    unsigned char elfdata = (*(const char*)&s) ? ELFDATA2LSB : ELFDATA2MSB;

    if (ehdr == NULL) {
        set_errmsg("invalid elf header address: NULL");
        return PLTHOOK_INTERNAL_ERROR;
    }

    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        set_errmsg("invalid file signature: 0x%02x,0x%02x,0x%02x,0x%02x", ehdr->e_ident[0], ehdr->e_ident[1], ehdr->e_ident[2], ehdr->e_ident[3]);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_ident[EI_CLASS] != ELF_CLASS) {
        set_errmsg("invalid elf class: 0x%02x", ehdr->e_ident[EI_CLASS]);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_ident[EI_DATA] != elfdata) {
        set_errmsg("invalid elf data: 0x%02x", ehdr->e_ident[EI_DATA]);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
        set_errmsg("invalid elf version: 0x%02x", ehdr->e_ident[EI_VERSION]);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        set_errmsg("invalid file type: 0x%04x", ehdr->e_type);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_version != EV_CURRENT) {
        set_errmsg("invalid object file version: %" ELF_WORD_FMT, ehdr->e_version);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_ehsize != sizeof(Elf_Ehdr)) {
        set_errmsg("invalid elf header size: %u", ehdr->e_ehsize);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_phentsize != sizeof(Elf_Phdr)) {
        set_errmsg("invalid program header table entry size: %u", ehdr->e_phentsize);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    return 0;
}
#endif

static int check_rel(const plthook_t* plthook, const Elf_Plt_Rel* plt, Elf_Xword r_type, const char** name_out, void*** addr_out)
{
    if (ELF_R_TYPE(plt->r_info) == r_type) {
        size_t idx = ELF_R_SYM(plt->r_info);
        idx = plthook->dynsym[idx].st_name;
        if (idx + 1 > plthook->dynstr_size) {
            set_errmsg("too big section header string table index: %" SIZE_T_FMT, idx);
            return PLTHOOK_INVALID_FILE_FORMAT;
        }
        *name_out = plthook->dynstr + idx;
        *addr_out = (void**)(plthook->plt_addr_base + plt->r_offset);
        return 0;
    }
    return -1;
}

int plthook_enum(plthook_t* plthook, unsigned int* pos, const char** name_out, void*** addr_out)
{
    return plthook_enum_with_prot(plthook, pos, name_out, addr_out, NULL);
}

int plthook_enum_with_prot(plthook_t* plthook, unsigned int* pos, const char** name_out, void*** addr_out, int* prot)
{
    while (*pos < plthook->rela_plt_cnt) {
        const Elf_Plt_Rel* plt = plthook->rela_plt + *pos;
        int rv = check_rel(plthook, plt, R_JUMP_SLOT, name_out, addr_out);
        (*pos)++;
        if (rv >= 0) {
            if (rv == 0 && prot != NULL) {
                *prot = plthook_get_mem_prot(plthook, *addr_out);
            }
            return rv;
        }
    }
#ifdef R_GLOBAL_DATA
    while (*pos < plthook->rela_plt_cnt + plthook->rela_dyn_cnt) {
        const Elf_Plt_Rel* plt = plthook->rela_dyn + (*pos - plthook->rela_plt_cnt);
        int rv = check_rel(plthook, plt, R_GLOBAL_DATA, name_out, addr_out);
        (*pos)++;
        if (rv >= 0) {
            if (rv == 0 && prot != NULL) {
                *prot = plthook_get_mem_prot(plthook, *addr_out);
            }
            return rv;
        }
    }
#endif
    *name_out = NULL;
    *addr_out = NULL;
    return EOF;
}

int plthook_replace(plthook_t* plthook, const char* funcname, void* funcaddr, void** oldfunc)
{
    size_t funcnamelen = strlen(funcname);
    unsigned int pos = 0;
    const char* name;
    void** addr;
    int rv;

    if (plthook == NULL) {
        set_errmsg("invalid argument: The first argument is null.");
        return PLTHOOK_INVALID_ARGUMENT;
    }
    while ((rv = plthook_enum(plthook, &pos, &name, &addr)) == 0) {
        if (strncmp(name, funcname, funcnamelen) == 0) {
            if (name[funcnamelen] == '\0' || name[funcnamelen] == '@') {
                int prot = plthook_get_mem_prot(plthook, addr);
                if (prot == 0) {
                    set_errmsg("Could not get the process memory permission at %p", ALIGN_ADDR(addr));
                    return PLTHOOK_INTERNAL_ERROR;
                }
                if (!(prot & PROT_WRITE)) {
                    if (mprotect(ALIGN_ADDR(addr), page_size, PROT_READ | PROT_WRITE) != 0) {
                        set_errmsg("Could not change the process memory permission at %p: %s", ALIGN_ADDR(addr), strerror(errno));
                        return PLTHOOK_INTERNAL_ERROR;
                    }
                }
                if (oldfunc) {
                    *oldfunc = *addr;
                }
                *addr = funcaddr;
                if (!(prot & PROT_WRITE)) {
                    mprotect(ALIGN_ADDR(addr), page_size, prot);
                }
                return 0;
            }
        }
    }
    if (rv == EOF) {
        set_errmsg("no such function: %s", funcname);
        rv = PLTHOOK_FUNCTION_NOT_FOUND;
    }
    return rv;
}

void plthook_close(plthook_t* plthook)
{
    if (plthook != NULL) {
        free(plthook);
    }
}

const char* plthook_error(void)
{
    return errmsg;
}

static void set_errmsg(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(errmsg, sizeof(errmsg) - 1, fmt, ap);
    va_end(ap);
}
