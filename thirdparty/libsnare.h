/* Copyright (c) 2026 Ethan Alexander
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#if defined(__sun) && defined(_XOPEN_SOURCE) && !defined(__EXTENSIONS__)
#define __EXTENSIONS__
#endif
#ifndef SNARE_H
#define SNARE_H
#include <stddef.h>

/* arch detection */
#if defined _M_IX86 || defined __i386__
#define SNARE_X86
#define SNARE_BITS 32
#elif defined _M_AMD64 || __amd64__
#define SNARE_X86_64
#define SNARE_BITS 64
#elif defined __aarch64__ || defined _M_ARM64
#define SNARE_ARM64
#define SNARE_BITS 64
#else
#error Unsupported architecture
#endif
/* os detection */
#if defined __linux__
#define SNARE_LINUX
#elif defined _WIN32
#define SNARE_WINDOWS
#elif defined __APPLE__
#define SNARE_MACOS
#else
#error Unsupported operating system
#endif
#if !defined SNARE_EXTERN
#if defined __cplusplus
#define SNARE_EXTERN extern "C"
#else
#define SNARE_EXTERN extern
#endif
#endif
#if defined SNARE_STATIC
#define SNARE_API
#define SNARE_EXPORT SNARE_EXTERN
#endif
#if !defined SNARE_API
#if defined SNARE_X86
#if defined SNARE_WINDOWS
#define SNARE_API __cdecl
#elif defined SNARE_LINUX
#define SNARE_API __attribute__((cdecl))
#endif
#else
#define SNARE_API
#endif
#endif
#if !defined SNARE_EXPORT
#if defined SNARE_WINDOWS
#if defined SNARE_IMPLEMENTATION
#define SNARE_EXPORT SNARE_EXTERN __declspec(dllexport)
#else
#define SNARE_EXPORT SNARE_EXTERN __declspec(dllimport)
#endif
#elif defined SNARE_LINUX
#if defined SNARE_IMPLEMENTATION
#define SNARE_EXPORT SNARE_EXTERN __attribute__((visibility("default")))
#else
#define SNARE_EXPORT SNARE_EXTERN
#endif
#else
#define SNARE_EXPORT SNARE_EXTERN
#endif
#endif

struct snare_inline_s;
typedef struct snare_inline_s *snare_inline_t;
SNARE_EXPORT snare_inline_t SNARE_API snare_inline_new(void *src, void *dst);
SNARE_EXPORT void SNARE_API snare_inline_free(snare_inline_t hook);
SNARE_EXPORT void *SNARE_API snare_inline_get_src(snare_inline_t hook);
SNARE_EXPORT void *SNARE_API snare_inline_get_dst(snare_inline_t hook);
SNARE_EXPORT void *SNARE_API snare_inline_get_trampoline(snare_inline_t hook);
SNARE_EXPORT int SNARE_API snare_inline_install(snare_inline_t hook);
SNARE_EXPORT int SNARE_API snare_inline_is_installed(snare_inline_t hook);
SNARE_EXPORT int SNARE_API snare_inline_remove(snare_inline_t hook);
/* batch install/remove with a single thread-freeze cycle */
SNARE_EXPORT int SNARE_API snare_inline_install_batch(snare_inline_t *hooks,
                                                      int count);
SNARE_EXPORT int SNARE_API snare_inline_remove_batch(snare_inline_t *hooks,
                                                     int count);
/* read jmp target from existing hook */
SNARE_EXPORT void *SNARE_API snare_inline_read_dst(void *src);

#define SNARE_PLT_SUCCESS 0
#define SNARE_PLT_FILE_NOT_FOUND 1
#define SNARE_PLT_INVALID_FILE_FORMAT 2
#define SNARE_PLT_FUNCTION_NOT_FOUND 3
#define SNARE_PLT_INVALID_ARGUMENT 4
#define SNARE_PLT_OUT_OF_MEMORY 5
#define SNARE_PLT_INTERNAL_ERROR 6
#define SNARE_PLT_NOT_IMPLEMENTED 7

struct snare_plt_s;
typedef struct snare_plt_s snare_plt_t;

SNARE_EXPORT int SNARE_API snare_plt_open(snare_plt_t **plthook_out,
                                          const char *filename);
SNARE_EXPORT int SNARE_API snare_plt_open_by_handle(snare_plt_t **plthook_out,
                                                    void *handle);
SNARE_EXPORT int SNARE_API snare_plt_open_by_address(snare_plt_t **plthook_out,
                                                     void *address);
SNARE_EXPORT int SNARE_API snare_plt_enum(snare_plt_t *plthook,
                                          unsigned int *pos,
                                          const char **name_out,
                                          void ***addr_out);
SNARE_EXPORT int SNARE_API snare_plt_replace(snare_plt_t *plthook,
                                             const char *funcname,
                                             void *funcaddr, void **oldfunc);
SNARE_EXPORT void SNARE_API snare_plt_close(snare_plt_t *plthook);
SNARE_EXPORT const char *SNARE_API snare_plt_error(void);
SNARE_EXPORT int SNARE_API snare_plt_enum_with_prot(snare_plt_t *plthook,
                                                    unsigned int *pos,
                                                    const char **name_out,
                                                    void ***addr_out,
                                                    int *prot);

typedef struct {
  const char *name;
  void **addr;
#ifdef SNARE_MACOS
  int addend;
  int prot;
  char weak;
#endif
} snare_plt_entry_t;

SNARE_EXPORT int SNARE_API snare_plt_enum_entry(snare_plt_t *plthook,
                                                unsigned int *pos,
                                                snare_plt_entry_t *entry);

#ifdef __cplusplus
class snare_inline {
public:
  snare_inline() : hook_(0) {}
  snare_inline(void *src, void *dst) : hook_(snare_inline_new(src, dst)) {}
  ~snare_inline() {
    snare_inline_remove(hook_);
    snare_inline_free(hook_);
  }
  void *get_src() { return snare_inline_get_src(hook_); }
  void *get_dst() { return snare_inline_get_dst(hook_); }
  void *get_trampoline() { return snare_inline_get_trampoline(hook_); }
  bool install() { return snare_inline_install(hook_) >= 0; }
  bool install(void *src, void *dst) {
    if (hook_ == 0) {
      hook_ = snare_inline_new(src, dst);
    }
    return install();
  }
  bool remove() { return snare_inline_remove(hook_) >= 0; }
  bool is_installed() const { return !!snare_inline_is_installed(hook_); }
  class scoped_remove {
  public:
    scoped_remove(snare_inline *hook)
        : hook_(hook), removed_(hook_->remove()) {}
    ~scoped_remove() {
      if (removed_) {
        hook_->install();
      }
    }

  private:
    scoped_remove(const scoped_remove &);
    void operator=(const scoped_remove &);

  private:
    snare_inline *hook_;
    bool removed_;
  };
  class scoped_install {
  public:
    scoped_install(snare_inline *hook)
        : hook_(hook), installed_(hook_->install()) {}
    ~scoped_install() {
      if (installed_) {
        hook_->remove();
      }
    }

  private:
    scoped_install(const scoped_install &);
    void operator=(const scoped_install &);

  private:
    snare_inline *hook_;
    bool installed_;
  };
  static void *read_dst(void *src) { return snare_inline_read_dst(src); }

private:
  snare_inline(const snare_inline &);
  void operator=(const snare_inline &);

private:
  snare_inline_t hook_;
};
class snare_plt {
public:
  snare_plt() : plt_(0) {}
  snare_plt(const char *filename) : plt_(0) { open(filename); }
  ~snare_plt() { close(); }
  bool open(const char *filename) {
    close();
    return snare_plt_open(&plt_, filename) == SNARE_PLT_SUCCESS;
  }
  bool open_by_handle(void *handle) {
    close();
    return snare_plt_open_by_handle(&plt_, handle) == SNARE_PLT_SUCCESS;
  }
  bool open_by_address(void *address) {
    close();
    return snare_plt_open_by_address(&plt_, address) == SNARE_PLT_SUCCESS;
  }
  int enum_next(unsigned int *pos, const char **name, void ***addr) {
    return snare_plt_enum(plt_, pos, name, addr);
  }
  bool replace(const char *funcname, void *funcaddr, void **oldfunc) {
    return snare_plt_replace(plt_, funcname, funcaddr, oldfunc) ==
           SNARE_PLT_SUCCESS;
  }
  void close() {
    if (plt_) {
      snare_plt_close(plt_);
      plt_ = 0;
    }
  }
  static const char *error() { return snare_plt_error(); }

private:
  snare_plt(const snare_plt &);
  void operator=(const snare_plt &);

private:
  snare_plt_t *plt_;
};
#endif /* __cplusplus */

#ifdef SNARE_IMPLEMENTATION
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef SNARE_WINDOWS
typedef unsigned __int8 uint8_t;
typedef __int32 int32_t;
#if SNARE_BITS == 64
typedef __int64 intptr_t;
#elif SNARE_BITS == 32
typedef __int32 intptr_t;
#endif
#else
#include <stdint.h>
#endif
#if defined SNARE_LINUX || defined SNARE_MACOS
#include <sys/mman.h>
#include <unistd.h>
#if defined SNARE_ARM64 && defined SNARE_MACOS
#include <libkern/OSCacheControl.h>
#endif
#endif
#ifdef SNARE_WINDOWS
#include <tlhelp32.h>
#include <windows.h>
#endif

#define SNARE_MAX_IPS 8

struct snare_inline_s {
  int installed;
  void *src;
  void *dst;
  void *code;
  void *trampoline;
  size_t
      orig_size; /* number of bytes overwritten at src (copied to trampoline) */
#if defined SNARE_X86 || defined SNARE_X86_64
  uint8_t n_ips;                    /* instruction boundary count          */
  uint8_t orig_ips[SNARE_MAX_IPS];  /* offsets within original code        */
  uint8_t tramp_ips[SNARE_MAX_IPS]; /* corresponding offsets in trampoline */
#endif
#ifdef SNARE_X86_64
  void *relay_page; /* near-allocated RWX page (trampoline + relay live here) */
  size_t relay_size;
  void *relay_jmp; /* pointer to abs jmp within relay_page, NULL if direct */
#endif
};
/* x86/x64: 5 byte rel32 jmp (e9 + rel32) */
#if defined SNARE_X86 || defined SNARE_X86_64
#define JMP_INSN_OPCODE 0xE9
#define JMP_INSN_LEN sizeof(struct snare_jmp_rel)
#define MAX_INSN_LEN 15
#pragma pack(push, 1)
struct snare_jmp_rel {
  uint8_t opcode;
  int32_t offset;
};
#pragma pack(pop)
#ifdef SNARE_X86_64
/* x64: 14 byte absolute jmp (ff 25 00 00 00 00 + 8 byte addr) */
#pragma pack(push, 1)
struct snare_jmp_abs {
  uint8_t opcode[2]; /* FF 25 */
  int32_t zero;      /* 00 00 00 00 = [RIP+0] */
  uint64_t addr;
};
#pragma pack(pop)
#define ABS_JMP_LEN sizeof(struct snare_jmp_abs)
/* trampoline may need abs jmp back on x64 */
#define MAX_TRAMPOLINE_LEN (MAX_INSN_LEN * 3 + ABS_JMP_LEN)
/* relay page: trampoline + abs jmp to detour */
#define RELAY_PAGE_CONTENT (MAX_TRAMPOLINE_LEN + ABS_JMP_LEN)
#else
#define MAX_TRAMPOLINE_LEN (MAX_INSN_LEN * 3 + JMP_INSN_LEN)
#endif /* SNARE_X86_64 */
#elif defined SNARE_ARM64
/* arm64: 16 byte ldr+br stub */
#define JMP_INSN_LEN 16
#define MAX_INSN_LEN 4
#define MAX_TRAMPOLINE_LEN (JMP_INSN_LEN + (MAX_INSN_LEN * 4))
#pragma pack(push, 1)
struct snare_jmp_rel {
  uint32_t ldr;
  uint32_t br;
  uint64_t addr;
};
#pragma pack(pop)
#endif

