#include <vector>
#include <iostream>
#include <utilities/log.hpp>
#include <tlhelp32.h>
#include <Psapi.h>

std::vector<DWORD> DiscoverProcessThreads() 
{
    std::vector<DWORD> out;

    DWORD processId = GetCurrentProcessId();
    DWORD currentThreadId = GetCurrentThreadId();
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if (hThreadSnap == INVALID_HANDLE_VALUE) 
    {
        std::cout << "Failed to create thread snapshot." << std::endl;
        return out;
    }

    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);

    if (!Thread32First(hThreadSnap, &te32)) 
    {
        std::cout << "Failed to retrieve the first thread." << std::endl;
        CloseHandle(hThreadSnap);
        return out;
    }

    do 
    {
        if (te32.th32OwnerProcessID == processId && te32.th32ThreadID != currentThreadId)
        {
            out.push_back(te32.th32ThreadID);
        }
    } 
    while (Thread32Next(hThreadSnap, &te32));

    CloseHandle(hThreadSnap);
    return out;
}

bool PauseSteamCore()
{
    std::vector<DWORD> threads = DiscoverProcessThreads();

    int frozeThreadRefCount = 0;
    int failedFreezeRefCount = 0;

    for (const auto& thread : threads) 
    {
        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, thread);
        if (hThread == NULL) 
        {
            std::cout << "Failed to open thread." << std::endl;
            failedFreezeRefCount++;
            continue;
        }

        // Suspend the thread
        if (SuspendThread(hThread) == -1) 
        {
            CloseHandle(hThread);
            failedFreezeRefCount++;
        }
        else 
        {
            frozeThreadRefCount++;
            break; // temp fix
        }
    }

    Logger.Log("[thread-hook] suspended main threads [{}/{}]", frozeThreadRefCount, failedFreezeRefCount);
    return true;
}

bool UnpauseSteamCore()
{
    std::vector<DWORD> threads = DiscoverProcessThreads();

    for (const auto& thread : threads) 
    {
        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, thread);
        if (hThread == NULL) 
        {
            std::cout << "Failed to open thread." << std::endl;
            continue;
        }

        if (ResumeThread(hThread) == -1) 
        {
            std::cout << "Failed to resume thread." << std::endl;
            CloseHandle(hThread);
        }
    }
    return true;
}

std::string GetProcessName(DWORD processID)
{
    char szProcessName[MAX_PATH] = "<unknown>";
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

    if (hProcess != NULL)
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
        {
            GetModuleBaseNameA(hProcess, hMod, szProcessName, sizeof(szProcessName));
        }
        CloseHandle(hProcess);
    }
    return szProcessName;
}