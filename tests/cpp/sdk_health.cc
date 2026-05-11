/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef __linux__
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;
using steady = std::chrono::steady_clock;
using tp = steady::time_point;

static int g_req_id = 0;

static std::string next_id()
{
    return std::to_string(++g_req_id);
}

static int ms_left(tp deadline)
{
    auto rem = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - steady::now()).count();
    return rem > 0 ? static_cast<int>(rem) : 0;
}

static bool recv_exact(int fd, void* buf, size_t n, tp deadline)
{
    auto* ptr = static_cast<uint8_t*>(buf);
    size_t got = 0;
    while (got < n) {
        int ms = ms_left(deadline);
        if (ms == 0) return false;
        struct pollfd pfd = { fd, POLLIN, 0 };
        if (poll(&pfd, 1, ms) <= 0) return false;
        ssize_t r = read(fd, ptr + got, n - got);
        if (r <= 0) return false;
        got += static_cast<size_t>(r);
    }
    return true;
}

static std::optional<json> recv_frame(int fd, tp deadline)
{
    uint8_t len_buf[4];
    if (!recv_exact(fd, len_buf, 4, deadline)) return std::nullopt;
    uint32_t len = len_buf[0] | (uint32_t(len_buf[1]) << 8) | (uint32_t(len_buf[2]) << 16) | (uint32_t(len_buf[3]) << 24);
    std::vector<uint8_t> payload(len);
    if (!recv_exact(fd, payload.data(), len, deadline)) return std::nullopt;
    try {
        return json::from_msgpack(payload);
    } catch (...) {
        return std::nullopt;
    }
}

static bool send_frame(int fd, const json& data)
{
    auto packed = json::to_msgpack(data);
    uint32_t len = static_cast<uint32_t>(packed.size());
    uint8_t len_buf[4] = {
        uint8_t(len & 0xff),
        uint8_t((len >> 8) & 0xff),
        uint8_t((len >> 16) & 0xff),
        uint8_t((len >> 24) & 0xff),
    };
    if (write(fd, len_buf, 4) != 4) return false;
    return write(fd, packed.data(), packed.size()) == static_cast<ssize_t>(packed.size());
}

static std::optional<json> mep_request(int fd, const std::string& method, const json& params, tp deadline)
{
    json req = {
        { "id",     next_id() },
        { "method", method    },
        { "params", params    }
    };
    if (!send_frame(fd, req)) return std::nullopt;
    return recv_frame(fd, deadline);
}

static void kill_existing_steam()
{
    if (system("pkill -x steam > /dev/null 2>&1") == 0) {
        std::cout << "killed existing Steam process, waiting for it to exit ...\n";
        sleep(2);
    }
}

static void stream_prefixed(int fd, const char* prefix)
{
    char buf[4096];
    std::string line;
    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        for (ssize_t i = 0; i < n; ++i) {
            if (buf[i] == '\n') {
                std::cout << prefix << line << "\n";
                line.clear();
            } else {
                line += buf[i];
            }
        }
    }
    if (!line.empty()) std::cout << prefix << line << "\n";
    close(fd);
}

static pid_t launch_steam()
{
    std::cout << "launching steam...\n";

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        return -1;
    }

    const char* steam_user = std::getenv("STEAM_USER");
    const char* steam_pass = std::getenv("STEAM_PASS");
    bool has_creds = steam_user && steam_pass && steam_user[0] && steam_pass[0];

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        if (has_creds) {
            const char* argv[] = { "xvfb-run", "-a", "dbus-run-session", "steam", "-login", steam_user, steam_pass, "-dev", "-silent", "-nofriendsui", "-nochatui", nullptr };
            execvp("xvfb-run", const_cast<char* const*>(argv));
        } else {
            const char* argv[] = { "xvfb-run", "-a", "dbus-run-session", "steam", "-dev", "-silent", "-nofriendsui", "-nochatui", nullptr };
            execvp("xvfb-run", const_cast<char* const*>(argv));
        }
        _exit(1);
    }

    close(pipefd[1]);
    std::thread(stream_prefixed, pipefd[0], "[steam] ").detach();
    return pid;
}

