#pragma once

#ifdef _WIN32
#include <windows.h>
void register_loopback_conn(HANDLE write_h, HANDLE read_h);
#else
void register_loopback_conn(int write_fd, int read_fd);
#endif
