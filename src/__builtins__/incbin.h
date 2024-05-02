#pragma once 
#include <incbin.h>

/// @brief include sdk text bin into memory
INCTXT(developer_sdk, "api.js");
static const char* API = gdeveloper_sdkData;

INCTXT(frontend_boot, "bootstrap.js");
static const char* fboot_script = gfrontend_bootData;