static void *snare_unprotect(void *address, size_t size) {
#if defined SNARE_LINUX || defined SNARE_MACOS
  long pagesize;
  pagesize = sysconf(_SC_PAGESIZE);
  address = (void *)((long)address & ~(pagesize - 1));
  if (mprotect(address, size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
    return NULL;
  return address;
#elif defined SNARE_WINDOWS
  DWORD old_protect;
  MEMORY_BASIC_INFORMATION mbi;
  if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0)
    return NULL;
  if (VirtualProtect(mbi.BaseAddress, size, PAGE_EXECUTE_READWRITE,
                     &old_protect) == 0)
    return NULL;
  return address;
#endif
}

#ifdef SNARE_X86_64
/* check if two addresses are within signed 32-bit reach */
static int snare_in_rel32_range(void *a, void *b) {
  intptr_t diff = (intptr_t)b - (intptr_t)a;
  return diff >= -0x7FFFFF00LL && diff <= 0x7FFFFF00LL;
}
/* allocate an executable page within ±2GB of target */
static void *snare_alloc_near(void *target, size_t size) {
  uintptr_t addr = (uintptr_t)target;
  uintptr_t lo =
      addr > (uintptr_t)0x80000000ULL ? addr - 0x80000000ULL : 0x10000ULL;
  uintptr_t hi = addr + 0x80000000ULL;
  if (hi < addr)
    hi = (uintptr_t)-1; /* overflow guard */
#ifdef SNARE_WINDOWS
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  uintptr_t gran = si.dwAllocationGranularity;
  uintptr_t try_addr;
  /* scan downward first (typically more address space below) */
  for (try_addr = (addr & ~(gran - 1)) - gran; try_addr >= lo;
       try_addr -= gran) {
    void *p = VirtualAlloc((void *)try_addr, size, MEM_COMMIT | MEM_RESERVE,
                           PAGE_EXECUTE_READWRITE);
    if (p)
      return p;
  }
  /* scan upward */
  for (try_addr = (addr & ~(gran - 1)) + gran; try_addr <= hi;
       try_addr += gran) {
    void *p = VirtualAlloc((void *)try_addr, size, MEM_COMMIT | MEM_RESERVE,
                           PAGE_EXECUTE_READWRITE);
    if (p)
      return p;
  }
#else
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0)
    page_size = 4096;
  uintptr_t page_mask = ~((uintptr_t)page_size - 1);
  uintptr_t try_addr;
  /* scan downward */
  for (try_addr = (addr & page_mask) - (uintptr_t)page_size; try_addr >= lo;
       try_addr -= (uintptr_t)page_size) {
    void *p = mmap((void *)try_addr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
      continue;
    if (snare_in_rel32_range(target, p))
      return p;
    munmap(p, size);
  }
  /* scan upward */
  for (try_addr = (addr & page_mask) + (uintptr_t)page_size; try_addr <= hi;
       try_addr += (uintptr_t)page_size) {
    void *p = mmap((void *)try_addr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
      continue;
    if (snare_in_rel32_range(target, p))
      return p;
    munmap(p, size);
  }
#endif
  return NULL;
}
static void snare_free_near(void *addr, size_t size) {
#ifdef SNARE_WINDOWS
  (void)size;
  VirtualFree(addr, 0, MEM_RELEASE);
#else
  munmap(addr, size);
#endif
}
static void snare_make_abs_jmp(void *where, void *target) {
  struct snare_jmp_abs *j = (struct snare_jmp_abs *)where;
  j->opcode[0] = 0xFF;
  j->opcode[1] = 0x25;
  j->zero = 0;
  j->addr = (uint64_t)(uintptr_t)target;
}
#endif /* SNARE_X86_64 */

#if defined SNARE_X86 || defined SNARE_X86_64
/*  able-driven length disassembler for x86/x86-64  *
 * Covers the full one-byte opcode map plus the 0F two-byte map.          *
 * Returns the total instruction length in bytes, or 0 on failure.        *
 * If `reloc` is non-NULL it receives the byte offset of a RIP-relative   *
 * or rel32 displacement that needs fixup when the instruction is copied. */

/* opcode-table flags (packed into uint8_t) */
#define SF_M 0x01u  /* has ModR/M byte                                  */
#define SF_I8 0x02u /* 8-bit immediate                                  */
#define SF_IV 0x04u /* operand-size immediate (2 or 4 bytes)            */
#define SF_R8 0x08u /* 8-bit relative (jcc short, jmp short, loop)      */
#define SF_RV 0x10u /* 32-bit relative (call/jmp near, 0F 8x jcc near) */
#define SF_SP 0x20u /* special: per-opcode handling in code             */
#define SF_2B 0x40u /* 0x0F two-byte escape                            */
#define SF_XX 0x80u /* unsupported / invalid opcode                    */

/* short aliases used only in the table literals */
#define _0 0
#define _M SF_M
#define MI (SF_M | SF_I8)
#define MV (SF_M | SF_IV)
#define MS (SF_M | SF_SP)
#define _8 SF_I8
#define _V SF_IV
#define R8 SF_R8
#define RV SF_RV
#define SP SF_SP
#define _E SF_2B
#define XX SF_XX

/* clang-format off */

/* One-byte opcode map (00-FF) */
static const uint8_t snare_op1[256] = {
/*        _0   _1   _2   _3   _4   _5   _6   _7   _8   _9   _A   _B   _C   _D   _E   _F */
/* 0_ */  _M,  _M,  _M,  _M,  _8,  _V,  _0,  _0,  _M,  _M,  _M,  _M,  _8,  _V,  _0,  _E,
/* 1_ */  _M,  _M,  _M,  _M,  _8,  _V,  _0,  _0,  _M,  _M,  _M,  _M,  _8,  _V,  _0,  _0,
/* 2_ */  _M,  _M,  _M,  _M,  _8,  _V,  _0,  _0,  _M,  _M,  _M,  _M,  _8,  _V,  _0,  _0,
/* 3_ */  _M,  _M,  _M,  _M,  _8,  _V,  _0,  _0,  _M,  _M,  _M,  _M,  _8,  _V,  _0,  _0,
/* 4_ */  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,
/* 5_ */  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,
/* 6_ */  _0,  _0,  XX,  _M,  _0,  _0,  _0,  _0,  _V,  MV,  _8,  MI,  _0,  _0,  _0,  _0,
/* 7_ */  R8,  R8,  R8,  R8,  R8,  R8,  R8,  R8,  R8,  R8,  R8,  R8,  R8,  R8,  R8,  R8,
/* 8_ */  MI,  MV,  MI,  MI,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,
/* 9_ */  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  XX,  _0,  _0,  _0,  _0,  _0,
/* A_ */  SP,  SP,  SP,  SP,  _0,  _0,  _0,  _0,  _8,  _V,  _0,  _0,  _0,  _0,  _0,  _0,
/* B_ */  _8,  _8,  _8,  _8,  _8,  _8,  _8,  _8,  _V,  _V,  _V,  _V,  _V,  _V,  _V,  _V,
/* C_ */  MI,  MI,  SP,  _0,  XX,  XX,  MI,  MV,  SP,  _0,  SP,  _0,  _0,  _8,  _0,  _0,
/* D_ */  _M,  _M,  _M,  _M,  _8,  _8,  _0,  _0,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,
/* E_ */  R8,  R8,  R8,  R8,  _8,  _8,  _8,  _8,  RV,  RV,  XX,  R8,  _0,  _0,  _0,  _0,
/* F_ */  _0,  _0,  _0,  _0,  _0,  _0,  MS,  MS,  _0,  _0,  _0,  _0,  _0,  _0,  _M,  _M,
};

/* 0F two-byte opcode map */
static const uint8_t snare_op2[256] = {
/*        _0   _1   _2   _3   _4   _5   _6   _7   _8   _9   _A   _B   _C   _D   _E   _F */
/* 0_ */  _M,  _M,  _M,  _M,  XX,  _0,  _0,  _0,  _0,  _0,  XX,  _0,  XX,  _M,  _0,  MI,
/* 1_ */  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,
/* 2_ */  _M,  _M,  _M,  _M,  XX,  XX,  XX,  XX,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,
/* 3_ */  _0,  _0,  _0,  _0,  _0,  _0,  XX,  XX,  XX,  XX,  XX,  XX,  XX,  XX,  XX,  XX,
/* 4_ */  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,
/* 5_ */  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,
/* 6_ */  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,
/* 7_ */  MI,  MI,  MI,  MI,  _M,  _M,  _M,  _0,  _M,  _M,  XX,  XX,  _M,  _M,  _M,  _M,
/* 8_ */  RV,  RV,  RV,  RV,  RV,  RV,  RV,  RV,  RV,  RV,  RV,  RV,  RV,  RV,  RV,  RV,
/* 9_ */  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,
/* A_ */  _0,  _0,  _0,  _M,  MI,  _M,  XX,  XX,  _0,  _0,  _0,  _M,  MI,  _M,  _M,  _M,
/* B_ */  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  MI,  _M,  _M,  _M,  _M,  _M,
/* C_ */  _M,  _M,  MI,  _M,  MI,  MI,  MI,  _M,  _0,  _0,  _0,  _0,  _0,  _0,  _0,  _0,
/* D_ */  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,
/* E_ */  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,
/* F_ */  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,  _M,
};

/* clang-format on */

#undef _0
#undef _M
#undef MI
#undef MV
#undef MS
#undef _8
#undef _V
#undef R8
#undef RV
#undef SP
#undef _E
#undef XX

static size_t snare_disasm(uint8_t *code, int *reloc) {
  int len = 0;
  int operand_size = 4;
  int address_size = SNARE_BITS / 8;
  int has_rex_w = 0;
  int opcode_offset;
  uint8_t opcode, flags;

  /* consume legacy prefixes (up to 4 from different groups) */
  for (;;) {
    uint8_t c = code[len];
    if (c == 0xF0 || c == 0xF2 || c == 0xF3 || /* lock / repne / rep */
        c == 0x2E || c == 0x36 || c == 0x3E || c == 0x26 || /* seg ovr */
        c == 0x64 || c == 0x65) {                           /* FS / GS */
      len++;
      continue;
    }
    if (c == 0x66) {
      operand_size = 2;
      len++;
      continue;
    }
    if (c == 0x67) {
      address_size = SNARE_BITS / 8 / 2;
      len++;
      continue;
    }
    break;
  }

#ifdef SNARE_X86_64
  /* consume REX prefix (0x40-0x4F) */
  if ((code[len] & 0xF0) == 0x40) {
    if (code[len] & 0x08) {
      has_rex_w = 1;
      operand_size = 8;
    }
    len++;
  }
#else
  (void)has_rex_w;
#endif

  opcode_offset = len;
  opcode = code[len++];
  flags = snare_op1[opcode];

  /* 0F two-byte escape */
  if (flags & SF_2B) {
    opcode = code[len++];
    /* 0F 38 xx: three-byte SSE4/AES/SHA — all ModR/M, no immediate */
    if (opcode == 0x38) {
      len++; /* consume third opcode byte */
      flags = SF_M;
    }
    /* 0F 3A xx: three-byte SSE4/AVX — all ModR/M + imm8 */
    else if (opcode == 0x3A) {
      len++; /* consume third opcode byte */
      flags = SF_M | SF_I8;
    } else {
      flags = snare_op2[opcode];
    }
  }

  if (flags & SF_XX)
    return 0; /* unsupported opcode */

  /* special-case opcodes */
  if (flags & SF_SP) {
    uint8_t op = code[opcode_offset];
    /* A0-A3: MOV moffs — immediate is address_size bytes */
    if (op >= 0xA0 && op <= 0xA3)
      return len + address_size;
    /* C2 / CA: RET / RETF imm16 — always 2-byte immediate */
    if (op == 0xC2 || op == 0xCA)
      return len + 2;
    /* C8: ENTER imm16, imm8 — 3 bytes of immediate */
    if (op == 0xC8)
      return len + 3;
    /* F6/F7: GRP3 — TEST (reg=0,1) has an immediate, others don't */
    if (op == 0xF6 || op == 0xF7) {
      int modrm = code[len++];
      int mod = modrm >> 6, rm = modrm & 7, reg = (modrm >> 3) & 7;
      if (mod != 3 && rm == 4)
        len++; /* SIB */
      if (mod == 1)
        len += 1;
      else if (mod == 2 || (mod == 0 && rm == 5))
        len += 4;
      if (reg <= 1) /* TEST */
        len += (op == 0xF6) ? 1 : ((operand_size == 2) ? 2 : 4);
      return len;
    }
    return 0; /* unknown special */
  }

  /* ModR/M + SIB + displacement */
  if (flags & SF_M) {
    int modrm = code[len++];
    int mod = modrm >> 6, rm = modrm & 7;
    if (mod != 3 && rm == 4)
      len++; /* SIB byte */
#ifdef SNARE_X86_64
    /* RIP-relative: mod=00 rm=5 → [RIP+disp32] */
    if (reloc && mod == 0 && rm == 5)
      *reloc = len;
#endif
    if (mod == 1)
      len += 1;
    else if (mod == 2 || (mod == 0 && rm == 5))
      len += 4;
  }

  /* immediates and relative offsets */
  if (flags & SF_I8)
    len += 1;
  if (flags & SF_R8)
    len += 1;
  if (flags & SF_IV) {
#ifdef SNARE_X86_64
    /* movabs r64, imm64: REX.W + B8+r */
    if (has_rex_w && (code[opcode_offset] & 0xF8) == 0xB8)
      len += 8;
    else
#endif
      len += (operand_size == 2) ? 2 : 4;
  }
  if (flags & SF_RV) {
    if (reloc)
      *reloc = len;
    len += (operand_size == 2) ? 2 : 4;
  }

  return len;
}
#endif /* SNARE_X86 || SNARE_X86_64 */

static size_t snare_make_rel_jmp(uint8_t *src, uint8_t *dst, size_t offset) {
  struct snare_jmp_rel *jmp = (struct snare_jmp_rel *)(src + offset);
#if defined SNARE_X86 || defined SNARE_X86_64
  jmp->opcode = JMP_INSN_OPCODE;
  jmp->offset = (int32_t)(dst - (src + offset + JMP_INSN_LEN));
#elif defined SNARE_ARM64
  jmp->ldr = 0x58000050; /* ldr x16, #8 */
  jmp->br = 0xD61F0200;  /* br x16 */
  jmp->addr = (uint64_t)dst;
#endif
  return sizeof(*jmp);
}

/* scan past prefix bytes in an instruction to find the opcode */
#if defined SNARE_X86 || defined SNARE_X86_64
static int snare_skip_prefixes(uint8_t *insn, int insn_len) {
  int p = 0;
  while (p < insn_len) {
    uint8_t c = insn[p];
    if (c == 0xF0 || c == 0xF2 || c == 0xF3 || c == 0x2E || c == 0x36 ||
        c == 0x3E || c == 0x26 || c == 0x64 || c == 0x65 || c == 0x66 ||
        c == 0x67) {
      p++;
      continue;
    }
#ifdef SNARE_X86_64
    if ((c & 0xF0) == 0x40) {
      p++;
      continue;
    } /* REX */
#endif
    break;
  }
  return p;
}
#endif

static size_t snare_make_trampoline(uint8_t *trampoline, uint8_t *src,
                                    int near_allocated, snare_inline_t hook) {
  size_t orig_size = 0;
#if defined SNARE_X86 || defined SNARE_X86_64
  size_t tramp_size = 0;
  if (hook)
    hook->n_ips = 0;
  while (orig_size < JMP_INSN_LEN) {
    int reloc = 0;
    size_t insn_len = snare_disasm(src + orig_size, &reloc);
    if (insn_len == 0) {
      fprintf(stdout, "[snare] unsupported opcode 0x%02X at %p (src+%zu)\n",
              (unsigned)(src[orig_size]), (void *)(src + orig_size), orig_size);
      return 0;
    }

    /* record instruction boundary mapping */
    if (hook && hook->n_ips < SNARE_MAX_IPS) {
      hook->orig_ips[hook->n_ips] = (uint8_t)orig_size;
      hook->tramp_ips[hook->n_ips] = (uint8_t)tramp_size;
      hook->n_ips++;
    }

    uint8_t *insn = src + orig_size;
    int pfx = snare_skip_prefixes(insn, (int)insn_len);
    uint8_t op = insn[pfx];

    /* short branch expansion */
    if (op == 0xEB && pfx + 2 == (int)insn_len) {
      /* JMP short rel8 → JMP near rel32 (EB xx → E9 xx xx xx xx) */
      int8_t rel8 = (int8_t)insn[pfx + 1];
      intptr_t target = (intptr_t)(src + orig_size + insn_len) + rel8;
      size_t exp_len = 5; /* E9 + rel32 */
      intptr_t new_ip = (intptr_t)(trampoline + tramp_size + exp_len);
      intptr_t diff = target - new_ip;
#ifdef SNARE_X86_64
      if (diff > 0x7FFFFFFFLL || diff < -(intptr_t)0x80000000LL)
        return 0;
#endif
      trampoline[tramp_size] = 0xE9;
      *(int32_t *)(trampoline + tramp_size + 1) = (int32_t)diff;
      tramp_size += exp_len;
      orig_size += insn_len;
      continue;
    }
    if (op >= 0x70 && op <= 0x7F && pfx + 2 == (int)insn_len) {
      /* Jcc short rel8 → Jcc near rel32 (7x xx → 0F 8x xx xx xx xx) */
      int8_t rel8 = (int8_t)insn[pfx + 1];
      intptr_t target = (intptr_t)(src + orig_size + insn_len) + rel8;
      size_t exp_len = 6; /* 0F 8x + rel32 */
      intptr_t new_ip = (intptr_t)(trampoline + tramp_size + exp_len);
      intptr_t diff = target - new_ip;
#ifdef SNARE_X86_64
      if (diff > 0x7FFFFFFFLL || diff < -(intptr_t)0x80000000LL)
        return 0;
#endif
      trampoline[tramp_size] = 0x0F;
      trampoline[tramp_size + 1] = 0x80 | (op & 0x0F);
      *(int32_t *)(trampoline + tramp_size + 2) = (int32_t)diff;
      tramp_size += exp_len;
      orig_size += insn_len;
      continue;
    }

#ifndef SNARE_X86_64
    if (pfx == 0 && op == 0xE8 && insn_len == 5 && reloc == 1) {
      int32_t call_disp;
      memcpy(&call_disp, insn + 1, 4);
      uint8_t *call_tgt = (uint8_t *)((uintptr_t)(src + orig_size + 5) +
                                      (uintptr_t)(intptr_t)call_disp);

      uint32_t pc_val = (uint32_t)(uintptr_t)(src + orig_size + 5);
      /* 8B /r 24 C3  ==>  mov r32, [esp]; ret */
      if (call_tgt[0] == 0x8B && (call_tgt[1] & 0xC7) == 0x04 &&
          call_tgt[2] == 0x24 && call_tgt[3] == 0xC3) {
        int reg_idx = (call_tgt[1] >> 3) & 7;
        trampoline[tramp_size] = (uint8_t)(0xB8 + reg_idx); /* MOV r32, imm32 */
        memcpy(trampoline + tramp_size + 1, &pc_val, 4);
        tramp_size += 5;
        orig_size += 5;
        continue;
      }
      /* (58+r) C3  ==>  pop reg; ret */
      if (call_tgt[0] >= 0x58 && call_tgt[0] <= 0x5F && call_tgt[1] == 0xC3) {
        int reg_idx = call_tgt[0] - 0x58;
        trampoline[tramp_size] = (uint8_t)(0xB8 + reg_idx); /* MOV r32, imm32 */
        memcpy(trampoline + tramp_size + 1, &pc_val, 4);
        tramp_size += 5;
        orig_size += 5;
        continue;
      }
    }
#endif /* !SNARE_X86_64 */

    /* regular instruction copy */
    memcpy(trampoline + tramp_size, insn, insn_len);
    if (reloc > 0) {
      /* adjust relative displacement (RIP-relative or CALL/JMP rel32) */
      int32_t *disp = (int32_t *)(trampoline + tramp_size + reloc);
      intptr_t adj =
          (intptr_t)(src + orig_size) - (intptr_t)(trampoline + tramp_size);
      intptr_t new_disp = (intptr_t)*disp + adj;
#ifdef SNARE_X86_64
      /* displacement overflow check */
      if (new_disp > 0x7FFFFFFFLL || new_disp < -(intptr_t)0x80000000LL)
        return 0;
#endif
      *disp = (int32_t)new_disp;
    }
    tramp_size += insn_len;
    orig_size += insn_len;
  }
  /* record the "end" boundary so IP at the end of the region maps correctly */
  if (hook && hook->n_ips < SNARE_MAX_IPS) {
    hook->orig_ips[hook->n_ips] = (uint8_t)orig_size;
    hook->tramp_ips[hook->n_ips] = (uint8_t)tramp_size;
    hook->n_ips++;
  }
  if (hook)
    hook->orig_size = orig_size;
#ifdef SNARE_X86_64
  if (near_allocated) {
    return tramp_size +
           snare_make_rel_jmp(trampoline, src + orig_size, tramp_size);
  } else {
    snare_make_abs_jmp(trampoline + tramp_size, src + orig_size);
    return tramp_size + ABS_JMP_LEN;
  }
#else
  (void)near_allocated;
  return tramp_size +
         snare_make_rel_jmp(trampoline, src + orig_size, tramp_size);
#endif
#elif defined SNARE_ARM64
  (void)near_allocated;
  memcpy(trampoline, src, JMP_INSN_LEN);
  orig_size = JMP_INSN_LEN;
  if (hook) {
    hook->orig_size = orig_size;
    hook->n_ips = 0;
  }
  return orig_size +
         snare_make_rel_jmp(trampoline, src + JMP_INSN_LEN, orig_size);
#endif
}

SNARE_EXPORT snare_inline_t SNARE_API snare_inline_new(void *src, void *dst) {
  snare_inline_t hook;
  if ((hook = (snare_inline_t)malloc(sizeof(*hook))) == NULL)
    return NULL;
  hook->installed = 0;
  hook->src = src;
  hook->dst = dst;
  hook->orig_size = 0;
#if defined SNARE_X86 || defined SNARE_X86_64
  hook->n_ips = 0;
  memset(hook->orig_ips, 0, sizeof(hook->orig_ips));
  memset(hook->tramp_ips, 0, sizeof(hook->tramp_ips));
#endif
#ifdef SNARE_X86_64
  hook->relay_page = NULL;
  hook->relay_size = 0;
  hook->relay_jmp = NULL;
#endif
  if ((hook->code = (uint8_t *)malloc(JMP_INSN_LEN)) == NULL) {
    free(hook);
    return NULL;
  }
  memcpy(hook->code, hook->src, JMP_INSN_LEN);

#ifdef SNARE_X86_64
  {
    /* on x64, allocate trampoline (and relay) near src so rel32 works */
    size_t page_size;
#ifdef SNARE_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    page_size = si.dwPageSize;
#else
    page_size = (size_t)sysconf(_SC_PAGESIZE);
    if (page_size == 0)
      page_size = 4096;
#endif
    hook->relay_size = page_size;
    hook->relay_page = snare_alloc_near(src, page_size);
    if (hook->relay_page == NULL) {
      /* fallback: calloc trampoline (may not work for far jumps) */
      hook->trampoline = (void *)calloc(1, MAX_TRAMPOLINE_LEN);
      if (hook->trampoline == NULL) {
        free(hook->code);
        free(hook);
        return NULL;
      }
      if (snare_unprotect(hook->trampoline, MAX_TRAMPOLINE_LEN) == NULL) {
        free(hook->trampoline);
        free(hook->code);
        free(hook);
        return NULL;
      }
    } else {
      memset(hook->relay_page, 0, page_size);
      /* trampoline is at the start of the relay page */
      hook->trampoline = hook->relay_page;
      /* if dst is too far for rel32 from src, set up an abs relay jump */
      if (!snare_in_rel32_range(src, dst)) {
        hook->relay_jmp = (uint8_t *)hook->relay_page + MAX_TRAMPOLINE_LEN;
        snare_make_abs_jmp(hook->relay_jmp, dst);
      }
    }
  }
#else
  /* x86 / arm64: simple allocation */
  if ((hook->trampoline = (void *)calloc(1, MAX_TRAMPOLINE_LEN)) == NULL) {
    free(hook->code);
    free(hook);
    return NULL;
  }
  if (snare_unprotect(hook->trampoline, MAX_TRAMPOLINE_LEN) == NULL) {
    free(hook->trampoline);
    free(hook->code);
    free(hook);
    return NULL;
  }
#endif

  if (snare_unprotect(hook->src, JMP_INSN_LEN) == NULL) {
#ifdef SNARE_X86_64
    if (hook->relay_page)
      snare_free_near(hook->relay_page, hook->relay_size);
    else
      free(hook->trampoline);
#else
    free(hook->trampoline);
#endif
    free(hook->code);
    free(hook);
    return NULL;
  }

  {
    int is_near = 0;
#ifdef SNARE_X86_64
    is_near = (hook->relay_page != NULL);
#endif
    if (snare_make_trampoline((uint8_t *)hook->trampoline, (uint8_t *)hook->src,
                              is_near, hook) == 0) {
      hook->trampoline = NULL;
    }
  }
#if defined SNARE_ARM64 && defined SNARE_MACOS
  if (hook->trampoline) {
    sys_icache_invalidate(hook->trampoline, MAX_TRAMPOLINE_LEN);
  }
#endif
  return hook;
}
SNARE_EXPORT void SNARE_API snare_inline_free(snare_inline_t hook) {
  if (hook == NULL)
    return;
#ifdef SNARE_X86_64
  if (hook->relay_page)
    snare_free_near(hook->relay_page, hook->relay_size);
  else
    free(hook->trampoline);
#else
  free(hook->trampoline);
#endif
  free(hook->code);
  free(hook);
}
SNARE_EXPORT void *SNARE_API snare_inline_get_trampoline(snare_inline_t hook) {
  return hook->trampoline;
}
SNARE_EXPORT void *SNARE_API snare_inline_get_src(snare_inline_t hook) {
  return hook->src;
}
SNARE_EXPORT void *SNARE_API snare_inline_get_dst(snare_inline_t hook) {
  return hook->dst;
}

SNARE_EXPORT int SNARE_API snare_inline_is_installed(snare_inline_t hook) {
  return hook->installed;
}

/* Thread safety: freeze / thaw other threads during patch */
#ifdef SNARE_WINDOWS
typedef struct {
  HANDLE *handles;
  size_t count;
  size_t capacity;
  DWORD *deferred_tids; /* freed in thaw to avoid heap-lock deadlock */
} snare_frozen_threads_t;

static void snare_freeze_threads(snare_frozen_threads_t *ft) {
  HANDLE hSnapshot;
  THREADENTRY32 te;
  DWORD pid = GetCurrentProcessId();
  DWORD tid = GetCurrentThreadId();

  DWORD *tids = NULL;
  size_t tid_count = 0, tid_cap = 0;

  ft->handles = NULL;
  ft->count = 0;
  ft->capacity = 0;
  ft->deferred_tids = NULL;

  hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE)
    return;

  te.dwSize = sizeof(te);
  if (Thread32First(hSnapshot, &te)) {
    do {
      if (te.th32OwnerProcessID == pid && te.th32ThreadID != tid) {
        if (tid_count >= tid_cap) {
          size_t new_cap = tid_cap ? tid_cap * 2 : 64;
          DWORD *buf = (DWORD *)realloc(tids, new_cap * sizeof(DWORD));
          if (!buf)
            continue;
          tids = buf;
          tid_cap = new_cap;
        }
        tids[tid_count++] = te.th32ThreadID;
      }
    } while (Thread32Next(hSnapshot, &te));
  }
  CloseHandle(hSnapshot);

  if (!tid_count) {
    free(tids);
    return;
  }

  ft->handles = (HANDLE *)malloc(tid_count * sizeof(HANDLE));
  if (!ft->handles) {
    free(tids);
    return;
  }
  ft->capacity = tid_count;

  for (size_t i = 0; i < tid_count; i++) {
    HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT |
                                    THREAD_SET_CONTEXT,
                                FALSE, tids[i]);
    if (hThread) {
      if (SuspendThread(hThread) != (DWORD)-1)
        ft->handles[ft->count++] = hThread;
      else
        CloseHandle(hThread);
    }
  }
  /* defer free until after thaw. calling free() while threads are suspended
   * deadlocks if a suspended thread holds the CRT heap lock */
  ft->deferred_tids = tids;
}

