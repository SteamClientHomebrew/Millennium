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

#define _POSIX_C_SOURCE 200809L
#include "hhx64/smem.h"
#include "hhx64/log.h"
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>

typedef struct
{
    HANDLE handle;
    lb_shm_arena_t* ptr;
} win_shm_ctx_t;

lb_shm_arena_t* g_lb_patch_arena;
static win_shm_ctx_t g_shm_ctx = { NULL, NULL };

lb_shm_arena_t* shm_arena_create(const char* name, uint32_t size)
{
    char win_name[256];
    snprintf(win_name, sizeof(win_name), "Local\\%s", name + 1); // Skip leading '/'

    HANDLE hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, win_name);
    if (!hMapFile) {
        DWORD err = GetLastError();
        log_error("CreateFileMappingA failed: %s (error: %lu)\n", win_name, err);
        return NULL;
    }

    DWORD creation_result = GetLastError();
    if (creation_result == ERROR_ALREADY_EXISTS) {
        log_warning("Shared memory already exists: %s\n", win_name);
    }

    lb_shm_arena_t* arena = (lb_shm_arena_t*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!arena) {
        DWORD err = GetLastError();
        log_error("MapViewOfFile failed (error: %lu)\n", err);
        CloseHandle(hMapFile);
        return NULL;
    }

    g_shm_ctx.handle = hMapFile;
    g_shm_ctx.ptr = arena;

    arena->size = size;
    arena->used = sizeof(lb_shm_arena_t);
    return arena;
}

lb_shm_arena_t* shm_arena_open(const char* name, uint32_t size)
{
    char win_name[256];
    snprintf(win_name, sizeof(win_name), "Local\\%s", name + 1);

    HANDLE hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, win_name);
    if (!hMapFile) {
        DWORD err = GetLastError();
        log_error("OpenFileMappingA failed: %s (error: %lu)\n", win_name, err);
        return NULL;
    }

    lb_shm_arena_t* arena = (lb_shm_arena_t*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!arena) {
        DWORD err = GetLastError();
        log_error("MapViewOfFile failed (error: %lu)\n", err);
        CloseHandle(hMapFile);
        return NULL;
    }

    g_shm_ctx.handle = hMapFile;
    g_shm_ctx.ptr = arena;

    return arena;
}

void shm_arena_close(lb_shm_arena_t* arena, uint32_t size)
{
    (void)size;
    UnmapViewOfFile(arena);
    if (g_shm_ctx.handle) {
        CloseHandle(g_shm_ctx.handle);
        g_shm_ctx.handle = NULL;
        g_shm_ctx.ptr = NULL;
    }
}

void shm_arena_unlink(const char* name)
{
    // Windows doesn't require explicit unlinking - handled by last CloseHandle
    (void)name;
}

#else
// Linux implementation
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

lb_shm_arena_t* shm_arena_create(const char* name, uint32_t size)
{
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) return NULL;

    if (ftruncate(fd, size) == -1) {
        close(fd);
        return NULL;
    }

    lb_shm_arena_t* arena = (lb_shm_arena_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (arena == MAP_FAILED) return NULL;

    arena->size = size;
    arena->used = sizeof(lb_shm_arena_t);
    return arena;
}

lb_shm_arena_t* shm_arena_open(const char* name, uint32_t size)
{
    int fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1) return NULL;

    lb_shm_arena_t* arena = (lb_shm_arena_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    return (arena == MAP_FAILED) ? NULL : arena;
}

void shm_arena_close(lb_shm_arena_t* arena, uint32_t size)
{
    munmap(arena, size);
}

void shm_arena_unlink(const char* name)
{
    shm_unlink(name);
}
#endif

uint32_t shm_arena_alloc(lb_shm_arena_t* arena, uint32_t size)
{
    /** align to 8 bytes across domain as srv is 32bit and client is 64bit */
    size = (size + 7) & ~7;

    if (arena->used + size > arena->size) {
        return 0; /** no space */
    }

    uint32_t offset = arena->used;
    arena->used += size;
    return offset;
}

static uint32_t shm_strdup(lb_shm_arena_t* arena, const char* str)
{
    uint32_t len = (uint32_t)strlen(str) + 1;
    uint32_t off = shm_arena_alloc(arena, len);
    if (off == 0) return 0;

    memcpy(SHM_PTR(arena, off, char), str, len);
    return off;
}

