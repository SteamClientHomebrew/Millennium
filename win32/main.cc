#include <windows.h>

const void ShutdownShim(HINSTANCE hinstDLL)
{
    FreeLibraryAndExitThread(hinstDLL, 0);
}

const void LoadMillennium(HINSTANCE hinstDLL)
{
    HMODULE hMillennium = LoadLibrary(TEXT("millennium.dll"));
    if (hMillennium == nullptr) 
    {
        MessageBoxA(nullptr, "Failed to load millennium.dll", "Error", MB_ICONERROR);
        return;
    }

    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)ShutdownShim, hinstDLL, 0, nullptr);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) 
    {
        LoadMillennium(hinstDLL);
    }

    return true;
}