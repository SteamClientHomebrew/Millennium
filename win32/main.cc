#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include "http.h"
#include "unzip.h"
#include "steam.h"
#include <filesystem>
#include "../src/procmon/cmd.h"
#include <thread>

const void shutdown_shim(HINSTANCE hinstDLL) {
    FreeLibraryAndExitThread(hinstDLL, 0);
}

std::string get_platform_module(nlohmann::basic_json<> latest_release, const std::string &latest_version) {
    for (const auto &asset : latest_release["assets"]) {
        if (asset["name"].get<std::string>() == "millennium-" + latest_version + "-windows-x86_64.zip") {
            return asset["browser_download_url"].get<std::string>();
        }
    }
    return {};
}

void download_latest(nlohmann::basic_json<> latest_release, const std::string &latest_version, std::string steam_path) {

    printf("updating from %s to %s\n", MILLENNIUM_VERSION, latest_version.c_str());
    const std::string download_url = get_platform_module(latest_release, latest_version);
    printf("downloading asset: %s\n", download_url.c_str());

    const std::string download_path = (std::filesystem::temp_directory_path() / "millennium.zip").string();
    std::cout << download_path << std::endl;

    if (download_file(download_url, download_path)) { 
        printf("successfully downloaded asset...\n");

        extract_zip(download_path.c_str(), steam_path.c_str()); 
        remove(download_path.c_str());

        printf("updated to %s\n", latest_version.c_str());
    }
    else {
        printf("failed to download asset: %s\n", download_url);
    }
}

const void check_for_updates(std::string steam_path) {

    const auto start = std::chrono::high_resolution_clock::now();

    printf("checking for updates...\n");
    printf("steam path: %s\n", steam_path.c_str());

    try {
        nlohmann::json latest_release;

        printf("fetching releases...");
        std::string releases_str = get("https://api.github.com/repos/shdwmtr/millennium/releases");
        printf(" ok\n");
        printf("parsing releases...");
        const nlohmann::json releases = nlohmann::json::parse(releases_str);
        printf(" ok\n");

        printf("finding latest release...");
        for (const auto &release : releases) {
            if (!release["prerelease"].get<bool>()) {
                latest_release = release;
                printf(" ok\n");
                break;
            }
        }

        const std::string latest_version = latest_release["tag_name"].get<std::string>();
        printf("latest version: %s\n", latest_version.c_str());

        if ((!latest_version.empty() && latest_version != MILLENNIUM_VERSION) || !std::filesystem::exists("millennium.dll")) {
            download_latest(latest_release, latest_version, steam_path);   
        }
        else {
            printf("no updates found\n");
        }
    }
    catch (const nlohmann::json::exception &e) {
        printf("Error parsing JSON: %s\n", e.what());
    }

    const auto end = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double> elapsed = end - start;
    printf("elapsed time: %fs\n", elapsed.count());
}

#ifdef _WIN32
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
#endif

void patch_shared_js_context(std::string steam_path) {
    try {
        // copy index.html to index.html.bak
        const auto shared_js_path = std::filesystem::path(steam_path) / "steamui" / "index.html";
        const auto shared_js_bak_path = std::filesystem::path(steam_path) / "steamui" / "orig.html";

        if (std::filesystem::exists(shared_js_bak_path) && std::filesystem::is_regular_file(shared_js_bak_path)) {
            std::cout << "shared_js_context already patched..." << std::endl;
            return;
        }

        std::cout << "renaming shared_js_context..." << std::endl;
        std::filesystem::rename(shared_js_path, shared_js_bak_path);

        std::cout << "patching shared_js_context..." << std::endl;
        std::ofstream shared_js_patched(shared_js_path, std::ios::trunc);
        shared_js_patched << "<!doctype html><html><head><title>SharedJSContext</title></head></html>";
        shared_js_patched.close();
    }
    catch (const std::exception &e) {
        printf("Error patching shared_js_context: %s\n", e.what());
    }
}

const void load_millennium(HINSTANCE hinstDLL) {

    std::string steam_path = get_steam_path();
    std::unique_ptr<StartupParameters> startupParams = std::make_unique<StartupParameters>();

    if (startupParams->HasArgument("-dev")) {

		if (static_cast<bool>(AllocConsole())) {
			SetConsoleTitleA(std::string("Millennium@" + std::string(MILLENNIUM_VERSION)).c_str());
		}
		
		SetConsoleOutputCP(CP_UTF8);
		void(freopen("CONOUT$", "w", stdout));
		void(freopen("CONOUT$", "w", stderr));
		EnableVirtualTerminalProcessing();
    }

    patch_shared_js_context(steam_path);

    // if (!IsDebuggerPresent()) 
    {
        check_for_updates(steam_path);
        printf("finished checking for updates\n");  
    }

    HMODULE hMillennium = LoadLibrary(TEXT("millennium.dll"));
    if (hMillennium == nullptr) {
        MessageBoxA(nullptr, "Failed to load millennium.dll", "Error", MB_ICONERROR);
        return;
    }
    else {
        printf("loaded millennium...\n");
    }

    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)shutdown_shim, hinstDLL, 0, nullptr);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        auto tr = std::thread(load_millennium, hinstDLL);
        tr.detach();
    }
    return true;
}