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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"

int on_match(unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void* ctx)
{
    const char* data = (const char*)ctx;
    log_info("MATCH: [%llu, %llu)\n", from, to);
    fwrite(data + from, 1, to - from, stderr);
    fprintf(stderr, "\n");
    return 0;
}

int main(int argc, char** argv)
{
    char* fpath = argv[1];
    char* rpath = argv[2];

    // Read test data from hello.txt
    FILE* data_file = fopen(fpath, "rb");
    if (!data_file) {
        log_info("Failed to open %s\n", fpath);
        return 1;
    }
    fseek(data_file, 0, SEEK_END);
    size_t data_len = ftell(data_file);
    fseek(data_file, 0, SEEK_SET);

    char* test_data = (char*)malloc(data_len + 1);
    if (!test_data) {
        log_info("Failed to allocate memory for test data\n");
        fclose(data_file);
        return 1;
    }
    fread(test_data, 1, data_len, data_file);
    test_data[data_len] = '\0';
    fclose(data_file);

    // Read pattern from selector.txt
    FILE* selector_file = fopen(rpath, "rb");
    if (!selector_file) {
        log_info("Failed to open %s\n", rpath);
        free(test_data);
        return 1;
    }
    fseek(selector_file, 0, SEEK_END);
    size_t pattern_len = ftell(selector_file);
    fseek(selector_file, 0, SEEK_SET);

    char* pattern = (char*)malloc(pattern_len + 1);
    if (!pattern) {
        log_info("Failed to allocate memory for pattern\n");
        fclose(selector_file);
        free(test_data);
        return 1;
    }
    fread(pattern, 1, pattern_len, selector_file);
    pattern[pattern_len] = '\0';
    fclose(selector_file);

    // Remove trailing newline if present
    if (pattern_len > 0 && pattern[pattern_len - 1] == '\n') {
        pattern[pattern_len - 1] = '\0';
    }

    hs_database_t* db;
    hs_compile_error_t* err;
    hs_scratch_t* scratch = {};

    const char* patterns[] = { pattern };
    unsigned int flags[] = { HS_FLAG_SOM_LEFTMOST };
    unsigned int ids[] = { 0 };
    int pattern_count = 1;

    log_info("Compiling pattern...\n");
    if (hs_compile_multi(patterns, flags, ids, pattern_count, HS_MODE_BLOCK, NULL, &db, &err) != HS_SUCCESS) {
        log_info("Compile error: %s\n", err->message);
        hs_free_compile_error(err);
        free(pattern);
        free(test_data);
        return 1;
    }
    log_info("Pattern compiled successfully\n");

    if (hs_alloc_scratch(db, &scratch) != HS_SUCCESS) {
        log_info("Failed to allocate scratch space\n");
        hs_free_database(db);
        free(pattern);
        free(test_data);
        return 1;
    }
    log_info("Scratch space allocated\n");

    log_info("Starting scan of %zu bytes...\n", data_len);
    int ret = hs_scan(db, test_data, data_len, 0, scratch, on_match, (void*)test_data);
    log_info("Scan returned: %d\n", ret);

    hs_free_scratch(scratch);
    hs_free_database(db);
    free(pattern);
    free(test_data);

    log_info("Finished scanning content!\n");
    return 0;
}
