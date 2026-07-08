#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
    int lb_url_is_patchable(const char* url);
    int lb_url_has_local_file(const char* url);
    int lb_handle_request(const char* url, char** out_data, uint32_t* out_size, const char** out_mime_type);
    int lb_patch_content(const char* url, const uint8_t* in_data, uint32_t in_size, char** out_data, uint32_t* out_size, const char** out_mime_type);
#ifdef __cplusplus
}
#endif
