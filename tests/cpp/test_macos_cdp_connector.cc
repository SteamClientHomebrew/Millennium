#include <catch2/catch_test_macros.hpp>
#include "millennium/cdp_connector.h"
#include "millennium/millennium_lifecycle.h"
#include <atomic>
#include <cstring>
#include <future>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

extern "C" int MillenniumAcceptPipeBroker(int);

namespace
{
struct connector_session
{
    int broker[2]{ -1, -1 };
    std::thread worker;
    std::atomic<int> connections{ 0 };
    std::promise<void> entered;
    std::promise<void> finished;
    std::future<void> started = entered.get_future();
    std::future<void> stopped = finished.get_future();
    std::promise<std::shared_ptr<cdp_client>> first_connection;
    std::future<std::shared_ptr<cdp_client>> client = first_connection.get_future();

    connector_session()
    {
        REQUIRE(socketpair(AF_UNIX, SOCK_DGRAM, 0, broker) == 0);
        REQUIRE(MillenniumAcceptPipeBroker(broker[0]) == 0);
    }

    void start()
    {
        auto props = std::make_shared<socket_utils::socket_t>();
        props->name = "test";
        props->on_connect = [this](auto client)
        {
            if (++connections == 1) first_connection.set_value(client);
            (void)client->send_host("test.connected");
        };
        worker = std::thread([this, props]
        {
            entered.set_value();
            socket_utils().connect_socket(props);
            finished.set_value();
        });
    }

    ~connector_session()
    {
        millennium_lifecycle::get().terminate.notify();
        if (worker.joinable()) worker.join();
        for (int fd : broker)
            if (fd >= 0) close(fd);
    }
};

struct helper_pipe
{
    int input[2]{ -1, -1 };
    int output[2]{ -1, -1 };

    helper_pipe()
    {
        REQUIRE(pipe(input) == 0);
        REQUIRE(pipe(output) == 0);
    }

    ~helper_pipe()
    {
        for (int fd : input)
            if (fd >= 0) close(fd);
        for (int fd : output)
            if (fd >= 0) close(fd);
    }

    void handoff(int broker)
    {
        char byte = 1;
        iovec iov{ &byte, 1 };
        alignas(cmsghdr) char control[CMSG_SPACE(2 * sizeof(int))]{};
        msghdr msg{};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);
        auto* rights = CMSG_FIRSTHDR(&msg);
        rights->cmsg_level = SOL_SOCKET;
        rights->cmsg_type = SCM_RIGHTS;
        rights->cmsg_len = CMSG_LEN(2 * sizeof(int));
        const int endpoints[]{ output[0], input[1] };
        memcpy(CMSG_DATA(rights), endpoints, sizeof(endpoints));
        REQUIRE(sendmsg(broker, &msg, 0) == 1);
        close(output[0]);
        close(input[1]);
        output[0] = input[1] = -1;
    }

    json read_message()
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        std::string message;
        while (std::chrono::steady_clock::now() < deadline) {
            pollfd ready{ input[0], POLLIN, 0 };
            const int result = poll(&ready, 1, 100);
            if (result < 0 && errno == EINTR) continue;
            REQUIRE(result >= 0);
            if (!result) continue;
            char byte;
            const auto count = read(input[0], &byte, 1);
            if (count < 0 && errno == EINTR) continue;
            REQUIRE(count == 1);
            if (byte == '\0') return json::parse(message);
            message += byte;
        }
        FAIL("Timed out waiting for a CDP frame");
        return {};
    }

    void respond(const json& request)
    {
        const std::string response = json{
            { "id",     request.at("id")      },
            { "result", { { "alive", true } } }
        }.dump() + '\0';
        REQUIRE(write(output[1], response.data(), response.size()) == static_cast<ssize_t>(response.size()));
    }

    void exit()
    {
        close(output[1]);
        close(input[0]);
        output[1] = input[0] = -1;
    }
};
} // namespace

TEST_CASE("macOS connector exits without a Helper", "[macos][cdp]")
{
    connector_session session;
    session.start();
    REQUIRE(session.started.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    REQUIRE(session.stopped.wait_for(std::chrono::milliseconds(150)) == std::future_status::timeout);
    REQUIRE(session.connections.load() == 0);
    millennium_lifecycle::get().terminate.notify();
    REQUIRE(session.stopped.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    session.worker.join();
}

TEST_CASE("macOS failed handoff preserves the current Helper connection", "[macos][cdp]")
{
    connector_session session;
    helper_pipe original;
    original.handoff(session.broker[1]);
    session.start();
    REQUIRE(original.read_message().at("method") == "test.connected");
    REQUIRE(session.client.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    auto client = session.client.get();

    helper_pipe failed;
    failed.handoff(session.broker[1]);
    failed.exit();
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    REQUIRE(session.connections.load() == 1);
    auto response = client->send_host("test.still_connected");
    const auto request = original.read_message();
    REQUIRE(request.at("method") == "test.still_connected");
    original.respond(request);
    REQUIRE(response.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    REQUIRE(response.get().at("alive") == true);

    helper_pipe replacement;
    replacement.handoff(session.broker[1]);
    original.exit();
    REQUIRE(replacement.read_message().at("method") == "test.connected");
}
