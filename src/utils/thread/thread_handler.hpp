#pragma once
#ifdef _WIN32
#include <Windows.h>
#endif
#include <vector>
#include <thread>

class c_threads {
public:
    std::vector<std::thread> runningThreads;

    static c_threads& get() {
        static c_threads instance; // This is now thread-safe in C++11 and later
        return instance;
    }

    void add(std::thread threadPool);

    bool killAllThreads(uint16_t exitCode);

    bool hookAllThreads();
    bool unhookAllThreads();

private:
#ifdef _WIN32
    bool isMillenniumThread(DWORD id);
    std::vector<DWORD> getProcThreads();
#endif

    c_threads();

    c_threads(const c_threads&) = delete;
    c_threads& operator=(const c_threads&) = delete;
};