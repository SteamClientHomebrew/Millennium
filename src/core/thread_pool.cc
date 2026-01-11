/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "millennium/thread_pool.h"
#include "millennium/logger.h"

thread_pool::thread_pool(size_t num_threads) : stop(false), shutdown_called(false)
{
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this]
        {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    condition.wait(lock, [this] { return stop || !tasks.empty(); });

                    if (stop && tasks.empty()) {
                        return;
                    }

                    if (tasks.empty()) {
                        continue;
                    }

                    task = std::move(tasks.front());
                    tasks.pop();
                }

                try {
                    task();
                } catch (const std::exception& ex) {
                    Logger.Log(std::string("ThreadPool worker exception: ") + ex.what());
                }
            }
        });
    }
}

thread_pool::~thread_pool()
{
    shutdown();
}

void thread_pool::shutdown()
{
    if (shutdown_called.exchange(true)) {
        return;
    }

    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
        while (!tasks.empty())
            tasks.pop();
    }

    condition.notify_all();

    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void thread_pool::enqueue(std::function<void()> f)
{
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop) {
            return;
        }
        tasks.emplace(std::move(f));
    }
    condition.notify_one();
}