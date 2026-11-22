#pragma once

#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace macos_cdp
{
struct broker
{
    broker() = default;
    broker(const broker&) = delete;
    broker& operator=(const broker&) = delete;
    int fd = -1;
    ~broker()
    {
        if (fd >= 0) close(fd);
    }
};

// A sender can outlive connect_socket. Descriptors are closed only when the last
// owner releases the connection, never concurrently with a read or write.
class connection
{
  public:
    const int read_fd;
    const int write_fd;
    // Owned here until handed to the patch worker.
    int loopback_read_fd = -1;
    int loopback_write_fd = -1;
    std::atomic<bool> stopped{ false };

    connection(int read, int write) : read_fd(read), write_fd(write)
    {
    }
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;
    ~connection()
    {
        close(read_fd);
        close(write_fd);
        if (loopback_read_fd >= 0) close(loopback_read_fd);
        if (loopback_write_fd >= 0) close(loopback_write_fd);
    }

    template <typename Cancelled> bool send(const std::string& payload, Cancelled cancelled)
    {
        std::lock_guard<std::mutex> lock(send_mutex);
        const std::string message = payload + '\0';
        size_t offset = 0;
        while (offset < message.size() && !stopped.load() && !cancelled()) {
            const auto count = write(write_fd, message.data() + offset, message.size() - offset);
            if (count > 0) {
                offset += static_cast<size_t>(count);
            } else if (count < 0 && errno == EINTR) {
                continue;
            } else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                pollfd wait{ write_fd, POLLOUT, 0 };
                const int result = poll(&wait, 1, 100);
                if (result < 0 && errno == EINTR) continue;
                if (result < 0 || (wait.revents & (POLLERR | POLLHUP | POLLNVAL))) break;
            } else {
                break;
            }
        }
        if (offset == message.size()) return true;
        // A partial frame cannot be retried on this stream.
        stopped.store(true);
        return false;
    }

  private:
    std::mutex send_mutex;
};

inline std::shared_ptr<connection> receive(int fd)
{
    char byte = 0;
    iovec iov{ &byte, 1 };
    alignas(cmsghdr) char control[CMSG_SPACE(4 * sizeof(int))] = {};
    msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    const auto count = recvmsg(fd, &msg, MSG_DONTWAIT);
    if (count < 0) return {};
    // Two CDP endpoints, optionally followed by two loopback IPC endpoints.
    int endpoints[4] = { -1, -1, -1, -1 };
    size_t received = 0;
    auto* rights = CMSG_FIRSTHDR(&msg);
    bool expected_rights = false;
    if (rights && rights->cmsg_level == SOL_SOCKET && rights->cmsg_type == SCM_RIGHTS && rights->cmsg_len >= CMSG_LEN(0)) {
        const size_t bytes = rights->cmsg_len - CMSG_LEN(0);
        received = bytes / sizeof(int);
        expected_rights = bytes == 2 * sizeof(int) || bytes == 4 * sizeof(int);
        // Close even a partial rights message on rejection.
        memcpy(endpoints, CMSG_DATA(rights), bytes < sizeof(endpoints) ? bytes : sizeof(endpoints));
    }
    bool valid = expected_rights && count == 1 && byte == 1 && !(msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC));
    for (size_t i = 0; valid && i < received; ++i) {
        valid = fcntl(endpoints[i], F_SETFD, FD_CLOEXEC) == 0 && fcntl(endpoints[i], F_SETFL, O_NONBLOCK) == 0;
    }
    if (valid) valid = fcntl(endpoints[1], F_SETNOSIGPIPE, 1) == 0;
    if (valid && received == 4) valid = fcntl(endpoints[3], F_SETNOSIGPIPE, 1) == 0;
    if (!valid) {
        for (int endpoint : endpoints)
            if (endpoint >= 0) close(endpoint);
        return {};
    }
    auto conn = std::make_shared<connection>(endpoints[0], endpoints[1]);
    if (received == 4) {
        conn->loopback_read_fd = endpoints[2];
        conn->loopback_write_fd = endpoints[3];
    }
    return conn;
}
} // namespace macos_cdp
