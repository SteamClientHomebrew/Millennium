#include "millennium/logger.h"
#include "millennium/environment.h"
#include <barrier>
#include <filesystem>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <sys/resource.h>

// Keep the real logger implementation; redirect its files into the test build directory.
std::string platform::environment::get(std::string)
{
    return std::filesystem::current_path().string();
}

static void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static void buffer_snapshots()
{
    plugin_logger logger("buffer-test");
    const std::string message(200, 'x');
    std::barrier start(3);
    std::vector<std::thread> writers;
    for (int writer = 0; writer < 2; ++writer) {
        writers.emplace_back([&]
        {
            start.arrive_and_wait();
            for (int i = 0; i < 300; ++i) {
                if (i % 3 == 0) logger.log(message, true, 42, "test.lua", i);
                else if (i % 3 == 1) logger.warn(message, true, 42, "test.lua", i);
                else logger.error(message, true, 42, "test.lua", i);
            }
        });
    }
    std::atomic<bool> valid{true};
    std::thread reader([&]
    {
        start.arrive_and_wait();
        for (int i = 0; i < 300; ++i) {
            for (const auto& entry : logger.collect_logs()) {
                if (entry.message != message || entry.file != "test.lua" || entry.timestamp_us != 42) valid = false;
            }
        }
    });
    for (auto& writer : writers) writer.join();
    reader.join();
    require(valid, "Corrupted log snapshot");
    require(logger.collect_logs().size() == 600, "Concurrent writes lost log entries");
}

static void registry_snapshots()
{
    std::barrier start(9);
    std::vector<std::thread> writers;
    for (int writer = 0; writer < 8; ++writer) {
        writers.emplace_back([&]
        {
            start.arrive_and_wait();
            for (int i = 0; i < 16; ++i) {
                InfoToLogger("registry-" + std::to_string(i % 4), std::string(200, 'r'), true);
            }
        });
    }
    std::atomic<bool> done{false};
    std::atomic<bool> valid{true};
    std::thread reader([&]
    {
        start.arrive_and_wait();
        while (!done) {
            for (auto* logger : get_plugin_logger_mgr()) {
                if (logger->get_plugin_name(false).find("registry-") != 0) valid = false;
                (void)logger->collect_logs();
            }
        }
    });
    for (auto& writer : writers) writer.join();
    done = true;
    reader.join();
    require(valid, "Invalid logger in registry snapshot");
    const auto loggers = get_plugin_logger_mgr();
    require(loggers.size() == 4, "Concurrent creation produced duplicate or lost loggers");
    for (auto* logger : loggers) require(logger->collect_logs().size() == 32, "Registry logger lost messages");
}

static void reentrant_listeners()
{
    auto& logger = get_or_create_plugin_logger("listener-test");
    int calls = 0;
    int token = -1;
    token = logger.add_listener([&](const logger_base::log_entry& entry)
    {
        ++calls;
        require(entry.message == "outer", "Unexpected callback message");
        require(logger.collect_logs().size() == 1, "Callback cannot collect its own log");
        require(&get_or_create_plugin_logger("listener-test") == &logger, "Callback cannot access registry");
        logger.remove_listener(token);
        logger.log("nested", true);
    });
    logger.log("outer", true);
    logger.warn("warning", true);
    logger.error("error", true);
    logger.print("raw");
    require(calls == 1, "Removed listener was invoked again");
    require(logger.collect_logs().size() == 5, "Reentrant logging lost entries");
}

int main(int argc, char** argv)
{
    const rlimit no_core{0, 0};
    setrlimit(RLIMIT_CORE, &no_core);
    std::promise<void> done;
    auto completion = done.get_future();
    std::thread watchdog([&]
    {
        if (completion.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
            std::cerr << "Logger concurrency test deadlocked\n";
            std::_Exit(EXIT_FAILURE);
        }
    });
    int status = EXIT_SUCCESS;
    try {
        require(argc == 2, "Expected a test mode");
        const std::string mode(argv[1]);
        if (mode == "buffer") buffer_snapshots();
        else if (mode == "registry") registry_snapshots();
        else if (mode == "listeners") reentrant_listeners();
        else throw std::runtime_error("Unknown test mode");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = EXIT_FAILURE;
    }
    done.set_value();
    watchdog.join();
    return status;
}
