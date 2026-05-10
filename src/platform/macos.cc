/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef __APPLE__
#include "millennium/logger.h"
#include "millennium/platform_hooks.h"
#include <stdint.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <string.h>

extern "C" void Plat_InstallMacOSMenuItems();
extern "C" void Plat_InstallMacOSNativeWindowStyling();

bool platform::should_setup_environment()
{
    char path[PATH_MAX];
    uint32_t path_size = static_cast<uint32_t>(sizeof(path));

    if (_NSGetExecutablePath(path, &path_size) != 0) {
        logger.warn("[Millennium] Could not resolve macOS executable path. Continuing with environment setup.");
        return true;
    }

    path[sizeof(path) - 1] = '\0';
    const char* executable_name = strrchr(path, '/');
    executable_name = executable_name ? executable_name + 1 : path;

    if (strcmp(executable_name, "steam_osx") != 0) {
        logger.warn("[Millennium] Process is not Steam main executable: {}", path);
        return false;
    }

    logger.log("[Millennium] Setting up environment for Steam process: {}", path);
    return true;
}

void platform::before_attach_millennium()
{
    Plat_InstallMacOSMenuItems();
    Plat_InstallMacOSNativeWindowStyling();
}

void platform::after_detach_millennium()
{
}
#endif
