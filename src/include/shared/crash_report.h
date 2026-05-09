/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|_|___|_|_|_|
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

#pragma once

#include <cstdio>
#include <string>

struct crash_report_info
{
    const char* title;
    const char* component;
    const char* plugin_name;
    const char* backend_file;
    const char* log_prefix;
};

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>

const char* crash_report_exception_name(DWORD code);
bool crash_report_is_fatal(DWORD code);
void crash_report_write_minidump(EXCEPTION_POINTERS* ep, const std::string& path);

void crash_report_write(const crash_report_info& info, const std::string& dir, EXCEPTION_POINTERS* ep);
void crash_report_dispatch(EXCEPTION_POINTERS* ep, void (*writer)(EXCEPTION_POINTERS*));

#else

const char* crash_report_signal_name(int sig);

void crash_report_write(const crash_report_info& info, const std::string& dir, int sig);

#endif
