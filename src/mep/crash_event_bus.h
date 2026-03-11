#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mep
{

struct crash_event
{
    std::string plugin_name; /* internal/directory name, used for plugin management */
    unsigned long exit_code;
    std::string crash_dump_dir; /* path to crash_dumps/<plugin>-<ts>/, empty if handler didn't run */
    std::string display_name;   /* common_name from plugin.json, empty if not yet resolved */
};

class crash_event_bus
{
  public:
    using listener_fn = std::function<void(const crash_event&)>;

    static crash_event_bus& instance();

    /**
     * Record a crash and dispatch to listeners.
     * the crash is always persisted in the internal buffer until
     * acknowledge_crash() is called by the frontend.
     */
    void notify(const crash_event& ev);

    /**
     * Register a listener. Any crash events that fired before this call are
     * replayed immediately so no notification is ever missed.
     */
    int add_listener(listener_fn fn);
    void remove_listener(int id);

    /** Return all unacknowledged crashes (does NOT clear the buffer). */
    std::vector<crash_event> get_pending_crashes() const;

    /**
     * Remove a crash from the buffer once the frontend has dealt with it
     * (user clicked Restart / Disable).
     */
    void acknowledge_crash(const std::string& plugin_name);

  private:
    crash_event_bus() = default;

    mutable std::mutex m_mutex;
    std::unordered_map<int, listener_fn> m_listeners;
    std::vector<crash_event> m_pending;      /* events fired before any listener was registered */
    std::vector<crash_event> m_crash_buffer; /* all unacknowledged crashes */
    std::atomic<int> m_id_counter{ 0 };
};

} // namespace mep
