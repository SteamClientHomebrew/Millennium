#pragma once

#ifdef _WIN32
#include <windows.h>
void register_loopback_conn(HANDLE write_h, HANDLE read_h);
#elif defined(__APPLE__)
// Owns the data pipes. The caller signals stop_fd to interrupt a blocked read or write.
void serve_loopback_conn(int write_fd, int read_fd, int stop_fd);
#else
void register_loopback_conn(int write_fd, int read_fd);
#endif
