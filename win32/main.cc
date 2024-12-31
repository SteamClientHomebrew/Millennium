#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include "http.h"
#include "unzip.h"
#include "steam.h"

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

    const std::string download_path = steam_path + "/millennium.zip";

    if (download_file(download_url, download_path.c_str())) { 
        printf("successfully downloaded asset...\n");

        extract_zip(download_path.c_str(), "C:/Program Files (x86)/Steam"); 
        remove(download_path.c_str());

        printf("updated to %s\n", latest_version.c_str());
    }
    else {
        printf("failed to download asset: %s\n", download_url);
    }
}

const void check_for_updates() {

    const auto start = std::chrono::high_resolution_clock::now();

    printf("checking for updates...\n");
    std::string steam_path = get_steam_path();
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

const void load_millennium(HINSTANCE hinstDLL) {
    FILE *stream;
    AllocConsole();
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);

    // check_for_updates();
    printf("finished checking for updates\n");

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
        load_millennium(hinstDLL);
    }
    return true;
}