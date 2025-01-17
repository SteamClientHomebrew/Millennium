#include <windows.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <shimlogger.h>
#include <fstream>
#include <http.h>
#include <unzip.h>
#include <steam.h>
#include <procmon/cmd.h>
#include <thread>

static const char* GITHUB_API_URL = "https://api.github.com/repos/shdwmtr/millennium/releases";

const void UnloadAndReleaseLibrary(HINSTANCE hinstDLL) 
{
    FreeLibraryAndExitThread(hinstDLL, 0);
}

std::string GetPlatformAssetModule(nlohmann::basic_json<> latestRelease, const std::string &latest_version) 
{
    for (const auto &asset : latestRelease["assets"]) 
    {
        if (asset["name"].get<std::string>() == "millennium-" + latest_version + "-windows-x86_64.zip") 
        {
            return asset["browser_download_url"].get<std::string>();
        }
    }
    return {};
}

extern "C" __attribute__((dllexport)) const char* __get_shim_version(void)
{
    return MILLENNIUM_VERSION;
}

void DownloadLatestAsset(std::string queuedDownloadUrl, std::string steam_path) 
{
    Print("Downloading asset: {}", queuedDownloadUrl);
    const std::string localDownloadPath = (std::filesystem::temp_directory_path() / "millennium.zip").string();

    if (DownloadResource(queuedDownloadUrl, localDownloadPath)) 
    { 
        Print("Successfully downloaded asset...");

        ExtractZippedArchive(localDownloadPath.c_str(), steam_path.c_str()); 
        remove(localDownloadPath.c_str());
    }
    else 
    {
        Error("Failed to download asset: {}", queuedDownloadUrl);
    }
}

/**
 * @brief Check for updates based off the update.flag file in the Steam directory.
 * Updates are not automatically derived from the server, they are specifically read from what the user has requested in the update.flag file.
 * the update.flag file is written to by assets\src\custom_components\UpdaterModal.tsx
 */
const void CheckForUpdates(std::string strSteamPath) 
{
    const auto start = std::chrono::high_resolution_clock::now();

    Print("Checking for updates...");
    Print("Steam path: {}", strSteamPath);

    const auto updateFlagPath = std::filesystem::path(strSteamPath) / "ext" / "update.flag";

    if (!std::filesystem::exists(updateFlagPath)) 
    {   
        Print("No updates were queried...");
        return;
    }

    std::ifstream updateFlagFile(updateFlagPath);
    std::string updateFlagContent((std::istreambuf_iterator<char>(updateFlagFile)), std::istreambuf_iterator<char>());
    updateFlagFile.close();

    if (std::remove(updateFlagPath.string().c_str()) != 0) 
    {
        Error("Failed to remove update.flag file...");
    }

    Print("Updating Millennium from queue: {}", updateFlagContent);
    DownloadLatestAsset(updateFlagContent, strSteamPath);  

    Print("Elapsed time: {}s", (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start)).count());
}

void EnableVirtualTerminalProcessing() 
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
    {
        return;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode))
    {
        return;
    }
}

void PatchSharedJSContext(std::string strSteamPath) 
{
    try 
    {
        const auto SteamUIModulePath       = std::filesystem::path(strSteamPath) / "steamui" / "index.html";
        const auto SteamUIModuleBackupPath = std::filesystem::path(strSteamPath) / "steamui" / "orig.html";

        if (std::filesystem::exists(SteamUIModuleBackupPath) && std::filesystem::is_regular_file(SteamUIModuleBackupPath)) 
        {
            Print("SharedJSContext already patched...");
            return;
        }

        Print("Backing up original SharedJSContext...");
        std::filesystem::rename(SteamUIModulePath, SteamUIModuleBackupPath);

        Print("Patching SharedJSContext...");
        
        // Write an empty document to the file, this halts the Steam UI from loading as we've removed the <script>'s
        std::ofstream SteamUI(SteamUIModulePath, std::ios::trunc);
        SteamUI << "<!doctype html><html><head><title>SharedJSContext</title></head></html>";
        SteamUI.close();
    }
    catch (const std::exception &exception) 
    {
        Error("Error patching SharedJSContext: {}", exception.what());
    }
}

const void BootstrapMillennium(HINSTANCE hinstDLL) 
{
    const std::string strSteamPath = GetSteamPath();
    std::unique_ptr<StartupParameters> startupParams = std::make_unique<StartupParameters>();

    if (startupParams->HasArgument("-dev")) 
    {
		if (static_cast<bool>(AllocConsole())) 
        {
			SetConsoleTitleA(std::string("Millennium@" + std::string(MILLENNIUM_VERSION)).c_str());
		}

        freopen("CONOUT$", "w", stdout);
        EnableVirtualTerminalProcessing();
    }

    Print("Loading Bootstrapper@{}", MILLENNIUM_VERSION);

    PatchSharedJSContext(strSteamPath);
    CheckForUpdates(strSteamPath);

    Print("Finished checking for updates");  

    // Check if millennium.dll is already loaded in process memory, and if so, unload it.
    HMODULE hLoadedMillennium = GetModuleHandleA("millennium.dll");
    if (hLoadedMillennium != nullptr) 
    {
        Print("Unloading millennium.dll and reloading...");
        FreeLibrary(hLoadedMillennium);
    }

    // After checking for updates, load Millenniums core library.
    HMODULE hMillennium = LoadLibraryA("millennium.dll");
    if (hMillennium == nullptr) 
    {
        MessageBoxA(nullptr, "Failed to load millennium.dll. Please reinstall/remove Millennium to continue.", "Error", MB_ICONERROR);
        return;
    }
    else 
    {
        Print("Loaded millennium...");
    }

    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)UnloadAndReleaseLibrary, hinstDLL, 0, nullptr);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) 
{
    if (fdwReason == DLL_PROCESS_ATTACH) 
    {
        // Detach from the main thread as to not wakeup the watchdog from Steam's debugger. 
        std::thread(BootstrapMillennium, hinstDLL).detach();
    }
    
    return true;
}