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

#define PCRE2_CODE_UNIT_WIDTH 8

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <hs.h>
#include <pcre2.h>

#include "match.h"
#include "smem.h"
#include "log.h"
#include "vecscan.h"

typedef struct
{
    uint32_t match_id;
    const char* plugin_name;
} match_plugin_map_t;

static int resize_pools(const char*** file_pool, match_plugin_map_t** plugin_map, uint32_t* capacity)
{
    uint32_t new_capacity = *capacity ? *capacity * 2 : 16;

    const char** new_pool = realloc(*file_pool, new_capacity * sizeof(char*));
    match_plugin_map_t* new_map = realloc(*plugin_map, new_capacity * sizeof(match_plugin_map_t));

    if (!new_pool || !new_map) {
        free(new_pool);
        free(*file_pool);
        free(*plugin_map);
        return -1;
    }

    *file_pool = new_pool;
    *plugin_map = new_map;
    *capacity = new_capacity;
    return 0;
}

static void add_file_entry(const char** file_pool, match_plugin_map_t* plugin_map, uint32_t index, const char* file, const char* plugin_name)
{
    file_pool[index] = file;
    plugin_map[index].match_id = index;
    plugin_map[index].plugin_name = plugin_name;
}

static int process_patch_list(lb_shm_arena_t* arena, lb_patch_list_shm_t* list, const char* plugin_name, const char*** file_pool, match_plugin_map_t** plugin_map,
                              uint32_t* capacity, uint32_t* file_count)
{
    if (!list->patches_off) return 0;

    lb_patch_shm_t* patches = SHM_PTR(arena, list->patches_off, lb_patch_shm_t);

    for (uint32_t j = 0; j < list->patch_count; j++) {
        if (*file_count >= *capacity) {
            if (resize_pools(file_pool, plugin_map, capacity) != 0) {
                return -1;
            }
        }

        const char* file = SHM_PTR(arena, patches[j].file_off, char);
        add_file_entry(*file_pool, *plugin_map, *file_count, file, plugin_name);
        (*file_count)++;
    }

    return 0;
}

int get_file_regex_pool(lb_shm_arena_t* arena, const char*** out_regex_pool, match_plugin_map_t** out_plugin_map, uint32_t* out_size)
{
    uint32_t capacity = 0;
    uint32_t file_count = 0;
    const char** file_regex_pool = NULL;
    match_plugin_map_t* plugin_map = NULL;

    lb_hash_map_shm* map = &arena->map;
    uint32_t* keys = SHM_PTR(arena, map->keys_off, uint32_t);
    lb_patch_list_shm_t* values = SHM_PTR(arena, map->values_off, lb_patch_list_shm_t);

    for (uint32_t i = 0; i < map->count; i++) {
        const char* plugin_name = SHM_PTR(arena, keys[i], char);

        if (process_patch_list(arena, &values[i], plugin_name, &file_regex_pool, &plugin_map, &capacity, &file_count) != 0) {
            return -1;
        }
    }

    *out_regex_pool = file_regex_pool;
    *out_plugin_map = plugin_map;
    *out_size = file_count;
    return 0;
}

static pcre2_code* compile_pattern(const char* pattern, int* out_errcode, PCRE2_SIZE* out_erroffset)
{
    pcre2_code* re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0, out_errcode, out_erroffset, NULL);

    if (!re) {
        PCRE2_UCHAR errbuf[256];
        pcre2_get_error_message(*out_errcode, errbuf, sizeof(errbuf));
        log_error("PCRE2 compile failed at offset %zu: %s\n", *out_erroffset, errbuf);
    }

    return re;
}

static char* preprocess_replacement(const char* replacement, const char* plugin_name)
{
    const char* placeholder = "#{{self}}";
    const char* ptr = strstr(replacement, placeholder);

    if (!ptr) {
        return strdup(replacement);
    }

    size_t placeholder_len = strlen(placeholder);
    size_t prefix_len = ptr - replacement;
    size_t suffix_len = strlen(ptr + placeholder_len);

    char substitute[256];
    snprintf(substitute, sizeof(substitute), "window.PLUGIN_LIST['%s']", plugin_name);
    size_t substitute_len = strlen(substitute);

    size_t total_len = prefix_len + substitute_len + suffix_len + 1;
    char* result = malloc(total_len);
    if (!result) {
        return NULL;
    }

    memcpy(result, replacement, prefix_len);
    memcpy(result + prefix_len, substitute, substitute_len);
    memcpy(result + prefix_len + substitute_len, ptr + placeholder_len, suffix_len + 1);

    return result;
}

