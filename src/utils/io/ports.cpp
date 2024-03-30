#include "ports.hpp"
#include <utils/config/config.hpp>
#include <locale>
#include <codecvt>

std::string wstring_to_string(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

addr_port::proc_info addr_port::GetProcessNameFromPID(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    if (hProcess != NULL) {
        WCHAR exePath[MAX_PATH];
        DWORD pathSize = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, exePath, &pathSize) != 0) {
            CloseHandle(hProcess);

            std::wstring widePath(exePath);
            std::string fullPath = wstring_to_string(widePath);

            size_t lastSlashPos = fullPath.find_last_of("\\");
            if (lastSlashPos != std::string::npos) {
                return {
                    fullPath.substr(lastSlashPos + 1),
                    fullPath
                };
            }
            else {
                return {
                    fullPath, fullPath
                };
            }
        }
        CloseHandle(hProcess);
    }
    return {};
}

bool addr_port::is_steam_socket(int port, std::vector<std::string>& plist) {
    pTcpTable = (MIB_TCPTABLE2*)malloc(sizeof(MIB_TCPTABLE2));
    if (pTcpTable == nullptr) {
        std::cerr << "Error allocating memory\n";
        return false;
    }

    dwSize = sizeof(MIB_TCPTABLE);
    if (GetTcpTable2(pTcpTable, &dwSize, TRUE) == ERROR_INSUFFICIENT_BUFFER) {
        free(pTcpTable);
        pTcpTable = (MIB_TCPTABLE2*)malloc(dwSize);
        if (pTcpTable == nullptr) {
            std::cerr << "Error allocating memory\n";
            return false;
        }
    }

    if (GetTcpTable2(pTcpTable, &dwSize, TRUE) != NO_ERROR) {
        std::cerr << "GetTcpTable2 failed with error " << GetLastError() << std::endl;
        free(pTcpTable);
        pTcpTable = nullptr;
        return false;
    }

    bool is_steam_open = false;

    for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++) {
        if (pTcpTable->table[i].dwLocalPort == htons(port)) {

            const auto proc = GetProcessNameFromPID(pTcpTable->table[i].dwOwningPid);
            plist.push_back(proc.path);

            if (proc.name == "steamwebhelper.exe") {
                is_steam_open = true;
            }
        }
    }

    return is_steam_open;
}