#include "fread.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * read a file from the provided path.
 * returns empty fread_data if the file has no content/does not exist.
 *
 * caller is responsible for freeing fread_data.content
 */
fread_data fread_file(const char* path)
{
    fread_data result = { 0, 0 };

    /** open as read bytes */
    FILE* f = fopen(path, "rb");
    if (!f) return result;

    /** get the length of the file by reading to the end */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        return result;
    }

    /** allocate file size in memory */
    result.content = malloc(file_size);
    if (!result.content) {
        fclose(f);
        return result;
    }

    long total = 0;
    while (total < file_size) {
        long to_read = file_size - total;
        /** read the file in chunks, this increases the read speed for large files */
        if (to_read > 65536) to_read = 65536;

        long n = fread(result.content + total, 1, to_read, f);
        if (n == 0) break;

        total += n;
    }

    /** total read bytes */
    result.size = total;
    fclose(f);
    return result;
}