static char* apply_substitution(pcre2_code* re, const char* input, uint32_t input_size, const char* replacement, uint32_t* out_size)
{
    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(re, NULL);
    if (!match_data) {
        log_error("Failed to create match data\n");
        return NULL;
    }

    PCRE2_SIZE required_size = 0;
    int rc = pcre2_substitute(re, (PCRE2_SPTR)input, input_size, 0, PCRE2_SUBSTITUTE_GLOBAL | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH, match_data, NULL, (PCRE2_SPTR)replacement,
                              PCRE2_ZERO_TERMINATED, NULL, &required_size);

    char* output = NULL;

    if (rc == PCRE2_ERROR_NOMATCH) {
        log_info("No match found\n");
        pcre2_match_data_free(match_data);
        return NULL;
    }

    if (rc == PCRE2_ERROR_NOMEMORY) {
        output = malloc(required_size);
        if (!output) {
            log_error("Failed to allocate %zu bytes\n", required_size);
            pcre2_match_data_free(match_data);
            return NULL;
        }

        PCRE2_SIZE actual_size = required_size;
        rc = pcre2_substitute(re, (PCRE2_SPTR)input, input_size, 0, PCRE2_SUBSTITUTE_GLOBAL, match_data, NULL, (PCRE2_SPTR)replacement, PCRE2_ZERO_TERMINATED, (PCRE2_UCHAR*)output,
                              &actual_size);

        if (rc >= 0) {
            *out_size = actual_size;
            log_info("Applied %d substitution(s), new size: %u\n", rc, actual_size);
        } else {
            PCRE2_UCHAR errbuf[256];
            pcre2_get_error_message(rc, errbuf, sizeof(errbuf));
            log_error("Substitution failed: %s\n", errbuf);
            free(output);
            output = NULL;
        }
    } else if (rc < 0) {
        PCRE2_UCHAR errbuf[256];
        pcre2_get_error_message(rc, errbuf, sizeof(errbuf));
        log_error("Substitution error: %s\n", errbuf);
    }

    pcre2_match_data_free(match_data);
    return output;
}

static int apply_transform_set(const transform_data_t* transform, const char* plugin_name, char** buffer, uint32_t* buffer_size)
{
    for (int i = 0; i < transform->count; i++) {
        char* processed_replacement = preprocess_replacement(transform->replaces[i], plugin_name);
        if (!processed_replacement) {
            log_error("Failed to preprocess replacement string\n");
            continue;
        }

        log_info("Applying transform %d: match='%s' replace='%s'\n", i, transform->matches[i], processed_replacement);

        int errcode;
        PCRE2_SIZE erroffset;
        pcre2_code* re = compile_pattern(transform->matches[i], &errcode, &erroffset);
        if (!re) {
            free(processed_replacement);
            continue;
        }

        uint32_t new_size = 0;
        char* new_buffer = apply_substitution(re, *buffer, *buffer_size, processed_replacement, &new_size);

        pcre2_code_free(re);
        free(processed_replacement);

        if (new_buffer) {
            free(*buffer);
            *buffer = new_buffer;
            *buffer_size = new_size;
        }
    }

    return 0;
}

static void log_matches(const match_list_t* matches, const char** finds, const transform_data_t* transforms)
{
    log_info("Processing %d match entries\n", matches->count);
    for (int i = 0; i < matches->count; i++) {
        log_info("  [%d] pattern: '%s' (%d transforms)\n", i, finds[i], transforms[i].count);
    }
}

static void log_file_matches(const match_list_t* matches)
{
    log_info("Found %d matches in file\n", matches->count);
    for (int i = 0; i < matches->count; i++) {
        log_info("  Match %d: offset %llu-%llu\n", i, matches->froms[i], matches->tos[i]);
    }
}