static void snare_thaw_threads(snare_frozen_threads_t *ft) {
  size_t i;
  for (i = 0; i < ft->count; i++) {
    ResumeThread(ft->handles[i]);
    CloseHandle(ft->handles[i]);
  }
  free(ft->handles);
  free(ft->deferred_tids);
  ft->handles = NULL;
  ft->deferred_tids = NULL;
  ft->count = 0;
  ft->capacity = 0;
}
/* adjust frozen threads whose IP falls within patched region */
static void snare_adjust_threads_ip(snare_frozen_threads_t *ft,
                                    uintptr_t from_base, uintptr_t to_base,
                                    uint8_t *from_ips, uint8_t *to_ips,
                                    uint8_t n_ips) {
  size_t i;
  uint8_t j;
  for (i = 0; i < ft->count; i++) {
    CONTEXT ctx;
    ctx.ContextFlags = CONTEXT_CONTROL;
    if (!GetThreadContext(ft->handles[i], &ctx))
      continue;
#if defined SNARE_X86_64 || defined _M_AMD64
    uintptr_t ip = (uintptr_t)ctx.Rip;
#else
    uintptr_t ip = (uintptr_t)ctx.Eip;
#endif
    for (j = 0; j < n_ips; j++) {
      if (ip == from_base + from_ips[j]) {
#if defined SNARE_X86_64 || defined _M_AMD64
        ctx.Rip = (DWORD64)(to_base + to_ips[j]);
#else
        ctx.Eip = (DWORD)(to_base + to_ips[j]);
#endif
        SetThreadContext(ft->handles[i], &ctx);
        break;
      }
    }
  }
}
#endif /* SNARE_WINDOWS */

/* internal: write the hook jump (shared by install and batch) */
static void snare_write_hook_jmp(snare_inline_t hook) {
#ifdef SNARE_X86_64
  if (hook->relay_jmp) {
    snare_make_rel_jmp((uint8_t *)hook->src, (uint8_t *)hook->relay_jmp, 0);
  } else {
    snare_make_rel_jmp((uint8_t *)hook->src, (uint8_t *)hook->dst, 0);
  }
#else
  snare_make_rel_jmp((uint8_t *)hook->src, (uint8_t *)hook->dst, 0);
#endif
}

SNARE_EXPORT int SNARE_API snare_inline_install(snare_inline_t hook) {
  if (hook->installed)
    return -EINVAL;
  if (!hook->trampoline) {
    return -EINVAL;
  }

#ifdef SNARE_WINDOWS
  snare_frozen_threads_t ft;
  snare_freeze_threads(&ft);
#if defined SNARE_X86 || defined SNARE_X86_64
  snare_adjust_threads_ip(&ft, (uintptr_t)hook->src,
                          (uintptr_t)hook->trampoline, hook->orig_ips,
                          hook->tramp_ips, hook->n_ips);
#endif
#endif

  snare_write_hook_jmp(hook);
  hook->installed = 1;

#ifdef SNARE_WINDOWS
  FlushInstructionCache(GetCurrentProcess(), hook->src, JMP_INSN_LEN);
  snare_thaw_threads(&ft);
#endif
#if defined SNARE_ARM64 && defined SNARE_MACOS
  sys_icache_invalidate(hook->src, JMP_INSN_LEN);
#endif
  return 0;
}
SNARE_EXPORT int SNARE_API snare_inline_remove(snare_inline_t hook) {
  if (!hook->installed)
    return -EINVAL;

#ifdef SNARE_WINDOWS
  snare_frozen_threads_t ft;
  snare_freeze_threads(&ft);
#if defined SNARE_X86 || defined SNARE_X86_64
  snare_adjust_threads_ip(&ft, (uintptr_t)hook->trampoline,
                          (uintptr_t)hook->src, hook->tramp_ips, hook->orig_ips,
                          hook->n_ips);
#endif
#endif

  memcpy(hook->src, hook->code, JMP_INSN_LEN);
  hook->installed = 0;

#ifdef SNARE_WINDOWS
  FlushInstructionCache(GetCurrentProcess(), hook->src, JMP_INSN_LEN);
  snare_thaw_threads(&ft);
#endif
  return 0;
}

/* Batch install/remove: single freeze/thaw for multiple hooks */
SNARE_EXPORT int SNARE_API snare_inline_install_batch(snare_inline_t *hooks,
                                                      int count) {
  int i, installed = 0;
#ifdef SNARE_WINDOWS
  snare_frozen_threads_t ft;
  snare_freeze_threads(&ft);
  for (i = 0; i < count; i++) {
    if (!hooks[i] || hooks[i]->installed)
      continue;
#if defined SNARE_X86 || defined SNARE_X86_64
    snare_adjust_threads_ip(&ft, (uintptr_t)hooks[i]->src,
                            (uintptr_t)hooks[i]->trampoline, hooks[i]->orig_ips,
                            hooks[i]->tramp_ips, hooks[i]->n_ips);
#endif
    snare_write_hook_jmp(hooks[i]);
    hooks[i]->installed = 1;
    FlushInstructionCache(GetCurrentProcess(), hooks[i]->src, JMP_INSN_LEN);
    installed++;
  }
  snare_thaw_threads(&ft);
#else
  for (i = 0; i < count; i++) {
    if (!hooks[i] || hooks[i]->installed)
      continue;
    snare_write_hook_jmp(hooks[i]);
    hooks[i]->installed = 1;
    installed++;
  }
#endif
  return installed;
}

SNARE_EXPORT int SNARE_API snare_inline_remove_batch(snare_inline_t *hooks,
                                                     int count) {
  int i, removed = 0;
#ifdef SNARE_WINDOWS
  snare_frozen_threads_t ft;
  snare_freeze_threads(&ft);
  for (i = 0; i < count; i++) {
    if (!hooks[i] || !hooks[i]->installed)
      continue;
#if defined SNARE_X86 || defined SNARE_X86_64
    snare_adjust_threads_ip(&ft, (uintptr_t)hooks[i]->trampoline,
                            (uintptr_t)hooks[i]->src, hooks[i]->tramp_ips,
                            hooks[i]->orig_ips, hooks[i]->n_ips);
#endif
    memcpy(hooks[i]->src, hooks[i]->code, JMP_INSN_LEN);
    hooks[i]->installed = 0;
    FlushInstructionCache(GetCurrentProcess(), hooks[i]->src, JMP_INSN_LEN);
    removed++;
  }
  snare_thaw_threads(&ft);
#else
  for (i = 0; i < count; i++) {
    if (!hooks[i] || !hooks[i]->installed)
      continue;
    memcpy(hooks[i]->src, hooks[i]->code, JMP_INSN_LEN);
    hooks[i]->installed = 0;
    removed++;
  }
#endif
  return removed;
}
SNARE_EXPORT void *SNARE_API snare_inline_read_dst(void *src) {
#if defined SNARE_X86 || defined SNARE_X86_64
  struct snare_jmp_rel *maybe_jmp = (struct snare_jmp_rel *)src;
  if (maybe_jmp->opcode != JMP_INSN_OPCODE)
    return NULL;
  return (void *)(maybe_jmp->offset + (uint8_t *)src + JMP_INSN_LEN);
#elif defined SNARE_ARM64
  struct snare_jmp_rel *maybe_jmp = (struct snare_jmp_rel *)src;
  if (maybe_jmp->ldr != 0x58000050 || maybe_jmp->br != 0xD61F0200)
    return NULL;
  return (void *)maybe_jmp->addr;
#endif
}
#if defined SNARE_LINUX
#include <dlfcn.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/mman.h>
#ifdef __sun
#include <procfs.h>
#include <sys/auxv.h>
#define ELF_TARGET_ALL
#endif /* __sun */
#ifdef __FreeBSD__
#include <libutil.h>
#include <sys/types.h>
#include <sys/user.h>
#endif
#include <elf.h>
#include <link.h>

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
#elif 0 /* disabled because not tested */ && (defined __s390__)
#define R_JUMP_SLOT R_390_JMP_SLOT
#define R_GLOBAL_DATA R_390_GLOB_DAT
#elif defined __s390x__
#define R_JUMP_SLOT R_390_JMP_SLOT
#define R_GLOBAL_DATA R_390_GLOB_DAT
#elif defined __loongarch64
#define R_JUMP_SLOT R_LARCH_JUMP_SLOT
#define R_GLOBAL_DATA R_LARCH_64
#elif 0 /* disabled because not working, needs to patch the GOT instead of PLT \
         */                                                                    \
    && defined __mips__
#define R_JUMP_SLOT R_MIPS_JUMP_SLOT
#define R_GLOBAL_DATA R_MIPS_GLOB_DAT
#elif 0 /* disabled because not tested */ &&                                   \
    (defined __sparc || defined __sparc__)
#define R_JUMP_SLOT R_SPARC_JMP_SLOT
#define R_GLOBAL_DATA R_SPARC_GLOB_DAT
#elif 0 /* disabled because not tested */ &&                                   \
    (defined __sparcv9 || defined __sparc_v9__)
#define R_JUMP_SLOT R_SPARC_JMP_SLOT
#define R_GLOBAL_DATA R_SPARC_GLOB_DAT
#elif 0 /* disabled because not tested */ &&                                   \
    (defined __ia64 || defined __ia64__)
#define R_JUMP_SLOT R_IA64_IPLTMSB
#else
#error unsupported OS
#endif

#ifdef USE_REL
#define Snare_Elf_Plt_Rel Snare_Elf_Rel
#define SNARE_PLT_PLT_DT_REL DT_REL
#define SNARE_PLT_PLT_DT_RELSZ DT_RELSZ
#define SNARE_PLT_PLT_DT_RELENT DT_RELENT
#else
#define Snare_Elf_Plt_Rel Snare_Elf_Rela
#define SNARE_PLT_PLT_DT_REL DT_RELA
#define SNARE_PLT_PLT_DT_RELSZ DT_RELASZ
#define SNARE_PLT_PLT_DT_RELENT DT_RELAENT
#endif

#if defined __LP64__
#ifndef ELF_CLASS
#define ELF_CLASS ELFCLASS64
#endif
#define SNARE_PLT_SIZE_T_FMT "lu"
#define SNARE_PLT_ELF_WORD_FMT "u"
#ifdef __ANDROID__
#define SNARE_PLT_ELF_XWORD_FMT "llu"
#else
#define SNARE_PLT_ELF_XWORD_FMT "lu"
#endif
#define SNARE_PLT_ELF_SXWORD_FMT "ld"
#define Snare_Elf_Half Elf64_Half
#define Snare_Elf_Xword Elf64_Xword
#define Snare_Elf_Sxword Elf64_Sxword
#define Snare_Elf_Ehdr Elf64_Ehdr
#define Snare_Elf_Phdr Elf64_Phdr
#define Snare_Elf_Sym Elf64_Sym
#define Snare_Elf_Dyn Elf64_Dyn
#define Snare_Elf_Rel Elf64_Rel
#define Snare_Elf_Rela Elf64_Rela
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
#define SNARE_PLT_SIZE_T_FMT "u"
#ifdef __sun
#define SNARE_PLT_ELF_WORD_FMT "lu"
#define SNARE_PLT_ELF_XWORD_FMT "lu"
#define SNARE_PLT_ELF_SXWORD_FMT "ld"
#else
#define SNARE_PLT_ELF_WORD_FMT "u"
#define SNARE_PLT_ELF_XWORD_FMT "u"
#define SNARE_PLT_ELF_SXWORD_FMT "d"
#endif
#define Snare_Elf_Half Elf32_Half
#define Snare_Elf_Xword Elf32_Word
#define Snare_Elf_Sxword Elf32_Sword
#define Snare_Elf_Ehdr Elf32_Ehdr
#define Snare_Elf_Phdr Elf32_Phdr
#define Snare_Elf_Sym Elf32_Sym
#define Snare_Elf_Dyn Elf32_Dyn
#define Snare_Elf_Rel Elf32_Rel
#define Snare_Elf_Rela Elf32_Rela
#ifndef ELF_R_SYM
#define ELF_R_SYM ELF32_R_SYM
#endif
#ifndef ELF_R_TYPE
#define ELF_R_TYPE ELF32_R_TYPE
#endif
#endif /* __LP64__ */

#ifdef SNARE_PLT_DEBUG
#define SNARE_PLT_DEBUG_MSG(...) fprintf(stderr, __VA_ARGS__)
#else
#define SNARE_PLT_DEBUG_MSG(...)
#endif
typedef struct snare_plt_mem_prot {
  size_t start;
  size_t end;
  int prot;
} snare_plt_mem_prot_t;

#define SNARE_PLT_NUM_MEM_PROT 20
struct snare_plt_s {
  const Snare_Elf_Sym *dynsym;
  const char *dynstr;
  size_t dynstr_size;
  const char *plt_addr_base;
  const Snare_Elf_Plt_Rel *rela_plt;
  size_t rela_plt_cnt;
#ifdef R_GLOBAL_DATA
  const Snare_Elf_Plt_Rel *rela_dyn;
  size_t rela_dyn_cnt;
#endif
  snare_plt_mem_prot_t mem_prot[SNARE_PLT_NUM_MEM_PROT];
};
static char snare_plt_errmsg[512];
static size_t snare_plt_page_size;
#define SNARE_PLT_ALIGN_ADDR(addr)                                             \
  ((void *)((size_t)(addr) & ~(snare_plt_page_size - 1)))

static int snare_plt_open_executable(snare_plt_t **plthook_out);
static int snare_plt_open_shared_library(snare_plt_t **plthook_out,
                                         const char *filename);
static const Snare_Elf_Dyn *snare_plt_find_dyn_by_tag(const Snare_Elf_Dyn *dyn,
                                                      Snare_Elf_Sxword tag);

typedef struct snare_plt_mem_prot_iter snare_plt_mem_prot_iter_t;
static int snare_plt_mem_prot_begin(snare_plt_mem_prot_iter_t *iter);
static int snare_plt_mem_prot_next(snare_plt_mem_prot_iter_t *iter,
                                   snare_plt_mem_prot_t *mem_prot);
static void snare_plt_mem_prot_end(snare_plt_mem_prot_iter_t *iter);

static int snare_plt_open_real(snare_plt_t **plthook_out,
                               struct link_map *lmap);
static int snare_plt_set_mem_prot(snare_plt_t *plthook);
static int snare_plt_get_mem_prot(snare_plt_t *plthook, void *addr);
#if defined __FreeBSD__ || defined __sun
static int snare_plt_check_elf_header(const Snare_Elf_Ehdr *ehdr);
#endif
static void snare_plt_set_errmsg(const char *fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)));

#if defined __ANDROID__ || defined __UCLIBC__
struct snare_plt_dl_iterate_data {
  char *addr;
  struct link_map lmap;
};
static int snare_plt_dl_iterate_cb(struct dl_phdr_info *info, size_t size,
                                   void *cb_data) {
  struct snare_plt_dl_iterate_data *data =
      (struct snare_plt_dl_iterate_data *)cb_data;
  Snare_Elf_Half idx = 0;

  for (idx = 0; idx < info->dlpi_phnum; ++idx) {
    const Snare_Elf_Phdr *phdr = &info->dlpi_phdr[idx];
    char *base = (char *)info->dlpi_addr + phdr->p_vaddr;
    if (base <= data->addr && data->addr < base + phdr->p_memsz) {
      break;
    }
  }
  if (idx == info->dlpi_phnum) {
    return 0;
  }
  for (idx = 0; idx < info->dlpi_phnum; ++idx) {
    const Snare_Elf_Phdr *phdr = &info->dlpi_phdr[idx];
    if (phdr->p_type == PT_DYNAMIC) {
      data->lmap.l_addr = info->dlpi_addr;
      data->lmap.l_ld = (Snare_Elf_Dyn *)(info->dlpi_addr + phdr->p_vaddr);
      return 1;
    }
  }
  return 0;
}
#endif
#if defined __ANDROID__
/* Callback to find the main executable on Android.
 * Uses /proc/self/exe to get the executable path, then matches it
 * against dl_iterate_phdr entries. This is the approach used by
 * Google Sanitizers (ASan) and is portable across different libc
 * implementations (glibc, bionic, musl).
 */
struct snare_plt_dl_iterate_exe_data {
  struct link_map lmap;
  char exe_path[PATH_MAX];
  size_t exe_path_len;
  int found;
};
static int
snare_plt_dl_iterate_exe_check(const char *name,
                               struct snare_plt_dl_iterate_exe_data *data) {
  if (name == NULL || name[0] == '\0') {
    return 1;
  }

  if (strncmp(name, data->exe_path, data->exe_path_len) == 0) {
    return 1;
  }

  return 0;
}
static int snare_plt_dl_iterate_exe_cb(struct dl_phdr_info *info, size_t size,
                                       void *cb_data) {
  struct snare_plt_dl_iterate_exe_data *data =
      (struct snare_plt_dl_iterate_exe_data *)cb_data;
  Snare_Elf_Half idx;
  if (!snare_plt_dl_iterate_exe_check(info->dlpi_name, data)) {
    return 0;
  }
  /* Found the main executable, extract its link_map info */
  for (idx = 0; idx < info->dlpi_phnum; ++idx) {
    const Snare_Elf_Phdr *phdr = &info->dlpi_phdr[idx];
    if (phdr->p_type == PT_DYNAMIC) {
      data->lmap.l_addr = info->dlpi_addr;
      data->lmap.l_ld = (Snare_Elf_Dyn *)(info->dlpi_addr + phdr->p_vaddr);
      data->found = 1;
      return 1;
    }
  }
  return 0;
}

struct snare_plt_dl_iterate_handle_data {
  void *target_handle;
  void *base_addr;
};

static int snare_plt_dl_iterate_handle_cb(struct dl_phdr_info *info,
                                          size_t size, void *data) {
  struct snare_plt_dl_iterate_handle_data *handle_data =
      (struct snare_plt_dl_iterate_handle_data *)data;
  void *handle;
  if (info->dlpi_name == NULL || info->dlpi_name[0] == '\0')
    return 0;
  handle = dlopen(info->dlpi_name, RTLD_NOLOAD);
  if (handle == NULL)
    return 0;
  if (handle == handle_data->target_handle) {
    handle_data->base_addr = (void *)info->dlpi_addr;
    dlclose(handle);
    return 1;
  }
  dlclose(handle);
  return 0;
}
#endif
SNARE_EXPORT int SNARE_API snare_plt_open(snare_plt_t **plthook_out,
                                          const char *filename) {
  *plthook_out = NULL;
  if (filename == NULL) {
    return snare_plt_open_executable(plthook_out);
  } else {
    return snare_plt_open_shared_library(plthook_out, filename);
  }
}

