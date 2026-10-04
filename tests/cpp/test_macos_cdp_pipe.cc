#include <catch2/catch_test_macros.hpp>
#include "bootstrap/macos/pipe_bootstrap.h"
#include "millennium/macos_cdp_pipe.h"
#include <chrono>
#include <future>
#include <sys/wait.h>

namespace
{
struct pipe_pair
{
    int input[2];
    int output[2];
    std::shared_ptr<macos_cdp::connection> conn;
    pipe_pair()
    {
        REQUIRE(pipe(input) == 0);
        REQUIRE(pipe(output) == 0);
        REQUIRE(fcntl(input[1], F_SETFL, O_NONBLOCK) == 0);
        REQUIRE(fcntl(input[1], F_SETNOSIGPIPE, 1) == 0);
        conn = std::make_shared<macos_cdp::connection>(output[0], input[1]);
    }
    ~pipe_pair()
    {
        if (input[0] >= 0) close(input[0]);
        close(output[1]);
    }
};
} // namespace

TEST_CASE("macOS CDP targets only the browser Helper", "[macos][cdp]")
{
    char name[] = "Steam Helper";
    char type[] = "--type=renderer";
    char* browser[] = { name, nullptr };
    char* renderer[] = { name, type, nullptr };
    REQUIRE(millennium_pipe_bootstrap::is_browser("/path/Steam Helper", browser));
    REQUIRE_FALSE(millennium_pipe_bootstrap::is_browser("/path/Steam Helper", renderer));
    REQUIRE_FALSE(millennium_pipe_bootstrap::is_browser("/path/Steam Helper (Renderer)", browser));
    REQUIRE_FALSE(millennium_pipe_bootstrap::is_browser("/bin/sh", browser));
}

TEST_CASE("macOS pipe writes complete null-delimited frames", "[macos][cdp]")
{
    pipe_pair pipes;
    const std::string payload(256 * 1024, 'x');
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    auto reader = std::async(std::launch::async, [&]()
    {
        std::string result;
        char chunk[137];
        while (result.size() < payload.size() + 1) {
            pollfd ready{ pipes.input[0], POLLIN, 0 };
            if (poll(&ready, 1, 3000) <= 0) break;
            const auto count = read(pipes.input[0], chunk, sizeof(chunk));
            if (count <= 0) break;
            result.append(chunk, count);
        }
        return result;
    });
    REQUIRE(pipes.conn->send(payload, [&]
    {
        return std::chrono::steady_clock::now() >= deadline;
    }));
    REQUIRE(reader.get() == payload + '\0');
}

TEST_CASE("macOS pipe backpressure is cancellable", "[macos][cdp]")
{
    pipe_pair pipes;
    std::atomic<bool> cancel{ false };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    auto send = std::async(std::launch::async, [&]()
    {
        return pipes.conn->send(std::string(1024 * 1024, 'x'), [&]
        {
            return cancel.load() || std::chrono::steady_clock::now() >= deadline;
        });
    });
    REQUIRE(send.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout);
    cancel.store(true);
    REQUIRE(send.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    REQUIRE_FALSE(send.get());
    REQUIRE(pipes.conn->stopped.load());
    REQUIRE_FALSE(pipes.conn->send("next", []
    {
        return false;
    }));
}

TEST_CASE("macOS closed pipe fails without SIGPIPE", "[macos][cdp]")
{
    pipe_pair pipes;
    close(pipes.input[0]);
    pipes.input[0] = -1;
    REQUIRE_FALSE(pipes.conn->send("request", []
    {
        return false;
    }));
}

TEST_CASE("macOS Helper launch hands off fresh descriptors and restores failed exec", "[macos][cdp]")
{
    int channel[2];
    REQUIRE(socketpair(AF_UNIX, SOCK_DGRAM, 0, channel) == 0);
    channel[0] = millennium_pipe_bootstrap::private_fd(channel[0]);
    channel[1] = millennium_pipe_bootstrap::private_fd(channel[1]);
    REQUIRE(channel[0] >= 0);
    REQUIRE(channel[1] >= 0);
    millennium_pipe_bootstrap::sender = channel[1];
    millennium_pipe_bootstrap::owner = getpid();
    for (int generation = 0; generation < 2; ++generation) {
        const pid_t child = fork();
        REQUIRE(child >= 0);
        if (child == 0) {
            alarm(5);
            int original = open("/dev/null", O_RDWR);
            if (original < 0 || dup2(original, 3) < 0 || dup2(original, 4) < 0) _exit(1);
            if (original > 4) close(original);
            fcntl(3, F_SETFD, FD_CLOEXEC);
            char name[] = "Steam Helper";
            char* argv[] = { name, nullptr };
            const int result = millennium_pipe_bootstrap::exec("/test/Steam Helper", argv, nullptr, [](const char*, char* const args[], char* const[])
            {
                if (!args[1] || strcmp(args[1], "--remote-debugging-pipe") != 0) _exit(2);
                if (fcntl(3, F_GETFD) != 0 || fcntl(4, F_GETFD) != 0) _exit(3);
                char byte;
                if (read(3, &byte, 1) != 1 || write(4, &byte, 1) != 1) _exit(4);
                errno = ENOENT;
                return -1;
            });
            if (result != -1 || errno != ENOENT || fcntl(3, F_GETFD) != FD_CLOEXEC) _exit(5);
            char byte;
            if (read(3, &byte, 1) != 0 || write(4, "x", 1) != 1) _exit(6);
            _exit(0);
        }
        pollfd ready{ channel[0], POLLIN, 0 };
        REQUIRE(poll(&ready, 1, 3000) == 1);
        auto conn = macos_cdp::receive(channel[0]);
        REQUIRE(conn);
        REQUIRE(conn->send("x", []
        {
            return false;
        }));
        pollfd response{ conn->read_fd, POLLIN, 0 };
        REQUIRE(poll(&response, 1, 3000) == 1);
        char byte;
        REQUIRE(read(conn->read_fd, &byte, 1) == 1);
        REQUIRE(byte == 'x');
        int status;
        REQUIRE(waitpid(child, &status, 0) == child);
        REQUIRE(WIFEXITED(status));
        REQUIRE(WEXITSTATUS(status) == 0);
    }
    close(channel[0]);
    millennium_pipe_bootstrap::shutdown();
}
