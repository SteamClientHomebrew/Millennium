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

#ifdef __linux__
#include "millennium/platform_hooks.h"
#include "millennium/logger.h"
#include <fmt/core.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>

namespace
{
int is_same_path(const char* path1, const char* path2)
{
    char realpath1[PATH_MAX], realpath2[PATH_MAX];

    if (realpath(path1, realpath1) == NULL || realpath(path2, realpath2) == NULL) {
        LOG_ERROR("Failed to resolve paths: {} or {}", path1, path2);
        return 0;
    }

    return strcmp(realpath1, realpath2) == 0;
}
} // namespace

bool Plat_ShouldSetupEnvironment()
{
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);

    if (len == -1) {
        perror("readlink");
        return false;
    }

    path[len] = '\0';

    std::string steam_path = fmt::format("{}/.steam/steam/ubuntu12_32/steam", std::getenv("HOME"));
    if (!is_same_path(path, steam_path.c_str())) {
        logger.warn("[Millennium] Process is not running under Steam: {}", path);
        return false;
    }

    logger.log("[Millennium] Setting up environment for Steam process: {}", path);
    return true;
}

void Plat_BeforeAttachMillennium()
{
}

void Plat_AfterDetachMillennium()
{
}
#endif
