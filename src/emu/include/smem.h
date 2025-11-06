#pragma once
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif
#define SHM_IPC_NAME "/lb_millennium_patches"
/** macro to convert offset to pointer */
#define SHM_PTR(arena, off, type) ((type*)((char*)(arena) + (off)))
#define SHM_OFF(arena, ptr) ((uint32_t)((char*)(ptr) - (char*)(arena)))

    typedef struct
    {
        uint32_t match_off;
        uint32_t replace_off;
    } lb_transform_shm_t;

    typedef struct
    {
        uint32_t file_off;
        uint32_t find_off;
        uint32_t transforms_off;
        uint32_t transform_count;
        uint32_t transform_capacity;
    } lb_patch_shm_t;

    typedef struct
    {
        uint32_t patches_off;
        uint32_t patch_count;
        uint32_t patch_capacity;
    } lb_patch_list_shm_t;

    typedef struct
    {
        uint32_t keys_off;   /** offset to uint32_t[] (offsets to strings) */
        uint32_t values_off; /** offset to lb_patch_list_shm_t[] */
        uint32_t count;
        uint32_t capacity;
    } lb_hash_map_shm;

    typedef struct
    {
        uint32_t size;
        uint32_t used;
        lb_hash_map_shm map;
    } lb_shm_arena_t;

    lb_shm_arena_t* shm_arena_create(const char* name, uint32_t size);
    lb_shm_arena_t* shm_arena_open(const char* name, uint32_t size);
    void shm_arena_close(lb_shm_arena_t* arena, uint32_t size);
    void shm_arena_unlink(const char* name);
    uint32_t shm_arena_alloc(lb_shm_arena_t* arena, uint32_t size);

    void hashmap_init(lb_shm_arena_t* arena);
    lb_patch_list_shm_t* hashmap_get(lb_shm_arena_t* arena, const char* key);
    lb_patch_list_shm_t* hashmap_add_key(lb_shm_arena_t* arena, const char* key);

    void patch_add_transform(lb_shm_arena_t* arena, lb_patch_shm_t* patch, const char* match, const char* replace);
    lb_patch_shm_t* patchlist_add(lb_shm_arena_t* arena, lb_patch_list_shm_t* list, const char* file, const char* find);

    void patch_print(lb_shm_arena_t* arena, const lb_patch_shm_t* patch);
    void patchlist_print(lb_shm_arena_t* arena, const lb_patch_list_shm_t* list);
    void hashmap_print(lb_shm_arena_t* arena);

#ifdef __cplusplus
}
#endif