SNARE_EXPORT int SNARE_API snare_plt_open_by_handle(snare_plt_t **plthook_out,
                                                    void *hndl) {
#if defined __ANDROID__
  struct snare_plt_dl_iterate_handle_data handle_data = {0};
  if (hndl == NULL) {
    snare_plt_set_errmsg("NULL handle");
    return SNARE_PLT_FILE_NOT_FOUND;
  }
  handle_data.target_handle = hndl;
  handle_data.base_addr = NULL;
  dl_iterate_phdr(snare_plt_dl_iterate_handle_cb, &handle_data);
  if (handle_data.base_addr == NULL) {
    snare_plt_set_errmsg("Could not find base address for handle.");
    return SNARE_PLT_INTERNAL_ERROR;
  }
  return snare_plt_open_by_address(plthook_out, handle_data.base_addr);
#elif defined __UCLIBC__
  const static char *symbols[] = {"__INIT_ARRAY__", "_end", "_start"};
  size_t i;
  if (hndl == NULL) {
    snare_plt_set_errmsg("NULL handle");
    return SNARE_PLT_FILE_NOT_FOUND;
  }
  for (i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
    char *addr = dlsym(hndl, symbols[i]);
    if (addr != NULL) {
      return snare_plt_open_by_address(plthook_out, addr - 1);
    }
  }
  snare_plt_set_errmsg("Could not find an address in the specified handle.");
  return SNARE_PLT_INTERNAL_ERROR;
#else
  struct link_map *lmap = NULL;

  if (hndl == NULL) {
    snare_plt_set_errmsg("NULL handle");
    return SNARE_PLT_FILE_NOT_FOUND;
  }
  if (dlinfo(hndl, RTLD_DI_LINKMAP, &lmap) != 0) {
    snare_plt_set_errmsg("dlinfo error");
    return SNARE_PLT_FILE_NOT_FOUND;
  }
  return snare_plt_open_real(plthook_out, lmap);
#endif
}
SNARE_EXPORT int SNARE_API snare_plt_open_by_address(snare_plt_t **plthook_out,
                                                     void *address) {
#if defined __FreeBSD__
  return SNARE_PLT_NOT_IMPLEMENTED;
#elif defined __ANDROID__ || defined __UCLIBC__
  struct snare_plt_dl_iterate_data data = {
      0,
  };
  data.addr = address;
  dl_iterate_phdr(snare_plt_dl_iterate_cb, &data);
  if (data.lmap.l_ld == NULL) {
    snare_plt_set_errmsg("Could not find memory region containing address %p",
                         address);
    return SNARE_PLT_INTERNAL_ERROR;
  }
  return snare_plt_open_real(plthook_out, &data.lmap);
#else
  Dl_info info;
  union {
    struct link_map *lmap;
    void *ptr;
  } addr = {NULL};

  *plthook_out = NULL;
  if (dladdr1(address, &info, (void **)(&addr.ptr), RTLD_DL_LINKMAP) == 0) {
    snare_plt_set_errmsg("dladdr error");
    return SNARE_PLT_FILE_NOT_FOUND;
  }
  return snare_plt_open_real(plthook_out, addr.lmap);
#endif
}
static int snare_plt_open_executable(snare_plt_t **plthook_out) {
#if defined __ANDROID__
  /* On Android, dlsym cannot find symbols like __INIT_ARRAY__, _end, _start
   * in the main executable. Use /proc/self/exe to get the executable path,
   * then find it via dl_iterate_phdr. */
  struct snare_plt_dl_iterate_exe_data data = {0};
  ssize_t len = readlink("/proc/self/exe", data.exe_path, PATH_MAX - 1);
  if (len <= 0) {
    snare_plt_set_errmsg("Could not open executable: %s", strerror(errno));
    return SNARE_PLT_INTERNAL_ERROR;
  }
  data.exe_path[len] = '\0';
  data.exe_path_len = (size_t)len;
  dl_iterate_phdr(snare_plt_dl_iterate_exe_cb, &data);
  if (!data.found) {
    snare_plt_set_errmsg("Could not find executable map of %s", data.exe_path);
    return SNARE_PLT_INTERNAL_ERROR;
  }
  return snare_plt_open_real(plthook_out, &data.lmap);
#elif defined __UCLIBC__
  return snare_plt_open_shared_library(plthook_out, NULL);
#elif defined __linux__
  return snare_plt_open_real(plthook_out, _r_debug.r_map);
#elif defined __sun
  const char *auxv_file = "/proc/self/auxv";
#define NUM_AUXV_CNT 10
  FILE *fp = fopen(auxv_file, "r");
  auxv_t auxv;
  struct r_debug *r_debug = NULL;

  if (fp == NULL) {
    snare_plt_set_errmsg("Could not open %s: %s", auxv_file, strerror(errno));
    return SNARE_PLT_INTERNAL_ERROR;
  }
  while (fread(&auxv, sizeof(auxv_t), 1, fp) == 1) {
    if (auxv.a_type == AT_SUN_LDDATA) {
      r_debug = (struct r_debug *)auxv.a_un.a_ptr;
      break;
    }
  }
  fclose(fp);
  if (r_debug == NULL) {
    snare_plt_set_errmsg("Could not find r_debug");
    return SNARE_PLT_INTERNAL_ERROR;
  }
  return snare_plt_open_real(plthook_out, r_debug->r_map);
#elif defined __FreeBSD__
  return snare_plt_open_shared_library(plthook_out, NULL);
#else
  snare_plt_set_errmsg(
      "Opening the main program is not supported on this platform.");
  return SNARE_PLT_NOT_IMPLEMENTED;
#endif
}

static int snare_plt_open_shared_library(snare_plt_t **plthook_out,
                                         const char *filename) {
  void *hndl = dlopen(filename, RTLD_LAZY | RTLD_NOLOAD);
#if defined __ANDROID__ || defined __UCLIBC__
  int rv;
#else
  struct link_map *lmap = NULL;
#endif

  if (hndl == NULL) {
    snare_plt_set_errmsg("dlopen error: %s", dlerror());
    return SNARE_PLT_FILE_NOT_FOUND;
  }
#if defined __ANDROID__ || defined __UCLIBC__
  rv = snare_plt_open_by_handle(plthook_out, hndl);
  dlclose(hndl);
  return rv;
#else
  if (dlinfo(hndl, RTLD_DI_LINKMAP, &lmap) != 0) {
    snare_plt_set_errmsg("dlinfo error");
    dlclose(hndl);
    return SNARE_PLT_FILE_NOT_FOUND;
  }
  dlclose(hndl);
  return snare_plt_open_real(plthook_out, lmap);
#endif
}
static const Snare_Elf_Dyn *snare_plt_find_dyn_by_tag(const Snare_Elf_Dyn *dyn,
                                                      Snare_Elf_Sxword tag) {
  while (dyn->d_tag != DT_NULL) {
    if (dyn->d_tag == tag) {
      return dyn;
    }
    dyn++;
  }
  return NULL;
}

#ifdef __linux__
struct snare_plt_mem_prot_iter {
  FILE *fp;
};
static int snare_plt_mem_prot_begin(snare_plt_mem_prot_iter_t *iter) {
  iter->fp = fopen("/proc/self/maps", "r");
  if (iter->fp == NULL) {
    snare_plt_set_errmsg("failed to open /proc/self/maps");
    return -1;
  }
  return 0;
}

static int snare_plt_mem_prot_next(snare_plt_mem_prot_iter_t *iter,
                                   snare_plt_mem_prot_t *mem_prot) {
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

static void snare_plt_mem_prot_end(snare_plt_mem_prot_iter_t *iter) {
  if (iter->fp != NULL) {
    fclose(iter->fp);
  }
}
#elif defined __FreeBSD__
struct snare_plt_mem_prot_iter {
  struct kinfo_vmentry *kve;
  int idx;
  int num;
};

static int snare_plt_mem_prot_begin(snare_plt_mem_prot_iter_t *iter) {
  iter->kve = kinfo_getvmmap(getpid(), &iter->num);
  if (iter->kve == NULL) {
    snare_plt_set_errmsg("failed to call kinfo_getvmmap()\n");
    return -1;
  }
  iter->idx = 0;
  return 0;
}

static int snare_plt_mem_prot_next(snare_plt_mem_prot_iter_t *iter,
                                   snare_plt_mem_prot_t *mem_prot) {
  if (iter->idx >= iter->num) {
    return -1;
  }
  struct kinfo_vmentry *kve = &iter->kve[iter->idx++];
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

static void snare_plt_mem_prot_end(snare_plt_mem_prot_iter_t *iter) {
  if (iter->kve != NULL) {
    free(iter->kve);
  }
}
#elif defined(__sun)
struct snare_plt_mem_prot_iter {
  FILE *fp;
  prmap_t maps[20];
  size_t idx;
  size_t num;
};

static int snare_plt_mem_prot_begin(snare_plt_mem_prot_iter_t *iter) {
  iter->fp = fopen("/proc/self/map", "r");
  if (iter->fp == NULL) {
    snare_plt_set_errmsg("failed to open /proc/self/map");
    return -1;
  }
  iter->idx = iter->num = 0;
  return 0;
}

static int snare_plt_mem_prot_next(snare_plt_mem_prot_iter_t *iter,
                                   snare_plt_mem_prot_t *mem_prot) {
  prmap_t *map;
  if (iter->idx == iter->num) {
    iter->num = fread(iter->maps, sizeof(iter->maps[0]),
                      sizeof(iter->maps) / sizeof(iter->maps[0]), iter->fp);
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

static void snare_plt_mem_prot_end(snare_plt_mem_prot_iter_t *iter) {
  if (iter->fp != NULL) {
    fclose(iter->fp);
  }
}
#else
#error Unsupported platform
#endif

static int snare_plt_open_real(snare_plt_t **plthook_out,
                               struct link_map *lmap) {
  snare_plt_t plthook = {
      NULL,
  };
  const Snare_Elf_Dyn *dyn;
  const char *dyn_addr_base = NULL;
  if (snare_plt_page_size == 0) {
    snare_plt_page_size = sysconf(_SC_PAGESIZE);
  }

#if defined __linux__
  plthook.plt_addr_base = (char *)lmap->l_addr;
#if defined __riscv
  const Snare_Elf_Ehdr *ehdr = (const Snare_Elf_Ehdr *)lmap->l_addr;
  if (ehdr->e_type == ET_DYN) {
    dyn_addr_base = (const char *)lmap->l_addr;
  }
#endif
#if defined __ANDROID__ || defined __UCLIBC__
  dyn_addr_base = (const char *)lmap->l_addr;
#endif
#elif defined __FreeBSD__ || defined __sun
#if __FreeBSD__ >= 13
  const Snare_Elf_Ehdr *ehdr = (const Snare_Elf_Ehdr *)lmap->l_base;
#else
  const Snare_Elf_Ehdr *ehdr = (const Snare_Elf_Ehdr *)lmap->l_addr;
#endif
  int rv_ = snare_plt_check_elf_header(ehdr);
  if (rv_ != 0) {
    return rv_;
  }
  if (ehdr->e_type == ET_DYN) {
    dyn_addr_base = (const char *)lmap->l_addr;
    plthook.plt_addr_base = (const char *)lmap->l_addr;
  }
#else
#error unsupported OS
#endif

  /* get .dynsym section */
  dyn = snare_plt_find_dyn_by_tag(lmap->l_ld, DT_SYMTAB);
  if (dyn == NULL) {
    snare_plt_set_errmsg("failed to find DT_SYMTAB");
    return SNARE_PLT_INTERNAL_ERROR;
  }
  plthook.dynsym = (const Snare_Elf_Sym *)(dyn_addr_base + dyn->d_un.d_ptr);

  /* Check sizeof(Snare_Elf_Sym) */
  dyn = snare_plt_find_dyn_by_tag(lmap->l_ld, DT_SYMENT);
  if (dyn == NULL) {
    snare_plt_set_errmsg("failed to find DT_SYMTAB");
    return SNARE_PLT_INTERNAL_ERROR;
  }
  if (dyn->d_un.d_val != sizeof(Snare_Elf_Sym)) {
    snare_plt_set_errmsg("DT_SYMENT size %" SNARE_PLT_ELF_XWORD_FMT
                         " != %" SNARE_PLT_SIZE_T_FMT,
                         dyn->d_un.d_val, sizeof(Snare_Elf_Sym));
    return SNARE_PLT_INTERNAL_ERROR;
  }

  /* Get .dynstr section */
  dyn = snare_plt_find_dyn_by_tag(lmap->l_ld, DT_STRTAB);
  if (dyn == NULL) {
    snare_plt_set_errmsg("failed to find DT_STRTAB");
    return SNARE_PLT_INTERNAL_ERROR;
  }
  plthook.dynstr = dyn_addr_base + dyn->d_un.d_ptr;

  /* Get .dynstr size */
  dyn = snare_plt_find_dyn_by_tag(lmap->l_ld, DT_STRSZ);
  if (dyn == NULL) {
    snare_plt_set_errmsg("failed to find DT_STRSZ");
    return SNARE_PLT_INTERNAL_ERROR;
  }
  plthook.dynstr_size = dyn->d_un.d_val;

  /* Get .rela.plt or .rel.plt section */
  dyn = snare_plt_find_dyn_by_tag(lmap->l_ld, DT_JMPREL);
  if (dyn != NULL) {
    plthook.rela_plt =
        (const Snare_Elf_Plt_Rel *)(dyn_addr_base + dyn->d_un.d_ptr);
    dyn = snare_plt_find_dyn_by_tag(lmap->l_ld, DT_PLTRELSZ);
    if (dyn == NULL) {
      snare_plt_set_errmsg("failed to find DT_PLTRELSZ");
      return SNARE_PLT_INTERNAL_ERROR;
    }
    plthook.rela_plt_cnt = dyn->d_un.d_val / sizeof(Snare_Elf_Plt_Rel);
  }
#ifdef R_GLOBAL_DATA
  /* Get .rela.dyn or .rel.dyn section */
  dyn = snare_plt_find_dyn_by_tag(lmap->l_ld, SNARE_PLT_PLT_DT_REL);
  if (dyn != NULL) {
    size_t total_size, elem_size;

    plthook.rela_dyn =
        (const Snare_Elf_Plt_Rel *)(dyn_addr_base + dyn->d_un.d_ptr);
    dyn = snare_plt_find_dyn_by_tag(lmap->l_ld, SNARE_PLT_PLT_DT_RELSZ);
    if (dyn == NULL) {
      snare_plt_set_errmsg("failed to find PLT_DT_RELSZ");
      return SNARE_PLT_INTERNAL_ERROR;
    }
    total_size = dyn->d_un.d_val;

    dyn = snare_plt_find_dyn_by_tag(lmap->l_ld, SNARE_PLT_PLT_DT_RELENT);
    if (dyn == NULL) {
      snare_plt_set_errmsg("failed to find PLT_DT_RELENT");
      return SNARE_PLT_INTERNAL_ERROR;
    }
    elem_size = dyn->d_un.d_val;
    plthook.rela_dyn_cnt = total_size / elem_size;
  }
#endif

#ifdef R_GLOBAL_DATA
  if (plthook.rela_plt == NULL && plthook.rela_dyn == NULL) {
    snare_plt_set_errmsg("failed to find either of DT_JMPREL and DT_REL");
    return SNARE_PLT_INTERNAL_ERROR;
  }
#else
  if (plthook.rela_plt == NULL) {
    snare_plt_set_errmsg("failed to find DT_JMPREL");
    return SNARE_PLT_INTERNAL_ERROR;
  }
#endif
  if (snare_plt_set_mem_prot(&plthook)) {
    return SNARE_PLT_INTERNAL_ERROR;
  }

  *plthook_out = (snare_plt_t *)malloc(sizeof(snare_plt_t));
  if (*plthook_out == NULL) {
    snare_plt_set_errmsg("failed to allocate memory: %" SNARE_PLT_SIZE_T_FMT
                         " bytes",
                         sizeof(snare_plt_t));
    return SNARE_PLT_OUT_OF_MEMORY;
  }
  **plthook_out = plthook;
  return 0;
}
static int snare_plt_set_mem_prot(snare_plt_t *plthook) {
  unsigned int pos = 0;
  const char *name;
  void **addr;
  size_t start = (size_t)-1;
  size_t end = 0;
  snare_plt_mem_prot_iter_t iter;
  snare_plt_mem_prot_t mem_prot;
  int idx = 0;

  while (snare_plt_enum(plthook, &pos, &name, &addr) == 0) {
    if (start > (size_t)addr) {
      start = (size_t)addr;
    }
    if (end < (size_t)addr) {
      end = (size_t)addr;
    }
  }
  end++;

  if (snare_plt_mem_prot_begin(&iter) != 0) {
    return SNARE_PLT_INTERNAL_ERROR;
  }
  while (snare_plt_mem_prot_next(&iter, &mem_prot) == 0 &&
         idx < SNARE_PLT_NUM_MEM_PROT) {
    if (mem_prot.prot != 0 && mem_prot.start < end && start < mem_prot.end) {
      plthook->mem_prot[idx++] = mem_prot;
    }
  }
  snare_plt_mem_prot_end(&iter);
  return 0;
}

static int snare_plt_get_mem_prot(snare_plt_t *plthook, void *addr) {
  snare_plt_mem_prot_t *ptr = plthook->mem_prot;
  snare_plt_mem_prot_t *end = ptr + SNARE_PLT_NUM_MEM_PROT;

  while (ptr < end && ptr->prot != 0) {
    if (ptr->start <= (size_t)addr && (size_t)addr < ptr->end) {
      return ptr->prot;
    }
    ++ptr;
  }
  return 0;
}

#if defined __FreeBSD__ || defined __sun
static int snare_plt_check_elf_header(const Snare_Elf_Ehdr *ehdr) {
  static const unsigned short s = 1;
  /* Check endianness at runtime. */
  unsigned char elfdata = (*(const char *)&s) ? ELFDATA2LSB : ELFDATA2MSB;
  if (ehdr == NULL) {
    snare_plt_set_errmsg("invalid elf header address: NULL");
    return SNARE_PLT_INTERNAL_ERROR;
  }
  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
    snare_plt_set_errmsg("invalid file signature: 0x%02x,0x%02x,0x%02x,0x%02x",
                         ehdr->e_ident[0], ehdr->e_ident[1], ehdr->e_ident[2],
                         ehdr->e_ident[3]);
    return SNARE_PLT_INVALID_FILE_FORMAT;
  }
  if (ehdr->e_ident[EI_CLASS] != ELF_CLASS) {
    snare_plt_set_errmsg("invalid elf class: 0x%02x", ehdr->e_ident[EI_CLASS]);
    return SNARE_PLT_INVALID_FILE_FORMAT;
  }
  if (ehdr->e_ident[EI_DATA] != elfdata) {
    snare_plt_set_errmsg("invalid elf data: 0x%02x", ehdr->e_ident[EI_DATA]);
    return SNARE_PLT_INVALID_FILE_FORMAT;
  }
  if (ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
    snare_plt_set_errmsg("invalid elf version: 0x%02x",
                         ehdr->e_ident[EI_VERSION]);
    return SNARE_PLT_INVALID_FILE_FORMAT;
  }
  if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
    snare_plt_set_errmsg("invalid file type: 0x%04x", ehdr->e_type);
    return SNARE_PLT_INVALID_FILE_FORMAT;
  }
  if (ehdr->e_version != EV_CURRENT) {
    snare_plt_set_errmsg(
        "invalid object file version: %" SNARE_PLT_ELF_WORD_FMT,
        ehdr->e_version);
    return SNARE_PLT_INVALID_FILE_FORMAT;
  }
  if (ehdr->e_ehsize != sizeof(Snare_Elf_Ehdr)) {
    snare_plt_set_errmsg("invalid elf header size: %u", ehdr->e_ehsize);
    return SNARE_PLT_INVALID_FILE_FORMAT;
  }
  if (ehdr->e_phentsize != sizeof(Snare_Elf_Phdr)) {
    snare_plt_set_errmsg("invalid program header table entry size: %u",
                         ehdr->e_phentsize);
    return SNARE_PLT_INVALID_FILE_FORMAT;
  }
  return 0;
}
#endif

static int snare_plt_check_rel(const snare_plt_t *plthook,
                               const Snare_Elf_Plt_Rel *plt,
                               Snare_Elf_Xword r_type, const char **name_out,
                               void ***addr_out) {
  if (ELF_R_TYPE(plt->r_info) == r_type) {
    size_t idx = ELF_R_SYM(plt->r_info);
    idx = plthook->dynsym[idx].st_name;
    if (idx + 1 > plthook->dynstr_size) {
      snare_plt_set_errmsg(
          "too big section header string table index: %" SNARE_PLT_SIZE_T_FMT,
          idx);
      return SNARE_PLT_INVALID_FILE_FORMAT;
    }
    *name_out = plthook->dynstr + idx;
    *addr_out = (void **)(plthook->plt_addr_base + plt->r_offset);
    SNARE_PLT_DEBUG_MSG("%s (%p)\n", *name_out, (void *)(*addr_out));
    return 0;
  }
  return -1;
}

SNARE_EXPORT int SNARE_API snare_plt_enum(snare_plt_t *plthook,
                                          unsigned int *pos,
                                          const char **name_out,
                                          void ***addr_out) {
  return snare_plt_enum_with_prot(plthook, pos, name_out, addr_out, NULL);
}

SNARE_EXPORT int SNARE_API snare_plt_enum_with_prot(snare_plt_t *plthook,
                                                    unsigned int *pos,
                                                    const char **name_out,
                                                    void ***addr_out,
                                                    int *prot) {
  while (*pos < plthook->rela_plt_cnt) {
    const Snare_Elf_Plt_Rel *plt = plthook->rela_plt + *pos;
    int rv = snare_plt_check_rel(plthook, plt, R_JUMP_SLOT, name_out, addr_out);
    (*pos)++;
    if (rv >= 0) {
      if (rv == 0 && prot != NULL) {
        *prot = snare_plt_get_mem_prot(plthook, *addr_out);
      }
      return rv;
    }
  }
#ifdef R_GLOBAL_DATA
  while (*pos < plthook->rela_plt_cnt + plthook->rela_dyn_cnt) {
    const Snare_Elf_Plt_Rel *plt =
        plthook->rela_dyn + (*pos - plthook->rela_plt_cnt);
    int rv =
        snare_plt_check_rel(plthook, plt, R_GLOBAL_DATA, name_out, addr_out);
    (*pos)++;
    if (rv >= 0) {
      if (rv == 0 && prot != NULL) {
        *prot = snare_plt_get_mem_prot(plthook, *addr_out);
      }
      return rv;
    }
  }
#endif
  *name_out = NULL;
  *addr_out = NULL;
  return EOF;
}
SNARE_EXPORT int SNARE_API snare_plt_replace(snare_plt_t *plthook,
                                             const char *funcname,
                                             void *funcaddr, void **oldfunc) {
  size_t funcnamelen = strlen(funcname);
  unsigned int pos = 0;
  const char *name;
  void **addr;
  int rv;

  if (plthook == NULL) {
    snare_plt_set_errmsg("invalid argument: The first argument is null.");
    return SNARE_PLT_INVALID_ARGUMENT;
  }
  while ((rv = snare_plt_enum(plthook, &pos, &name, &addr)) == 0) {
    if (strncmp(name, funcname, funcnamelen) == 0) {
      if (name[funcnamelen] == '\0' || name[funcnamelen] == '@') {
        int prot = snare_plt_get_mem_prot(plthook, addr);
        if (prot == 0) {
          snare_plt_set_errmsg(
              "Could not get the process memory permission at %p",
              SNARE_PLT_ALIGN_ADDR(addr));
          return SNARE_PLT_INTERNAL_ERROR;
        }
        if (!(prot & PROT_WRITE)) {
          if (mprotect(SNARE_PLT_ALIGN_ADDR(addr), snare_plt_page_size,
                       PROT_READ | PROT_WRITE) != 0) {
            snare_plt_set_errmsg(
                "Could not change the process memory permission at %p: %s",
                SNARE_PLT_ALIGN_ADDR(addr), strerror(errno));
            return SNARE_PLT_INTERNAL_ERROR;
          }
        }
        if (oldfunc) {
          void *prev = dlsym(RTLD_DEFAULT, funcname);
          *oldfunc = (prev != NULL) ? prev : *addr;
        }
        *addr = funcaddr;
        if (!(prot & PROT_WRITE)) {
          mprotect(SNARE_PLT_ALIGN_ADDR(addr), snare_plt_page_size, prot);
        }
        return 0;
      }
    }
  }
  if (rv == EOF) {
    snare_plt_set_errmsg("no such function: %s", funcname);
    rv = SNARE_PLT_FUNCTION_NOT_FOUND;
  }
  return rv;
}

SNARE_EXPORT void SNARE_API snare_plt_close(snare_plt_t *plthook) {
  if (plthook != NULL) {
    free(plthook);
  }
}

SNARE_EXPORT const char *SNARE_API snare_plt_error(void) {
  return snare_plt_errmsg;
}

static void snare_plt_set_errmsg(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(snare_plt_errmsg, sizeof(snare_plt_errmsg) - 1, fmt, ap);
  va_end(ap);
}

SNARE_EXPORT int SNARE_API snare_plt_enum_entry(snare_plt_t *plthook,
                                                unsigned int *pos,
                                                snare_plt_entry_t *entry) {
  (void)plthook;
  (void)pos;
  (void)entry;
  return SNARE_PLT_NOT_IMPLEMENTED;
}

#elif defined SNARE_WINDOWS
#include <dbghelp.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef _MSC_VER
#pragma comment(lib, "dbghelp.lib")
#endif

#ifndef _Printf_format_string_
#define _Printf_format_string_
#endif
#ifndef __GNUC__
#define __attribute__(arg)
#endif

#if defined _LP64 /* data model: I32/LP64 */
#define SNARE_PLT_SIZE_T_FMT "lu"
#elif defined _WIN64 /* data model: IL32/P64 */
#define SNARE_PLT_SIZE_T_FMT "I64u"
#else
#define SNARE_PLT_SIZE_T_FMT "u"
#endif

#if defined(__CYGWIN__) || defined(__MINGW32__) || defined(__MINGW64__) ||     \
    ((defined(__MSYS__) || defined(__MSYS2__)) && defined(__clang__))
#define stricmp strcasecmp
#endif

#ifdef SNARE_PLT_DEBUG
#define SNARE_PLT_DEBUG_MSG(...) fprintf(stderr, __VA_ARGS__)
#define SNARE_PLT_DEBUG_MSG_ENTRY(title, id, ordinal)                          \
  SNARE_PLT_DEBUG_MSG(title " [%zu] [Ordinal: %d]: %s %s %p\n", id, ordinal,   \
                      plthook->entries[id].mod_name,                           \
                      plthook->entries[id].name, plthook->entries[id].addr)
#else
#define SNARE_PLT_DEBUG_MSG(...)
#define SNARE_PLT_DEBUG_MSG_ENTRY(title, id, ordinal)
#endif

#define SNARE_PLT_DLATTR_RVA 0x1

typedef struct {
  const char *mod_name;
  const char *name;
  void **addr;
} snare_plt_import_entry_t;

struct snare_plt_s {
  HMODULE hMod;
  unsigned int num_entries;
  snare_plt_import_entry_t entries[1];
};

static char snare_plt_errbuf[512];
static int snare_plt_open_real(snare_plt_t **plthook_out, HMODULE hMod);
static void snare_plt_set_errmsg(_Printf_format_string_ const char *fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)));
static void snare_plt_set_errmsg2(_Printf_format_string_ const char *fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)));
static const char *snare_plt_ws2_ordinal2name(int ordinal);

