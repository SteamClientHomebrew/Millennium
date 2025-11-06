#include "cef_def.h"

int urlp_path_from_lb(const char* url, char** out_abs, char** out_rel);
int urlp_should_block_lb_req(cef_string_userfree_t url);
