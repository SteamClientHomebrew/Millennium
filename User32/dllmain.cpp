#define WIN32_LEAN_AND_MEAN 
#include <inject.hpp>

#pragma warning(disable: 6258)

SkinConfig Config; Console Out;
HANDLE threadHandle;

class Millennium
{
private:
    static void AllocateConsole(void) {
        AllocConsole();
        FILE* fp = freopen("CONOUT$", "w", stdout);
    }
public:
    static DWORD WINAPI Start(LPVOID lpParam)
    {
        if (std::string(GetCommandLine()).find("-cef-enable-debugging") == std::string::npos) {
            MessageBoxA(0, "start steam with -cef-enable-debugging in order to enable webview skinning", "Millennium", MB_ICONINFORMATION);
        }

        if (Config.GetConfig()["show-console"]) {
            AllocateConsole();
            Out.log(GetCommandLine());
            Out.warn("show-console is set to true");
        }

        threadHandle = (HANDLE)CreateThread(NULL, NULL, Initialize, NULL, NULL, NULL);
        return TRUE;
    }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  call, LPVOID lpReserved)
{
    if (call == DLL_PROCESS_ATTACH) CreateThread(NULL, NULL, Millennium::Start, NULL, NULL, NULL);
    if (call == DLL_PROCESS_DETACH) TerminateThread(threadHandle, TRUE);
    return true;
}

