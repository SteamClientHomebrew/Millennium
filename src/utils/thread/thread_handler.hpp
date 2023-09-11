#pragma once
#include <Windows.h>
#include <vector>

class threadContainer {
public:
    std::vector<HANDLE> runningThreads;

    static threadContainer& getInstance() {
        // Create the instance if it doesn't exist
        if (!instance) {
            instance = new threadContainer();
        }
        return *instance;
    }

    void addThread(HANDLE threadPool);

    bool killAllThreads(uint16_t exitCode);

private:
    threadContainer();

    threadContainer(const threadContainer&) = delete;
    threadContainer& operator=(const threadContainer&) = delete;

    static threadContainer* instance;
};