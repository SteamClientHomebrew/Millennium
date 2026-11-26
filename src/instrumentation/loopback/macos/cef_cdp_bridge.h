#pragma once
#include "cef_devtools.h"

// Run CDP in the browser process: install a DevTools observer on `browser` and
// relay raw CDP frames (NUL-delimited) over a unix socket to the runtime. Call it
// from any thread; connecting and socket I/O run on a dispatch queue, the CEF
// calls on the UI thread. `cef` is CEF's dlopen handle. Returns 0 once connecting
// has been scheduled.
int millennium_cdp_bridge_start(cef_browser_t* browser, const char* socket_path, void* cef);
