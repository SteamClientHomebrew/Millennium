#pragma once

#include <iostream>
#include <Windows.h>
#include <Mstcpip.h>
#include <iphlpapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "iphlpapi.lib")

class addr_port {
private:
    PMIB_TCPTABLE2 pTcpTable;
    DWORD dwSize;

    struct proc_info {
        std::string name, path;
    };

    proc_info GetProcessNameFromPID(DWORD processID);

public:
    addr_port() : pTcpTable(nullptr), dwSize(0) {}

    ~addr_port() {
        if (pTcpTable != nullptr) {
            free(pTcpTable);
        }
    }

    bool is_steam_socket(int port, std::vector<std::string>& plist);
};