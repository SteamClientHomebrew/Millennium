#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace platform
{
class file_watcher
{
  public:
    using callback_t = std::function<void()>;
    file_watcher(std::filesystem::path file_path, callback_t on_change, std::chrono::milliseconds debounce = std::chrono::milliseconds(100));
    ~file_watcher();

    file_watcher(const file_watcher&) = delete;
    file_watcher& operator=(const file_watcher&) = delete;

    void start();
    void stop();
    bool is_running() const;

  private:
    void watch_loop();

    std::filesystem::path m_file_path;
    std::string m_directory;
    callback_t m_on_change;
    std::chrono::milliseconds m_debounce;

    std::thread m_thread;
    std::atomic<bool> m_running{ false };

#ifdef _WIN32
    HANDLE m_dir_handle{ INVALID_HANDLE_VALUE };
#endif
};

} // namespace platform