int handle_file_patches(lb_shm_arena_t* arena, match_list_t* matches, match_plugin_map_t* plugin_map, char* file_content, uint32_t f_size, char** out_content, uint32_t* out_size)
{
    transform_data_t* transforms = NULL;
    const char** finds = NULL;
    char* working_buffer = NULL;
    match_list_t file_matches = { 0 };
    uint32_t working_size = f_size;
    int ret = -1;

    log_info("Starting file patching, initial size: %u\n", f_size);

    if (get_transform_from_matches(arena, matches, &finds, &transforms) != 0) {
        log_error("Failed to get transforms from matches\n");
        goto cleanup;
    }

    log_matches(matches, finds, transforms);

    if (vecscan_multi(finds, matches->count, file_content, f_size, &file_matches) != 0) {
        log_error("vecscan_multi failed\n");
        goto cleanup;
    }

    log_file_matches(&file_matches);

    working_buffer = malloc(f_size);
    if (!working_buffer) {
        log_error("Failed to allocate working buffer\n");
        goto cleanup;
    }
    memcpy(working_buffer, file_content, f_size);

    for (int i = 0; i < file_matches.count; i++) {
        uint32_t match_id = file_matches.ids[i];
        const char* plugin_name = plugin_map[match_id].plugin_name;

        log_info("Processing match %d for plugin '%s'\n", i, plugin_name);
        apply_transform_set(&transforms[i], plugin_name, &working_buffer, &working_size);
    }

    *out_content = working_buffer;
    *out_size = working_size;
    working_buffer = NULL;
    ret = 0;

    log_info("Patching complete, final size: %u\n", working_size);

cleanup:
    match_list_destroy(&file_matches);
    if (transforms) {
        for (int i = 0; i < matches->count; i++) {
            free(transforms[i].matches);
            free(transforms[i].replaces);
        }
        free(transforms);
    }
    free(finds);
    free(working_buffer);

    return ret;
}

int find_file_matches(char* file_content, uint32_t size, char* local_path, char** out_file_content, uint32_t* out_file_size)
{
    assert(hs_valid_platform() == HS_SUCCESS);

    lb_shm_arena_t* arena = shm_arena_open(SHM_IPC_NAME, 1024 * 1024);
    if (!arena) {
        log_error("Failed to open shared memory arena\n");
        return -1;
    }

    const char** file_regex_pool = NULL;
    match_plugin_map_t* plugin_map = NULL;
    uint32_t file_count = 0;

    if (get_file_regex_pool(arena, &file_regex_pool, &plugin_map, &file_count) != 0) {
        log_error("Failed to get file regex pool\n");
        shm_arena_close(arena, arena->size);
        return -1;
    }

    match_list_t matches = { 0 };
    if (vecscan_multi(file_regex_pool, file_count, file_content, size, &matches) != 0) {
        log_error("vecscan_multi failed\n");
        free(file_regex_pool);
        free(plugin_map);
        shm_arena_close(arena, arena->size);
        return -1;
    }

    if (!matches.count) {
        log_info("No matches found for file %s\n", local_path);
        *out_file_content = file_content;
        *out_file_size = size;
        match_list_destroy(&matches);
        free(file_regex_pool);
        free(plugin_map);
        shm_arena_close(arena, arena->size);
        return 0;
    }

    for (int i = 0; i < matches.count; i++) {
        log_info("'file' match %u at [%llu, %llu) for plugin '%s'\n", matches.ids[i], matches.froms[i], matches.tos[i], plugin_map[matches.ids[i]].plugin_name);
    }

    char* out_data = NULL;
    uint32_t out_size = 0;
    int ret = handle_file_patches(arena, &matches, plugin_map, file_content, size, &out_data, &out_size);

    if (ret == 0) {
        log_info("[find_file_matches] out_size: %u\n", out_size);
        *out_file_content = out_data;
        *out_file_size = out_size;
    } else {
        log_error("handle_file_patches failed\n");
    }

    match_list_destroy(&matches);
    free(file_regex_pool);
    free(plugin_map);
    shm_arena_close(arena, arena->size);
    return ret;
}