void hashmap_init(lb_shm_arena_t* arena)
{
    lb_hash_map_shm* map = &arena->map;
    map->capacity = 16;
    map->count = 0;

    /** allocate keys array (array of offsets) */
    map->keys_off = shm_arena_alloc(arena, map->capacity * sizeof(uint32_t));
    uint32_t* keys = SHM_PTR(arena, map->keys_off, uint32_t);
    memset(keys, 0, map->capacity * sizeof(uint32_t));

    /** allocate values array */
    map->values_off = shm_arena_alloc(arena, map->capacity * sizeof(lb_patch_list_shm_t));
    lb_patch_list_shm_t* values = SHM_PTR(arena, map->values_off, lb_patch_list_shm_t);
    memset(values, 0, map->capacity * sizeof(lb_patch_list_shm_t));
}

lb_patch_list_shm_t* hashmap_get(lb_shm_arena_t* arena, const char* key)
{
    lb_hash_map_shm* map = &arena->map;
    uint32_t* keys = SHM_PTR(arena, map->keys_off, uint32_t);
    lb_patch_list_shm_t* values = SHM_PTR(arena, map->values_off, lb_patch_list_shm_t);

    for (uint32_t i = 0; i < map->count; i++) {
        char* stored_key = SHM_PTR(arena, keys[i], char);
        if (strcmp(stored_key, key) == 0) {
            return &values[i];
        }
    }
    return NULL;
}

lb_patch_list_shm_t* hashmap_add_key(lb_shm_arena_t* arena, const char* key)
{
    lb_patch_list_shm_t* existing = hashmap_get(arena, key);
    if (existing) return existing;

    lb_hash_map_shm* map = &arena->map;

    if (map->count >= map->capacity) {
        /** need to grow, alloc exponentially more space */
        uint32_t new_capacity = map->capacity * 2;

        uint32_t new_keys_off = shm_arena_alloc(arena, new_capacity * sizeof(uint32_t));
        uint32_t new_values_off = shm_arena_alloc(arena, new_capacity * sizeof(lb_patch_list_shm_t));

        if (new_keys_off == 0 || new_values_off == 0) return NULL;

        /** copy old data */
        memcpy(SHM_PTR(arena, new_keys_off, uint32_t), SHM_PTR(arena, map->keys_off, uint32_t), map->capacity * sizeof(uint32_t));
        memcpy(SHM_PTR(arena, new_values_off, lb_patch_list_shm_t), SHM_PTR(arena, map->values_off, lb_patch_list_shm_t), map->capacity * sizeof(lb_patch_list_shm_t));

        /** zero mem new space */
        memset(SHM_PTR(arena, new_keys_off + map->capacity * sizeof(uint32_t), uint32_t), 0, map->capacity * sizeof(uint32_t));
        memset(SHM_PTR(arena, new_values_off + map->capacity * sizeof(lb_patch_list_shm_t), lb_patch_list_shm_t), 0, map->capacity * sizeof(lb_patch_list_shm_t));

        map->keys_off = new_keys_off;
        map->values_off = new_values_off;
        map->capacity = new_capacity;
    }

    uint32_t* keys = SHM_PTR(arena, map->keys_off, uint32_t);
    lb_patch_list_shm_t* values = SHM_PTR(arena, map->values_off, lb_patch_list_shm_t);

    keys[map->count] = shm_strdup(arena, key);
    values[map->count].patches_off = 0;
    values[map->count].patch_count = 0;
    values[map->count].patch_capacity = 0;

    return &values[map->count++];
}

void hashmap_remove(lb_shm_arena_t* arena, const char* key)
{
    lb_hash_map_shm* map = &arena->map;
    uint32_t* keys = SHM_PTR(arena, map->keys_off, uint32_t);
    lb_patch_list_shm_t* values = SHM_PTR(arena, map->values_off, lb_patch_list_shm_t);

    for (uint32_t i = 0; i < map->count; i++) {
        char* stored_key = SHM_PTR(arena, keys[i], char);
        if (strcmp(stored_key, key) == 0) {
            /** found the key, remove it by shifting remaining elements */
            if (i < map->count - 1) {
                /** shift keys and values down */
                memmove(&keys[i], &keys[i + 1], (map->count - i - 1) * sizeof(uint32_t));
                memmove(&values[i], &values[i + 1], (map->count - i - 1) * sizeof(lb_patch_list_shm_t));
            }
            map->count--;
            return;
        }
    }
}

