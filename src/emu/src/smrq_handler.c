#include <linux/limits.h>
#include <stdlib.h>

#include "log.h"
#include "urlp.h"
#include "fread.h"

int find_file_matches(char* file_content, uint32_t size, char* local_path);

int handle_loopback_request(const char* url, char** out_data, uint32_t* out_size)
{
    log_info("processing lb request...\n");

    *out_data = NULL;
    *out_size = 0;

    char *local_path = NULL, *local_short_path = NULL;
    fread_data file_data = { 0 };
    uint8_t* buf = NULL;

    if (urlp_path_from_lb(url, &local_path, &local_short_path) != 0) {
        log_error("Failed to get path from URL: %s\n", url);
        goto cleanup;
    }

    file_data = fread_file(local_path);
    if (!file_data.content) {
        log_error("Failed to read file: %s\n", local_path);
        goto cleanup;
    }

    log_info("read file %s, size: %d\n", local_path, file_data.size);
    find_file_matches(file_data.content, file_data.size, local_short_path);

    *out_data = file_data.content;
    *out_size = file_data.size;
    file_data.content = NULL; /** transfer ownership */

cleanup:
    free(file_data.content);
    free(buf);
    free(local_path);
    free(local_short_path);

    return (*out_data != NULL) ? 0 : -1;
}
