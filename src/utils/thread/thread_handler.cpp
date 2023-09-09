#include "thread_handler.hpp"
#include <utils/cout/logger.hpp>
#include <format>

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

threadContainer::threadContainer()
{
}