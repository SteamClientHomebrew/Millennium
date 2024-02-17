//            Copyright (c) Glyn Matthews 2016.
//         (C) Copyright 2007-9 Anthony Williams
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_UTILS_THREAD_GROUP_INC
#define BOOST_NETWORK_UTILS_THREAD_GROUP_INC

#include <thread>
#include <mutex>
#include <memory>
#include <list>
#include <algorithm>

namespace boost {
namespace network {
namespace utils {
class thread_group {
 private:
  thread_group(thread_group const&);
  thread_group& operator=(thread_group const&);

 public:
  thread_group() {}
  ~thread_group() {}

  template <typename F>
  std::thread* create_thread(F threadfunc) {
    std::lock_guard<std::mutex> guard(m);
    std::unique_ptr<std::thread> new_thread(new std::thread(threadfunc));
    threads.push_back(std::move(new_thread));
    return threads.back().get();
  }

  void add_thread(std::thread* thrd) {
    if (thrd) {
      std::lock_guard<std::mutex> guard(m);
      threads.push_back(std::unique_ptr<std::thread>(thrd));
    }
  }

  void remove_thread(std::thread* thrd) {
    std::lock_guard<std::mutex> guard(m);
    auto const it = std::find_if(threads.begin(), threads.end(),
                                 [&thrd] (std::unique_ptr<std::thread> &arg) {
                                   return arg.get() == thrd;
                                 });
    if (it != threads.end()) {
      threads.erase(it);
    }
  }

  void join_all() {
    std::unique_lock<std::mutex> guard(m);

    for (auto &thread : threads) {
      if (thread->joinable() && thread->get_id() != std::this_thread::get_id()) {
        thread->join();
      }
    }
  }

  size_t size() const {
    std::unique_lock<std::mutex> guard(m);
    return threads.size();
  }

 private:
  std::list<std::unique_ptr<std::thread>> threads;
  mutable std::mutex m;
};

}  // namespace utils
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_UTILS_THREAD_GROUP_INC
