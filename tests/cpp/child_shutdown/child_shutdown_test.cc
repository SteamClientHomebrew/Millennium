#include "millennium/child_process.h"
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <spawn.h>
#include <stdexcept>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
using namespace std::chrono_literals;

static void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static pid_t wait_child(pid_t pid, int* status, int options)
{
    pid_t result;
    do {
        result = waitpid(pid, status, options);
    } while (result < 0 && errno == EINTR);
    return result;
}

// This guard can signal only an unreaped child created by this test.
struct child_guard
{
    pid_t pid{-1};
    int fd{-1};
    ~child_guard()
    {
        if (fd >= 0) close(fd);
        int status;
        if (pid > 0 && wait_child(pid, &status, WNOHANG) == 0) {
            kill(pid, SIGKILL);
            wait_child(pid, &status, 0);
        }
    }
};

static int run_child(const std::string& mode)
{
    // Bound fixture lifetime even if a timed-out test parent cannot run its guard.
    signal(SIGALRM, SIG_DFL);
    alarm(10);
    if (mode == "stubborn") signal(SIGTERM, SIG_IGN);
    else signal(SIGTERM, SIG_DFL);

    nlohmann::json request;
    if (!plugin_ipc::read_msg(3, request) || request.value("method", "") != plugin_ipc::parent_method::SHUTDOWN) return 2;
    const nlohmann::json response = {
        {"type", plugin_ipc::TYPE_RESPONSE}, {"id", request.at("id")}, {"result", {{"ok", true}}}
    };
    if (!plugin_ipc::write_msg(3, response)) return 3;
    close(3);
    if (mode == "zombie" || mode == "reaped") return 0;
    // Remain alive after the RPC reply to exercise the parent's termination policy.
    for (;;) pause();
}

static void run_parent(const std::string& executable, const std::string& mode)
{
    int sockets[2];
    require(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0, "socketpair failed");
    child_guard child;
    child.fd = sockets[0];
    posix_spawn_file_actions_t actions;
    require(posix_spawn_file_actions_init(&actions) == 0, "spawn actions failed");
    require(posix_spawn_file_actions_addclose(&actions, sockets[0]) == 0, "spawn close action failed");
    require(posix_spawn_file_actions_adddup2(&actions, sockets[1], 3) == 0, "spawn dup action failed");
    if (sockets[1] != 3) require(posix_spawn_file_actions_addclose(&actions, sockets[1]) == 0, "spawn close action failed");
    char* args[] = {const_cast<char*>(executable.c_str()), const_cast<char*>("--child"), const_cast<char*>(mode.c_str()), nullptr};
    const int spawned = posix_spawn(&child.pid, executable.c_str(), &actions, nullptr, args, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(sockets[1]);
    require(spawned == 0, "posix_spawn failed");

    auto process = std::make_unique<PluginProcess>("shutdown-test", "", child.fd, child.pid);
    child.fd = -1; // PluginProcess owns this endpoint now.
    process->shutdown();

    if (mode == "zombie") {
        siginfo_t info{};
        int result;
        do {
            result = waitid(P_PID, child.pid, &info, WEXITED | WNOWAIT);
        } while (result < 0 && errno == EINTR);
        require(result == 0 && info.si_code == CLD_EXITED && info.si_status == 0, "Could not leave child as a zombie");
    } else if (mode == "reaped") {
        int status;
        require(wait_child(child.pid, &status, 0) == child.pid && WIFEXITED(status) && WEXITSTATUS(status) == 0, "Could not reap fixture child");
    }

    const auto start = std::chrono::steady_clock::now();
    process.reset();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::cout << mode << " destruction: " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << " ms\n";

    int status;
    require(wait_child(child.pid, &status, WNOHANG) == -1 && errno == ECHILD, "Destructor did not reap child");
    if (mode == "stubborn") {
        require(elapsed >= 2800ms && elapsed < 5s, "Termination grace period changed");
    } else {
        require(elapsed < 1s, "Destructor waited for an already terminated child");
    }
}

int main(int argc, char** argv)
{
    const rlimit no_core{0, 0};
    setrlimit(RLIMIT_CORE, &no_core);
    if (argc == 3 && std::string(argv[1]) == "--child") return run_child(argv[2]);
    try {
        require(argc == 2, "Expected a test mode");
        const std::string mode(argv[1]);
        require(mode == "zombie" || mode == "reaped" || mode == "sigterm" || mode == "stubborn", "Unknown mode");
        run_parent(std::filesystem::absolute(argv[0]).string(), mode);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
