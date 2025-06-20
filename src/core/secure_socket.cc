/**
 * ==================================================
 *   _____ _ _ _             _                     
 *  |     |_| | |___ ___ ___|_|_ _ _____           
 *  | | | | | | | -_|   |   | | | |     |          
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
 * 
 * ==================================================
 * 
 * Copyright (c) 2025 Project Millennium
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "secure_socket.h"
#include <winsock2.h>
#include <windows.h>
#include "url_parser.h"
#include <fmt/format.h>
#include <optional>
#include <iostream>
#include <string>
#include <random>
#include <ws2tcpip.h>
#include <iprtrmib.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <log.h>

std::string GenerateAuthToken(size_t length) 
{
    const std::string charset = "0123456789""ABCDEFGHIJKLMNOPQRSTUVWXYZ""abcdefghijklmnopqrstuvwxyz";
    
    std::random_device rd;  // Non-deterministic random number generator
    std::mt19937 generator(rd());  // Mersenne Twister engine
    std::uniform_int_distribution<> dist(0, charset.size() - 1);

    std::string token;
    token.reserve(length);

    for (size_t i = 0; i < length; ++i) 
    {
        token += charset[dist(generator)];
    }

    return token;
}

const std::string GetAuthToken()
{
    static const std::string authToken = GenerateAuthToken(128); 
    return authToken;
}

struct ClientInfo 
{
    DWORD addr;
    WORD port;
    bool isValid;
};

struct EndpointInfo 
{
    std::string ip;
    int port;
    bool isValid;
};

ClientInfo ExtractClientInfo(const sockaddr_storage& remoteAddr) 
{
    ClientInfo info = {0, 0, false};
    
    if (remoteAddr.ss_family == AF_INET) 
    {
        sockaddr_in* addr4 = (sockaddr_in*)&remoteAddr;
        info.addr = addr4->sin_addr.s_addr;
        info.port = addr4->sin_port;
        info.isValid = true;
        
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr4->sin_addr, ipStr, INET_ADDRSTRLEN);
        Logger.Log("[ipcMain] IPv4 client: {}:{}", ipStr, ntohs(info.port));
    }
    else if (remoteAddr.ss_family == AF_INET6) 
    {
        sockaddr_in6* addr6 = (sockaddr_in6*)&remoteAddr;
        info.port = addr6->sin6_port;
        
        char ipStr[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &addr6->sin6_addr, ipStr, INET6_ADDRSTRLEN);
        Logger.Log("[ipcMain] IPv6 client: {}:{}", ipStr, ntohs(info.port));
        
        // Check if it's an IPv4-mapped IPv6 address (::ffff:x.x.x.x)
        if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) 
        {
            info.addr = *((DWORD*)&addr6->sin6_addr.s6_addr[12]);
            info.isValid = true;
            Logger.Log("[ipcMain] IPv4-mapped address detected: {}:{}", inet_ntoa(*(struct in_addr*)&info.addr), ntohs(info.port));
        } 
        else 
        {
            Logger.Warn("[ipcMain] Pure IPv6 address - not supported yet");
        }
    }
    
    return info;
}

PMIB_TCPTABLE_OWNER_PID GetTcpTable() 
{
    DWORD tableSize = 0;
    GetExtendedTcpTable(NULL, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    
    PMIB_TCPTABLE_OWNER_PID tcpTable = (PMIB_TCPTABLE_OWNER_PID)malloc(tableSize);
    if (tcpTable == NULL) 
    {
        return NULL;
    }
    
    if (GetExtendedTcpTable(tcpTable, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) 
    {
        free(tcpTable);
        return NULL;
    }
    
    return tcpTable;
}

DWORD FindProcessByExactMatch(PMIB_TCPTABLE_OWNER_PID tcpTable, DWORD clientAddr, WORD clientPort) 
{
    Logger.Log("[ipcMain] Searching {} TCP entries...", tcpTable->dwNumEntries);
    
    for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) 
    {
        if (tcpTable->table[i].dwLocalAddr == clientAddr && tcpTable->table[i].dwLocalPort == clientPort && tcpTable->table[i].dwState == MIB_TCP_STATE_ESTAB) 
        {
            DWORD processId = tcpTable->table[i].dwOwningPid;
            Logger.Log("[ipcMain] Found matching TCP entry with PID: {}", processId);
            return processId;
        }
    }
    
    return 0;
}

void LogPortMatchEntries(PMIB_TCPTABLE_OWNER_PID tcpTable, WORD clientPort) 
{
    Logger.Log("[ipcMain] Searching for TCP entries with local port: {}", ntohs(clientPort));
    
    for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) 
    {
        if (tcpTable->table[i].dwLocalPort == clientPort && tcpTable->table[i].dwState == MIB_TCP_STATE_ESTAB) 
        {
            char localIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &tcpTable->table[i].dwLocalAddr, localIP, INET_ADDRSTRLEN);
            
            char remoteIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &tcpTable->table[i].dwRemoteAddr, remoteIP, INET_ADDRSTRLEN);
            
            Logger.Log("[ipcMain] Entry {}: {}:{} -> {}:{} PID:{}", i, localIP, ntohs((u_short)tcpTable->table[i].dwLocalPort), remoteIP, ntohs((u_short)tcpTable->table[i].dwRemotePort), tcpTable->table[i].dwOwningPid);
        }
    }
}

std::string GetProcessNameFromPID(DWORD processId) 
{
    if (processId == 0) 
    {
        Logger.Warn("[ipcMain] No matching process found for socket connection");
        return "Unknown";
    }
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess == NULL) 
    {
        Logger.Warn("[ipcMain] Failed to open process with PID: {}, error: {}", processId, (unsigned long)GetLastError());
        return "Unknown";
    }
    
    char processName[MAX_PATH];
    if (GetModuleFileNameEx(hProcess, NULL, processName, MAX_PATH) == 0) 
    {
        Logger.Warn("[ipcMain] Failed to get module name for PID: {}, error: {}", processId, (unsigned long)GetLastError());
        CloseHandle(hProcess);
        return "Unknown";
    }
    
    CloseHandle(hProcess);
    return processName;
}

EndpointInfo ParseEndpoint(const std::string& endpoint) 
{
    EndpointInfo info = {"", 0, false};
    
    size_t colonPos = endpoint.find_last_of(':');
    if (colonPos == std::string::npos) 
    {
        return info;
    }
    
    info.ip = endpoint.substr(0, colonPos);
    info.port = std::atoi(endpoint.substr(colonPos + 1).c_str());
    info.isValid = true;
    
    return info;
}

DWORD FindProcessByEndpoint(PMIB_TCPTABLE_OWNER_PID tcpTable, ULONG clientAddr, int port) 
{
    for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) 
    {
        if (tcpTable->table[i].dwLocalAddr == clientAddr && ntohs((u_short)tcpTable->table[i].dwLocalPort) == port && tcpTable->table[i].dwState == MIB_TCP_STATE_ESTAB) 
        {
            return tcpTable->table[i].dwOwningPid;
        }
    }
    
    return 0;
}

std::string GetProcessNameFromSocket(SOCKET sock) 
{
    sockaddr_storage remoteAddr;
    int addrLen = sizeof(remoteAddr);
    if (getpeername(sock, (sockaddr*)&remoteAddr, &addrLen) != 0) 
    {
        Logger.Warn("[ipcMain] getpeername failed: {}", WSAGetLastError());
        return "Unknown";
    }
    
    ClientInfo clientInfo = ExtractClientInfo(remoteAddr);
    if (!clientInfo.isValid) 
    {
        return "Unknown";
    }
    
    PMIB_TCPTABLE_OWNER_PID tcpTable = GetTcpTable();
    if (tcpTable == NULL) 
    {
        return "Unknown";
    }
    
    DWORD processId = FindProcessByExactMatch(tcpTable, clientInfo.addr, clientInfo.port);
    
    if (processId == 0) 
    {
        LogPortMatchEntries(tcpTable, clientInfo.port);
    }
    
    free(tcpTable);
    return GetProcessNameFromPID(processId);
}

std::string GetProcessNameFromEndpoint(const std::string& endpoint) 
{
    EndpointInfo endpointInfo = ParseEndpoint(endpoint);
    if (!endpointInfo.isValid) 
    {
        return "Unknown";
    }
    
    ULONG clientAddr;
    if (inet_pton(AF_INET, endpointInfo.ip.c_str(), &clientAddr) != 1) 
    {
        return "Unknown";
    }
    
    PMIB_TCPTABLE_OWNER_PID tcpTable = GetTcpTable();
    if (!tcpTable) 
    {
        return "Unknown";
    }
    
    DWORD processId = FindProcessByEndpoint(tcpTable, clientAddr, endpointInfo.port);
    free(tcpTable);
    
    return GetProcessNameFromPID(processId);
}

std::string GetProcessNameFromConnectionHandle(socketServer* serv, websocketpp::connection_hdl hdl) 
{
    try 
    {
        socketServer::connection_ptr connection = serv->get_con_from_hdl(hdl);
        if (!connection) 
        {
            LOG_ERROR("[ipcMain] Could not get connection from handle");
            return "Unknown";
        }
        
        std::string remoteEndpoint = connection->get_remote_endpoint();
        Logger.Log("[ipcMain] Remote endpoint: {}", remoteEndpoint);
        
        try 
        {
            auto& socket = connection->get_socket();
            auto nativeHandle = socket.lowest_layer().native_handle();
            
            if (nativeHandle == INVALID_SOCKET) 
            {
                LOG_ERROR("[ipcMain] Invalid socket handle for connection: {}", remoteEndpoint);
                return "Unknown";
            }
            
            Logger.Log("[ipcMain] Got socket handle: {}", (void*)&nativeHandle);
            return GetProcessNameFromSocket((SOCKET)nativeHandle);
        }
        catch (const std::exception& e) 
        {
            LOG_ERROR("[ipcMain] Error accessing socket: {}", e.what());
            return GetProcessNameFromEndpoint(remoteEndpoint);
        }
    }
    catch (const std::exception& e) 
    {
        LOG_ERROR("[ipcMain] Error getting process name from connection handle: {}", e.what());
        return "Unknown";
    }
}