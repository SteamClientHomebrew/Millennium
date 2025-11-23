/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|___|_|_|_|_|
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

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <re2/re2.h>
#include <re2/set.h>
#include <vector>
#include <string>
#include "hhx64/match.h"
#include "hhx64/smem.h"
#include "hhx64/log.h"

static const uint32_t MAX_FILE_SIZE = 256 * 1024 * 1024; /** 256MB */
static const uint32_t MAX_POOL_CAPACITY = 1000000;       /** 1M entries */
static const unsigned long long MAX_PATTERN_COUNT = 100000;

#ifdef _WIN32
#define plat_strdup _strdup
#elif __linux__
#define plat_strdup strdup
#endif

/** there should absolutely never be a scenario where this happens, but just to be safe */
static const uint32_t MAX_PLUGIN_NAME_LEN = 1024;

typedef struct
{
    uint32_t match_id;
    const char* plugin_name;
} match_plugin_map_t;

typedef struct
{
    RE2** patterns;
    uint32_t count;
} regex_cache_t;

static inline bool safe_mul_u32(uint32_t a, uint32_t b, uint32_t* result)
{
    if (b != 0 && a > UINT32_MAX / b) {
        return false;
    }
    *result = a * b;
    return true;
}

static inline bool safe_add_u32(uint32_t a, uint32_t b, uint32_t* result)
{
    if (a > UINT32_MAX - b) {
        return false;
    }
    *result = a + b;
    return true;
}

static inline bool safe_mul_size(size_t a, size_t b, size_t* result)
{
    if (b != 0 && a > SIZE_MAX / b) {
        return false;
    }
    *result = a * b;
    return true;
}

static inline bool safe_add_size(size_t a, size_t b, size_t* result)
{
    if (a > SIZE_MAX - b) {
        return false;
    }
    *result = a + b;
    return true;
}