void patch_add_transform(lb_shm_arena_t* arena, lb_patch_shm_t* patch, const char* match, const char* replace)
{
    if (patch->transform_count >= patch->transform_capacity) {
        uint32_t new_capacity = patch->transform_capacity ? patch->transform_capacity * 2 : 4;
        uint32_t new_off = shm_arena_alloc(arena, new_capacity * sizeof(lb_transform_shm_t));
        if (new_off == 0) return;

        if (patch->transforms_off) {
            memcpy(SHM_PTR(arena, new_off, lb_transform_shm_t), SHM_PTR(arena, patch->transforms_off, lb_transform_shm_t), patch->transform_capacity * sizeof(lb_transform_shm_t));
        }

        patch->transforms_off = new_off;
        patch->transform_capacity = new_capacity;
    }

    lb_transform_shm_t* transforms = SHM_PTR(arena, patch->transforms_off, lb_transform_shm_t);
    lb_transform_shm_t* t = &transforms[patch->transform_count++];
    t->match_off = shm_strdup(arena, match);
    t->replace_off = shm_strdup(arena, replace);
}

lb_patch_shm_t* patchlist_add(lb_shm_arena_t* arena, lb_patch_list_shm_t* list, const char* file, const char* find)
{
    if (list->patch_count >= list->patch_capacity) {
        uint32_t new_capacity = list->patch_capacity ? list->patch_capacity * 2 : 4;
        uint32_t new_off = shm_arena_alloc(arena, new_capacity * sizeof(lb_patch_shm_t));
        if (new_off == 0) return NULL;

        if (list->patches_off) {
            memcpy(SHM_PTR(arena, new_off, lb_patch_shm_t), SHM_PTR(arena, list->patches_off, lb_patch_shm_t), list->patch_capacity * sizeof(lb_patch_shm_t));
        }

        list->patches_off = new_off;
        list->patch_capacity = new_capacity;
    }

    lb_patch_shm_t* patches = SHM_PTR(arena, list->patches_off, lb_patch_shm_t);
    lb_patch_shm_t* p = &patches[list->patch_count++];
    p->file_off = shm_strdup(arena, file);
    p->find_off = shm_strdup(arena, find);
    p->transforms_off = 0;
    p->transform_count = 0;
    p->transform_capacity = 0;

    return p;
}

void patch_print(lb_shm_arena_t* arena, const lb_patch_shm_t* patch)
{
    log_info("Patch {\n");
    log_info("  file: %s\n", patch->file_off ? SHM_PTR(arena, patch->file_off, char) : "(null)");
    log_info("  find: %s\n", patch->find_off ? SHM_PTR(arena, patch->find_off, char) : "(null)");
    log_info("  transforms (%u):\n", patch->transform_count);

    if (patch->transforms_off) {
        lb_transform_shm_t* transforms = SHM_PTR(arena, patch->transforms_off, lb_transform_shm_t);
        for (uint32_t i = 0; i < patch->transform_count; i++) {
            log_info("    [%u] match: %s\n", i, transforms[i].match_off ? SHM_PTR(arena, transforms[i].match_off, char) : "(null)");
            log_info("        replace: %s\n", transforms[i].replace_off ? SHM_PTR(arena, transforms[i].replace_off, char) : "(null)");
        }
    }
    log_info("}\n");
}

void patchlist_print(lb_shm_arena_t* arena, const lb_patch_list_shm_t* list)
{
    log_info("PatchList (%u patches):\n", list->patch_count);
    if (list->patches_off) {
        lb_patch_shm_t* patches = SHM_PTR(arena, list->patches_off, lb_patch_shm_t);
        for (uint32_t i = 0; i < list->patch_count; i++) {
            log_info("[%u] ", i);
            patch_print(arena, &patches[i]);
        }
    }
}

void hashmap_print(lb_shm_arena_t* arena)
{
    lb_hash_map_shm* map = &arena->map;
    log_info("HashMap (%u entries):\n", map->count);

    uint32_t* keys = SHM_PTR(arena, map->keys_off, uint32_t);
    lb_patch_list_shm_t* values = SHM_PTR(arena, map->values_off, lb_patch_list_shm_t);

    for (uint32_t i = 0; i < map->count; i++) {
        log_info("Key: %s\n", SHM_PTR(arena, keys[i], char));
        patchlist_print(arena, &values[i]);
    }
}

void shm_init_simple()
{
    /** delete any stale instance of the millennium ipc shared memory */
    shm_arena_unlink(SHM_IPC_NAME);

    /** allocate the ipc arena */
    if (!g_lb_patch_arena) {
        g_lb_patch_arena = shm_arena_create(SHM_IPC_NAME, SHM_IPC_SIZE);
        if (!g_lb_patch_arena) {
            log_error("Failed to create shared memory");
            return;
        }
        hashmap_init(g_lb_patch_arena);
    }
}