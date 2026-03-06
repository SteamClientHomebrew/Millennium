#pragma once

/*
 * crash_handler — installs signal/exception handlers in the lua-host child
 * process so that crashes produce a human-readable dump file with:
 *   - Millennium version + git commit
 *   - plugin name + backend file
 *   - signal / exception info
 *   - C stack trace (with function names via -rdynamic / DbgHelp)
 *   - core dump path hint (Linux) / minidump (Windows)
 *
 * Call install_crash_handler() once, early in main().
 */

void install_crash_handler(const char* plugin_name, const char* backend_file, const char* steam_path);
