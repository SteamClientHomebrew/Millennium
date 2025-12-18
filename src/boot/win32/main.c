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
#include <wchar.h>
#include <windows.h>
#ifdef _WIN32
/** micro optimization -- declare functions instead of importing them from the windows header */
// unsigned long __stdcall GetLastError(void);
// void* __stdcall AddDllDirectory(const wchar_t* NewDirectory);
// void* __stdcall LoadLibraryA(const char* lpLibFileName);
// unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
// int __stdcall MessageBoxA(void* hWnd, const char* lpText, const char* lpCaption, unsigned int uType);

// /** fwd decl standard c funcs */
// int _stricmp(const char* str1, const char* str2);
// int wsprintfA(char* buf, const char* fmt, ...);

#define MAX_PATH 260

__declspec(dllexport) const char* __get_shim_version(void)
{
    return MILLENNIUM_VERSION;
}

#ifdef MILLENNIUM_32BIT
#define MILLENNIUM_LIBRARY_PATH L"millennium.dll"
#elif defined(MILLENNIUM_64BIT)
#define MILLENNIUM_LIBRARY_ROOT L"ext\\compat64"
#define MILLENNIUM_LIBRARY_PATH L"\\millennium_x64.dll"
#else
#error "Neither 32bit or 64bit Millennium was provided"
#endif

wchar_t* g_millennium_path = 0;

/**
 * Show an error message box if we fail to load Millennium
 * it assumes the message is less than 256 characters (which it should be)
 */
 static void show_error(unsigned long errorCode, const wchar_t* dll_path)
 {
     wchar_t msg[512];
     wsprintfW(msg, L"Millennium failed to load (%s). This will not affect the functionality of the Steam Client.\n\nCommon Causes:\n- Antivirus deleting Millennium\n- Outdated Millennium version, try updating\n\nError Code: %lu", dll_path, errorCode);
     MessageBoxW(0, msg, L"Millennium", 0x00000010l /** magic number -> MB_ICONERROR */);
 }

/**
 * Check if the current process is steam.exe
 * although I haven't seen any causes where this module
 * loads into a non-steam process, its a good safeguard.
 */
static int is_steam_client(wchar_t* steam_path, size_t path_size)
{
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(0, path, MAX_PATH)) {
        return 0;
    }

    const wchar_t* filename = path;
    for (const wchar_t* p = path; *p; ++p) {
        if (*p == L'\\' || *p == L'/') {
            filename = p + 1;
        }
    }

    int is_steam = !_wcsicmp(filename, L"steam.exe");
    if (is_steam && steam_path) {
        wcsncpy(steam_path, path, path_size - 1);
        steam_path[path_size - 1] = L'\0';
    }

    return is_steam;
}

/** entry point of the dll */
int __stdcall DllMain(void* hinstDLL, unsigned long fdwReason, void* lpReserved)
{
    if (fdwReason == 1 /** DLL_PROCESS_ATTACH */ && is_steam_client(NULL, 0)) {

        #ifdef MILLENNIUM_LIBRARY_ROOT
        wchar_t abs_root[MAX_PATH];
        wchar_t abs_path[MAX_PATH];

        if (is_steam_client(abs_root, MAX_PATH)) {
            wchar_t* last_slash = wcsrchr(abs_root, L'\\');
            if (last_slash) {
                *(last_slash + 1) = L'\0';
            }
            wcscat_s(abs_root, MAX_PATH, MILLENNIUM_LIBRARY_ROOT);

            wcscpy_s(abs_path, MAX_PATH, abs_root);
            wcscat_s(abs_path, MAX_PATH, MILLENNIUM_LIBRARY_PATH);

            AddDllDirectory(abs_root);
            const void* hModule = LoadLibraryExW(abs_path, NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (!hModule) {
                show_error(GetLastError(), abs_path);
            }
        }
        #else
        const void* hModule = LoadLibraryExW(MILLENNIUM_LIBRARY_PATH, NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!hModule) {
            show_error(GetLastError(), MILLENNIUM_LIBRARY_PATH);
        }
        #endif
    }
    return 1;
}
#endif
