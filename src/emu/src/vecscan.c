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

#include <hs.h>
#include <assert.h>

#include "vecscan.h"
#include "log.h"

/**
 * vector scan a target string from a regex selector pool
 * sz_re_pool && file_content are not freed by this function.
 */
int vecscan_multi(const char** sz_re_pool, uint32_t u_re_count, char* file_content, uint32_t u_file_size, match_list_t* out_matches)
{
    assert(hs_valid_platform() == HS_SUCCESS);

    hs_database_t* db = NULL;
    hs_compile_error_t* err = NULL;
    hs_scratch_t* scratch = NULL;
    unsigned int* flags = NULL;
    unsigned int* ids = NULL;
    int ret = -1;

    flags = malloc(u_re_count * sizeof(unsigned int));
    ids = malloc(u_re_count * sizeof(unsigned int));

    if (!flags || !ids) {
        log_error("failed to allocate flags &| ids\n");
        goto cleanup;
    }

    /** construct the matches object, used to store the matched files */
    match_list_t matches = { 0 };
    if (match_list_alloc(&matches, u_re_count) != 0) {
        log_error("failed to allocate match list!\n");
        goto cleanup;
    }

    for (int i = 0; i < u_re_count; i++) {
        flags[i] = HS_FLAG_SOM_LEFTMOST;
        ids[i] = i;
    }

    if (hs_compile_multi(sz_re_pool, flags, ids, u_re_count, HS_MODE_BLOCK, NULL, &db, &err) != HS_SUCCESS) {
        log_error("Compile error: %s\n", err->message);
        hs_free_compile_error(err);
        match_list_destroy(&matches);
        goto cleanup;
    }

    hs_error_t hs_err = hs_alloc_scratch(db, &scratch);
    if (hs_err != HS_SUCCESS) {
        log_error("Failed to allocate scratch space: error code %d\n", hs_err);
        match_list_destroy(&matches);
        goto cleanup;
    }

    hs_scan(db, file_content, u_file_size, 0, scratch, match_list_vecscan_handler, (void*)&matches);

    /** return matches */
    if (out_matches) {
        *out_matches = matches;
    } else {
        match_list_destroy(&matches);
    }

    ret = 0;

cleanup:
    if (scratch) hs_free_scratch(scratch);
    if (db) hs_free_database(db);
    free(flags);
    free(ids);
    return ret;
}
