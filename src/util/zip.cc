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

#include <mz.h>
#include <mz_zip.h>
#include <mz_strm.h>
#include <mz_strm_buf.h>
#include <mz_strm_mem.h>
#include <mz_strm_os.h>
#include <mz_zip_rw.h>

#include "millennium/zip.h"
#include "millennium/logger.h"

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_set>

bool Util::ExtractZipArchive(const std::string& zip_path, const std::string& dest_dir, ProgressCallback progress_cb)
{
    void* zip_reader = mz_zip_reader_create();
    if (zip_reader == nullptr)
        return false;

    int32_t err = mz_zip_reader_open_file(zip_reader, zip_path.c_str());
    if (err != MZ_OK) {
        Logger.Log("Failed to open zip file: {} (error: {})", zip_path, err);
        mz_zip_reader_delete(&zip_reader);
        return false;
    }

    /** get file count */
    int32_t file_count = 0;
    err = mz_zip_reader_goto_first_entry(zip_reader);
    while (err == MZ_OK) {
        file_count++;
        err = mz_zip_reader_goto_next_entry(zip_reader);
    }

    std::filesystem::create_directories(dest_dir);

    /** Pre-allocate containers for better performance */
    std::unordered_set<std::string> created_dirs;
    created_dirs.reserve(file_count / 4);
    std::string out_path;
    out_path.reserve(dest_dir.length() + 256);

    int32_t file_index = 0;
    int32_t failed_count = 0;
    mz_zip_file* file_info = nullptr;

    err = mz_zip_reader_goto_first_entry(zip_reader);
    while (err == MZ_OK) {
        mz_zip_reader_entry_get_info(zip_reader, &file_info);

        out_path.clear();
        out_path += dest_dir;
        out_path += '/';
        out_path += file_info->filename;

        if (file_info->filename[strlen(file_info->filename) - 1] == '/') {
            if (created_dirs.insert(out_path).second) {
                std::filesystem::create_directories(out_path);
            }
        } else {
            std::string parent_path = std::filesystem::path(out_path).parent_path().string();
            if (created_dirs.insert(parent_path).second) {
                std::filesystem::create_directories(parent_path);
            }

            int32_t extract_err = mz_zip_reader_entry_save_file(zip_reader, out_path.c_str());
            if (extract_err != MZ_OK) {
                Logger.Warn("Failed to extract file: {} (error code: {})", out_path, extract_err);
                failed_count++;
            }
        }

        file_index++;
        if (progress_cb)
            progress_cb(file_index, file_count, file_info->filename);

        err = mz_zip_reader_goto_next_entry(zip_reader);
    }

    mz_zip_reader_close(zip_reader);
    mz_zip_reader_delete(&zip_reader);

    if (failed_count > 0) {
        Logger.Warn("Extraction completed with {} failed files", failed_count);
    }

    return (err == MZ_END_OF_LIST || err == MZ_OK);
}