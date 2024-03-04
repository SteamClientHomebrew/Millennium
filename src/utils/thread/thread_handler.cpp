#include "thread_handler.hpp"
#include <utils/cout/logger.hpp>
#include <format>
#ifdef _WIN32
#include <TlHelp32.h>
#endif
#include <iostream>

void c_threads::add(std::thread newThread) {
    newThread.detach();
    runningThreads.push_back(std::move(newThread));
}

bool c_threads::killAllThreads(uint16_t exitCode) {
    for (auto& thread : runningThreads) {
        if (thread.joinable()) {
            // Terminate the thread
            // Note: Terminating threads abruptly is generally not recommended
            thread.detach(); // Detach the thread from the container
        }
    }
    // Clear the container
    runningThreads.clear();
    return true; // Indicate success
}

bool c_threads::isMillenniumThread(DWORD id) 
{
    //for (const auto& thread : runningThreads) 
    //{
    //    const DWORD procThread = GetThreadId(thread);

    //    if (procThread == id)
    //    {
    //        return true;
    //    }
    //}
    return false;
}

#ifdef _WIN32
std::vector<DWORD> c_threads::getProcThreads() {

    std::vector<DWORD> out;

    DWORD processId = GetCurrentProcessId();
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
        if (te32.th32OwnerProcessID == processId) {
            out.push_back(te32.th32ThreadID);
        }
    } while (Thread32Next(hThreadSnap, &te32));

    CloseHandle(hThreadSnap);

    return out;
}
#endif

bool c_threads::hookAllThreads()
{
#ifdef _WIN32
    std::vector<DWORD> threads = this->getProcThreads();

    for (const auto& thread : threads) {
        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, thread);
        if (hThread == NULL) {
            std::cout << "Failed to open thread." << std::endl;
            continue;
        }

        std::cout << "hooking thread " << thread << std::endl;

        // Suspend the thread
        if (SuspendThread(hThread) == -1) {
            std::cout << "Failed to suspend thread." << std::endl;
            CloseHandle(hThread);
        }
        break;
    }
#endif

    return true;
}

bool c_threads::unhookAllThreads()
{
#ifdef _WIN32
    std::vector<DWORD> threads = this->getProcThreads();

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
#endif

    return true;
}

c_threads::c_threads() { }