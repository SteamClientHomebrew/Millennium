#pragma once
#include "cef_devtools.h"

// Run CDP in the browser process: install a DevTools observer on `browser` and
// relay raw CDP frames (NUL-delimited) over a unix socket to the runtime. Capture
// `browser` from the resource-handler hook on any thread; connecting and the CEF
// calls happen on their own threads. Returns 0 once the connect thread is started.
int millennium_cdp_bridge_start(cef_browser_t* browser, const char* socket_path);
