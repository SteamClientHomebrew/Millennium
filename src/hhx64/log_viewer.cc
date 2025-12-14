#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <algorithm>

#pragma pack(push, 1)
struct DBWIN_BUFFER
{
    DWORD dwProcessId;
    CHAR data[4096 - sizeof(DWORD)];
};
#pragma pack(pop)

static std::string pid_to_imagepath(DWORD pid)
{
    if (pid == 0) return "<idle>";
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProc) return "<cannot open process>";
    char path[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameA(hProc, 0, path, &size)) {
        CloseHandle(hProc);
        return std::string(path, size);
    }
    if (GetModuleFileNameExA(hProc, NULL, path, MAX_PATH) > 0) {
        CloseHandle(hProc);
        return std::string(path);
    }
    CloseHandle(hProc);
    return "<unknown>";
}

static std::string filename_from_path(const std::string& path)
{
    size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

void EnableVirtualTerminalProcessing()
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        return;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) {
        return;
    }

    SetConsoleOutputCP(CP_UTF8);
}

int main(int argc, char** argv)
{
    (void)static_cast<bool>(AllocConsole());
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    const std::string targetExe = "steamwebhelper.exe";
    const char* mutexName = "DBWinMutex";
    const char* mapName = "DBWIN_BUFFER";
    const char* eventDataName = "DBWIN_DATA_READY";
    const char* eventBufferReadyName = "DBWIN_BUFFER_READY";

    HANDLE hMutex = CreateMutexA(NULL, FALSE, mutexName);
    if (!hMutex) {
        std::fprintf(stderr, "CreateMutexA failed: %lu\n", GetLastError());
        return 1;
    }

    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(DBWIN_BUFFER), mapName);
    if (!hMap) {
        std::fprintf(stderr, "CreateFileMappingA failed: %lu\n", GetLastError());
        CloseHandle(hMutex);
        return 1;
    }

    DBWIN_BUFFER* buf = (DBWIN_BUFFER*)MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(DBWIN_BUFFER));
    if (!buf) {
        std::fprintf(stderr, "MapViewOfFile failed: %lu\n", GetLastError());
        CloseHandle(hMap);
        CloseHandle(hMutex);
        return 1;
    }

    HANDLE hEventData = CreateEventA(NULL, FALSE, FALSE, eventDataName);
    if (!hEventData) {
        std::fprintf(stderr, "CreateEventA (data) failed: %lu\n", GetLastError());
        UnmapViewOfFile(buf);
        CloseHandle(hMap);
        CloseHandle(hMutex);
        return 1;
    }

    HANDLE hEventBufferReady = CreateEventA(NULL, FALSE, TRUE, eventBufferReadyName);
    if (!hEventBufferReady) {
        std::fprintf(stderr, "CreateEventA (buffer ready) failed: %lu\n", GetLastError());
        CloseHandle(hEventData);
        UnmapViewOfFile(buf);
        CloseHandle(hMap);
        CloseHandle(hMutex);
        return 1;
    }

    std::cout << "Listening for OutputDebugString messages (filter: " << targetExe << ")...\n";

    while (true) {
        DWORD wait = WaitForSingleObject(hEventData, INFINITE);
        if (wait != WAIT_OBJECT_0) {
            std::fprintf(stderr, "WaitForSingleObject failed/aborted: %lu\n", GetLastError());
            break;
        }

        buf->data[sizeof(buf->data) - 1] = '\0';
        DWORD pid = buf->dwProcessId;
        std::string message = buf->data;

        std::string procPath = pid_to_imagepath(pid);
        std::string procName = filename_from_path(procPath);
        std::string procNameLower = procName;
        std::transform(procNameLower.begin(), procNameLower.end(), procNameLower.begin(), ::tolower);

        if (procNameLower == targetExe) {
            std::cout << message;
            std::cout.flush();
        }

        SetEvent(hEventBufferReady);
    }

    CloseHandle(hEventBufferReady);
    CloseHandle(hEventData);
    UnmapViewOfFile(buf);
    CloseHandle(hMap);
    CloseHandle(hMutex);
    return 0;
}
#endif
