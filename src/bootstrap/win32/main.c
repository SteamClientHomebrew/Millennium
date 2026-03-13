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
#ifdef _WIN32
#include <wchar.h>

#define MAX_PATH 260
/** millennium.dll is loaded from %LOCALAPPDATA%\Millennium\lib\ */

#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x00001000
#define LOAD_LIBRARY_SEARCH_USER_DIRS 0x00000400

/** micro optimization -- declare functions instead of importing them from the windows header */
unsigned long __stdcall GetLastError(void);
void* __stdcall AddDllDirectory(const wchar_t* NewDirectory);
void* __stdcall LoadLibraryExW(const wchar_t* lpLibFileName, void* hFile, unsigned long dwFlags);
unsigned long __stdcall GetModuleFileNameW(void* hModule, wchar_t* lpFilename, unsigned long nSize);
int __stdcall MessageBoxW(void* hWnd, const wchar_t* lpText, const wchar_t* lpCaption, unsigned int uType);

/** fwd decl standard c funcs */
int __cdecl wsprintfW(wchar_t* buf, const wchar_t* fmt, ...);
wchar_t* __cdecl _wgetenv(const wchar_t*);

static wchar_t g_millennium_dll_path[MAX_PATH];
static wchar_t g_millennium_lib_dir[MAX_PATH];

static int resolve_millennium_path(void)
{
    const wchar_t* localappdata = _wgetenv(L"LOCALAPPDATA");
    if (!localappdata) {
        MessageBoxW(0, L"Failed to _wgetenv LOCALAPPDATA, this is likely an issue with your Windows installation.", L"Millennium", 0x00000010l /** magic number -> MB_ICONERROR */);
        return 0;
    }
    wsprintfW(g_millennium_lib_dir, L"%s\\Millennium\\lib", localappdata);
    wsprintfW(g_millennium_dll_path, L"%s\\millennium.dll", g_millennium_lib_dir);
    return 1;
}

__declspec(dllexport) const char* __get_shim_version(void)
{
    return MILLENNIUM_VERSION;
}

static void show_error(unsigned long errorCode, const wchar_t* dll_path)
{
    wchar_t msg[512];
    wsprintfW(msg,
              L"Millennium failed to load (%s). This will not affect the functionality of the Steam Client.\n\nCommon Causes:\n- Antivirus deleting Millennium\n- Outdated "
              L"Millennium version, try updating\n\nError Code: %lu",
              dll_path, errorCode);
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
        if (!resolve_millennium_path()) {
            MessageBoxW(0, L"Millennium failed to resolve LOCALAPPDATA path.", L"Millennium", 0x00000010l);
            return 1;
        }
        AddDllDirectory(g_millennium_lib_dir);
        const void* hModule = LoadLibraryExW(g_millennium_dll_path, NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!hModule) {
            show_error(GetLastError(), g_millennium_dll_path);
        }
    }
    return 1;
}
#endif
