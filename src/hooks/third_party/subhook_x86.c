/* Copyright (c) 2012-2014 Zeex
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

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "subhook.h"
#include "subhook_private.h"

#ifdef SUBHOOK_WINDOWS
typedef unsigned __int8 uint8_t;
typedef __int32 int32_t;
#if SUBHOOK_BITS == 64
typedef __int64 intptr_t;
#elif SUBHOOK_BITS == 32
typedef __int32 intptr_t;
#endif
#else
#include <stdint.h>
#endif

#define JMP_INSN_OPCODE 0xE9
#define JMP_INSN_LEN sizeof(struct subhook_jmp)

#define MAX_INSN_LEN 15
#define MAX_TRAMPOLINE_LEN (JMP_INSN_LEN + MAX_INSN_LEN - 1)

#pragma pack(push, 1)

struct subhook_jmp
{
    uint8_t opcode;
    int32_t offset;
};

#pragma pack(pop)

static size_t subhook_disasm(uint8_t* code, int* reloc)
{
    enum flags
    {
        MODRM = 1,
        PLUS_R = 1 << 1,
        REG_OPCODE = 1 << 2,
        IMM8 = 1 << 3,
        IMM16 = 1 << 4,
        IMM32 = 1 << 5,
        RELOC = 1 << 6
    };

    static int prefixes[] = {
        0xF0, 0xF2, 0xF3, 0x2E, 0x36, 0x3E, 0x26, 0x64, 0x65, 0x66, /* operand size override */
        0x67                                                        /* address size override */
    };

    struct opcode_info
    {
        int opcode;
        int reg_opcode;
        int flags;
    };

    static struct opcode_info opcodes[] = {
        /* CALL rel32       */ { 0xE8, 0, IMM32 | RELOC              },
        /* CALL r/m32       */
        { 0xFF, 2, MODRM | REG_OPCODE         },
        /* JMP rel32        */
        { 0xE9, 0, IMM32 | RELOC              },
        /* JMP r/m32        */
        { 0xFF, 4, MODRM | REG_OPCODE         },
        /* LEA r16,m        */
        { 0x8D, 0, MODRM                      },
        /* MOV r/m8,r8      */
        { 0x88, 0, MODRM                      },
        /* MOV r/m32,r32    */
        { 0x89, 0, MODRM                      },
        /* MOV r8,r/m8      */
        { 0x8A, 0, MODRM                      },
        /* MOV r32,r/m32    */
        { 0x8B, 0, MODRM                      },
        /* MOV r/m16,Sreg   */
        { 0x8C, 0, MODRM                      },
        /* MOV Sreg,r/m16   */
        { 0x8E, 0, MODRM                      },
        /* MOV AL,moffs8    */
        { 0xA0, 0, IMM8                       },
        /* MOV EAX,moffs32  */
        { 0xA1, 0, IMM32                      },
        /* MOV moffs8,AL    */
        { 0xA2, 0, IMM8                       },
        /* MOV moffs32,EAX  */
        { 0xA3, 0, IMM32                      },
        /* MOV r8, imm8     */
        { 0xB0, 0, PLUS_R | IMM8              },
        /* MOV r32, imm32   */
        { 0xB8, 0, PLUS_R | IMM32             },
        /* MOV r/m8, imm8   */
        { 0xC6, 0, MODRM | REG_OPCODE | IMM8  },
        /* MOV r/m32, imm32 */
        { 0xC7, 0, MODRM | REG_OPCODE | IMM32 },
        /* POP r/m32        */
        { 0x8F, 0, MODRM | REG_OPCODE         },
        /* POP r32          */
        { 0x58, 0, PLUS_R                     },
        /* PUSH r/m32       */
        { 0xFF, 6, MODRM | REG_OPCODE         },
        /* PUSH r32         */
        { 0x50, 0, PLUS_R                     },
        /* PUSH imm8        */
        { 0x6A, 0, IMM8                       },
        /* PUSH imm32       */
        { 0x68, 0, IMM32                      },
        /* RET              */
        { 0xC3, 0, 0                          },
        /* RET imm16        */
        { 0xC2, 0, IMM16                      },
        /* SUB r/m32, imm8  */
        { 0x83, 5, MODRM | REG_OPCODE | IMM8  },
        /* SUB r/m32, r32   */
        { 0x29, 0, MODRM                      },
        /* SUB r32, r/m32   */
        { 0x2B, 0, MODRM                      }
    };

    int i;
    int len = 0;
    int operand_size = 4;
    int address_size = 4;
    (void)address_size;
    int opcode = 0;

    for (i = 0; i < (int)(sizeof(prefixes) / sizeof(*prefixes)); i++) {
        if (code[len] == prefixes[i]) {
            len++;
            if (prefixes[i] == 0x66)
                operand_size = 2;
            if (prefixes[i] == 0x67)
                address_size = SUBHOOK_BITS / 8 / 2;
        }
    }

    for (i = 0; i < (int)(sizeof(opcodes) / sizeof(*opcodes)); i++) {
        int found = 0;

        if (code[len] == opcodes[i].opcode)
            found = !(opcodes[i].flags & REG_OPCODE) || ((code[len + 1] >> 3) & 7) == opcodes[i].reg_opcode;

        if ((opcodes[i].flags & PLUS_R) && (code[len] & 0xF8) == opcodes[i].opcode)
            found = 1;

        if (found) {
            opcode = code[len++];
            break;
        }
    }

    if (opcode == 0)
        return 0;

    if (reloc != NULL && opcodes[i].flags & RELOC)
        *reloc = len; /* relative call or jump */

    if (opcodes[i].flags & MODRM) {
        int modrm = code[len++];
        int mod = modrm >> 6;
        int rm = modrm & 7;

        if (mod != 3 && rm == 4)
            len++; /* for SIB */

#ifdef SUBHOOK_X86_64
        if (reloc != NULL && rm == 5)
            *reloc = len; /* RIP-relative addressing */
#endif

        if (mod == 1)
            len += 1; /* for disp8 */
        if (mod == 2 || (mod == 0 && rm == 5))
            len += 4; /* for disp32 */
    }

    if (opcodes[i].flags & IMM8)
        len += 1;
    if (opcodes[i].flags & IMM16)
        len += 2;
    if (opcodes[i].flags & IMM32)
        len += operand_size;
    ;

    return len;
}

