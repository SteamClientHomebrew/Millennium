#include <windows.h>
#include <stdio.h>

unsigned int __stdcall EntryMain(HMODULE hModule)
{
    HINSTANCE hDll = LoadLibrary("millennium.dll");

    if (!hDll) 
    {
        printf("Couldn't locate millennium.dll, a vital library needed.\n");
        return 1;
    }

    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

int __stdcall DllMain(HMODULE hModule, unsigned long fdwReason, void* _)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        const HANDLE thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)EntryMain, hModule, 0, 0);

        if (thread)
        {
            CloseHandle(thread);
        }
    }
    return 1;
}