static int resize_pools(const char*** file_pool, match_plugin_map_t** plugin_map, uint32_t* capacity)
{
    if (!file_pool || !plugin_map || !capacity) {
        return -1;
    }

    uint32_t new_capacity;
    if (*capacity == 0) {
        new_capacity = 16;
    } else {
        if (*capacity > MAX_POOL_CAPACITY / 2) {
            log_error("Pool capacity would exceed maximum (%u)\n", MAX_POOL_CAPACITY);
            return -1;
        }
        if (!safe_mul_u32(*capacity, 2, &new_capacity)) {
            log_error("Capacity overflow\n");
            return -1;
        }
    }

    if (new_capacity > MAX_POOL_CAPACITY) {
        new_capacity = MAX_POOL_CAPACITY;
    }

    size_t pool_size, map_size;
    if (!safe_mul_size(new_capacity, sizeof(char*), &pool_size) || !safe_mul_size(new_capacity, sizeof(match_plugin_map_t), &map_size)) {
        log_error("Size calculation overflow\n");
        return -1;
    }

    const char** new_pool = (const char**)realloc(*file_pool, pool_size);
    match_plugin_map_t* new_map = (match_plugin_map_t*)realloc(*plugin_map, map_size);

    if (!new_pool || !new_map) {
        free(new_pool);
        log_error("Failed to allocate pools\n");
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
    if (!arena || !list || !plugin_name || !file_pool || !plugin_map || !capacity || !file_count) {
        return -1;
    }

    if (!list->patches_off) return 0;

    if (list->patch_count > MAX_PATTERN_COUNT) {
        log_error("Patch count %u exceeds maximum %u\n", list->patch_count, MAX_PATTERN_COUNT);
        return -1;
    }

    lb_patch_shm_t* patches = SHM_PTR(arena, list->patches_off, lb_patch_shm_t);

    for (uint32_t j = 0; j < list->patch_count; j++) {
        if (*file_count >= UINT32_MAX) {
            log_error("File count overflow\n");
            return -1;
        }

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

extern "C" int get_file_regex_pool(lb_shm_arena_t* arena, const char*** out_regex_pool, match_plugin_map_t** out_plugin_map, uint32_t* out_size)
{
    if (!arena || !out_regex_pool || !out_plugin_map || !out_size) {
        return -1;
    }

    uint32_t capacity = 0;
    uint32_t file_count = 0;
    const char** file_regex_pool = NULL;
    match_plugin_map_t* plugin_map = NULL;

    lb_hash_map_shm* map = &arena->map;
    uint32_t* keys = SHM_PTR(arena, map->keys_off, uint32_t);
    lb_patch_list_shm_t* values = SHM_PTR(arena, map->values_off, lb_patch_list_shm_t);

    if (map->count > MAX_PATTERN_COUNT) {
        log_error("Map count %u exceeds maximum\n", map->count);
        return -1;
    }

    for (uint32_t i = 0; i < map->count; i++) {
        const char* plugin_name = SHM_PTR(arena, keys[i], char);

        size_t name_len = strnlen(plugin_name, MAX_PLUGIN_NAME_LEN + 1);
        if (name_len > MAX_PLUGIN_NAME_LEN) {
            log_error("Plugin name exceeds maximum length\n");
            free(file_regex_pool);
            free(plugin_map);
            return -1;
        }

        if (process_patch_list(arena, &values[i], plugin_name, &file_regex_pool, &plugin_map, &capacity, &file_count) != 0) {
            free(file_regex_pool);
            free(plugin_map);
            return -1;
        }
    }

    *out_regex_pool = file_regex_pool;
    *out_plugin_map = plugin_map;
    *out_size = file_count;
    return 0;
}

static char* preprocess_replacement(const char* replacement, const char* plugin_name)
{
    if (!replacement || !plugin_name) {
        return NULL;
    }

    size_t plugin_name_len = strnlen(plugin_name, MAX_PLUGIN_NAME_LEN + 1);
    if (plugin_name_len > MAX_PLUGIN_NAME_LEN) {
        log_error("Plugin name too long\n");
        return NULL;
    }

    const char* placeholder = "#{{self}}";
    const char* ptr = strstr(replacement, placeholder);

    if (!ptr) {
        return plat_strdup(replacement);
    }

    size_t placeholder_len = strlen(placeholder);
    size_t prefix_len = ptr - replacement;
    size_t suffix_len = strlen(ptr + placeholder_len);

    const char* template_str = "window.PLUGIN_LIST['%s']";
    size_t template_base_len = strlen(template_str) - 2;

    size_t substitute_len;
    if (!safe_add_size(template_base_len, plugin_name_len, &substitute_len)) {
        log_error("Substitute length overflow\n");
        return NULL;
    }

    if (substitute_len > 8192) {
        log_error("Substitute string too long\n");
        return NULL;
    }

    char* substitute = (char*)malloc(substitute_len + 1);
    if (!substitute) {
        return NULL;
    }

    int written = snprintf(substitute, substitute_len + 1, template_str, plugin_name);
    if (written < 0 || (size_t)written >= substitute_len + 1) {
        free(substitute);
        log_error("snprintf failed or truncated\n");
        return NULL;
    }

    size_t total_len;
    if (!safe_add_size(prefix_len, substitute_len, &total_len) || !safe_add_size(total_len, suffix_len, &total_len) || !safe_add_size(total_len, 1, &total_len)) {
        free(substitute);
        log_error("Total length overflow\n");
        return NULL;
    }

    char* result = (char*)malloc(total_len);
    if (!result) {
        free(substitute);
        return NULL;
    }

    memcpy(result, replacement, prefix_len);
    memcpy(result + prefix_len, substitute, substitute_len);
    memcpy(result + prefix_len + substitute_len, ptr + placeholder_len, suffix_len);
    result[total_len - 1] = '\0';

    free(substitute);
    return result;
}

static char* apply_substitution(RE2* re, const char* input, uint32_t input_size, const char* replacement, uint32_t* out_size)
{
    if (!re || !re->ok() || !input || !replacement || !out_size) {
        log_error("Invalid parameters to apply_substitution\n");
        return NULL;
    }

    if (input_size > MAX_FILE_SIZE) {
        log_error("Input size %u exceeds maximum %u\n", input_size, MAX_FILE_SIZE);
        return NULL;
    }

    std::string output(input, input_size);
    int count;

    try {
        count = RE2::GlobalReplace(&output, *re, replacement);
    } catch (const std::exception& e) {
        log_error("RE2::GlobalReplace threw exception: %s\n", e.what());
        return NULL;
    } catch (...) {
        log_error("RE2::GlobalReplace threw unknown exception\n");
        return NULL;
    }

    if (count == 0) {
        log_info("No match found\n");
        return NULL;
    }

    if (output.size() > MAX_FILE_SIZE) {
        log_error("Output size %zu exceeds maximum %u\n", output.size(), MAX_FILE_SIZE);
        return NULL;
    }

    log_info("Applied %d substitution(s), new size: %zu\n", count, output.size());

    char* result = (char*)malloc(output.size());
    if (!result) {
        log_error("Failed to allocate %zu bytes\n", output.size());
        return NULL;
    }

    memcpy(result, output.data(), output.size());
    *out_size = static_cast<uint32_t>(output.size());
    return result;
}

static regex_cache_t* compile_transform_patterns(const transform_data_t* transforms, int count)
{
    if (!transforms || count < 0 || count > (int)MAX_PATTERN_COUNT) {
        return NULL;
    }

    regex_cache_t* cache = (regex_cache_t*)malloc(sizeof(regex_cache_t));
    if (!cache) return NULL;

    cache->patterns = (RE2**)calloc(count, sizeof(RE2*));
    if (!cache->patterns) {
        free(cache);
        return NULL;
    }
    cache->count = count;

    for (int i = 0; i < count; i++) {
        if (transforms[i].count > 0 && transforms[i].matches && transforms[i].matches[0]) {
            try {
                cache->patterns[i] = new RE2(transforms[i].matches[0]);
                if (!cache->patterns[i]->ok()) {
                    log_error("Failed to compile pattern %d: %s\n", i, cache->patterns[i]->error().c_str());
                    delete cache->patterns[i];
                    cache->patterns[i] = NULL;
                }
            } catch (const std::exception& e) {
                log_error("Exception compiling pattern %d: %s\n", i, e.what());
                cache->patterns[i] = NULL;
            } catch (...) {
                log_error("Unknown exception compiling pattern %d\n", i);
                cache->patterns[i] = NULL;
            }
        }
    }

    return cache;
}

static void free_regex_cache(regex_cache_t* cache)
{
    if (!cache) return;
    if (cache->patterns) {
        for (uint32_t i = 0; i < cache->count; i++) {
            delete cache->patterns[i];
        }
        free(cache->patterns);
    }
    free(cache);
}

static int apply_transform_set(const transform_data_t* transform, RE2* compiled_pattern, const char* plugin_name, char** buffer, uint32_t* buffer_size)
{
    if (!transform || !compiled_pattern || !plugin_name || !buffer || !*buffer || !buffer_size) {
        return -1;
    }

    if (*buffer_size > MAX_FILE_SIZE) {
        log_error("Buffer size exceeds maximum\n");
        return -1;
    }

    for (int i = 0; i < transform->count; i++) {
        if (!transform->replaces || !transform->replaces[i]) {
            continue;
        }

        char* processed_replacement = preprocess_replacement(transform->replaces[i], plugin_name);
        if (!processed_replacement) {
            log_error("Failed to preprocess replacement string\n");
            continue;
        }

        log_info("Applying transform %d: match='%s' replace='%s'\n", i, transform->matches ? transform->matches[i] : "NULL", processed_replacement);

        uint32_t new_size = 0;
        char* new_buffer = apply_substitution(compiled_pattern, *buffer, *buffer_size, processed_replacement, &new_size);

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
    if (!matches) return;

    log_info("Processing %d match entries\n", matches->count);
    for (int i = 0; i < matches->count; i++) {
        const char* pattern = (finds && finds[i]) ? finds[i] : "NULL";
        int transform_count = transforms ? transforms[i].count : 0;
        log_info("  [%d] pattern: '%s' (%d transforms)\n", i, pattern, transform_count);
    }
}

static void log_file_matches(const match_list_t* matches)
{
    if (!matches) return;

    log_info("Found %d matches in file\n", matches->count);
    for (int i = 0; i < matches->count; i++) {
        if (matches->froms && matches->tos) {
            log_info("  Match %d: offset %llu-%llu\n", i, matches->froms[i], matches->tos[i]);
        }
    }
}

static int re2_multi_match(const char** patterns, uint32_t pattern_count, const char* text, uint32_t text_size, match_list_t* matches)
{
    if (!patterns || !text || !matches || text_size > MAX_FILE_SIZE) {
        log_error("Invalid parameters to re2_multi_match\n");
        return -1;
    }

    if (pattern_count == 0) {
        matches->count = 0;
        return 0;
    }

    if (pattern_count > MAX_PATTERN_COUNT) {
        log_error("Pattern count %u exceeds maximum\n", pattern_count);
        return -1;
    }

    RE2::Set re_set(RE2::DefaultOptions, RE2::UNANCHORED);

    for (uint32_t i = 0; i < pattern_count; i++) {
        if (!patterns[i]) {
            log_error("NULL pattern at index %u\n", i);
            return -1;
        }

        std::string error;
        if (re_set.Add(patterns[i], &error) < 0) {
            log_error("Failed to add pattern %u: %s\n", i, error.c_str());
            return -1;
        }
    }

    if (!re_set.Compile()) {
        log_error("Failed to compile RE2::Set\n");
        return -1;
    }

    std::vector<int> match_ids;
    re2::StringPiece text_piece(text, text_size);

    try {
        if (!re_set.Match(text_piece, &match_ids)) {
            matches->count = 0;
            return 0;
        }
    } catch (const std::exception& e) {
        log_error("Exception during RE2::Set::Match: %s\n", e.what());
        return -1;
    } catch (...) {
        log_error("Unknown exception during RE2::Set::Match\n");
        return -1;
    }

    if (match_ids.size() > MAX_PATTERN_COUNT) {
        log_error("Match count exceeds maximum\n");
        return -1;
    }

    matches->count = match_ids.size();

    size_t ids_size, ranges_size;
    if (!safe_mul_size(matches->count, sizeof(uint32_t), &ids_size) || !safe_mul_size(matches->count, sizeof(uint64_t), &ranges_size)) {
        log_error("Match allocation size overflow\n");
        return -1;
    }

    matches->ids = (unsigned int*)malloc(ids_size);
    matches->froms = (unsigned long long*)malloc(ranges_size);
    matches->tos = (unsigned long long*)malloc(ranges_size);

    if (!matches->ids || !matches->froms || !matches->tos) {
        free(matches->ids);
        free(matches->froms);
        free(matches->tos);
        matches->ids = NULL;
        matches->froms = NULL;
        matches->tos = NULL;
        return -1;
    }

    std::vector<RE2*> compiled_patterns;
    compiled_patterns.reserve(match_ids.size());

    for (size_t i = 0; i < match_ids.size(); i++) {
        if (match_ids[i] < 0 || (uint32_t)match_ids[i] >= pattern_count) {
            log_error("Invalid match_id %d at index %zu\n", match_ids[i], i);
            for (auto* re : compiled_patterns) {
                delete re;
            }
            free(matches->ids);
            free(matches->froms);
            free(matches->tos);
            matches->ids = NULL;
            matches->froms = NULL;
            matches->tos = NULL;
            return -1;
        }

        try {
            compiled_patterns.push_back(new RE2(patterns[match_ids[i]]));
        } catch (const std::exception& e) {
            log_error("Exception creating RE2 for pattern %d: %s\n", match_ids[i], e.what());
            for (auto* re : compiled_patterns) {
                delete re;
            }
            free(matches->ids);
            free(matches->froms);
            free(matches->tos);
            matches->ids = NULL;
            matches->froms = NULL;
            matches->tos = NULL;
            return -1;
        }
    }

    for (size_t i = 0; i < match_ids.size(); i++) {
        matches->ids[i] = match_ids[i];

        re2::StringPiece match;
        try {
            if (compiled_patterns[i]->Match(text_piece, 0, text_size, RE2::UNANCHORED, &match, 1)) {
                ptrdiff_t offset = match.data() - text;
                if (offset < 0 || offset > (ptrdiff_t)text_size) {
                    matches->froms[i] = 0;
                    matches->tos[i] = 0;
                } else {
                    matches->froms[i] = offset;
                    size_t end_offset;
                    if (safe_add_size(offset, match.size(), &end_offset) && end_offset <= text_size) {
                        matches->tos[i] = end_offset;
                    } else {
                        matches->tos[i] = text_size;
                    }
                }
            } else {
                matches->froms[i] = 0;
                matches->tos[i] = 0;
            }
        } catch (const std::exception& e) {
            log_error("Exception during match: %s\n", e.what());
            matches->froms[i] = 0;
            matches->tos[i] = 0;
        }
    }

    for (auto* re : compiled_patterns) {
        delete re;
    }

    return 0;
}

extern "C" int handle_file_patches(lb_shm_arena_t* arena, match_list_t* matches, match_plugin_map_t* plugin_map, char* file_content, uint32_t f_size, char** out_content,
                                   uint32_t* out_size)
{
    if (!arena || !matches || !plugin_map || !file_content || !out_content || !out_size) {
        log_error("Invalid parameters to handle_file_patches\n");
        return -1;
    }

    if (f_size > MAX_FILE_SIZE) {
        log_error("File size %u exceeds maximum %u\n", f_size, MAX_FILE_SIZE);
        return -1;
    }

    transform_data_t* transforms = NULL;
    const char** finds = NULL;
    char* working_buffer = NULL;
    match_list_t file_matches = { 0 };
    regex_cache_t* pattern_cache = NULL;
    uint32_t working_size = f_size;
    int ret = -1;

    log_info("Starting file patching, initial size: %u\n", f_size);

    if (get_transform_from_matches(arena, matches, &finds, &transforms) != 0) {
        log_error("Failed to get transforms from matches\n");
        goto cleanup;
    }

    log_matches(matches, finds, transforms);

    if (re2_multi_match(finds, matches->count, file_content, f_size, &file_matches) != 0) {
        log_error("re2_multi_match failed\n");
        goto cleanup;
    }

    log_file_matches(&file_matches);

    // Allocate working buffer with size check
    size_t alloc_size;
    if (!safe_mul_size(f_size, 2, &alloc_size)) {
        log_error("Working buffer size overflow\n");
        goto cleanup;
    }

    if (alloc_size > MAX_FILE_SIZE * 2) {
        alloc_size = MAX_FILE_SIZE * 2;
    }

    working_buffer = (char*)malloc(alloc_size);
    if (!working_buffer) {
        log_error("Failed to allocate working buffer\n");
        goto cleanup;
    }
    memcpy(working_buffer, file_content, f_size);

    pattern_cache = compile_transform_patterns(transforms, matches->count);
    if (!pattern_cache) {
        log_error("Failed to compile pattern cache\n");
        goto cleanup;
    }

    for (int i = 0; i < file_matches.count; i++) {
        uint32_t match_id = file_matches.ids[i];

        // Bounds check on match_id
        if (match_id >= matches->count) {
            log_error("match_id %u out of bounds (max %u)\n", match_id, matches->count);
            continue;
        }

        const char* plugin_name = plugin_map[match_id].plugin_name;
        if (!plugin_name) {
            log_error("NULL plugin_name for match_id %u\n", match_id);
            continue;
        }

        log_info("Processing match %d for plugin '%s'\n", i, plugin_name);

        if (match_id < pattern_cache->count && pattern_cache->patterns[match_id]) {
            apply_transform_set(&transforms[i], pattern_cache->patterns[match_id], plugin_name, &working_buffer, &working_size);
        }
    }

    *out_content = working_buffer;
    *out_size = working_size;
    working_buffer = NULL; // Transfer ownership
    ret = 0;

    log_info("Patching complete, final size: %u\n", working_size);

cleanup:
    free_regex_cache(pattern_cache);
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

extern "C" int find_file_matches(char* file_content, uint32_t size, char* local_path, char** out_file_content, uint32_t* out_file_size)
{
    if (!file_content || !local_path || !out_file_content || !out_file_size) {
        log_error("Invalid parameters to find_file_matches\n");
        return -1;
    }

    if (size > MAX_FILE_SIZE) {
        log_error("File size %u exceeds maximum %u\n", size, MAX_FILE_SIZE);
        return -1;
    }

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

    if (file_count == 0 || !file_regex_pool) {
        log_info("Skipping patch, no regex pool to process. file_count: %u, file_regex_pool: %d\n", file_count, !!file_regex_pool);

        *out_file_content = file_content;
        *out_file_size = size;
        if (file_regex_pool) free(file_regex_pool);
        free(plugin_map);
        shm_arena_close(arena, arena->size);
        return 0;
    }

    match_list_t matches = { 0 };
    if (re2_multi_match(file_regex_pool, file_count, file_content, size, &matches) != 0) {
        log_error("re2_multi_match failed\n");
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
        if (matches.ids && matches.ids[i] < file_count && plugin_map) {
            log_info("'file' match %u at [%llu, %llu) for plugin '%s'\n", matches.ids[i], matches.froms ? matches.froms[i] : 0, matches.tos ? matches.tos[i] : 0,
                     plugin_map[matches.ids[i]].plugin_name);
        }
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
