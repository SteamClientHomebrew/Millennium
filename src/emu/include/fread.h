#include <stddef.h>
#include <stdint.h>

typedef struct
{
    char* content;
    uint32_t size;
} fread_data;

fread_data fread_file(const char* path);
