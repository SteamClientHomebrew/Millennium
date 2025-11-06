#include <assert.h>
#include <hs.h>
#include <stdint.h>
#include <string.h>

#include "smem.h"
#include "log.h"

typedef struct
{
    unsigned int* ids;
    unsigned long long* froms;
    unsigned long long* tos;
    int count;
    int capacity;
} match_list_t;

int on_file_patch(unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void* ctx)
{
    char* file_content = (char*)ctx;
    log_debug("Matched: %.*s\n", (int)(to - from), file_content + from);
    return 0;
}

int on_file_match(unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void* ctx)
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

int matches_has_id(match_list_t* __haystack, unsigned int __needle)
{
    for (uint32_t i = 0; i < __haystack->count; i++) {
        if (__haystack->ids[i] == __needle) {
            return 1;
        }
    }
    return 0;
}

int get_file_regex_pool(lb_shm_arena_t* arena, const char*** out_regex_pool, uint32_t* out_size)
{
    uint32_t capacity = 0;
    uint32_t file_count = 0;
    const char** file_regex_pool = NULL;

    /** open the shared memory map */
    lb_hash_map_shm* map = &arena->map;
    lb_patch_list_shm_t* values = SHM_PTR(arena, map->values_off, lb_patch_list_shm_t);

    for (uint32_t i = 0; i < map->count; i++) {
        lb_patch_list_shm_t* list = &values[i];
        if (!list->patches_off) continue;

        lb_patch_shm_t* patches = SHM_PTR(arena, list->patches_off, lb_patch_shm_t);
        for (uint32_t j = 0; j < list->patch_count; j++) {
            if (file_count >= capacity) {
                capacity = capacity ? capacity * 2 : 16;
                file_regex_pool = realloc(file_regex_pool, capacity * sizeof(char*));
            }

            char* file = SHM_PTR(arena, patches[j].file_off, char);
            file_regex_pool[file_count++] = file;
        }
    }

    *out_regex_pool = file_regex_pool;
    *out_size = file_count;
    return 0;
}

int patch_file(lb_shm_arena_t* arena, match_list_t* matches, char* file_content, uint32_t f_size)
{
    /** create list of "find" which is the length of the file matches */
    const char** finds = malloc(matches->count * sizeof(char*));
    unsigned int* flags = malloc(matches->count * sizeof(unsigned int));
    unsigned int* ids = malloc(matches->count * sizeof(unsigned int));

    lb_hash_map_shm* map = &arena->map;
    lb_patch_list_shm_t* values = SHM_PTR(arena, map->values_off, lb_patch_list_shm_t);

    /** Build finds array based on matched IDs */
    for (int i = 0; i < matches->count; i++) {
        unsigned int match_id = matches->ids[i];
        flags[i] = HS_FLAG_SOM_LEFTMOST;
        ids[i] = i;

        /** Find the patch corresponding to this match_id */
        int found = 0;
        uint32_t global_idx = 0;
        for (uint32_t j = 0; j < map->count && !found; j++) {
            lb_patch_list_shm_t* list = &values[j];
            if (!list->patches_off) {
                continue;
            }
            lb_patch_shm_t* patches = SHM_PTR(arena, list->patches_off, lb_patch_shm_t);
            for (uint32_t k = 0; k < list->patch_count; k++) {
                if (global_idx == match_id) {
                    finds[i] = SHM_PTR(arena, patches[k].find_off, char);
                    found = 1;
                    break;
                }
                global_idx++;
            }
        }
    }

    for (int i = 0; i < matches->count; i++) {
        log_info("find: %d -> %s\n", i, finds[i]);
    }

    hs_database_t* db = NULL;
    hs_compile_error_t* err = NULL;
    hs_scratch_t* scratch = NULL;

    if (hs_compile_multi(finds, flags, ids, matches->count, HS_MODE_BLOCK, NULL, &db, &err) != HS_SUCCESS) {
        log_error("Compile error: %s\n", err->message);
        hs_free_compile_error(err);
        goto cleanup;
    }

    hs_error_t hs_err = hs_alloc_scratch(db, &scratch);
    if (hs_err != HS_SUCCESS) {
        log_error("Failed to allocate scratch space: error code %d\n", hs_err);
        goto cleanup;
    }

    log_info("scanning finds against file (length %d)...\n", f_size);
    int ret = hs_scan(db, file_content, f_size, 0, scratch, on_file_patch, (void*)file_content);

cleanup:
    if (scratch) hs_free_scratch(scratch);
    if (db) hs_free_database(db);

    free(finds);
    free(flags);
    free(ids);

    return 0;
}

int find_file_matches(char* file_content, uint32_t size, char* local_path)
{
    assert(hs_valid_platform() == HS_SUCCESS);
    lb_shm_arena_t* arena = shm_arena_open(SHM_IPC_NAME, 1024 * 1024);

    uint32_t file_count = 0;
    const char** file_regex_pool = NULL;

    /** get a list of all "file" regex selectors from all plugins*/
    get_file_regex_pool(arena, &file_regex_pool, &file_count);

    /** construct the matches object, used to store the matched files */
    match_list_t matches;
    matches.ids = malloc(file_count * sizeof(unsigned int));
    matches.froms = malloc(file_count * sizeof(unsigned int));
    matches.tos = malloc(file_count * sizeof(unsigned int));
    matches.count = 0;
    matches.capacity = file_count;

    hs_database_t* db = NULL;
    hs_compile_error_t* err = NULL;
    hs_scratch_t* scratch = NULL;

    unsigned int* flags = malloc(file_count * sizeof(unsigned int));
    unsigned int* ids = malloc(file_count * sizeof(unsigned int));

    for (int i = 0; i < file_count; i++) {
        log_info("matching %s against %s\n", local_path, file_regex_pool[i]);
        flags[i] = HS_FLAG_SOM_LEFTMOST;
        ids[i] = i;
    }

    if (hs_compile_multi(file_regex_pool, flags, ids, file_count, HS_MODE_BLOCK, NULL, &db, &err) != HS_SUCCESS) {
        log_error("Compile error: %s\n", err->message);
        hs_free_compile_error(err);
        goto cleanup;
    }

    hs_error_t hs_err = hs_alloc_scratch(db, &scratch);
    if (hs_err != HS_SUCCESS) {
        log_error("Failed to allocate scratch space: error code %d\n", hs_err);
        goto cleanup;
    }

    int ret = hs_scan(db, local_path, strlen(local_path), 0, scratch, on_file_match, (void*)&matches);
    if (ret != HS_SUCCESS) {
        log_error("Failed to finish hyperscan, callback signaled to stop...");
    }

    if (!matches.count) {
        log_info("No matches found for file %s\n", local_path);
        goto cleanup;
    }

    for (int i = 0; i < matches.count; i++) {
        log_info("'file' match %u at [%llu, %llu)\n", matches.ids[i], matches.froms[i], matches.tos[i]);
    }

    patch_file(arena, &matches, file_content, size);

cleanup:
    if (scratch) hs_free_scratch(scratch);
    if (db) hs_free_database(db);
    shm_arena_close(arena, arena->size);

    free(file_regex_pool);
    free(matches.ids);
    free(matches.froms);
    free(matches.tos);
    free(flags);
    free(ids);
    return 0;
}
