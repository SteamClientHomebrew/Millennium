/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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
#include <numeric>
#include <algorithm>
#include <re2/re2.h>
#include <re2/set.h>
#include <vector>
#include <string>
#include "state/shared_memory.h"
#include "instrumentation/resource_matcher.h"
#include "instrumentation/logger.h"

static const uint32_t MAX_FILE_SIZE = 256 * 1024 * 1024; /** 256MB */
static const uint32_t MAX_POOL_CAPACITY = 1000000;       /** 1M entries */
static const unsigned long long MAX_PATTERN_COUNT = 100000;

#ifdef _WIN32
#define plat_strdup _strdup
#elif __linux__ || __APPLE__
#define plat_strdup strdup
#endif

/** there should absolutely never be a scenario where this happens, but just to be safe */
static const uint32_t MAX_PLUGIN_NAME_LEN = 1024;

typedef struct
{
    uint32_t match_id;
    const char* plugin_name;
} match_plugin_map_t;

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

static int process_patch_list(platform::shared_memory::lb_shm_arena_t* arena, platform::shared_memory::lb_patch_list_shm_t* list, const char* plugin_name, const char*** file_pool,
                              match_plugin_map_t** plugin_map, uint32_t* capacity, uint32_t* file_count)
{
    if (!arena || !list || !plugin_name || !file_pool || !plugin_map || !capacity || !file_count) {
        return -1;
    }

    if (!list->patches_off) return 0;

    if (list->patch_count > MAX_PATTERN_COUNT) {
        log_error("Patch count %u exceeds maximum %u\n", list->patch_count, MAX_PATTERN_COUNT);
        return -1;
    }

    platform::shared_memory::lb_patch_shm_t* patches = SHM_PTR(arena, list->patches_off, platform::shared_memory::lb_patch_shm_t);

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

extern "C" int get_file_regex_pool(platform::shared_memory::lb_shm_arena_t* arena, const char*** out_regex_pool, match_plugin_map_t** out_plugin_map, uint32_t* out_size)
{
    if (!arena || !out_regex_pool || !out_plugin_map || !out_size) {
        return -1;
    }

    uint32_t capacity = 0;
    uint32_t file_count = 0;
    const char** file_regex_pool = NULL;
    match_plugin_map_t* plugin_map = NULL;

    platform::shared_memory::lb_hash_map_shm* map = &arena->map;
    uint32_t* keys = SHM_PTR(arena, map->keys_off, uint32_t);
    platform::shared_memory::lb_patch_list_shm_t* values = SHM_PTR(arena, map->values_off, platform::shared_memory::lb_patch_list_shm_t);

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

    matches->count = (int)match_ids.size();

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

/**
 * Run a single RE2 pattern against a buffer and return the first match bounds.
 * Returns true if a match was found, false otherwise.
 */
static bool re2_find_in_buffer(const char* pattern, const char* buffer, uint32_t buffer_size, uint64_t* out_from, uint64_t* out_to)
{
    if (!pattern || !buffer || !out_from || !out_to || buffer_size > MAX_FILE_SIZE) {
        return false;
    }

    try {
        RE2 re(pattern);
        if (!re.ok()) {
            log_error("Failed to compile find pattern: %s\n", re.error().c_str());
            return false;
        }

        re2::StringPiece text(buffer, buffer_size);
        re2::StringPiece match;

        if (!re.Match(text, 0, buffer_size, RE2::UNANCHORED, &match, 1)) {
            return false;
        }

        ptrdiff_t offset = match.data() - buffer;
        if (offset < 0 || (size_t)offset > buffer_size) {
            return false;
        }

        *out_from = (uint64_t)offset;
        size_t end;
        if (!safe_add_size((size_t)offset, match.size(), &end) || end > buffer_size) {
            *out_to = buffer_size;
        } else {
            *out_to = (uint64_t)end;
        }

        return true;
    } catch (const std::exception& e) {
        log_error("Exception in re2_find_in_buffer: %s\n", e.what());
        return false;
    } catch (...) {
        log_error("Unknown exception in re2_find_in_buffer\n");
        return false;
    }
}

/**
 * Apply a GlobalReplace only within [from, to) of input, splice the result
 * back into a new full-length buffer. Returns NULL if the pattern has no match
 * within the region (graceful no-op — caller keeps the original buffer).
 */
static char* apply_scoped_substitution(RE2* re, const char* input, uint32_t input_size, uint64_t from, uint64_t to, const char* replacement, uint32_t* out_size)
{
    if (!re || !re->ok() || !input || !replacement || !out_size) {
        log_error("Invalid parameters to apply_scoped_substitution\n");
        return NULL;
    }

    if (from > to || to > input_size || input_size > MAX_FILE_SIZE) {
        log_error("Invalid bounds [%llu, %llu) for input_size %u\n", from, to, input_size);
        return NULL;
    }

    std::string region(input + from, to - from);
    int count;

    try {
        count = RE2::GlobalReplace(&region, *re, replacement);
    } catch (const std::exception& e) {
        log_error("GlobalReplace threw exception: %s\n", e.what());
        return NULL;
    } catch (...) {
        log_error("GlobalReplace threw unknown exception\n");
        return NULL;
    }

    if (count == 0) {
        return NULL; /** no match within the region — graceful no-op */
    }

    uint32_t suffix_len = input_size - (uint32_t)to;
    size_t new_total;
    if (!safe_add_size((size_t)from, region.size(), &new_total) || !safe_add_size(new_total, suffix_len, &new_total)) {
        log_error("New total size overflow\n");
        return NULL;
    }

    if (new_total > MAX_FILE_SIZE) {
        log_error("Scoped substitution result would exceed MAX_FILE_SIZE\n");
        return NULL;
    }

    char* result = (char*)malloc(new_total);
    if (!result) {
        log_error("Failed to allocate result buffer (%zu bytes)\n", new_total);
        return NULL;
    }

    memcpy(result, input, from);
    memcpy(result + from, region.data(), region.size());
    memcpy(result + from + region.size(), input + to, suffix_len);

    *out_size = (uint32_t)new_total;
    log_info("Scoped substitution: %d replacement(s) in [%llu, %llu), new total size: %u\n", count, from, to, *out_size);
    return result;
}

/**
 * Apply all transforms for a single patch, scoped to [from, to).
 * Each transform compiles its own match pattern. After each replacement the
 * `to` bound is adjusted by the size delta so subsequent transforms within
 * the same find region remain correctly positioned.
 */
static void apply_transform_set_scoped(const transform_data_t* transform, const char* plugin_name, char** buffer, uint32_t* buffer_size, uint64_t from, uint64_t to)
{
    if (!transform || !plugin_name || !buffer || !*buffer || !buffer_size) {
        return;
    }

    for (int i = 0; i < transform->count; i++) {
        if (!transform->matches || !transform->matches[i] || !transform->replaces || !transform->replaces[i]) {
            continue;
        }

        char* processed = preprocess_replacement(transform->replaces[i], plugin_name);
        if (!processed) {
            log_error("Failed to preprocess replacement for plugin '%s' transform %d\n", plugin_name, i);
            continue;
        }

        try {
            RE2 pattern(transform->matches[i]);
            if (!pattern.ok()) {
                log_error("Failed to compile transform match for plugin '%s' transform %d: %s\n", plugin_name, i, pattern.error().c_str());
                free(processed);
                continue;
            }

            log_info("Plugin '%s' transform %d: match='%s' in [%llu, %llu)\n", plugin_name, i, transform->matches[i], from, to);

            uint32_t new_size = 0;
            char* new_buffer = apply_scoped_substitution(&pattern, *buffer, *buffer_size, from, to, processed, &new_size);

            free(processed);

            if (new_buffer) {
                int64_t delta = (int64_t)new_size - (int64_t)*buffer_size;
                to = (uint64_t)((int64_t)to + delta); /** shift end of region by size change */
                free(*buffer);
                *buffer = new_buffer;
                *buffer_size = new_size;
            }
        } catch (const std::exception& e) {
            free(processed);
            log_error("Exception in transform %d for plugin '%s': %s\n", i, plugin_name, e.what());
        } catch (...) {
            free(processed);
            log_error("Unknown exception in transform %d for plugin '%s'\n", i, plugin_name);
        }
    }
}

/**
 * Run all find patterns against the original file content and perform a
 * pairwise overlap check across different plugins. Overlapping [from, to)
 * intervals between two distinct plugins indicate a conflict: the
 * alphabetically-earlier
 * plugin wins; the later one may partially or fully
 * become a no-op on the modified content.
 */
static void detect_and_log_conflicts(const char** finds, int find_count, match_list_t* matches, match_plugin_map_t* plugin_map, const char* file_content, uint32_t f_size,
                                     const char* filename)
{
    if (!finds || find_count <= 0 || !matches || !plugin_map || !file_content) {
        return;
    }

    match_list_t original_bounds = { 0 };
    if (re2_multi_match(finds, (uint32_t)find_count, file_content, f_size, &original_bounds) != 0) {
        log_error("detect_and_log_conflicts: re2_multi_match failed\n");
        return;
    }

    for (int i = 0; i < original_bounds.count; i++) {
        for (int j = i + 1; j < original_bounds.count; j++) {
            uint32_t idx_i = original_bounds.ids[i];
            uint32_t idx_j = original_bounds.ids[j];

            if (idx_i >= (uint32_t)find_count || idx_j >= (uint32_t)find_count) {
                continue;
            }

            if (idx_i >= (uint32_t)matches->count || idx_j >= (uint32_t)matches->count) {
                continue;
            }

            const char* plugin_i = plugin_map[matches->ids[idx_i]].plugin_name;
            const char* plugin_j = plugin_map[matches->ids[idx_j]].plugin_name;

            if (!plugin_i || !plugin_j || strcmp(plugin_i, plugin_j) == 0) {
                continue; /** same plugin — not a cross-plugin conflict */
            }

            uint64_t a = original_bounds.froms[i], b = original_bounds.tos[i];
            uint64_t c = original_bounds.froms[j], d = original_bounds.tos[j];

            if (a < d && c < b) {
                const char* winner = (strcmp(plugin_i, plugin_j) < 0) ? plugin_i : plugin_j;
                const char* loser = (strcmp(plugin_i, plugin_j) < 0) ? plugin_j : plugin_i;
                log_error("[CONFLICT] '%s' and '%s' have overlapping find regions [%llu, %llu) and [%llu, %llu) in '%s'\n", plugin_i, plugin_j, a, b, c, d,
                          filename ? filename : "unknown");
                log_error("  '%s' patches first (alphabetical order). '%s' may partially or fully become a no-op on the modified content.\n", winner, loser);
            }
        }
    }

    match_list_destroy(&original_bounds);
}

extern "C" int handle_file_patches(platform::shared_memory::lb_shm_arena_t* arena, match_list_t* matches, match_plugin_map_t* plugin_map, char* file_content, uint32_t f_size,
                                   char** out_content, uint32_t* out_size, const char* filename)
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
    uint32_t working_size = f_size;
    int ret = -1;

    log_info("Starting file patching for '%s', size: %u, %d plugin patch(es)\n", filename ? filename : "?", f_size, matches->count);

    if (get_transform_from_matches(arena, matches, &finds, &transforms) != 0) {
        log_error("Failed to get transforms from matches\n");
        goto cleanup;
    }

    /** detect cross-plugin find region overlaps on the original content and log warnings */
    detect_and_log_conflicts(finds, matches->count, matches, plugin_map, file_content, f_size, filename);

    working_buffer = (char*)malloc(f_size);
    if (!working_buffer) {
        log_error("Failed to allocate working buffer\n");
        goto cleanup;
    }
    memcpy(working_buffer, file_content, f_size);

    /**
     * Sort patch indices alphabetically by plugin name — this is the load order guarantee.
     * It is deterministic on every OS and machine, and no plugin author can influence it
     * through configuration. Within the sorted order, each plugin's find pattern is
     * re-executed against the live (already-modified) buffer so that offset drift from
     * prior plugins' size-changing substitutions is automatically handled.
     */
    {
        std::vector<int> sorted(matches->count);
        std::iota(sorted.begin(), sorted.end(), 0);
        std::sort(sorted.begin(), sorted.end(), [&](int a, int b)
        {
            const char* na = plugin_map[matches->ids[a]].plugin_name;
            const char* nb = plugin_map[matches->ids[b]].plugin_name;
            if (!na) return false;
            if (!nb) return true;
            return strcmp(na, nb) < 0;
        });

        for (int si : sorted) {
            if (!finds[si] || !finds[si][0]) {
                continue;
            }

            const char* plugin_name = plugin_map[matches->ids[si]].plugin_name;
            if (!plugin_name) {
                continue;
            }

            /**
             * Re-run the find pattern against the current working buffer.
             * This accounts for any size changes made by earlier plugins so that
             * byte offsets remain accurate even when substitutions grew or shrank
             * the content.
             */
            uint64_t from = 0, to = 0;
            if (!re2_find_in_buffer(finds[si], working_buffer, working_size, &from, &to)) {
                log_info("Plugin '%s': find did not match current buffer (graceful no-op)\n", plugin_name);
                continue;
            }

            log_info("Plugin '%s': find matched [%llu, %llu) — applying %d transform(s)\n", plugin_name, from, to, transforms[si].count);
            apply_transform_set_scoped(&transforms[si], plugin_name, &working_buffer, &working_size, from, to);
        }
    }

    *out_content = working_buffer;
    *out_size = working_size;
    working_buffer = NULL; /** transfer ownership */
    ret = 0;

    log_info("Patching complete for '%s', final size: %u\n", filename ? filename : "?", working_size);

cleanup:
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
    *out_file_content = file_content;
    *out_file_size = size;

    if (!file_content || !local_path || !out_file_content || !out_file_size) {
        log_error("Invalid parameters to find_file_matches\n");
        return -1;
    }

    if (size > MAX_FILE_SIZE) {
        log_error("File size %u exceeds maximum %u\n", size, MAX_FILE_SIZE);
        return -1;
    }

    platform::shared_memory::lb_shm_arena_t* arena = platform::shared_memory::sopen(SHM_IPC_NAME, 1024 * 1024);
    if (!arena) {
        log_error("Failed to open shared memory arena\n");
        return -1;
    }

    const char** file_regex_pool = NULL;
    match_plugin_map_t* plugin_map = NULL;
    uint32_t file_count = 0;

    if (get_file_regex_pool(arena, &file_regex_pool, &plugin_map, &file_count) != 0) {
        log_error("Failed to get file regex pool\n");
        sclose(arena, arena->size);
        return -1;
    }

    if (file_count == 0 || !file_regex_pool) {
        log_info("Skipping patch, no regex pool to process. file_count: %u, file_regex_pool: %d\n", file_count, !!file_regex_pool);
        if (file_regex_pool) free(file_regex_pool);
        free(plugin_map);
        sclose(arena, arena->size);
        return 0;
    }

    match_list_t matches = { 0 };
    if (re2_multi_match(file_regex_pool, file_count, file_content, size, &matches) != 0) {
        log_error("re2_multi_match failed\n");
        free(file_regex_pool);
        free(plugin_map);
        sclose(arena, arena->size);
        return -1;
    }

    if (!matches.count) {
        log_info("No matches found for file %s\n", local_path);
        match_list_destroy(&matches);
        free(file_regex_pool);
        free(plugin_map);
        sclose(arena, arena->size);
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
    int ret = handle_file_patches(arena, &matches, plugin_map, file_content, size, &out_data, &out_size, local_path);

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
    sclose(arena, arena->size);
    return ret;
}
