#include "crash_event_bus.h"
#include <algorithm>

namespace mep
{

crash_event_bus& crash_event_bus::instance()
{
    static crash_event_bus s;
    return s;
}

void crash_event_bus::notify(const crash_event& ev)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    /* Always persist — this is the reliable path. The buffer stays until the
       frontend explicitly acknowledges the crash. */
    m_crash_buffer.push_back(ev);

    if (m_listeners.empty()) {
        /* No listener yet — also buffer for replay when add_listener() is called. */
        m_pending.push_back(ev);
        return;
    }

    for (auto& [id, fn] : m_listeners) {
        try {
            fn(ev);
        } catch (...) {
        }
    }
}

int crash_event_bus::add_listener(listener_fn fn)
{
    int id = ++m_id_counter;

    std::vector<crash_event> to_replay;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listeners[id] = fn;
        to_replay = std::move(m_pending);
    }

    /* Replay any crashes that fired before the listener existed.
       These are already in m_crash_buffer, so this is purely for
       the real-time JS dispatch path. */
    for (const auto& ev : to_replay) {
        try {
            fn(ev);
        } catch (...) {
        }
    }

    return id;
}

void crash_event_bus::remove_listener(int id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_listeners.erase(id);
}

std::vector<crash_event> crash_event_bus::get_pending_crashes() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_crash_buffer;
}

void crash_event_bus::acknowledge_crash(const std::string& plugin_name)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_crash_buffer.erase(std::remove_if(m_crash_buffer.begin(), m_crash_buffer.end(),
                                        [&](const crash_event& ev)
    {
        return ev.plugin_name == plugin_name;
    }),
                         m_crash_buffer.end());
}

} // namespace mep
