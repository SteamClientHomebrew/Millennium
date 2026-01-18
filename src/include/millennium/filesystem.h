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

#pragma once
#define MINI_CASE_SENSITIVE
#include <filesystem>
#include <mini/ini.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace platform
{
    class file_exception : public std::exception
    {
      public:
        file_exception(const std::string& message) : msg(message)
        {
        }
        virtual const char* what() const noexcept override
        {
            return msg.c_str();
        }

      private:
        std::string msg;
    };

    std::filesystem::path get_steam_path();

    /**
     * FIXME: This function is no longer useful and should be removed.
     * It was intended to point to the local install path, however there isn't one anymore on unix.
     */
    std::filesystem::path get_install_path();
    nlohmann::json read_file_json(const std::string& filename, bool* success = nullptr);
    std::string read_file(const std::string& filename);
    std::vector<char> read_file_bytes(const std::string& filePath);
    void write_file(const std::filesystem::path& filePath, std::string content);
    void write_file_bytes(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent);
    std::string get_millennium_preload_path();
    void safe_remove_directory(const std::filesystem::path& root);
    void make_writable(const std::filesystem::path& p);
    bool remove_directory(const std::filesystem::path& p);
}; // namespace platform
