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

#include "millennium/logger.h"
#include "millennium/steam_hooks.h"
#include "millennium/cmdline_parse.h"
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#elif __linux__
#include <dlfcn.h>
#elif defined(__APPLE__)
#include <crt_externs.h>
#endif

bool CommandLineArguments::has_argument(const std::string& targetArgument)
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
#elif __linux__
    using Plat_CommandLineParamExists_t = bool (*)(const char*);
    void* handle = dlopen("libtier0_s.so", RTLD_NOW);
    if (!handle) {
        LOG_ERROR("Failed to get handle of libtier0_s.so!");
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
#elif defined(__APPLE__)
    int* argc = _NSGetArgc();
    char*** argv = _NSGetArgv();
    if (!argc || !argv || !*argv) {
        return false;
    }

    for (int i = 1; i < *argc; ++i) {
        const char* argument = (*argv)[i];
        if (argument && targetArgument == argument) {
            return true;
        }
    }
    return false;
#else
    return false;
#endif
}

unsigned short CommandLineArguments::get_remote_debugger_port()
{
    /**
     * Always prefer explicit -devtools-port when present.
     * This keeps CDP attachment aligned with wrapper/bootstrap on macOS.
     */
#if defined(__APPLE__)
    int* argc = _NSGetArgc();
    char*** argv = _NSGetArgv();
    if (argc && argv && *argv) {
        constexpr const char* option = "-devtools-port";
        constexpr size_t option_len = 14;

        for (int i = 1; i < *argc; ++i) {
            const char* argument = (*argv)[i];
            if (!argument) {
                continue;
            }

            if (strcmp(argument, option) == 0) {
                if (i + 1 < *argc && (*argv)[i + 1]) {
                    try {
                        return static_cast<unsigned short>(std::stoi((*argv)[i + 1]));
                    } catch (const std::exception&) {
                        return 0;
                    }
                }

                return 0;
            }

            if (strncmp(argument, "-devtools-port=", option_len + 1) == 0) {
                const char* value = argument + option_len + 1;
                if (!value || value[0] == '\0') {
                    return 0;
                }

                try {
                    return static_cast<unsigned short>(std::stoi(value));
                } catch (const std::exception&) {
                    return 0;
                }
            }
        }
    }
#endif

    const char* portValue = GetAppropriateDevToolsPort(CommandLineArguments::has_argument("-dev"));

    if (!portValue) {
        return 0;
    }

    try {
        return static_cast<unsigned short>(std::stoi(portValue));
    } catch (const std::exception&) {
        return 0;
    }
}