SNARE_EXPORT int SNARE_API snare_plt_open(snare_plt_t **plthook_out,
                                          const char *filename) {
  HMODULE hMod;

  *plthook_out = NULL;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          filename, &hMod)) {
    snare_plt_set_errmsg2("Cannot get module %s: ", filename);
    return SNARE_PLT_FILE_NOT_FOUND;
  }
  return snare_plt_open_real(plthook_out, hMod);
}

SNARE_EXPORT int SNARE_API snare_plt_open_by_handle(snare_plt_t **plthook_out,
                                                    void *hndl) {
  if (hndl == NULL) {
    snare_plt_set_errmsg("NULL handle");
    return SNARE_PLT_FILE_NOT_FOUND;
  }
  return snare_plt_open_real(plthook_out, (HMODULE)hndl);
}

SNARE_EXPORT int SNARE_API snare_plt_open_by_address(snare_plt_t **plthook_out,
                                                     void *address) {
  HMODULE hMod;

  *plthook_out = NULL;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
                              GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                          (LPCSTR)address, &hMod)) {
    snare_plt_set_errmsg2("Cannot get module at address %p: ", address);
    return SNARE_PLT_FILE_NOT_FOUND;
  }
  return snare_plt_open_real(plthook_out, hMod);
}

static int snare_plt_open_real(snare_plt_t **plthook_out, HMODULE hMod) {
  snare_plt_t *plthook;
  ULONG ulSize;
  IMAGE_IMPORT_DESCRIPTOR *desc_head, *desc;
  PIMAGE_NT_HEADERS nt;
  IMAGE_DATA_DIRECTORY *dir;
  IMAGE_DELAYLOAD_DESCRIPTOR *dload_head = NULL, *dload;
  size_t dload_count = 0;
  unsigned int num_entries = 0;
  size_t ordinal_name_buflen = 0;
  size_t idx;
  char *ordinal_name_buf;

  desc_head = (IMAGE_IMPORT_DESCRIPTOR *)ImageDirectoryEntryToData(
      hMod, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulSize);
  if (desc_head == NULL) {
    snare_plt_set_errmsg2("ImageDirectoryEntryToData error: ");
    return SNARE_PLT_INTERNAL_ERROR;
  }

  /* calculate size to allocate memory (import table) */
  for (desc = desc_head; desc->Name != 0; desc++) {
    /* OriginalFirstThunk (ILT) contains the names/hints */
    IMAGE_THUNK_DATA *ilt_thunk =
        (IMAGE_THUNK_DATA *)((uintptr_t)hMod + desc->OriginalFirstThunk);
    const char *module_name = (char *)((uintptr_t)hMod + desc->Name);
    int is_winsock2_dll = (stricmp(module_name, "WS2_32.DLL") == 0);

    while (ilt_thunk->u1.AddressOfData != 0) {
      if (IMAGE_SNAP_BY_ORDINAL(ilt_thunk->u1.Ordinal)) {
        int ordinal = IMAGE_ORDINAL(ilt_thunk->u1.Ordinal);
        const char *name = NULL;
        if (is_winsock2_dll) {
          name = snare_plt_ws2_ordinal2name(ordinal);
        }
        if (name == NULL) {
#if defined(__CYGWIN__) || defined(__MINGW32__) || defined(__MINGW64__) ||     \
    ((defined(__MSYS__) || defined(__MSYS2__)) && defined(__clang__))
          ordinal_name_buflen +=
              snprintf(NULL, 0, "%s:@%d", module_name, ordinal) + 1;
#else
          ordinal_name_buflen += _scprintf("%s:@%d", module_name, ordinal) + 1;
#endif
        }
      }
      num_entries++;
      ilt_thunk++;
    }
  }
  /* Calculate size to allocate memory (Delayed Load Import Table) */
  nt = (PIMAGE_NT_HEADERS)((uintptr_t)hMod +
                           ((PIMAGE_DOS_HEADER)hMod)->e_lfanew);
  dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
  if (dir->VirtualAddress && dir->Size) {
    size_t iterator;

    dload_head = dload =
        (IMAGE_DELAYLOAD_DESCRIPTOR *)((uintptr_t)hMod + dir->VirtualAddress);
    dload_count = dir->Size / sizeof(*dload_head);

    for (iterator = 0; iterator < dload_count && dload->DllNameRVA != 0;
         iterator++, dload++) {
      /* Check if it uses absolute addresses or RVAs */
      BOOL uses_rva = dload->Attributes.AllAttributes & SNARE_PLT_DLATTR_RVA;
      /* Import Name Table (INT) contains function names */
      IMAGE_THUNK_DATA *int_thunk =
          (IMAGE_THUNK_DATA *)(uses_rva
                                   ? ((uintptr_t)hMod +
                                      dload->ImportNameTableRVA)
                                   : ((uintptr_t)dload->ImportNameTableRVA));
      const char *module_name = (char *)((uintptr_t)hMod + dload->DllNameRVA);
      int is_winsock2_dll = (stricmp(module_name, "WS2_32.DLL") == 0);

      if (*module_name == '\0') {
        continue;
      }

      while (int_thunk->u1.AddressOfData != 0) {
        if (IMAGE_SNAP_BY_ORDINAL(int_thunk->u1.Ordinal)) {
          int ordinal = IMAGE_ORDINAL(int_thunk->u1.Ordinal);
          const char *name = NULL;
          if (is_winsock2_dll) {
            name = snare_plt_ws2_ordinal2name(ordinal);
          }
          if (name == NULL) {
#if defined(__CYGWIN__) || defined(__MINGW32__) || defined(__MINGW64__) ||     \
    ((defined(__MSYS__) || defined(__MSYS2__)) && defined(__clang__))
            ordinal_name_buflen +=
                snprintf(NULL, 0, "%s:@%d", module_name, ordinal) + 1;
#else
            ordinal_name_buflen +=
                _scprintf("%s:@%d", module_name, ordinal) + 1;
#endif
          }
        }
        num_entries++;
        int_thunk++;
      }
    }
  }
  plthook = (snare_plt_t *)calloc(1, offsetof(snare_plt_t, entries) +
                                         sizeof(snare_plt_import_entry_t) *
                                             num_entries +
                                         ordinal_name_buflen);
  if (plthook == NULL) {
    snare_plt_set_errmsg("failed to allocate memory: %" SNARE_PLT_SIZE_T_FMT
                         " bytes",
                         sizeof(snare_plt_t));
    return SNARE_PLT_OUT_OF_MEMORY;
  }
  plthook->hMod = hMod;
  plthook->num_entries = num_entries;
  ordinal_name_buf =
      (char *)((uintptr_t)plthook + offsetof(snare_plt_t, entries) +
               sizeof(snare_plt_import_entry_t) * num_entries);
  idx = 0;
  /* Import Table */
  for (desc = desc_head; desc->Name != 0; desc++) {
    /* OriginalFirstThunk (Import Lookup Table) contains the names/hints */
    IMAGE_THUNK_DATA *ilt_thunk =
        (IMAGE_THUNK_DATA *)((uintptr_t)hMod + desc->OriginalFirstThunk);
    /* FirstThunk (Import Address Table) contains the actual function addresses
     */
    IMAGE_THUNK_DATA *iat_thunk =
        (IMAGE_THUNK_DATA *)((uintptr_t)hMod + desc->FirstThunk);

    const char *module_name = (char *)hMod + desc->Name;
    int is_winsock2_dll = (stricmp(module_name, "WS2_32.DLL") == 0);
    SNARE_PLT_DEBUG_MSG("Imported Library: '%s'\n", module_name);
    while (ilt_thunk->u1.AddressOfData != 0) {
      const char *name = NULL;
      if (IMAGE_SNAP_BY_ORDINAL(ilt_thunk->u1.Ordinal)) {
        int ordinal = IMAGE_ORDINAL(ilt_thunk->u1.Ordinal);
        if (is_winsock2_dll) {
          name = snare_plt_ws2_ordinal2name(ordinal);
        }
        if (name == NULL) {
          name = ordinal_name_buf;
          ordinal_name_buf +=
              sprintf(ordinal_name_buf, "%s:@%d", module_name, ordinal) + 1;
        }
      } else {
        PIMAGE_IMPORT_BY_NAME import_by_name =
            (PIMAGE_IMPORT_BY_NAME)((uintptr_t)hMod +
                                    ilt_thunk->u1.AddressOfData);
        name = (char *)import_by_name->Name;
      }

      plthook->entries[idx].mod_name = module_name;
      plthook->entries[idx].name = name;
      plthook->entries[idx].addr = (void **)&iat_thunk->u1.Function;

      SNARE_PLT_DEBUG_MSG_ENTRY("Import Symbol    ", idx,
                                IMAGE_SNAP_BY_ORDINAL(ilt_thunk->u1.Ordinal));
      idx++;
      ilt_thunk++;
      iat_thunk++;
    }
  }

  /* Delayed Load Import Table */
  if (dload_head != NULL) {
    size_t iterator;

    for (iterator = 0, dload = dload_head;
         iterator < dload_count && dload->DllNameRVA != 0;
         iterator++, dload++) {
      /* Check if it uses absolute addresses or RVAs */
      BOOL uses_rva = dload->Attributes.AllAttributes & SNARE_PLT_DLATTR_RVA;
      /* Import Name Table (INT) contains function names */
      IMAGE_THUNK_DATA *int_thunk =
          (IMAGE_THUNK_DATA *)(uses_rva
                                   ? ((uintptr_t)hMod +
                                      dload->ImportNameTableRVA)
                                   : ((uintptr_t)dload->ImportNameTableRVA));
      /* Import Address Table (IAT) contains current function addresses (stubs
       * or resolved addresses) */
      IMAGE_THUNK_DATA *iat_thunk =
          (IMAGE_THUNK_DATA *)(uses_rva
                                   ? ((uintptr_t)hMod +
                                      dload->ImportAddressTableRVA)
                                   : ((uintptr_t)dload->ImportAddressTableRVA));

      const char *module_name = (char *)((uintptr_t)hMod + dload->DllNameRVA);
      int is_winsock2_dll = (stricmp(module_name, "WS2_32.DLL") == 0);

      if (*module_name == '\0') {
        continue;
      }

      SNARE_PLT_DEBUG_MSG("Imported Delayed Library: '%s'\n", module_name);

      while (int_thunk->u1.AddressOfData != 0) {
        const char *name = NULL;

        if (IMAGE_SNAP_BY_ORDINAL(int_thunk->u1.Ordinal)) {
          int ordinal = IMAGE_ORDINAL(int_thunk->u1.Ordinal);
          if (is_winsock2_dll) {
            name = snare_plt_ws2_ordinal2name(ordinal);
          }
          if (name == NULL) {
            name = ordinal_name_buf;
            ordinal_name_buf +=
                sprintf(ordinal_name_buf, "%s:@%d", module_name, ordinal) + 1;
          }
        } else {
          PIMAGE_IMPORT_BY_NAME import_by_name =
              (PIMAGE_IMPORT_BY_NAME)((uintptr_t)hMod +
                                      int_thunk->u1.AddressOfData);
          name = (char *)import_by_name->Name;
        }

        plthook->entries[idx].mod_name = module_name;
        plthook->entries[idx].name = name;
        plthook->entries[idx].addr = (void **)&iat_thunk->u1.Function;

        SNARE_PLT_DEBUG_MSG_ENTRY("Delay Load Symbol", idx,
                                  IMAGE_SNAP_BY_ORDINAL(int_thunk->u1.Ordinal));
        idx++;
        int_thunk++;
        iat_thunk++;
      }
    }
  }

  SNARE_PLT_DEBUG_MSG("Number of entries: %u (counted) == %zu (loaded)\n",
                      num_entries, idx);
  SNARE_PLT_DEBUG_MSG("Ordinal buffer lenght: %zu (counted) == %zu (written)\n",
                      ordinal_name_buflen,
                      (uintptr_t)ordinal_name_buf -
                          ((uintptr_t)plthook + offsetof(snare_plt_t, entries) +
                           sizeof(snare_plt_import_entry_t) * num_entries));
  *plthook_out = plthook;
  return 0;
}

