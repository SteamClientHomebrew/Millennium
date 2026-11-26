#include <catch2/catch_test_macros.hpp>
#include "instrumentation/patch_registry.h"
#include "mep/patch_update_notifier.h"
#include "millennium/cdp_connector.h"
#include "millennium/millennium_lifecycle.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <future>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

namespace
{
struct connector_session
{
    std::string dir;
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
        char tmpl[] = "/tmp/millennium-test-XXXXXX";
        REQUIRE(mkdtemp(tmpl));
        dir = tmpl;
        setenv("MILLENNIUM_CDP_SOCKET", cdp_path().c_str(), 1);
        setenv("MILLENNIUM_LOOPBACK_SOCKET", loopback_path().c_str(), 1);
    }

    std::string cdp_path() const { return dir + "/cdp.sock"; }
    std::string loopback_path() const { return dir + "/loopback.sock"; }

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
        unlink(cdp_path().c_str());
        unlink(loopback_path().c_str());
        rmdir(dir.c_str());
    }
};

// One end of a socket the runtime listens on, as hhx64 in the browser holds it.
struct helper_socket
{
    int fd = -1;

    explicit helper_socket(const std::string& path)
    {
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, path.c_str());
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline) {
            fd = socket(AF_UNIX, SOCK_STREAM, 0);
            REQUIRE(fd >= 0);
            if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
                REQUIRE(fcntl(fd, F_SETNOSIGPIPE, 1) == 0);
                return;
            }
            close(fd);
            fd = -1;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        FAIL("Timed out connecting to " << path);
    }

    ~helper_socket() { exit(); }

    json read_message()
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        std::string message;
        while (std::chrono::steady_clock::now() < deadline) {
            pollfd ready{ fd, POLLIN, 0 };
            const int result = poll(&ready, 1, 100);
            if (result < 0 && errno == EINTR) continue;
            REQUIRE(result >= 0);
            if (!result) continue;
            char byte;
            const auto count = read(fd, &byte, 1);
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
        REQUIRE(write(fd, response.data(), response.size()) == static_cast<ssize_t>(response.size()));
    }

    void exit()
    {
        if (fd >= 0) close(fd);
        fd = -1;
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

TEST_CASE("macOS CDP can connect and stop with a full loopback socket", "[macos][cdp]")
{
    connector_session session;
    patch_registry_set("test", { { "test", "sp.js", "original", { { "original", std::string(128 * 1024, 'x') } } } });
    session.start();
    helper_socket browser(session.cdp_path());
    helper_socket patches(session.loopback_path());

    pollfd pending{ patches.fd, POLLIN, 0 };
    REQUIRE(poll(&pending, 1, 2000) == 1);
    REQUIRE(browser.read_message().at("method") == "test.connected");
    auto update = std::async(std::launch::async, []
    {
        patch_registry_remove("test");
        patch_update_notifier::instance().notify();
    });
    const auto notified = update.wait_for(std::chrono::seconds(1));
    millennium_lifecycle::get().terminate.notify();
    REQUIRE(session.stopped.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    REQUIRE(notified == std::future_status::ready);
    update.get();

    // The runtime closed its end; writes from the browser side now fail.
    const auto written = write(patches.fd, "x", 1);
    const int error = errno;
    REQUIRE(written == -1);
    REQUIRE(error == EPIPE);
}

TEST_CASE("macOS CDP accepts a new Helper after the previous one exits", "[macos][cdp]")
{
    connector_session session;
    session.start();
    helper_socket original(session.cdp_path());
    REQUIRE(original.read_message().at("method") == "test.connected");
    REQUIRE(session.client.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    auto client = session.client.get();

    auto response = client->send_host("test.still_connected");
    const auto request = original.read_message();
    REQUIRE(request.at("method") == "test.still_connected");
    original.respond(request);
    REQUIRE(response.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    REQUIRE(response.get().at("alive") == true);

    original.exit();
    helper_socket replacement(session.cdp_path());
    REQUIRE(replacement.read_message().at("method") == "test.connected");
    REQUIRE(session.connections.load() == 2);
}
