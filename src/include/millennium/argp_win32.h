/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
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
#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif
#include "millennium/steam_hooks.h"

namespace CommandLineArguments
{
static bool HasArgument(const std::string& targetArgument)
{
#ifdef _WIN32
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        std::wstring targetW(targetArgument.begin(), targetArgument.end());
        for (int i = 0; i < argc; ++i) {
            if (targetW == argv[i]) {
                LocalFree(argv);
                return true;
            }
        }
        LocalFree(argv);
    }
    return false;
#elif defined(__linux__) || defined(__APPLE__)
    using Plat_CommandLineParamExists_t = bool (*)(const char*);
#if defined(__APPLE__)
    const char* libraryName = "libtier0_s.dylib";
#else
    const char* libraryName = "libtier0_s.so";
#endif
    void* handle = dlopen(libraryName, RTLD_NOW);
    if (!handle) {
        LOG_ERROR("Failed to get handle of {}!", libraryName);
        return false;
    }

    auto func = (Plat_CommandLineParamExists_t)dlsym(handle, "Plat_CommandLineParamExists");
    if (!func) {
        LOG_ERROR("Failed to get the function handle of Plat_CommandLineParamExists!");
        dlclose(handle);
        return false;
    }

    bool result = func(targetArgument.c_str());
    dlclose(handle);
    return result;
#endif
    return false;
}

inline u_short GetRemoteDebuggerPort()
{
    const char* portValue = GetAppropriateDevToolsPort(CommandLineArguments::HasArgument("-dev"));

    if (!portValue) {
        return 0;
    }

    try {
        return static_cast<u_short>(std::stoi(portValue));
    } catch (const std::exception&) {
        return 0;
    }
}
}; // namespace CommandLineArguments
