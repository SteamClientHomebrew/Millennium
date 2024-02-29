#include "thread_handler.hpp"
#include <utils/cout/logger.hpp>
#include <format>
#include <TlHelp32.h>
#include <iostream>

threadContainer* threadContainer::instance = nullptr;

void threadContainer::addThread(HANDLE threadPool) {

    if (console.consoleAllocated) {
        console.log(std::format("[thread_pool] Thread created 0x{}, total count: {}", threadPool, runningThreads.size()));
    }

    runningThreads.push_back(threadPool);
}

//killing thread doesn't allow proper cleanup (dont give a fuck)
#pragma warning(disable:6258)
bool threadContainer::killAllThreads(uint16_t exitCode) {
    bool success = true;
    for (const auto thread : runningThreads) {
        if (!TerminateThread(thread, exitCode)) {
            success = false;
        }
    }
    return success;
}

bool threadContainer::isMillenniumThread(DWORD id) 
{
    for (const auto& thread : runningThreads) 
    {
        const DWORD procThread = GetThreadId(thread);

        if (procThread == id)
        {
            return true;
        }
    }
    return false;
}

std::vector<DWORD> threadContainer::getProcThreads() {

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

            if (!isMillenniumThread(te32.th32ThreadID)) {
                out.push_back(te32.th32ThreadID);
                break;
            }
            else {
                std::cout << te32.th32ThreadID << " is owned by millennium" << std::endl;
            }
        }
    } while (Thread32Next(hThreadSnap, &te32));

    CloseHandle(hThreadSnap);

    return out;
}

bool threadContainer::hookAllThreads()
{
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
    }

    return true;
}

bool threadContainer::unhookAllThreads()
{
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

    return true;
}

threadContainer::threadContainer() { }