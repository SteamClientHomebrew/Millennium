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

#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>

class millennium_lifecycle
{
  public:
    struct gate
    {
        std::mutex mtx;
        std::condition_variable cv;
        std::atomic<bool> flag{ false };

        void wait()
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [this]
            {
                return flag.load();
            });
        }

        void notify()
        {
            {
                std::lock_guard<std::mutex> lk(mtx);
                flag.store(true);
            }
            cv.notify_all();
        }
    };

    static millennium_lifecycle& get()
    {
        static millennium_lifecycle instance;
        return instance;
    }

    gate steam_ui_loaded;
    gate backends_loaded;
    gate steam_unloaded;
    gate terminate;

    std::atomic<bool> disconnect_frontend{ false }; // Windows only

    millennium_lifecycle(const millennium_lifecycle&) = delete;
    millennium_lifecycle& operator=(const millennium_lifecycle&) = delete;

  private:
    millennium_lifecycle() = default;
};
