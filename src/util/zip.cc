#include <minizip-ng/mz.h>
#include <minizip-ng/mz_zip.h>
#include <minizip-ng/mz_strm.h>
#include <minizip-ng/mz_strm_buf.h>
#include <minizip-ng/mz_strm_mem.h>
#include <minizip-ng/mz_strm_os.h>
#include <minizip-ng/mz_zip_rw.h>

#include "millennium/zip.h"

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

            err = mz_zip_reader_entry_save_file(zip_reader, out_path.c_str());
            if (err != MZ_OK)
                break;
        }

        file_index++;

        if (progress_cb)
            progress_cb(file_index, file_count, file_info->filename);

        err = mz_zip_reader_goto_next_entry(zip_reader);
    }

    mz_zip_reader_close(zip_reader);
    mz_zip_reader_delete(&zip_reader);

    return (err == MZ_END_OF_LIST || err == MZ_OK);
}