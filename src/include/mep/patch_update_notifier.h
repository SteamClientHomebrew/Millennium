#pragma once
#include <functional>
#include <mutex>
#include <unordered_map>

class patch_update_notifier
{
  public:
    using listener_fn = std::function<void()>;

    static patch_update_notifier& instance()
    {
        static patch_update_notifier inst;
        return inst;
    }

    int add_listener(listener_fn fn)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        int id = m_next_id++;
        m_listeners[id] = std::move(fn);
        return id;
    }

    void remove_listener(int id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listeners.erase(id);
    }

    void notify()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [id, fn] : m_listeners)
            fn();
    }

  private:
    std::mutex m_mutex;
    std::unordered_map<int, listener_fn> m_listeners;
    int m_next_id = 0;
};
