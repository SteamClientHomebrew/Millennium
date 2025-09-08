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
#include "internal_logger.h"
#include <string>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#endif
#include <steam_hooks.h>

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
#else
    assert(!"Command line argument parsing not implemented for this platform");
    return false;
#endif
}

#ifdef _WIN32
static u_short GetRemoteDebuggerPort()
{
    const char* portValue = GetAppropriateDevToolsPort();

    if (!portValue) {
        return 0;
    }

    try {
        return static_cast<u_short>(std::stoi(portValue));
    } catch (const std::exception&) {
        return 0;
    }
}
#endif
}; // namespace CommandLineArguments