SNARE_EXPORT int SNARE_API snare_plt_enum(snare_plt_t *plthook,
                                          unsigned int *pos,
                                          const char **name_out,
                                          void ***addr_out) {
  if (*pos >= plthook->num_entries) {
    *name_out = NULL;
    *addr_out = NULL;
    return EOF;
  }
  *name_out = plthook->entries[*pos].name;
  *addr_out = plthook->entries[*pos].addr;
  (*pos)++;
  return 0;
}

static void snare_plt_replace_funcaddr(void **addr, void *newfunc,
                                       void **oldfunc) {
  DWORD dwOld;
  DWORD dwDummy;

  if (oldfunc != NULL) {
    *oldfunc = *addr;
  }
  VirtualProtect(addr, sizeof(void *), PAGE_EXECUTE_READWRITE, &dwOld);
  *addr = newfunc;
  VirtualProtect(addr, sizeof(void *), dwOld, &dwDummy);
}

SNARE_EXPORT int SNARE_API snare_plt_replace(snare_plt_t *plthook,
                                             const char *funcname,
                                             void *funcaddr, void **oldfunc) {
#ifndef _WIN64
  size_t funcnamelen = strlen(funcname);
#endif
  unsigned int pos = 0;
  const char *name;
  void **addr;
  int rv;
  BOOL import_by_ordinal = funcname[0] != '?' && strstr(funcname, ":@") != NULL;

  if (plthook == NULL) {
    snare_plt_set_errmsg("invalid argument: The first argument is null.");
    return SNARE_PLT_INVALID_ARGUMENT;
  }
  while ((rv = snare_plt_enum(plthook, &pos, &name, &addr)) == 0) {
    if (import_by_ordinal) {
      if (stricmp(name, funcname) == 0) {
        goto found;
      }
    } else {
      /* Import by name */
#ifdef _WIN64
      if (strcmp(name, funcname) == 0) {
        goto found;
      }
#else
      /* Function names may be decorated in Windows 32-bit applications */
      if (strncmp(name, funcname, funcnamelen) == 0) {
        if (name[funcnamelen] == '\0' || name[funcnamelen] == '@') {
          goto found;
        }
      }
      if (name[0] == '_' || name[0] == '@') {
        name++;
        if (strncmp(name, funcname, funcnamelen) == 0) {
          if (name[funcnamelen] == '\0' || name[funcnamelen] == '@') {
            goto found;
          }
        }
      }
#endif
    }
  }
  if (rv == EOF) {
    snare_plt_set_errmsg("no such function: %s", funcname);
    rv = SNARE_PLT_FUNCTION_NOT_FOUND;
  }
  return rv;
found:
  snare_plt_replace_funcaddr(addr, funcaddr, oldfunc);
  return 0;
}

SNARE_EXPORT void SNARE_API snare_plt_close(snare_plt_t *plthook) {
  if (plthook != NULL) {
    free(plthook);
  }
}

SNARE_EXPORT const char *SNARE_API snare_plt_error(void) {
  return snare_plt_errbuf;
}

static void snare_plt_set_errmsg(_Printf_format_string_ const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(snare_plt_errbuf, sizeof(snare_plt_errbuf) - 1, fmt, ap);
  va_end(ap);
}

static void snare_plt_set_errmsg2(_Printf_format_string_ const char *fmt, ...) {
  va_list ap;
  size_t len;

  va_start(ap, fmt);
  vsnprintf(snare_plt_errbuf, sizeof(snare_plt_errbuf) - 1, fmt, ap);
  va_end(ap);
  len = strlen(snare_plt_errbuf);
  FormatMessageA(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      snare_plt_errbuf + len, sizeof(snare_plt_errbuf) - len - 1, NULL);
}

static const char *snare_plt_ws2_ordinal2name(int ordinal) {
  switch (ordinal) {
  case 1:
    return "accept";
  case 2:
    return "bind";
  case 3:
    return "closesocket";
  case 4:
    return "connect";
  case 5:
    return "getpeername";
  case 6:
    return "getsockname";
  case 7:
    return "getsockopt";
  case 8:
    return "htonl";
  case 9:
    return "htons";
  case 10:
    return "inet_addr";
  case 11:
    return "inet_ntoa";
  case 12:
    return "ioctlsocket";
  case 13:
    return "listen";
  case 14:
    return "ntohl";
  case 15:
    return "ntohs";
  case 16:
    return "recv";
  case 17:
    return "recvfrom";
  case 18:
    return "select";
  case 19:
    return "send";
  case 20:
    return "sendto";
  case 21:
    return "setsockopt";
  case 22:
    return "shutdown";
  case 23:
    return "socket";
  case 24:
    return "MigrateWinsockConfiguration";
  case 51:
    return "gethostbyaddr";
  case 52:
    return "gethostbyname";
  case 53:
    return "getprotobyname";
  case 54:
    return "getprotobynumber";
  case 55:
    return "getservbyname";
  case 56:
    return "getservbyport";
  case 57:
    return "gethostname";
  case 101:
    return "WSAAsyncSelect";
  case 102:
    return "WSAAsyncGetHostByAddr";
  case 103:
    return "WSAAsyncGetHostByName";
  case 104:
    return "WSAAsyncGetProtoByNumber";
  case 105:
    return "WSAAsyncGetProtoByName";
  case 106:
    return "WSAAsyncGetServByPort";
  case 107:
    return "WSAAsyncGetServByName";
  case 108:
    return "WSACancelAsyncRequest";
  case 109:
    return "WSASetBlockingHook";
  case 110:
    return "WSAUnhookBlockingHook";
  case 111:
    return "WSAGetLastError";
  case 112:
    return "WSASetLastError";
  case 113:
    return "WSACancelBlockingCall";
  case 114:
    return "WSAIsBlocking";
  case 115:
    return "WSAStartup";
  case 116:
    return "WSACleanup";
  case 151:
    return "__WSAFDIsSet";
  case 500:
    return "WEP";
  case 1000:
    return "WSApSetPostRoutine";
  case 1001:
    return "WsControl";
  case 1002:
    return "closesockinfo";
  case 1003:
    return "Arecv";
  case 1004:
    return "Asend";
  case 1005:
    return "WSHEnumProtocols";
  case 1100:
    return "inet_network";
  case 1101:
    return "getnetbyname";
  case 1102:
    return "rcmd";
  case 1103:
    return "rexec";
  case 1104:
    return "rresvport";
  case 1105:
    return "sethostname";
  case 1106:
    return "dn_expand";
  case 1107:
    return "WSARecvEx";
  case 1108:
    return "s_perror";
  case 1109:
    return "GetAddressByNameA";
  case 1110:
    return "GetAddressByNameW";
  case 1111:
    return "EnumProtocolsA";
  case 1112:
    return "EnumProtocolsW";
  case 1113:
    return "GetTypeByNameA";
  case 1114:
    return "GetTypeByNameW";
  case 1115:
    return "GetNameByTypeA";
  case 1116:
    return "GetNameByTypeW";
  case 1117:
    return "SetServiceA";
  case 1118:
    return "SetServiceW";
  case 1119:
    return "GetServiceA";
  case 1120:
    return "GetServiceW";
  case 1130:
    return "NPLoadNameSpaces";
  case 1131:
    return "NSPStartup";
  case 1140:
    return "TransmitFile";
  case 1141:
    return "AcceptEx";
  case 1142:
    return "GetAcceptExSockaddrs";
  }
  return NULL;
}

SNARE_EXPORT int SNARE_API snare_plt_enum_with_prot(snare_plt_t *plthook,
                                                    unsigned int *pos,
                                                    const char **name_out,
                                                    void ***addr_out,
                                                    int *prot) {
  (void)prot;
  return snare_plt_enum(plthook, pos, name_out, addr_out);
}

SNARE_EXPORT int SNARE_API snare_plt_enum_entry(snare_plt_t *plthook,
                                                unsigned int *pos,
                                                snare_plt_entry_t *entry) {
  (void)plthook;
  (void)pos;
  (void)entry;
  return SNARE_PLT_NOT_IMPLEMENTED;
}

#elif defined SNARE_MACOS
#include <dlfcn.h>
#include <inttypes.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/mman.h>
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 120000
#include <errno.h>
#include <mach-o/fixup-chains.h>
#endif

#ifdef SNARE_PLT_DEBUG
#define SNARE_PLT_DEBUG_CMD 1
#define SNARE_PLT_DEBUG_BIND 1
#define SNARE_PLT_DEBUG_FIXUPS 1
#define SNARE_PLT_DEBUG_ADDR 1
#endif

#ifdef SNARE_PLT_DEBUG_CMD
#define SNARE_PLT_DEBUG_CMD_MSG(...) fprintf(stderr, __VA_ARGS__)
#else
#define SNARE_PLT_DEBUG_CMD_MSG(...)
#endif

#ifdef SNARE_PLT_DEBUG_FIXUPS
#define SNARE_PLT_DEBUG_FIXUPS_MSG(...) fprintf(stderr, __VA_ARGS__)
#else
#define SNARE_PLT_DEBUG_FIXUPS_MSG(...)
#endif

#ifdef SNARE_PLT_DEBUG_BIND
#define SNARE_PLT_DEBUG_BIND_MSG(...) fprintf(stderr, __VA_ARGS__)
#define SNARE_PLT_DEBUG_BIND_IF(cond, ...)                                     \
  if (cond)                                                                    \
  fprintf(stderr, __VA_ARGS__)
#else
#define SNARE_PLT_DEBUG_BIND_MSG(...)
#define SNARE_PLT_DEBUG_BIND_IF(cond, ...)
#endif

#ifdef SNARE_PLT_DEBUG_ADDR
#include <mach/mach.h>

#define SNARE_PLT_INHERIT_MAX_SIZE 11
static char *snare_plt_inherit_to_str(vm_inherit_t inherit, char *buf) {
  switch (inherit) {
  case VM_INHERIT_SHARE:
    return "share";
  case VM_INHERIT_COPY:
    return "copy";
  case VM_INHERIT_NONE:
    return "none";
  case VM_INHERIT_DONATE_COPY:
    return "donate_copy";
  default:
    sprintf(buf, "%d", inherit);
    return buf;
  }
}

#define SNARE_PLT_BEHAVIOR_MAX_SIZE 16
static char *snare_plt_behavior_to_str(vm_behavior_t behavior, char *buf) {
  switch (behavior) {
  case VM_BEHAVIOR_DEFAULT:
    return "default";
  case VM_BEHAVIOR_RANDOM:
    return "random";
  case VM_BEHAVIOR_SEQUENTIAL:
    return "sequential";
  case VM_BEHAVIOR_RSEQNTL:
    return "rseqntl";
  case VM_BEHAVIOR_WILLNEED:
    return "willneed";
  case VM_BEHAVIOR_DONTNEED:
    return "dontneed";
  case VM_BEHAVIOR_FREE:
    return "free";
  case VM_BEHAVIOR_ZERO_WIRED_PAGES:
    return "zero";
  case VM_BEHAVIOR_REUSABLE:
    return "reusable";
  case VM_BEHAVIOR_REUSE:
    return "reuse";
  case VM_BEHAVIOR_CAN_REUSE:
    return "can";
  case VM_BEHAVIOR_PAGEOUT:
    return "pageout";
  default:
    sprintf(buf, "%d", behavior);
    return buf;
  }
}
static void snare_plt_dump_maps(const char *image_name) {
  mach_port_t task = mach_task_self();
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
  memory_object_name_t object = 0;
  vm_address_t addr = 0;
  vm_size_t size;
  char inherit_buf[SNARE_PLT_INHERIT_MAX_SIZE + 1];
  char behavior_buf[SNARE_PLT_BEHAVIOR_MAX_SIZE + 1];

  fprintf(stderr, "MEMORY MAP(%s)\n", image_name);
  fprintf(
      stderr,
      " start address    end address      protection    max_protection inherit "
      "    shared reserved offset   behavior         user_wired_count\n");
  while (vm_region_64(task, &addr, &size, VM_REGION_BASIC_INFO_64,
                      (vm_region_info_t)&info, &info_count,
                      &object) == KERN_SUCCESS) {
    fprintf(stderr,
            " %016lx-%016lx %c%c%c(%08x) %c%c%c(%08x)  %-*s %c      %c        "
            "%08llx %-*s %u\n",
            addr, addr + size, (info.protection & VM_PROT_READ) ? 'r' : '-',
            (info.protection & VM_PROT_WRITE) ? 'w' : '-',
            (info.protection & VM_PROT_EXECUTE) ? 'x' : '-', info.protection,
            (info.max_protection & VM_PROT_READ) ? 'r' : '-',
            (info.max_protection & VM_PROT_WRITE) ? 'w' : '-',
            (info.max_protection & VM_PROT_EXECUTE) ? 'x' : '-',
            info.max_protection, SNARE_PLT_INHERIT_MAX_SIZE,
            snare_plt_inherit_to_str(info.inheritance, inherit_buf),
            info.shared ? 'Y' : 'N', info.reserved ? 'Y' : 'N', info.offset,
            SNARE_PLT_BEHAVIOR_MAX_SIZE,
            snare_plt_behavior_to_str(info.behavior, behavior_buf),
            info.user_wired_count);
    addr += size;
  }
}
#endif

typedef struct {
  const char *name;
  int addend;
  char weak;
  void **addr;
} snare_plt_bind_address_t;

typedef struct snare_plt_mem_prot {
  size_t start;
  size_t end;
  int prot;
} snare_plt_mem_prot_t;

#define SNARE_PLT_NUM_MEM_PROT 100

struct snare_plt_s {
  unsigned int num_entries;
  snare_plt_mem_prot_t mem_prot[SNARE_PLT_NUM_MEM_PROT];
  snare_plt_bind_address_t entries[1];
};

#define SNARE_PLT_MAX_SEGMENTS 8
#define SNARE_PLT_MAX_SECTIONS 30

typedef struct {
  snare_plt_t *plthook;
  intptr_t slide;
  int num_segments;
  int linkedit_segment_idx;
  const struct segment_command_64 *segments[SNARE_PLT_MAX_SEGMENTS];
#ifdef SNARE_PLT_DEBUG_FIXUPS
  int num_sections;
  const struct section_64 *sections[SNARE_PLT_MAX_SECTIONS];
#endif
  const struct linkedit_data_command *chained_fixups;
} snare_plt_data_t;

static int snare_plt_open_real(snare_plt_t **plthook_out, uint32_t image_idx,
                               const struct mach_header *mh,
                               const char *image_name);
static unsigned int snare_plt_set_bind_addrs(snare_plt_data_t *data,
                                             unsigned int idx,
                                             uint32_t bind_off,
                                             uint32_t bind_size, char weak);
static void snare_plt_set_bind_addr(snare_plt_data_t *d, unsigned int *idx,
                                    const char *sym_name, int seg_index,
                                    int seg_offset, int addend, char weak);
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 120000
static int snare_plt_read_chained_fixups(snare_plt_data_t *d,
                                         const char *image_name);
static uint8_t *snare_plt_fileoff_to_vmaddr(snare_plt_data_t *data,
                                            size_t offset);
#ifdef SNARE_PLT_DEBUG_FIXUPS
static const char *snare_plt_segment_name_from_addr(snare_plt_data_t *d,
                                                    size_t addr);
static const char *snare_plt_section_name_from_addr(snare_plt_data_t *d,
                                                    size_t addr);
#endif
#endif

static int snare_plt_set_mem_prot(snare_plt_t *plthook);
static int snare_plt_get_mem_prot(snare_plt_t *plthook, void *addr);

static inline uint8_t *
snare_plt_fileoff_to_vmaddr_in_segment(snare_plt_data_t *d, int segment_index,
                                       size_t offset) {
  const struct segment_command_64 *seg = d->segments[segment_index];
  return (uint8_t *)(seg->vmaddr - seg->fileoff + d->slide + offset);
}

static void snare_plt_set_errmsg(const char *fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)));

static uint64_t snare_plt_uleb128(const uint8_t **p) {
  uint64_t r = 0;
  int s = 0;
  do {
    r |= (uint64_t)(**p & 0x7f) << s;
    s += 7;
  } while (*(*p)++ >= 0x80);
  return r;
}

static int64_t snare_plt_sleb128(const uint8_t **p) {
  int64_t r = 0;
  int s = 0;
  for (;;) {
    uint8_t b = *(*p)++;
    if (b < 0x80) {
      if (b & 0x40) {
        r -= (0x80 - b) << s;
      } else {
        r |= (b & 0x3f) << s;
      }
      break;
    }
    r |= (b & 0x7f) << s;
    s += 7;
  }
  return r;
}

static char snare_plt_errmsg[512];

SNARE_EXPORT int SNARE_API snare_plt_open(snare_plt_t **plthook_out,
                                          const char *filename) {
  size_t namelen;
  uint32_t cnt;
  uint32_t idx;

  if (filename == NULL) {
    return snare_plt_open_real(plthook_out, 0, NULL, NULL);
  }
  cnt = _dyld_image_count();
  namelen = strlen(filename);

  for (idx = 0; idx < cnt; idx++) {
    const char *image_name = _dyld_get_image_name(idx);
    size_t offset = 0;

    if (image_name == NULL) {
      *plthook_out = NULL;
      snare_plt_set_errmsg("Cannot find file at image index %u", idx);
      return SNARE_PLT_INTERNAL_ERROR;
    }
    if (*filename != '/') {
      size_t image_name_len = strlen(image_name);
      if (image_name_len > namelen) {
        offset = image_name_len - namelen;
      }
    }
    if (strcmp(image_name + offset, filename) == 0) {
      return snare_plt_open_real(plthook_out, idx, NULL, image_name);
    }
  }
  *plthook_out = NULL;
  snare_plt_set_errmsg("Cannot find file: %s", filename);
  return SNARE_PLT_FILE_NOT_FOUND;
}

SNARE_EXPORT int SNARE_API snare_plt_open_by_handle(snare_plt_t **plthook_out,
                                                    void *hndl) {
  int flags[] = {
      RTLD_LAZY | RTLD_NOLOAD,
      RTLD_LAZY | RTLD_NOLOAD | RTLD_FIRST,
  };
  int flag_idx;
  uint32_t cnt = _dyld_image_count();
#define SNARE_PLT_NUM_FLAGS (sizeof(flags) / sizeof(flags[0]))

  if (hndl == NULL) {
    snare_plt_set_errmsg("NULL handle");
    return SNARE_PLT_FILE_NOT_FOUND;
  }
  for (flag_idx = 0; flag_idx < (int)SNARE_PLT_NUM_FLAGS; flag_idx++) {
    uint32_t idx;

    for (idx = 0; idx < cnt; idx++) {
      const char *image_name = idx ? _dyld_get_image_name(idx) : NULL;
      void *handle = dlopen(image_name, flags[flag_idx]);
      if (handle != NULL) {
        dlclose(handle);
        if (handle == hndl) {
          return snare_plt_open_real(plthook_out, idx, NULL, image_name);
        }
      }
    }
  }
  snare_plt_set_errmsg("Cannot find the image correspond to handle %p", hndl);
  return SNARE_PLT_FILE_NOT_FOUND;
}

