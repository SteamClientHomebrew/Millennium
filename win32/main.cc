#include <windows.h>

const void ShutdownShim(HINSTANCE hinstDLL)
{
    // Unload current module
    FreeLibraryAndExitThread(hinstDLL, 0);
}

const void LoadMillennium(HINSTANCE hinstDLL)
{
    HMODULE hMillennium = LoadLibrary(TEXT("millennium.dll"));
    if (hMillennium == nullptr) {
        MessageBoxA(nullptr, "Failed to load millennium.dll", "Error", MB_ICONERROR);
        return; // Exit with error code
    }

    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)ShutdownShim, hinstDLL, 0, nullptr);
}

BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,  // handle to DLL module
    DWORD fdwReason,     // reason for calling function
    LPVOID lpvReserved )  // reserved
{
    switch (fdwReason) 
    {
        case DLL_PROCESS_ATTACH: 
        {
            LoadMillennium(hinstDLL);
            break;
        }
        case DLL_PROCESS_DETACH: 
        {
            // ShutdownShim();
            break;
        }
    }

    return true;
}