static bool wait_for_socket(const std::string& path, int timeout_secs)
{
    auto deadline = steady::now() + std::chrono::seconds(timeout_secs);
    while (steady::now() < deadline) {
        struct stat st;
        if (stat(path.c_str(), &st) == 0) return true;
        usleep(500'000);
    }
    return false;
}

static int check_millennium_api(int fd, tp deadline)
{
    auto resp = mep_request(fd, "millennium.api_health", nullptr, deadline);
    if (!resp) {
        std::cerr << "FAIL: millennium.api_health timed out or connection dropped\n";
        return 1;
    }
    if (!(*resp)["error"].is_null()) {
        std::cerr << "FAIL: millennium.api_health error: " << (*resp)["error"] << "\n";
        return 1;
    }
    const auto& result = (*resp)["result"];
    const int total = result.value("total", 0);
    const auto& missing = result["missing"];

    if (!missing.empty()) {
        std::cerr << "FAIL: " << missing.size() << "/" << total << " MILLENNIUM_API exports are null/empty:\n";
        for (const auto& key : missing) {
            std::cerr << "  - " << key << "\n";
        }
        return 1;
    }
    std::cout << "OK: all " << total << " SDK component hooks are registered\n";
    return 0;
}

int main(int argc, char* argv[])
{
    std::string mep_socket = "/tmp/millennium-mep.sock";
    std::string dump_file = "/tmp/sdk-health-dump.json";
    double timeout_sec = 60.0;
    bool no_launch = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mep-socket" && i + 1 < argc)
            mep_socket = argv[++i];
        else if (arg == "--dump-file" && i + 1 < argc)
            dump_file = argv[++i];
        else if (arg == "--timeout" && i + 1 < argc)
            timeout_sec = std::stod(argv[++i]);
        else if (arg == "--no-launch")
            no_launch = true;
    }

    pid_t steam_pid = -1;
    int sock_fd = -1;

    auto cleanup = [&]()
    {
        if (sock_fd >= 0) {
            close(sock_fd);
            sock_fd = -1;
        }
        if (steam_pid > 0) {
            kill(steam_pid, SIGTERM);
            for (int i = 0; i < 50; ++i) {
                int status;
                if (waitpid(steam_pid, &status, WNOHANG) != 0) {
                    steam_pid = -1;
                    break;
                }
                usleep(100'000);
            }
            if (steam_pid > 0) {
                kill(steam_pid, SIGKILL);
                waitpid(steam_pid, nullptr, 0);
                steam_pid = -1;
            }
        }
        kill_existing_steam();
    };

    signal(SIGTERM, [](int)
    {
        exit(1);
    });

    if (!no_launch) {
        kill_existing_steam();
        unlink(mep_socket.c_str());
        steam_pid = launch_steam();
    }

    std::cout << "waiting to connect to MEP...";
    if (!wait_for_socket(mep_socket, 30)) {
        std::cerr << "FAIL: MEP socket never appeared\n";
        cleanup();
        return 1;
    }
    std::cout << " OK\n";
    std::cout << "connecting to MEP...";
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    {
        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, mep_socket.c_str(), sizeof(addr.sun_path) - 1);

        auto connect_deadline = steady::now() + std::chrono::seconds(10);
        while (true) {
            if (connect(sock_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) break;
            if (steady::now() >= connect_deadline) {
                std::cerr << "FAIL: MEP socket exists but nothing is listening\n";
                cleanup();
                return 1;
            }
            usleep(250'000);
        }
    }
    std::cout << " OK\n";
    auto race_deadline = steady::now() + std::chrono::milliseconds(static_cast<int64_t>(timeout_sec * 1000.0));

    auto exc_resp = mep_request(sock_fd, "runtime.exceptions", nullptr, race_deadline);
    if (!exc_resp || !(*exc_resp)["error"].is_null()) {
        std::cerr << "FAIL: runtime.exceptions subscribe error\n";
        cleanup();
        return 1;
    }
    const std::string exc_sub_id = (*exc_resp)["result"]["subscription_id"].get<std::string>();

    auto ready_resp = mep_request(sock_fd, "sdk.ready", nullptr, race_deadline);
    if (!ready_resp || !(*ready_resp)["error"].is_null()) {
        std::cerr << "FAIL: sdk.ready subscribe error\n";
        cleanup();
        return 1;
    }

    auto on_sdk_ready = [&](const json& data) -> int
    {
        const std::string sdk_ver = data.value("sdk_version", std::string{ "unknown" });
        const std::string mil_ver = data.value("millennium_version", std::string{ "unknown" });
        std::cout << "sdk version:        " << sdk_ver << "\n";
        std::cout << "millennium version: " << mil_ver << "\n";
        int rc = check_millennium_api(sock_fd, race_deadline);
        cleanup();
        return rc;
    };

    if (!(*ready_resp)["result"]["ready"].is_null()) return on_sdk_ready((*ready_resp)["result"]["ready"]);

    const std::string ready_sub_id = (*ready_resp)["result"]["subscription_id"].get<std::string>();
    std::cout << "subscribed exceptions=" << exc_sub_id << "#ready=" << ready_sub_id << ". waiting ...\n";

    while (ms_left(race_deadline) > 0) {
        auto event = recv_frame(sock_fd, race_deadline);
        if (!event) break;

        if ((*event).value("type", std::string{}) != "event") continue;

        const std::string sub_id = (*event).value("subscription_id", std::string{});

        if (sub_id == ready_sub_id) return on_sdk_ready((*event)["data"]);

        if (sub_id == exc_sub_id) {
            const auto& details = (*event)["data"]["exceptionDetails"];
            const std::string text = details.value("text", "unknown error");
            const std::string url = details.value("url", std::string{});
            const auto line = details.value("lineNumber", 0);
            const auto col = details.value("columnNumber", 0);
            const std::string desc = details["exception"].value("description", std::string{});
            const std::string plugin = (*event).value("plugin", std::string{ "unknown" });

            std::cerr << "FAIL: uncaught JS exception in '" << plugin << "' at " << url << ":" << line << ":" << col << "\n";
            if (!desc.empty()) std::cerr << "  " << desc << "\n";

            std::ofstream dump(dump_file);
            if (dump) {
                dump << (*event)["data"].dump(4) << "\n";
                std::cerr << "  full details written to " << dump_file << "\n";
            }

            cleanup();
            return 1;
        }
    }

    std::cerr << "FAIL: timed out after " << timeout_sec << "s waiting for sdk.ready\n";
    cleanup();
    return 1;
}
#elif _WIN32
#include <print>
int main(int argc, char* argv[])
{
    std::println("Windows not implemented!");
    return 0;
}
#endif