SNARE_EXPORT int SNARE_API snare_plt_open_by_address(snare_plt_t **plthook_out,
                                                     void *address) {
  Dl_info dlinfo;
  uint32_t idx = 0;
  uint32_t cnt = _dyld_image_count();

  if (!dladdr(address, &dlinfo)) {
    *plthook_out = NULL;
    snare_plt_set_errmsg("Cannot find address: %p", address);
    return SNARE_PLT_FILE_NOT_FOUND;
  }
  for (idx = 0; idx < cnt; idx++) {
    if (dlinfo.dli_fbase == _dyld_get_image_header(idx)) {
      return snare_plt_open_real(plthook_out, idx, dlinfo.dli_fbase,
                                 dlinfo.dli_fname);
    }
  }
  snare_plt_set_errmsg("Cannot find the image index for base address: %p",
                       dlinfo.dli_fbase);
  return SNARE_PLT_FILE_NOT_FOUND;
}

static int snare_plt_open_real(snare_plt_t **plthook_out, uint32_t image_idx,
                               const struct mach_header *mh,
                               const char *image_name) {
  struct load_command *cmd;
  const struct dyld_info_command *dyld_info = NULL;
  unsigned int nbind;
  snare_plt_data_t data;
  size_t size;
  uint32_t i;

  memset(&data, 0, sizeof(snare_plt_data_t));

  data.linkedit_segment_idx = -1;
  data.slide = _dyld_get_image_vmaddr_slide(image_idx);
  if (mh == NULL) {
    mh = _dyld_get_image_header(image_idx);
  }
  if (image_name == NULL) {
    image_name = _dyld_get_image_name(image_idx);
  }
#if defined(SNARE_PLT_DEBUG_CMD) || defined(SNARE_PLT_DEBUG_ADDR)
  fprintf(stderr, "mh=%" PRIxPTR " slide=%" PRIxPTR "\n", (uintptr_t)mh,
          data.slide);
#endif
#ifdef SNARE_PLT_DEBUG_ADDR
  snare_plt_dump_maps(image_name);
#endif

  cmd = (struct load_command *)((size_t)mh + sizeof(struct mach_header_64));
  SNARE_PLT_DEBUG_CMD_MSG("CMD START\n");
  for (i = 0; i < mh->ncmds; i++) {
#ifdef SNARE_PLT_DEBUG_CMD
    struct segment_command *segment;
#endif
    struct segment_command_64 *segment64;

    switch (cmd->cmd) {
    case LC_SEGMENT: /* 0x1 */
#ifdef SNARE_PLT_DEBUG_CMD
      segment = (struct segment_command *)cmd;
#endif
      SNARE_PLT_DEBUG_CMD_MSG(
          "LC_SEGMENT\n"
          "  segname   %s\n"
          "  vmaddr    %8x  vmsize     %8x\n"
          "  fileoff   %8x  filesize   %8x\n"
          "  maxprot   %8x  initprot   %8x\n"
          "  nsects    %8d  flags      %8x\n",
          segment->segname, segment->vmaddr, segment->vmsize, segment->fileoff,
          segment->filesize, segment->maxprot, segment->initprot,
          segment->nsects, segment->flags);
      break;
    case LC_SEGMENT_64: /* 0x19 */
      segment64 = (struct segment_command_64 *)cmd;
      SNARE_PLT_DEBUG_CMD_MSG(
          "LC_SEGMENT_64\n"
          "  segname   %s\n"
          "  vmaddr    %8llx  vmsize     %8llx\n"
          "  fileoff   %8llx  filesize   %8llx\n"
          "  maxprot   %8x  initprot   %8x\n"
          "  nsects    %8d  flags      %8x\n",
          segment64->segname, segment64->vmaddr, segment64->vmsize,
          segment64->fileoff, segment64->filesize, segment64->maxprot,
          segment64->initprot, segment64->nsects, segment64->flags);
      if (strcmp(segment64->segname, "__LINKEDIT") == 0) {
        data.linkedit_segment_idx = data.num_segments;
      }
#ifdef SNARE_PLT_DEBUG_FIXUPS
      struct section_64 *sec = (struct section_64 *)(segment64 + 1);
      uint32_t i;
      for (i = 0; i < segment64->nsects; i++) {
        SNARE_PLT_DEBUG_CMD_MSG("  section_64 (%u)\n"
                                "      sectname  %s\n"
                                "      segname   %s\n"
                                "      addr      0x%llx\n"
                                "      size      0x%llx\n"
                                "      offset    0x%x\n"
                                "      align     0x%x\n"
                                "      reloff    0x%x\n"
                                "      nreloc    %d\n"
                                "      flags     0x%x\n"
                                "      reserved1 %d\n"
                                "      reserved2 %d\n"
                                "      reserved3 %d\n",
                                i, sec->sectname, sec->segname, sec->addr,
                                sec->size, sec->offset, sec->align, sec->reloff,
                                sec->nreloc, sec->flags, sec->reserved1,
                                sec->reserved2, sec->reserved3);
        sec++;
      }
#endif
      if (data.num_segments == SNARE_PLT_MAX_SEGMENTS) {
        snare_plt_set_errmsg("Too many segments: %s", image_name);
        return SNARE_PLT_INTERNAL_ERROR;
      }
      data.segments[data.num_segments++] = segment64;
#ifdef SNARE_PLT_DEBUG_FIXUPS
      {
        struct section_64 *sec = (struct section_64 *)(segment64 + 1);
        struct section_64 *sec_end = sec + segment64->nsects;
        while (sec < sec_end) {
          if (data.num_sections == SNARE_PLT_MAX_SECTIONS) {
            snare_plt_set_errmsg("Too many sections: %s", image_name);
            return SNARE_PLT_INTERNAL_ERROR;
          }
          data.sections[data.num_sections++] = sec;
          sec++;
        }
      }
#endif
      break;
    case LC_DYLD_INFO_ONLY: /* (0x22|LC_REQ_DYLD) */
      dyld_info = (struct dyld_info_command *)cmd;
      SNARE_PLT_DEBUG_CMD_MSG(
          "LC_DYLD_INFO_ONLY\n"
          "                 offset     size\n"
          "  rebase       %8x %8x\n"
          "  bind         %8x %8x\n"
          "  weak_bind    %8x %8x\n"
          "  lazy_bind    %8x %8x\n"
          "  export_bind  %8x %8x\n",
          dyld_info->rebase_off, dyld_info->rebase_size, dyld_info->bind_off,
          dyld_info->bind_size, dyld_info->weak_bind_off,
          dyld_info->weak_bind_size, dyld_info->lazy_bind_off,
          dyld_info->lazy_bind_size, dyld_info->export_off,
          dyld_info->export_size);
      break;
    case LC_SYMTAB: /* 0x2 */
      SNARE_PLT_DEBUG_CMD_MSG("LC_SYMTAB\n");
      break;
    case LC_DYSYMTAB: /* 0xb */
      SNARE_PLT_DEBUG_CMD_MSG("LC_DYSYMTAB\n");
      break;
    case LC_LOAD_DYLIB: /* 0xc */
      SNARE_PLT_DEBUG_CMD_MSG("LC_LOAD_DYLIB\n");
      break;
    case LC_ID_DYLIB: /* 0xd */
      SNARE_PLT_DEBUG_CMD_MSG("LC_ID_DYLIB\n");
      break;
    case LC_LOAD_DYLINKER: /* 0xe */
      SNARE_PLT_DEBUG_CMD_MSG("LC_LOAD_DYLINKER\n");
      break;
    case LC_ROUTINES_64: /* 0x1a */
      SNARE_PLT_DEBUG_CMD_MSG("LC_ROUTINES_64\n");
      break;
    case LC_UUID: /* 0x1b */
      SNARE_PLT_DEBUG_CMD_MSG("LC_UUID\n");
      break;
    case LC_RPATH: /* (0x1c|LC_REQ_DYLD) */
      SNARE_PLT_DEBUG_CMD_MSG("LC_RPATH\n");
      break;
    case LC_CODE_SIGNATURE: /* 0x1d */
      SNARE_PLT_DEBUG_CMD_MSG("LC_CODE_SIGNATURE\n");
      break;
    case LC_VERSION_MIN_MACOSX: /* 0x24 */
      SNARE_PLT_DEBUG_CMD_MSG("LC_VERSION_MIN_MACOSX\n");
      break;
    case LC_FUNCTION_STARTS: /* 0x26 */
      SNARE_PLT_DEBUG_CMD_MSG("LC_FUNCTION_STARTS\n");
      break;
    case LC_MAIN: /* 0x28|LC_REQ_DYLD */
      SNARE_PLT_DEBUG_CMD_MSG("LC_MAIN\n");
      break;
    case LC_DATA_IN_CODE: /* 0x29 */
      SNARE_PLT_DEBUG_CMD_MSG("LC_DATA_IN_CODE\n");
      break;
    case LC_SOURCE_VERSION: /* 0x2A */
      SNARE_PLT_DEBUG_CMD_MSG("LC_SOURCE_VERSION\n");
      break;
    case LC_DYLIB_CODE_SIGN_DRS: /* 0x2B */
      SNARE_PLT_DEBUG_CMD_MSG("LC_DYLIB_CODE_SIGN_DRS\n");
      break;
    case LC_BUILD_VERSION: /* 0x32 */
      SNARE_PLT_DEBUG_CMD_MSG("LC_BUILD_VERSION\n");
      break;
    case LC_DYLD_EXPORTS_TRIE: /* (0x33|LC_REQ_DYLD) */
      SNARE_PLT_DEBUG_CMD_MSG("LC_DYLD_EXPORTS_TRIE\n");
      break;
    case LC_DYLD_CHAINED_FIXUPS: /* (0x34|LC_REQ_DYLD) */
      data.chained_fixups = (struct linkedit_data_command *)cmd;
      SNARE_PLT_DEBUG_CMD_MSG(
          "LC_DYLD_CHAINED_FIXUPS\n"
          "  cmdsize   %u\n"
          "  dataoff   %u (0x%x)\n"
          "  datasize  %u\n",
          data.chained_fixups->cmdsize, data.chained_fixups->dataoff,
          data.chained_fixups->dataoff, data.chained_fixups->datasize);
      break;
    default:
      SNARE_PLT_DEBUG_CMD_MSG("LC_? (0x%x)\n", cmd->cmd);
    }
    cmd = (struct load_command *)((size_t)cmd + cmd->cmdsize);
  }
  SNARE_PLT_DEBUG_CMD_MSG("CMD END\n");
  if (data.linkedit_segment_idx == -1) {
    snare_plt_set_errmsg("Cannot find the linkedit segment: %s", image_name);
    return SNARE_PLT_INVALID_FILE_FORMAT;
  }
  if (data.chained_fixups != NULL) {
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 120000
    int rv = snare_plt_read_chained_fixups(&data, image_name);
    if (rv != 0) {
      return rv;
    }
#else
    snare_plt_set_errmsg("failed to read chained fixups segment, this is only "
                         "available after macOS 12 Monterrey: %s",
                         image_name);
    return SNARE_PLT_INTERNAL_ERROR;
#endif
  } else {
    nbind = 0;
    nbind = snare_plt_set_bind_addrs(&data, nbind, dyld_info->bind_off,
                                     dyld_info->bind_size, 0);
    nbind = snare_plt_set_bind_addrs(&data, nbind, dyld_info->weak_bind_off,
                                     dyld_info->weak_bind_size, 1);
    nbind = snare_plt_set_bind_addrs(&data, nbind, dyld_info->lazy_bind_off,
                                     dyld_info->lazy_bind_size, 0);
    size = offsetof(snare_plt_t, entries) +
           sizeof(snare_plt_bind_address_t) * nbind;
    data.plthook = (snare_plt_t *)calloc(1, size);
    if (data.plthook == NULL) {
      snare_plt_set_errmsg("failed to allocate memory: %" PRIuPTR " bytes",
                           size);
      return SNARE_PLT_OUT_OF_MEMORY;
    }
    data.plthook->num_entries = nbind;
    nbind = 0;
    nbind = snare_plt_set_bind_addrs(&data, nbind, dyld_info->bind_off,
                                     dyld_info->bind_size, 0);
    nbind = snare_plt_set_bind_addrs(&data, nbind, dyld_info->weak_bind_off,
                                     dyld_info->weak_bind_size, 1);
    nbind = snare_plt_set_bind_addrs(&data, nbind, dyld_info->lazy_bind_off,
                                     dyld_info->lazy_bind_size, 0);
  }
  snare_plt_set_mem_prot(data.plthook);

  *plthook_out = data.plthook;
  return 0;
}

static unsigned int snare_plt_set_bind_addrs(snare_plt_data_t *data,
                                             unsigned int idx,
                                             uint32_t bind_off,
                                             uint32_t bind_size, char weak) {
  const uint8_t *ptr = snare_plt_fileoff_to_vmaddr_in_segment(
      data, data->linkedit_segment_idx, bind_off);
  const uint8_t *end = ptr + bind_size;
  const char *sym_name;
  int seg_index = 0;
  uint64_t seg_offset = 0;
  int addend = 0;
  int count, skip;
#ifdef SNARE_PLT_DEBUG_BIND
  int cond = data->plthook != NULL;
#endif

  while (ptr < end) {
    uint8_t op = *ptr & BIND_OPCODE_MASK;
    uint8_t imm = *ptr & BIND_IMMEDIATE_MASK;
    int i;

    SNARE_PLT_DEBUG_BIND_IF(cond, "0x%02x: ", *ptr);
    ptr++;
    switch (op) {
    case BIND_OPCODE_DONE:
      SNARE_PLT_DEBUG_BIND_IF(cond, "BIND_OPCODE_DONE\n");
      break;
    case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
      SNARE_PLT_DEBUG_BIND_IF(
          cond, "BIND_OPCODE_SET_DYLIB_ORDINAL_IMM: ordinal = %u\n", imm);
      break;
    case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
#ifdef SNARE_PLT_DEBUG_BIND
      SNARE_PLT_DEBUG_BIND_IF(
          cond, "BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB: ordinal = %llu\n",
          snare_plt_uleb128(&ptr));
#else
      snare_plt_uleb128(&ptr);
#endif
      break;
    case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
      if (imm == 0) {
        SNARE_PLT_DEBUG_BIND_IF(
            cond, "BIND_OPCODE_SET_DYLIB_SPECIAL_IMM: ordinal = 0\n");
      } else {
        SNARE_PLT_DEBUG_BIND_IF(
            cond, "BIND_OPCODE_SET_DYLIB_SPECIAL_IMM: ordinal = %u\n",
            BIND_OPCODE_MASK | imm);
      }
      break;
    case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
      sym_name = (const char *)ptr;
      ptr += strlen(sym_name) + 1;
      SNARE_PLT_DEBUG_BIND_IF(
          cond, "BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM: sym_name = %s\n",
          sym_name);
      break;
    case BIND_OPCODE_SET_TYPE_IMM:
      SNARE_PLT_DEBUG_BIND_IF(cond, "BIND_OPCODE_SET_TYPE_IMM: type = %u\n",
                              imm);
      break;
    case BIND_OPCODE_SET_ADDEND_SLEB:
      addend = snare_plt_sleb128(&ptr);
      SNARE_PLT_DEBUG_BIND_IF(
          cond, "BIND_OPCODE_SET_ADDEND_SLEB: ordinal = %lld\n", addend);
      break;
    case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
      seg_index = imm;
      seg_offset = snare_plt_uleb128(&ptr);
      SNARE_PLT_DEBUG_BIND_IF(cond,
                              "BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB: "
                              "seg_index = %u, seg_offset = 0x%llx\n",
                              seg_index, seg_offset);
      break;
    case BIND_OPCODE_ADD_ADDR_ULEB:
      seg_offset += snare_plt_uleb128(&ptr);
      SNARE_PLT_DEBUG_BIND_IF(
          cond, "BIND_OPCODE_ADD_ADDR_ULEB: seg_offset = 0x%llx\n", seg_offset);
      break;
    case BIND_OPCODE_DO_BIND:
      snare_plt_set_bind_addr(data, &idx, sym_name, seg_index, seg_offset,
                              addend, weak);
      seg_offset += sizeof(void *);
      SNARE_PLT_DEBUG_BIND_IF(cond, "BIND_OPCODE_DO_BIND\n");
      break;
    case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
      snare_plt_set_bind_addr(data, &idx, sym_name, seg_index, seg_offset,
                              addend, weak);
      seg_offset += snare_plt_uleb128(&ptr) + sizeof(void *);
      SNARE_PLT_DEBUG_BIND_IF(
          cond, "BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB: seg_offset = 0x%llx\n",
          seg_offset);
      break;
    case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
      snare_plt_set_bind_addr(data, &idx, sym_name, seg_index, seg_offset,
                              addend, weak);
      seg_offset += imm * sizeof(void *) + sizeof(void *);
      SNARE_PLT_DEBUG_BIND_IF(cond,
                              "BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED\n");
      break;
    case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
      count = snare_plt_uleb128(&ptr);
      skip = snare_plt_uleb128(&ptr);
      for (i = 0; i < count; i++) {
        snare_plt_set_bind_addr(data, &idx, sym_name, seg_index, seg_offset,
                                addend, weak);
        seg_offset += skip + sizeof(void *);
      }
      SNARE_PLT_DEBUG_BIND_IF(cond,
                              "BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB\n");
      break;
    default:
      SNARE_PLT_DEBUG_BIND_IF(cond, "op: 0x%x, imm: 0x%x\n", op, imm);
    }
  }
  return idx;
}

