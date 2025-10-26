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

/** micro optimization -- declare functions instead of importing them from the windows header */
unsigned long __stdcall GetLastError(void);
void* __stdcall LoadLibraryA(const char* lpLibFileName);
unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
int __stdcall MessageBoxA(void* hWnd, const char* lpText, const char* lpCaption, unsigned int uType);

/** fwd decl standard c funcs */
int _stricmp(const char* str1, const char* str2);
int wsprintfA(char* buf, const char* fmt, ...);

#define MAX_PATH 260

__attribute__((dllexport)) const char* __get_shim_version(void)
{
    return MILLENNIUM_VERSION;
}

/**
 * Show an error message box if we fail to load Millennium
 * it assumes the message is less than 256 characters (which it should be)
 */
static void show_error(unsigned long errorCode)
{
    char msg[256];
    wsprintfA(msg, "Failed to load millennium.dll. Error: %lu", errorCode);
    MessageBoxA(0, msg, "Error", 0x00000010l /** magic number -> MB_ICONERROR */);
}

/**
 * Check if the current process is steam.exe
 * although I haven't seen any causes where this module
 * loads into a non-steam process, its a good safeguard.
 */
static int is_steam_client(void)
{
    char path[MAX_PATH];
    if (!GetModuleFileNameA(0 /** current module */, path, MAX_PATH)) {
        return 0;
    }

    /** parse the filename from the fullpath */
    const char* filename = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '\\' || *p == '/') {
            filename = p + 1;
        }
    }

    return !_stricmp(filename, "steam.exe");
}

/** entry point of the dll */
int __stdcall DllMain(void*, unsigned long fdwReason, void*)
{
    if (fdwReason == 1 /** DLL_PROCESS_ATTACH */ && is_steam_client()) {
        const void* hModule = LoadLibraryA("millennium.dll");
        if (!hModule) {
            show_error(GetLastError());
        }
    }
    return 1;
}