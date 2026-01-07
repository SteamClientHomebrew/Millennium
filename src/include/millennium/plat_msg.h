/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2023 - 2026. Project Millennium
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

#pragma once

#include "millennium/logger.h"
#include "millennium/sysfs.h"

#include <cstdio>
#include <cstdlib>
#ifdef __linux__
#include <linux/limits.h>
#endif

typedef enum
{
    MESSAGEBOX_ERROR,
    MESSAGEBOX_WARNING,
    MESSAGEBOX_INFO,
    MESSAGEBOX_QUESTION
} Plat_MessageBoxMessageLevel;

[[maybe_unused]]
static int Plat_ShowMessageBox(const char* title, const char* message, const Plat_MessageBoxMessageLevel level)
{
#ifdef _WIN32
    if (!message) {
        LOG_ERROR("No message provided to message box call");
        return -1;
    }

    UINT type = MB_OK;
    switch (level) {
        case MESSAGEBOX_ERROR:
            type |= MB_ICONERROR;
            break;
        case MESSAGEBOX_WARNING:
            type |= MB_ICONWARNING;
            break;
        case MESSAGEBOX_INFO:
            type |= MB_ICONINFORMATION;
            break;
        case MESSAGEBOX_QUESTION:
            type |= MB_YESNO | MB_ICONQUESTION;
            break;
    }

    return MessageBoxA(nullptr, message, title ? title : "Message", type);
#elif __linux__
    if (!message) {
        LOG_ERROR("No message provided to message box call");
        return -1;
    }

    /**
     * try getting zenity from the system first, and fallback to using steams pinned version.
     * Both should work without issue, but if zenity is installed on the system its better to prefer it.
     */
    const std::string steam_path = SystemIO::GetSteamPath().generic_string();
    static char steam_msg_path[PATH_MAX];
    snprintf(steam_msg_path, sizeof(steam_msg_path), "%s%s", steam_path.c_str(), "ubuntu12_32/steam-runtime/i386/usr/bin/zenity");

    char cmd[PATH_MAX * 2];
    snprintf(cmd, sizeof(cmd), "%s ", steam_msg_path);

    switch (level) {
        case MESSAGEBOX_ERROR:
            strncat(cmd, "--error", sizeof(cmd) - strlen(cmd) - 1);
            break;
        case MESSAGEBOX_WARNING:
            strncat(cmd, "--warning", sizeof(cmd) - strlen(cmd) - 1);
            break;
        case MESSAGEBOX_INFO:
            strncat(cmd, "--info", sizeof(cmd) - strlen(cmd) - 1);
            break;
        case MESSAGEBOX_QUESTION:
            strncat(cmd, "--question", sizeof(cmd) - strlen(cmd) - 1);
            break;
    }

    if (title) {
        strncat(cmd, " --title=\"", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, title, sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, "\"", sizeof(cmd) - strlen(cmd) - 1);
    }

    strncat(cmd, " --text=\"", sizeof(cmd) - strlen(cmd) - 1);
    strncat(cmd, message, sizeof(cmd) - strlen(cmd) - 1);
    strncat(cmd, "\"", sizeof(cmd) - strlen(cmd) - 1);

    Logger.Log("executing command: {}", cmd);
    const int ret = system(cmd);

    if (level == MESSAGEBOX_QUESTION) return (ret == 0) ? 1 : 0; // 1=Yes/OK, 0=No/Cancel
    return ret;
#else
#error "Unsupported Platform"
    return -1;
#endif
}
