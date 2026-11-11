#include "millennium/millennium.h"
#include "millennium/millennium_lifecycle.h"
#include "millennium/platform_hooks.h"
#include "millennium/steam_hooks.h"
#include "mep/crash_event_bus.h"
#include "mep/sdk_ready_bus.h"
#include "mep/ffi_recorder.h"
#include "mep/patch_stream_recorder.h"
#include "mep/patch_update_notifier.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <semaphore>

extern "C" int StartMillennium();
extern "C" int StopMillennium();

std::unique_ptr<millennium> g_millennium;

namespace
{
std::binary_semaphore allow_worker{ 0 };
std::binary_semaphore waiter_ready{ 0 };
std::atomic<bool> worker_stopped{ false };
std::atomic<int> destroyed_listeners{ 0 };

struct listener_lifetime
{
    ~listener_lifetime()
    {
        ++destroyed_listeners;
    }
};

auto make_listener()
{
    return [lifetime = std::make_shared<listener_lifetime>()](const auto&)
    {
        (void)lifetime;
    };
}

void stop_at_exit()
{
    if (StopMillennium() != 0 || !worker_stopped.load() || g_millennium) {
        std::abort();
    }
    std::fputs("Shutdown callback completed.\n", stderr);
}
} // namespace

namespace platform
{
bool should_setup_environment()
{
    return false;
}

void before_attach_millennium()
{
    // Deterministically let the bootstrap register atexit before the worker starts.
    allow_worker.acquire();
}

bool initialize_steam_hooks()
{
    return true;
}

void after_detach_millennium()
{
}
} // namespace platform

void millennium::entry()
{
    // These services are first used by the worker, after bootstrap cleanup is registered.
    const int crash_id = mep::crash_event_bus::instance().add_listener(make_listener());
    const int sdk_id = mep::sdk_ready_bus::instance().add_listener(make_listener());
    const int ffi_id = mep::ffi_recorder::instance().add_listener("test", make_listener());
    const int patch_id = mep::patch_stream_recorder::instance().add_listener("test", make_listener());
    const int notifier_id = patch_update_notifier::instance().add_listener([lifetime = std::make_shared<listener_lifetime>()]()
    {
        (void)lifetime;
    });

    auto& terminate = millennium_lifecycle::get().terminate;
    std::unique_lock<std::mutex> lock(terminate.mtx);
    waiter_ready.release();
    terminate.cv.wait(lock, [&terminate]
    {
        return terminate.flag.load();
    });

    // plugin_loader and the MEP server remove listeners while StopMillennium joins them.
    // Detect premature destruction before touching a freed listener map.
    if (destroyed_listeners.load() != 0) {
        std::fputs("Service listeners were destroyed before worker shutdown.\n", stderr);
        std::abort();
    }
    mep::crash_event_bus::instance().remove_listener(crash_id);
    mep::sdk_ready_bus::instance().remove_listener(sdk_id);
    mep::ffi_recorder::instance().remove_listener(ffi_id);
    mep::patch_stream_recorder::instance().remove_listener(patch_id);
    patch_update_notifier::instance().remove_listener(notifier_id);
    if (destroyed_listeners.load() != 5) {
        std::abort();
    }
    worker_stopped.store(true);
}

int main(int argc, char** argv)
{
    if (argc != 2 || StartMillennium() != 0) {
        std::abort();
    }

    // Match the Linux bootstrap: register cleanup only after StartMillennium returns.
    if (std::atexit(stop_at_exit) != 0) {
        std::abort();
    }
    allow_worker.release();
    waiter_ready.acquire();

    {
        // Taking this mutex ensures the worker has entered cv.wait, not just signalled readiness.
        auto& terminate = millennium_lifecycle::get().terminate;
        std::lock_guard<std::mutex> lock(terminate.mtx);
        if (terminate.flag.load()) {
            std::abort();
        }
    }
    std::fputs("Worker is waiting; starting process shutdown.\n", stderr);

    if (std::strcmp(argv[1], "explicit-stop") == 0) {
        if (StopMillennium() != 0 || !worker_stopped.load()) {
            std::abort();
        }
    } else if (std::strcmp(argv[1], "exit") == 0) {
        // Steam can exit directly when restarting instead of returning from main.
        std::exit(EXIT_SUCCESS);
    } else if (std::strcmp(argv[1], "return") != 0) {
        std::abort();
    }

    return EXIT_SUCCESS;
}
