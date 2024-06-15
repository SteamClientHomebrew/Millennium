#include <windows.h>
#include <iostream>
#include <thread>

uintptr_t WINAPI EntryMain(HMODULE hModule)
{
    HINSTANCE hDll = LoadLibrary("millennium.dll");

    if (!hDll) 
    {
        std::cout << "Could not load the DLL" << std::endl;
        return 1;
    }

    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

int __stdcall DllMain(HMODULE hModule, unsigned long fdwReason, void*)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        const HANDLE thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)EntryMain, hModule, 0, 0);

        if (thread) 
            CloseHandle(thread);
    }
    return true;
}
