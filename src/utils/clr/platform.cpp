#include <stdafx.h>
#include "platform.hpp"
#include <iostream>
#include <format>
#include <regex>

/**
 * @brief Converts a UTF-8 encoded std::string to an LPCWSTR.
 *
 * This function converts a UTF-8 encoded std::string to an LPCWSTR (wide-character string).
 *
 * @param str The input std::string to be converted.
 * @return An LPCWSTR representing the converted string. The caller is responsible for freeing the memory.
 */
LPCWSTR s_lpcwstr(const std::string& str) {
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    wchar_t* wideStr = new wchar_t[size];
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wideStr, size);

    return wideStr;
}

namespace clr_interop {

    clr_base::clr_base() : pMetaHost(nullptr), pRuntimeInfo(nullptr), pClrRuntimeHost(nullptr) {
        // Constructor, initialize any other members if needed
    }

    clr_base::~clr_base() {
        // Destructor, cleanup any remaining resources
        cleanup_clr();
    }

    /**
     * @brief Initializes the Common Language Runtime (CLR).
     *
     * This function initializes the CLR by creating an instance of the CLRMetaHost,
     * obtaining the runtime information for the specified version, checking if the runtime
     * is loadable, and starting the CLR runtime host.
     *
     * @return true if the CLR initialization succeeds, false otherwise.
     */
    bool clr_base::initialize_clr() {
        HRESULT hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost, (LPVOID*)&pMetaHost);

        if (FAILED(hr)) {

            console.log(std::format("[-] CLRCreateInstance failed: -> {}", hr));
            return false;
        }

        hr = pMetaHost->GetRuntime(L"v4.0.30319", IID_ICLRRuntimeInfo, (LPVOID*)&pRuntimeInfo);
        if (FAILED(hr)) {
            console.log(std::format("[-] pMetaHost->GetRuntime failed: -> {}", hr));
            pMetaHost->Release();
            return false;
        }

        BOOL loadable;
        hr = pRuntimeInfo->IsLoadable(&loadable);
        if (FAILED(hr)) {
            console.log(std::format("[-] pRuntimeInfo->IsLoadable failed: -> {}", hr));
            pRuntimeInfo->Release();
            pMetaHost->Release();
            return false;
        }

        if (!loadable) {
            console.log(std::format("[-] CLR is not loadable: -> {}", hr));
            pRuntimeInfo->Release();
            pMetaHost->Release();
            return false;
        }

        hr = pRuntimeInfo->GetInterface(CLSID_CLRRuntimeHost, IID_ICLRRuntimeHost, (LPVOID*)&pClrRuntimeHost);
        if (FAILED(hr)) {
            std::cout << "Failed to get CLR runtime host interface. Error code: 0x" << std::hex << hr << std::endl;
            console.log(std::format("[-] pRuntimeInfo->GetInterface() failed: -> {}", hr));
            pRuntimeInfo->Release();
            pMetaHost->Release();
            return false;
        }

        hr = pClrRuntimeHost->Start();
        if (FAILED(hr)) {
            std::cout << "Failed to start CLR. Error code: 0x" << std::hex << hr << std::endl;
            console.log(std::format("[-] pClrRuntimeHost->Start() failed: -> {}", hr));
            pClrRuntimeHost->Release();
            pRuntimeInfo->Release();
            pMetaHost->Release();
            return false;
        }

        return true;
    }

    /**
     * @brief Starts the update process using the CLR runtime host.
     *
     * This function starts the update process by executing the specified assembly in the
     * default application domain of the CLR runtime host. It first checks if the CLR runtime host
     * is initialized, and if not, initializes it. Then, it executes the assembly specified by the
     * path parameter, passing the provided parameters to the entry point method.
     *
     * @param param Additional parameters to be passed to the entry point method.
     * @return true if the update process starts successfully, false otherwise.
     */
    bool clr_base::start_update(const std::string& param) {

        // Check if the CLR runtime host is initialized, and initialize if not
        if (!pClrRuntimeHost) {
            if (!initialize_clr()) {
                return false;
            }
        }

        DWORD out;

        try {

            char buffer[MAX_PATH];
            DWORD bufferSize = GetEnvironmentVariableA("SteamPath", buffer, MAX_PATH);

            // Construct the path to the updater assembly
            std::string path = std::format("{}/millennium.updater.dll", std::string(buffer, bufferSize));

            const auto hr = pClrRuntimeHost->ExecuteInDefaultAppDomain(
                s_lpcwstr(path),
                L"updater.Entry",
                L"Bootstrapper",
                s_lpcwstr(param),
                &out
            );

            if (FAILED(hr))
            {
                console.log(std::format("[+] clr_base->start_update() > can't host app context : -> {}", hr));
            }

            console.log(std::format("[+] pClrRuntimeHost->ExecuteInDefaultAppDomain().result : -> {}", out));
        }
        catch (std::exception& ex) {
            console.log(std::format("[+] pClrRuntimeHost->ExecuteInDefaultAppDomain() exception : -> {}", ex.what()));
        }
        return (bool)out;
    }

    /**
     * @brief Cleans up the CLR resources.
     *
     * This function stops the CLR runtime host, releases the allocated resources, and sets the pointers to nullptr
     * to indicate that the CLR is not initialized.
     */
    void clr_base::cleanup_clr() {
        if (pClrRuntimeHost) {
            // Stop the CLR
            pClrRuntimeHost->Stop();

            // Release resources
            pClrRuntimeHost->Release();
            pRuntimeInfo->Release();
            pMetaHost->Release();

            // Set to nullptr to indicate CLR is not initialized
            pClrRuntimeHost = nullptr;
            pRuntimeInfo = nullptr;
            pMetaHost = nullptr;
        }
    }
}