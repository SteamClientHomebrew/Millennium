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

#include "match.h"
#include "smem.h"
#include <stdint.h>
#include <stdlib.h>

int match_list_alloc(match_list_t* m, unsigned int count)
{
    m->ids = (unsigned int*)malloc(count * sizeof(unsigned int));
    m->froms = (unsigned long long*)malloc(count * sizeof(unsigned int));
    m->tos = (unsigned long long*)malloc(count * sizeof(unsigned int));
    m->count = 0;
    m->capacity = count;

    if (!m->ids || !m->froms || !m->tos) {
        return -1; /** failed to allocate memory */
    }
    return 0;
}

void match_list_destroy(match_list_t* m)
{
    if (m->froms) free(m->froms);
    if (m->tos) free(m->tos);
    if (m->ids) free(m->ids);
}

int match_list_vecscan_handler(unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void* ctx)
{
    match_list_t* matches = (match_list_t*)ctx;
    if (matches->count < matches->capacity) {
        matches->ids[matches->count] = id;
        matches->froms[matches->count] = from;
        matches->tos[matches->count] = to;
        matches->count++;
    }
    return 0;
}

static lb_patch_shm_t* find_patch_by_id(lb_shm_arena_t* arena, lb_hash_map_shm* map, unsigned int match_id)
{
    lb_patch_list_shm_t* values = SHM_PTR(arena, map->values_off, lb_patch_list_shm_t);
    uint32_t global_idx = 0;

    for (uint32_t j = 0; j < map->count; j++) {
        lb_patch_list_shm_t* list = &values[j];
        if (!list->patches_off) continue;

        lb_patch_shm_t* patches = SHM_PTR(arena, list->patches_off, lb_patch_shm_t);
        for (uint32_t k = 0; k < list->patch_count; k++) {
            if (global_idx == match_id) {
                return &patches[k];
            }
            global_idx++;
        }
    }
    return NULL;
}

static int populate_transform(lb_shm_arena_t* arena, lb_patch_shm_t* patch, transform_data_t* transform)
{
    transform->count = patch->transform_count;
    transform->matches = (const char**)malloc(patch->transform_count * sizeof(char*));
    transform->replaces = (const char**)malloc(patch->transform_count * sizeof(char*));

    if (!transform->matches || !transform->replaces) {
        free(transform->matches);
        free(transform->replaces);
        return -1;
    }

    lb_transform_shm_t* trans = SHM_PTR(arena, patch->transforms_off, lb_transform_shm_t);
    for (uint32_t t = 0; t < patch->transform_count; t++) {
        transform->matches[t] = SHM_PTR(arena, trans[t].match_off, char);
        transform->replaces[t] = SHM_PTR(arena, trans[t].replace_off, char);
    }
    return 0;
}

static void cleanup_transforms(const char** finds, transform_data_t* transforms, int count)
{
    if (transforms) {
        for (int i = 0; i < count; i++) {
            free(transforms[i].matches);
            free(transforms[i].replaces);
        }
        free(transforms);
    }
    free(finds);
}

int get_transform_from_matches(lb_shm_arena_t* arena, match_list_t* matches, const char*** out_finds, transform_data_t** out_transforms)
{
    const char** finds = (const char**)calloc(matches->count, sizeof(char*));
    transform_data_t* transforms = (transform_data_t*)calloc(matches->count, sizeof(transform_data_t));

    if (!finds || !transforms) {
        free(finds);
        free(transforms);
        return -1;
    }

    lb_hash_map_shm* map = &arena->map;

    for (int i = 0; i < matches->count; i++) {
        lb_patch_shm_t* patch = find_patch_by_id(arena, map, matches->ids[i]);
        if (patch) {
            finds[i] = SHM_PTR(arena, patch->find_off, char);
            if (populate_transform(arena, patch, &transforms[i]) != 0) {
                cleanup_transforms(finds, transforms, i);
                return -1;
            }
        }
    }

    *out_finds = finds;
    *out_transforms = transforms;
    return 0;
}
