/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "smem.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        unsigned int* ids;
        unsigned long long* froms;
        unsigned long long* tos;
        int count;
        int capacity;
    } match_list_t;

    typedef struct
    {
        const char** matches;
        const char** replaces;
        int count;
    } transform_data_t;

    typedef struct
    {
        const char** finds;
        unsigned int* flags;
        unsigned int* ids;
    } finds_from_file_match_t;

    int get_transform_from_matches(lb_shm_arena_t* arena, match_list_t* matches, const char*** out_finds, transform_data_t** out_transforms);
    int match_list_alloc(match_list_t* m, unsigned int count);
    void match_list_destroy(match_list_t* m);
    int match_list_vecscan_handler(unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void* ctx);

#ifdef __cplusplus
}
#endif
