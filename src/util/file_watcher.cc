#include "millennium/file_watcher.h"

#ifndef _WIN32
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>
#endif

namespace platform
{
file_watcher::file_watcher(std::filesystem::path file_path, callback_t on_change, std::chrono::milliseconds debounce)
    : m_file_path(std::move(file_path)), m_directory(m_file_path.parent_path().string()), m_on_change(std::move(on_change)), m_debounce(debounce)
{
}

file_watcher::~file_watcher()
{
    stop();
}

void file_watcher::start()
{
    if (m_running.load()) return;
    m_running.store(true);
    m_thread = std::thread(&file_watcher::watch_loop, this);
}

void file_watcher::stop()
{
    m_running.store(false);
#ifdef _WIN32
    if (m_dir_handle != INVALID_HANDLE_VALUE) {
        CancelIoEx(m_dir_handle, NULL);
    }
#endif
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool file_watcher::is_running() const
{
    return m_running.load();
}

#ifdef _WIN32
void file_watcher::watch_loop()
{
    const std::wstring targetFile = m_file_path.filename().wstring();

    m_dir_handle = CreateFileA(m_directory.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    if (m_dir_handle == INVALID_HANDLE_VALUE) return;

    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    alignas(DWORD) char buffer[4096];

    while (m_running.load()) {
        ResetEvent(overlapped.hEvent);
        DWORD bytesReturned = 0;

        BOOL ok = ReadDirectoryChangesW(m_dir_handle, buffer, sizeof(buffer), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &overlapped, NULL);
        if (!ok) break;

        DWORD status = WaitForSingleObject(overlapped.hEvent, INFINITE);

        if (!m_running.load()) break;
        if (status != WAIT_OBJECT_0) continue;
        if (!GetOverlappedResult(m_dir_handle, &overlapped, &bytesReturned, FALSE) || bytesReturned == 0) continue;

        bool targetChanged = false;
        auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
        while (true) {
            std::wstring fileName(info->FileName, info->FileNameLength / sizeof(WCHAR));
            if (fileName == targetFile) {
                targetChanged = true;
                break;
            }
            if (info->NextEntryOffset == 0) break;
            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<char*>(info) + info->NextEntryOffset);
        }

        if (!targetChanged) continue;

        while (m_running.load()) {
            ResetEvent(overlapped.hEvent);
            bytesReturned = 0;
            if (!ReadDirectoryChangesW(m_dir_handle, buffer, sizeof(buffer), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &overlapped, NULL)) break;

            if (WaitForSingleObject(overlapped.hEvent, static_cast<DWORD>(m_debounce.count())) == WAIT_TIMEOUT) {
                CancelIo(m_dir_handle);
                GetOverlappedResult(m_dir_handle, &overlapped, &bytesReturned, TRUE);
                break;
            }
            GetOverlappedResult(m_dir_handle, &overlapped, &bytesReturned, FALSE);
        }

        if (m_running.load() && m_on_change) {
            m_on_change();
        }
    }

    CloseHandle(overlapped.hEvent);
    CloseHandle(m_dir_handle);
    m_dir_handle = INVALID_HANDLE_VALUE;
}
#else
void file_watcher::watch_loop()
{
    auto lastModTime = std::filesystem::exists(m_file_path) ? std::filesystem::last_write_time(m_file_path) : std::filesystem::file_time_type{};

    int inotifyFd = inotify_init1(IN_NONBLOCK);
    if (inotifyFd < 0) return;

    int watchFd = inotify_add_watch(inotifyFd, m_directory.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO);
    if (watchFd < 0) {
        close(inotifyFd);
        return;
    }

    struct pollfd pfd = { inotifyFd, POLLIN, 0 };
    const int debounceMs = static_cast<int>(m_debounce.count());

    while (m_running.load()) {
        int ret = poll(&pfd, 1, 500);

        if (!m_running.load()) break;
        if (ret <= 0 || !(pfd.revents & POLLIN)) continue;

        char buf[4096];
        while (read(inotifyFd, buf, sizeof(buf)) > 0) {
        }

        while (m_running.load()) {
            int debounceRet = poll(&pfd, 1, debounceMs);
            if (debounceRet <= 0) break;
            while (read(inotifyFd, buf, sizeof(buf)) > 0) {
            }
        }

        if (std::filesystem::exists(m_file_path)) {
            auto currentModTime = std::filesystem::last_write_time(m_file_path);
            if (currentModTime != lastModTime) {
                lastModTime = currentModTime;
                if (m_running.load() && m_on_change) {
                    m_on_change();
                }
            }
        }
    }

    inotify_rm_watch(inotifyFd, watchFd);
    close(inotifyFd);
}
#endif
} // namespace platform
