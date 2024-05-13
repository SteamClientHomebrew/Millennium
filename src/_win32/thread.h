#include <vector>
#include <processthreadsapi.h>
#include <tlhelp32.h>
#include <handleapi.h>
#include <iostream>
#include <utilities/log.hpp>

std::vector<DWORD> get_threads_sync() {

    std::vector<DWORD> out;

    DWORD processId = GetCurrentProcessId();
    DWORD currentThreadId = GetCurrentThreadId(); // Get ID of the current thread
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if (hThreadSnap == INVALID_HANDLE_VALUE) {
        std::cout << "Failed to create thread snapshot." << std::endl;
        return out;
    }

    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);

    if (!Thread32First(hThreadSnap, &te32)) {
        std::cout << "Failed to retrieve the first thread." << std::endl;
        CloseHandle(hThreadSnap);
        return out;
    }

    do {
        // Skip the current thread
        if (te32.th32OwnerProcessID == processId && te32.th32ThreadID != currentThreadId) {
            out.push_back(te32.th32ThreadID);
        }
    } while (Thread32Next(hThreadSnap, &te32));

    CloseHandle(hThreadSnap);

    return out;
}

bool hook_steam_thread()
{
    std::vector<DWORD> threads = get_threads_sync();

    int froze = 0, failed = 0;

    for (const auto& thread : threads) {
        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, thread);
        if (hThread == NULL) {
            std::cout << "Failed to open thread." << std::endl;
            failed++;
            continue;
        }

        // Suspend the thread
        if (SuspendThread(hThread) == -1) {
            CloseHandle(hThread);
            failed++;
        }
        else {
            froze++;
            break; // temp fix
        }
    }
    console.log("[thread-hook] suspended main threads [{}/{}]", froze, failed);
    return true;
}

bool unhook_steam_thread()
{
    std::vector<DWORD> threads = get_threads_sync();

    for (const auto& thread : threads) {
        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, thread);
        if (hThread == NULL) {
            std::cout << "Failed to open thread." << std::endl;
            continue;
        }

        // Suspend the thread
        if (ResumeThread(hThread) == -1) {
            std::cout << "Failed to resume thread." << std::endl;
            CloseHandle(hThread);
        }
    }
    return true;
}