static size_t subhook_make_jmp(uint8_t* src, uint8_t* dst, int32_t offset)
{
    struct subhook_jmp* jmp = (struct subhook_jmp*)(src + offset);

    jmp->opcode = JMP_INSN_OPCODE;
    jmp->offset = dst - (src + JMP_INSN_LEN);

    return sizeof(jmp);
}

static size_t subhook_make_trampoline(uint8_t* trampoline, uint8_t* src)
{
    size_t orig_size = 0;
    size_t insn_len;

    while (orig_size < JMP_INSN_LEN) {
        int reloc = 0;

        insn_len = subhook_disasm(src + orig_size, &reloc);

        if (insn_len == 0)
            return 0;

        memcpy(trampoline + orig_size, src + orig_size, insn_len);

        if (reloc > 0)
            *(int32_t*)(trampoline + orig_size + reloc) -= (intptr_t)trampoline - (intptr_t)src;

        orig_size += insn_len;
    }

    return orig_size + subhook_make_jmp(trampoline, src, orig_size);
}

SUBHOOK_EXPORT subhook_t SUBHOOK_API subhook_new(void* src, void* dst)
{
    subhook_t hook;

    if ((hook = malloc(sizeof(*hook))) == NULL)
        return NULL;

    hook->installed = 0;
    hook->src = src;
    hook->dst = dst;

    if ((hook->code = malloc(JMP_INSN_LEN)) == NULL) {
        free(hook);
        return NULL;
    }

    memcpy(hook->code, hook->src, JMP_INSN_LEN);

    if ((hook->trampoline = calloc(1, MAX_TRAMPOLINE_LEN)) == NULL) {
        free(hook->code);
        free(hook);
        return NULL;
    }

    if (subhook_unprotect(hook->src, JMP_INSN_LEN) == NULL || subhook_unprotect(hook->trampoline, MAX_TRAMPOLINE_LEN) == NULL) {
        free(hook->trampoline);
        free(hook->code);
        free(hook);
        return NULL;
    }

    if (subhook_make_trampoline(hook->trampoline, hook->src) == 0) {
        free(hook->trampoline);
        hook->trampoline = NULL;
    }

    return hook;
}

SUBHOOK_EXPORT void SUBHOOK_API subhook_free(subhook_t hook)
{
    free(hook->trampoline);
    free(hook->code);
    free(hook);
}

SUBHOOK_EXPORT void* SUBHOOK_API subhook_get_trampoline(subhook_t hook)
{
    return hook->trampoline;
}

SUBHOOK_EXPORT void* SUBHOOK_API subhook_get_src(subhook_t hook)
{
    return hook->src;
}

SUBHOOK_EXPORT int SUBHOOK_API subhook_install(subhook_t hook)
{
    if (hook->installed)
        return -EINVAL;

    subhook_make_jmp(hook->src, hook->dst, 0);
    hook->installed = 1;

    return 0;
}

SUBHOOK_EXPORT int SUBHOOK_API subhook_remove(subhook_t hook)
{
    if (!hook->installed)
        return -EINVAL;

    memcpy(hook->src, hook->code, JMP_INSN_LEN);
    hook->installed = 0;

    return 0;
}

SUBHOOK_EXPORT void* SUBHOOK_API subhook_read_dst(void* src)
{
    struct subhook_jmp* maybe_jmp = (struct subhook_jmp*)src;

    if (maybe_jmp->opcode != JMP_INSN_OPCODE)
        return NULL;

    return (void*)(maybe_jmp->offset + (uint8_t*)src + JMP_INSN_LEN);
}
