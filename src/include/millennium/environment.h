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

/**
 * @file env.cc
 *
 * @brief This file is responsible for setting up environment variables that are used throughout the application.
 */
#pragma once
#include <memory>
#include <string>
#include <map>
#include <vector>

extern std::map<std::string, std::string> envVariables;

/** Maintain compatibility with different compilers */
#ifdef __GNUC__
#define CONSTRUCTOR __attribute__((constructor))
#define DESTRUCTOR __attribute__((destructor))
#define DLL_EXPORT extern "C" __attribute__((dllexport))
#else
#define CONSTRUCTOR
#define DESTRUCTOR
#define DLL_EXPORT extern "C" __declspec(dllexport)
#endif

namespace platform
{
    class environment
    {
      public:
        static void setup();
        static std::string get(std::string key);
        static std::string get(std::string key, std::string fallback);
        static void set(const std::string& key, const std::string& value);

      private:
        static std::vector<std::unique_ptr<char[]>> allocated_strings;
        static bool add(std::unique_ptr<char[]> strEnv);
        static bool set_unix(const std::string& name, const std::string& value);
    }; // namespace environment
} // namespace platform