static void snare_plt_set_bind_addr(snare_plt_data_t *data, unsigned int *idx,
                                    const char *sym_name, int seg_index,
                                    int seg_offset, int addend, char weak) {
  if (data->plthook != NULL) {
    size_t vmaddr = data->segments[seg_index]->vmaddr;
    snare_plt_bind_address_t *bind_addr = &data->plthook->entries[*idx];
    bind_addr->name = sym_name;
    bind_addr->addend = addend;
    bind_addr->weak = weak;
    bind_addr->addr = (void **)(vmaddr + data->slide + seg_offset);
    SNARE_PLT_DEBUG_BIND_MSG("bind_address[%u]: %s, %d, %d, %p, %p, %p\n", *idx,
                             sym_name, seg_index, seg_offset, (void *)vmaddr,
                             (void *)data->slide, bind_addr->addr);
  }
  (*idx)++;
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 120000
typedef struct {
  const char *image_name;
  FILE *fp;
  const struct dyld_chained_starts_in_image *starts;
  uint32_t seg_index;  /* i */
  uint16_t page_index; /* j */
  off_t offset;
} snare_plt_chained_fixups_iter_t;

typedef struct {
  uint16_t ptr_format;
  union {
    uint64_t raw;
    struct dyld_chained_ptr_64_rebase rebase;
    struct dyld_chained_ptr_64_bind bind;
    struct dyld_chained_ptr_arm64e_rebase arm64e_rebase;
    struct dyld_chained_ptr_arm64e_bind arm64e_bind;
    struct dyld_chained_ptr_arm64e_bind24 arm64e_bind24;
    struct dyld_chained_ptr_arm64e_auth_rebase arm64e_auth_rebase;
    struct dyld_chained_ptr_arm64e_auth_bind arm64e_auth_bind;
    struct dyld_chained_ptr_arm64e_auth_bind24 arm64e_auth_bind24;
  } ptr;
  off_t offset;
} snare_plt_chained_fixups_entry_t;

static int snare_plt_chained_fixups_iter_init(
    snare_plt_chained_fixups_iter_t *iter, const char *image_name,
    const struct dyld_chained_starts_in_image *starts_offset);
static void
snare_plt_chained_fixups_iter_deinit(snare_plt_chained_fixups_iter_t *iter);
static int
snare_plt_chained_fixups_iter_rewind(snare_plt_chained_fixups_iter_t *iter);
static int
snare_plt_chained_fixups_iter_next(snare_plt_chained_fixups_iter_t *iter,
                                   snare_plt_chained_fixups_entry_t *entry);

static int snare_plt_chained_fixups_iter_init(
    snare_plt_chained_fixups_iter_t *iter, const char *image_name,
    const struct dyld_chained_starts_in_image *starts) {
  memset(iter, 0, sizeof(*iter));
  iter->fp = fopen(image_name, "r");
  if (iter->fp == NULL) {
    snare_plt_set_errmsg("failed to open file %s (error: %s)", image_name,
                         strerror(errno));
    return SNARE_PLT_FILE_NOT_FOUND;
  }
  iter->image_name = image_name;
  iter->starts = starts;
  return 0;
}

static void
snare_plt_chained_fixups_iter_deinit(snare_plt_chained_fixups_iter_t *iter) {
  if (iter->fp != NULL) {
    fclose(iter->fp);
    iter->fp = NULL;
  }
}

static int
snare_plt_chained_fixups_iter_rewind(snare_plt_chained_fixups_iter_t *iter) {
  iter->seg_index = 0;
  iter->page_index = 0;
  iter->offset = 0;
  return 0;
}

static int
snare_plt_chained_fixups_iter_next(snare_plt_chained_fixups_iter_t *iter,
                                   snare_plt_chained_fixups_entry_t *entry) {
  const struct dyld_chained_starts_in_image *starts = iter->starts;
  uint32_t i = iter->seg_index;
  uint16_t j = iter->page_index;
  off_t offset = iter->offset;

next_segment:
  if (i == starts->seg_count) {
    return -1;
  }
  if (j == 0 && offset == 0) {
    SNARE_PLT_DEBUG_FIXUPS_MSG("  seg_info_offset[%u] %u\n", i,
                               starts->seg_info_offset[i]);
  }
  if (starts->seg_info_offset[i] == 0) {
    i++;
    j = 0;
    offset = 0;
    goto next_segment;
  }
  const struct dyld_chained_starts_in_segment *seg =
      (const struct dyld_chained_starts_in_segment
           *)((char *)starts + starts->seg_info_offset[i]);
  if (j == 0 && offset == 0) {
    SNARE_PLT_DEBUG_FIXUPS_MSG("    dyld_chained_starts_in_segment\n"
                               "      size              %u\n"
                               "      page_size         0x%x\n"
                               "      pointer_format    %u\n"
                               "      segment_offset    %llu (0x%llx)\n"
                               "      max_valid_pointer %u\n"
                               "      page_count        %u\n",
                               seg->size, seg->page_size, seg->pointer_format,
                               seg->segment_offset, seg->segment_offset,
                               seg->max_valid_pointer, seg->page_count);
  }
next_page:
  if (j == seg->page_count) {
    i++;
    j = 0;
    offset = 0;
    goto next_segment;
  }

  if (seg->page_start[j] == DYLD_CHAINED_PTR_START_NONE) {
    SNARE_PLT_DEBUG_FIXUPS_MSG(
        "      page_start[%u]     DYLD_CHAINED_PTR_START_NONE\n", j);
    j++;
    offset = 0;
    goto next_page;
  }
  if (offset == 0) {
    SNARE_PLT_DEBUG_FIXUPS_MSG("      page_start[%u]     %u\n", j,
                               seg->page_start[j]);
  }
  if (offset == 0) {
    offset = seg->segment_offset + j * seg->page_size + seg->page_start[j];
  }
  if (fseeko(iter->fp, offset, SEEK_SET) != 0) {
    snare_plt_set_errmsg("failed to seek to %lld in %s", offset,
                         iter->image_name);
    return SNARE_PLT_INVALID_FILE_FORMAT;
  }
  entry->ptr_format = seg->pointer_format;
  if (fread(&entry->ptr, sizeof(entry->ptr), 1, iter->fp) != 1) {
    snare_plt_set_errmsg("failed to read fixup chain from %s",
                         iter->image_name);
    return SNARE_PLT_INVALID_FILE_FORMAT;
  }
  entry->offset = offset;
  switch (seg->pointer_format) {
  case DYLD_CHAINED_PTR_64_OFFSET:
    if (entry->ptr.bind.next) {
      offset += entry->ptr.bind.next * 4;
    } else {
      j++;
      offset = 0;
    }
    break;
  default:
    snare_plt_set_errmsg("unsupported pointer format %u in %s",
                         seg->pointer_format, iter->image_name);
    return SNARE_PLT_INTERNAL_ERROR;
  }
  iter->seg_index = i;
  iter->page_index = j;
  iter->offset = offset;
  return 0;
}

static int snare_plt_read_chained_fixups(snare_plt_data_t *d,
                                         const char *image_name) {
  const uint8_t *ptr = snare_plt_fileoff_to_vmaddr_in_segment(
      d, d->linkedit_segment_idx, d->chained_fixups->dataoff);
  const struct dyld_chained_fixups_header *header =
      (const struct dyld_chained_fixups_header *)ptr;
  const char *symbol_pool = (const char *)ptr + header->symbols_offset;
  int rv;
  unsigned int num_binds;
  size_t size;
  const struct dyld_chained_starts_in_image *starts =
      (const struct dyld_chained_starts_in_image *)(ptr +
                                                    header->starts_offset);
  const struct dyld_chained_import *import =
      (const struct dyld_chained_import *)(ptr + header->imports_offset);
  snare_plt_chained_fixups_iter_t iter;
  snare_plt_chained_fixups_entry_t entry;

  memset(&iter, 0, sizeof(snare_plt_chained_fixups_iter_t));

  rv = snare_plt_chained_fixups_iter_init(&iter, image_name, starts);
  if (rv != 0) {
    return rv;
  }

  SNARE_PLT_DEBUG_FIXUPS_MSG("dyld_chained_fixups_header\n"
                             "  fixups_version  %u\n"
                             "  starts_offset   %u\n"
                             "  imports_offset  %u\n"
                             "  symbols_offset  %u\n"
                             "  imports_count   %u\n"
                             "  imports_format  %u\n"
                             "  symbols_format  %u\n",
                             header->fixups_version, header->starts_offset,
                             header->imports_offset, header->symbols_offset,
                             header->imports_count, header->imports_format,
                             header->symbols_format);
  if (header->fixups_version != 0) {
    snare_plt_set_errmsg("unknown chained fixups version %u",
                         header->fixups_version);
    rv = SNARE_PLT_INVALID_FILE_FORMAT;
    goto cleanup;
  }

  SNARE_PLT_DEBUG_FIXUPS_MSG("dyld_chained_starts_in_image\n"
                             "  seg_count       %u\n",
                             starts->seg_count);
  num_binds = 0;
  while ((rv = snare_plt_chained_fixups_iter_next(&iter, &entry)) == 0) {
    if (entry.ptr_format == DYLD_CHAINED_PTR_64_OFFSET && entry.ptr.bind.bind) {
      num_binds++;
    }
#if 0
        if (entry.ptr.rebase.bind) {
            SNARE_PLT_DEBUG_FIXUPS_MSG("  0x%08lX:  raw: 0x%016lX         bind: (next: %03u, ordinal: %06X, addend: %d)\n",
                         entry.offset, entry.ptr.raw, entry.ptr.bind.next, entry.ptr.bind.ordinal, entry.ptr.bind.addend);
        } else {
            SNARE_PLT_DEBUG_FIXUPS_MSG("  0x%08lX:  raw: 0x%016lX       rebase: (next: %03u, target: 0x%011lX, high8: 0x%02X)\n",
                         entry.offset, entry.ptr.raw, entry.ptr.rebase.next, entry.ptr.rebase.target, entry.ptr.rebase.high8);
        }
#endif
  }
  if (rv > 0) {
    goto cleanup;
  }

  size = offsetof(snare_plt_t, entries) +
         sizeof(snare_plt_bind_address_t) * num_binds;
  d->plthook = (snare_plt_t *)calloc(1, size);
  if (d->plthook == NULL) {
    snare_plt_set_errmsg("failed to allocate memory: %" PRIuPTR " bytes", size);
    rv = SNARE_PLT_OUT_OF_MEMORY;
    goto cleanup;
  }
  d->plthook->num_entries = num_binds;

  snare_plt_chained_fixups_iter_rewind(&iter);
  num_binds = 0;
  while ((rv = snare_plt_chained_fixups_iter_next(&iter, &entry)) == 0) {
    if (entry.ptr_format == DYLD_CHAINED_PTR_64_OFFSET && entry.ptr.bind.bind) {
      uint16_t ordinal = entry.ptr.bind.ordinal;
      uint32_t name_offset;
      char weak = 0;
      snare_plt_bind_address_t *bind_addr = &d->plthook->entries[num_binds];
#ifdef SNARE_PLT_DEBUG_FIXUPS
      int32_t lib_ordinal;
      const char *libname;
#endif
      switch (header->imports_format) {
      case DYLD_CHAINED_IMPORT:
        name_offset = import[ordinal].name_offset;
        weak = import[ordinal].weak_import;
#ifdef SNARE_PLT_DEBUG_FIXUPS
        if (import[ordinal].lib_ordinal >=
            (uint8_t)BIND_SPECIAL_DYLIB_WEAK_LOOKUP) {
          lib_ordinal = (int8_t)import[ordinal].lib_ordinal;
        } else {
          lib_ordinal = (uint8_t)import[ordinal].lib_ordinal;
        }
#endif
        break;
      default:
        SNARE_PLT_DEBUG_FIXUPS_MSG("imports_format: %u\n",
                                   header->imports_format);
        snare_plt_set_errmsg("unsupported imports format %u",
                             header->imports_format);
        rv = SNARE_PLT_INTERNAL_ERROR;
        goto cleanup;
      }
      bind_addr->name = symbol_pool + name_offset;
      bind_addr->addr = (void **)snare_plt_fileoff_to_vmaddr(d, entry.offset);
      bind_addr->addend = entry.ptr.bind.addend;
      bind_addr->weak = weak;
#ifdef SNARE_PLT_DEBUG_FIXUPS
      switch (lib_ordinal) {
      case BIND_SPECIAL_DYLIB_SELF:
        libname = "this-image";
        break;
      case BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE:
        libname = "main-executable";
        break;
      case BIND_SPECIAL_DYLIB_FLAT_LOOKUP:
        libname = "flat-namespace";
        break;
      case BIND_SPECIAL_DYLIB_WEAK_LOOKUP:
        libname = "weak";
        break;
      default:
        libname = "?";
      }
#endif
      SNARE_PLT_DEBUG_FIXUPS_MSG(
          "        %-12s %-16s 0x%08llX              bind  %s/%s",
          snare_plt_segment_name_from_addr(d, entry.offset),
          snare_plt_section_name_from_addr(d, entry.offset), entry.offset,
          libname, symbol_pool + name_offset);
      if (entry.ptr.bind.addend != 0) {
        SNARE_PLT_DEBUG_FIXUPS_MSG(" + 0x%X", entry.ptr.bind.addend);
      }
      if (weak) {
        SNARE_PLT_DEBUG_FIXUPS_MSG(" [weak-import]");
      }
      SNARE_PLT_DEBUG_FIXUPS_MSG("\n");
      num_binds++;
    } else if (entry.ptr_format == DYLD_CHAINED_PTR_64_OFFSET &&
               !entry.ptr.bind.bind) {
      SNARE_PLT_DEBUG_FIXUPS_MSG(
          "        %-12s %-16s 0x%08llX            rebase  0x%08llX\n",
          snare_plt_segment_name_from_addr(d, entry.offset),
          snare_plt_section_name_from_addr(d, entry.offset), entry.offset,
          entry.ptr.rebase.target);
    }
  }
  snare_plt_chained_fixups_iter_deinit(&iter);
  rv = 0;
cleanup:
  snare_plt_chained_fixups_iter_deinit(&iter);
  if (rv != 0 && d->plthook) {
    free(d->plthook);
    d->plthook = NULL;
  }
  return rv;
}

static uint8_t *snare_plt_fileoff_to_vmaddr(snare_plt_data_t *d,
                                            size_t offset) {
  int i;
  for (i = 0; i < d->num_segments; i++) {
    const struct segment_command_64 *seg = d->segments[i];
    if (seg->fileoff <= offset && offset < seg->fileoff + seg->filesize) {
      return snare_plt_fileoff_to_vmaddr_in_segment(d, i, offset);
    }
  }
  return NULL;
}

#ifdef SNARE_PLT_DEBUG_FIXUPS
static const char *snare_plt_segment_name_from_addr(snare_plt_data_t *d,
                                                    size_t addr) {
  int i;
  for (i = 0; i < d->num_segments; i++) {
    const struct segment_command_64 *seg = d->segments[i];
    if (seg->fileoff <= addr && addr < seg->fileoff + seg->filesize) {
      return seg->segname;
    }
  }
  return "?";
}

static const char *snare_plt_section_name_from_addr(snare_plt_data_t *d,
                                                    size_t addr) {
  int i;
  for (i = 0; i < d->num_sections; i++) {
    const struct section_64 *sec = d->sections[i];
    if (sec->offset <= addr && addr < sec->offset + sec->size) {
      return sec->sectname;
    }
  }
  return "?";
}
#endif
#endif

static int snare_plt_set_mem_prot(snare_plt_t *plthook) {
  unsigned int pos = 0;
  const char *name;
  void **addr;
  size_t start = (size_t)-1;
  size_t end = 0;
  mach_port_t task = mach_task_self();
  vm_address_t vm_addr = 0;
  vm_size_t vm_size;
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
  memory_object_name_t object = 0;
  int idx = 0;

  while (snare_plt_enum(plthook, &pos, &name, &addr) == 0) {
    if (start > (size_t)addr) {
      start = (size_t)addr;
    }
    if (end < (size_t)addr) {
      end = (size_t)addr;
    }
  }
  end++;

  while (vm_region_64(task, &vm_addr, &vm_size, VM_REGION_BASIC_INFO_64,
                      (vm_region_info_t)&info, &info_count,
                      &object) == KERN_SUCCESS) {
    snare_plt_mem_prot_t mem_prot = {vm_addr, vm_addr + vm_size,
                                     info.protection &
                                         (PROT_READ | PROT_WRITE | PROT_EXEC)};
    if (mem_prot.prot != 0 && mem_prot.start < end && start < mem_prot.end) {
      plthook->mem_prot[idx++] = mem_prot;
      if (idx == SNARE_PLT_NUM_MEM_PROT) {
        break;
      }
    }
    vm_addr += vm_size;
  }
  return 0;
}

static int snare_plt_get_mem_prot(snare_plt_t *plthook, void *addr) {
  snare_plt_mem_prot_t *ptr = plthook->mem_prot;
  snare_plt_mem_prot_t *end = ptr + SNARE_PLT_NUM_MEM_PROT;

  while (ptr < end && ptr->prot != 0) {
    if (ptr->start <= (size_t)addr && (size_t)addr < ptr->end) {
      return ptr->prot;
    }
    ++ptr;
  }
  return 0;
}

SNARE_EXPORT int SNARE_API snare_plt_enum(snare_plt_t *plthook,
                                          unsigned int *pos,
                                          const char **name_out,
                                          void ***addr_out) {
  snare_plt_entry_t entry;
  int rv = snare_plt_enum_entry(plthook, pos, &entry);
  if (rv == 0) {
    *name_out = entry.name;
    *addr_out = entry.addr;
  }
  return rv;
}

SNARE_EXPORT int SNARE_API snare_plt_enum_with_prot(snare_plt_t *plthook,
                                                    unsigned int *pos,
                                                    const char **name_out,
                                                    void ***addr_out,
                                                    int *prot) {
  snare_plt_entry_t entry;
  int rv = snare_plt_enum_entry(plthook, pos, &entry);
  if (rv == 0) {
    *name_out = entry.name;
    *addr_out = entry.addr;
    if (prot) {
      *prot = entry.prot;
    }
  }
  return rv;
}

SNARE_EXPORT int SNARE_API snare_plt_enum_entry(snare_plt_t *plthook,
                                                unsigned int *pos,
                                                snare_plt_entry_t *entry) {
  memset(entry, 0, sizeof(*entry));
  while (*pos < plthook->num_entries) {
    if (strcmp(plthook->entries[*pos].name, "__tlv_bootstrap") == 0) {
      (*pos)++;
      continue;
    }
    entry->name = plthook->entries[*pos].name;
    entry->addr = plthook->entries[*pos].addr;
    entry->addend = plthook->entries[*pos].addend;
    entry->prot = snare_plt_get_mem_prot(plthook, entry->addr);
    entry->weak = plthook->entries[*pos].weak;
    (*pos)++;
    return 0;
  }
  return EOF;
}

SNARE_EXPORT int SNARE_API snare_plt_replace(snare_plt_t *plthook,
                                             const char *funcname,
                                             void *funcaddr, void **oldfunc) {
  size_t funcnamelen = strlen(funcname);
  unsigned int pos = 0;
  snare_plt_entry_t entry;
  int rv;

  if (plthook == NULL) {
    snare_plt_set_errmsg("invalid argument: The first argument is null.");
    return SNARE_PLT_INVALID_ARGUMENT;
  }
  while ((rv = snare_plt_enum_entry(plthook, &pos, &entry)) == 0) {
    const char *name = entry.name;
    void **addr = entry.addr;
    if (strncmp(name, funcname, funcnamelen) == 0) {
      if (name[funcnamelen] == '\0' || name[funcnamelen] == '$') {
        goto matched;
      }
    }
    if (name[0] == '@') {
      /* I doubt this code... */
      name++;
      if (strncmp(name, funcname, funcnamelen) == 0) {
        if (name[funcnamelen] == '\0' || name[funcnamelen] == '$') {
          goto matched;
        }
      }
    }
    if (name[0] == '_') {
      name++;
      if (strncmp(name, funcname, funcnamelen) == 0) {
        if (name[funcnamelen] == '\0' || name[funcnamelen] == '$') {
          goto matched;
        }
      }
    }
    continue;
  matched:
    if (oldfunc) {
      *oldfunc = *addr;
    }
    if (!(entry.prot & PROT_WRITE)) {
      size_t page_size = sysconf(_SC_PAGESIZE);
      void *base = (void *)((size_t)addr & ~(page_size - 1));
      if (mprotect(base, page_size, PROT_READ | PROT_WRITE) != 0) {
        snare_plt_set_errmsg("Cannot change memory protection at address %p",
                             base);
        return SNARE_PLT_INTERNAL_ERROR;
      }
      *addr = funcaddr;
      mprotect(base, page_size, entry.prot);
    } else {
      *addr = funcaddr;
    }
    return 0;
  }
  if (rv == EOF) {
    snare_plt_set_errmsg("no such function: %s", funcname);
    rv = SNARE_PLT_FUNCTION_NOT_FOUND;
  }
  return rv;
}

SNARE_EXPORT void SNARE_API snare_plt_close(snare_plt_t *plthook) {
  if (plthook != NULL) {
    free(plthook);
  }
  return;
}

SNARE_EXPORT const char *SNARE_API snare_plt_error(void) {
  return snare_plt_errmsg;
}

static void snare_plt_set_errmsg(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(snare_plt_errmsg, sizeof(snare_plt_errmsg) - 1, fmt, ap);
  va_end(ap);
}

#endif
#endif /* SNARE_IMPLEMENTATION */
#endif /* SNARE_H */
