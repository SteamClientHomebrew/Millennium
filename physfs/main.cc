#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cstring>
#include "mz.h"
#include "mz_zip.h"
#include "mz_strm.h"
#include "mz_strm_mem.h"

class ZipMemoryReader {
private:
    void* zip_handle;
    void* mem_stream;
    std::vector<uint8_t> zip_data;

public:
    ZipMemoryReader() : zip_handle(NULL), mem_stream(NULL) {}
    
    ~ZipMemoryReader() {
        close();
    }

    // Load zip file into memory
    bool loadZipFromFile(const std::string& zip_path) {
        FILE* file = fopen(zip_path.c_str(), "rb");
        if (!file) {
            std::cerr << "Error: Cannot open zip file: " << zip_path << std::endl;
            return false;
        }

        // Get file size
        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Read entire file into memory
        zip_data.resize(file_size);
        size_t bytes_read = fread(zip_data.data(), 1, file_size, file);
        fclose(file);

        if (bytes_read != file_size) {
            std::cerr << "Error: Failed to read entire zip file" << std::endl;
            return false;
        }

        return openZipFromMemory();
    }

    // Load zip data directly from memory buffer
    bool loadZipFromMemory(const std::vector<uint8_t>& data) {
        zip_data = data;
        return openZipFromMemory();
    }

    // Read a specific file from the zip archive into memory
    std::vector<uint8_t> readFileFromZip(const std::string& filename) {
        std::vector<uint8_t> result;
        
        if (!zip_handle) {
            std::cerr << "Error: Zip file not loaded" << std::endl;
            return result;
        }

        // Locate the file in the zip - correct signature: (handle, filename, ignore_case)
        uint8_t ignore_case = 0;
        int32_t err = mz_zip_locate_entry(zip_handle, filename.c_str(), ignore_case);
        if (err != MZ_OK) {
            std::cerr << "Error: File '" << filename << "' not found in zip archive" << std::endl;
            return result;
        }

        // Get file info
        mz_zip_file* file_info = NULL;
        err = mz_zip_entry_get_info(zip_handle, &file_info);
        if (err != MZ_OK || !file_info) {
            std::cerr << "Error: Cannot get file info for '" << filename << "'" << std::endl;
            return result;
        }

        // Check if it's a directory
        if (mz_zip_entry_is_dir(zip_handle) == MZ_OK) {
            std::cerr << "Error: '" << filename << "' is a directory, not a file" << std::endl;
            return result;
        }

        // Open the entry for reading - correct signature: (handle, raw, password)
        uint8_t raw = 0;
        err = mz_zip_entry_read_open(zip_handle, raw, NULL);
        if (err != MZ_OK) {
            std::cerr << "Error: Cannot open entry '" << filename << "' for reading (error: " << err << ")" << std::endl;
            return result;
        }

        // Allocate buffer based on uncompressed size
        uint64_t uncompressed_size = file_info->uncompressed_size;
        result.resize(static_cast<size_t>(uncompressed_size));

        // Read the file data
        int32_t bytes_read = mz_zip_entry_read(zip_handle, result.data(), static_cast<int32_t>(uncompressed_size));
        if (bytes_read < 0) {
            std::cerr << "Error: Failed to read file data from '" << filename << "'" << std::endl;
            result.clear();
        } else if (static_cast<uint64_t>(bytes_read) != uncompressed_size) {
            std::cerr << "Warning: Read " << bytes_read << " bytes, expected " << uncompressed_size << std::endl;
            result.resize(bytes_read);
        }

        // Close the entry - correct signature: (handle, crc32, compressed_size, uncompressed_size)
        uint32_t crc32 = 0;
        int64_t compressed_size = 0;
        int64_t uncompressed_size_actual = uncompressed_size;
        mz_zip_entry_read_close(zip_handle, &crc32, &compressed_size, &uncompressed_size_actual);
        
        return result;
    }

    // Read a file as string (for text files)
    std::string readTextFileFromZip(const std::string& filename) {
        auto data = readFileFromZip(filename);
        if (data.empty()) {
            return "";
        }
        return std::string(data.begin(), data.end());
    }

    // List all files in the zip archive
    std::vector<std::string> listFiles() {
        std::vector<std::string> files;
        
        if (!zip_handle) {
            std::cerr << "Error: Zip file not loaded" << std::endl;
            return files;
        }

        int32_t err = mz_zip_goto_first_entry(zip_handle);
        while (err == MZ_OK) {
            mz_zip_file* file_info = NULL;
            if (mz_zip_entry_get_info(zip_handle, &file_info) == MZ_OK && file_info) {
                if (file_info->filename) {
                    files.push_back(std::string(file_info->filename));
                }
            }
            err = mz_zip_goto_next_entry(zip_handle);
        }

        return files;
    }

    void close() {
        if (zip_handle) {
            mz_zip_close(zip_handle);
            mz_zip_delete(&zip_handle);
            zip_handle = NULL;
        }
        if (mem_stream) {
            mz_stream_close(mem_stream);
            mz_stream_delete(&mem_stream);
            mem_stream = NULL;
        }
    }

private:
    bool openZipFromMemory() {
        // Create memory stream - correct signature: no parameters
        mem_stream = mz_stream_mem_create();
        if (!mem_stream) {
            std::cerr << "Error: Failed to create memory stream" << std::endl;
            return false;
        }
        
        mz_stream_mem_set_buffer(mem_stream, zip_data.data(), static_cast<int32_t>(zip_data.size()));
        
        // Open memory stream - correct signature: (stream, path, mode)
        int32_t mode = MZ_OPEN_MODE_READ;
        if (mz_stream_open(mem_stream, NULL, mode) != MZ_OK) {
            std::cerr << "Error: Failed to open memory stream" << std::endl;
            return false;
        }

        // Create zip reader - correct signature: no parameters
        zip_handle = mz_zip_create();
        if (!zip_handle) {
            std::cerr << "Error: Failed to create zip handle" << std::endl;
            return false;
        }
        
        int32_t err = mz_zip_open(zip_handle, mem_stream, MZ_OPEN_MODE_READ);
        if (err != MZ_OK) {
            std::cerr << "Error: Failed to open zip from memory (error code: " << err << ")" << std::endl;
            mz_zip_delete(&zip_handle);
            zip_handle = NULL;
            return false;
        }

        return true;
    }
};

// Example usage
int main() {
    ZipMemoryReader reader;
    
    // Load zip file from disk into memory
    std::string zip_path = "C:/Program Files (x86)/Steam/ext/data/loader.bin.zip";
    if (!reader.loadZipFromFile(zip_path)) {
        std::cerr << "Failed to load zip file: " << zip_path << std::endl;
        return 1;
    }

    std::cout << "Successfully loaded zip file into memory!" << std::endl;

    // List all files in the zip
    std::cout << "\nFiles in the zip archive:" << std::endl;
    auto files = reader.listFiles();
    for (const auto& file : files) {
        std::cout << "  " << file << std::endl;
    }

    std::string desired_file = "loader/millennium-pre.c5.2.20.b5.2.20.js";
    std::cout << "\nReading file: " << desired_file << std::endl;
    
    std::string text_content = reader.readTextFileFromZip(desired_file);
    if (!text_content.empty()) {
        std::cout << "Text content (" << text_content.size() << " bytes):" << std::endl;
        std::cout << text_content.substr(0, 500);  // Show first 500 chars
        if (text_content.size() > 500) {
            std::cout << "...";
        }
        std::cout << std::endl;
    }

    // For binary files
    auto binary_data = reader.readFileFromZip(desired_file);
    if (!binary_data.empty()) {
        std::cout << "Binary data size: " << binary_data.size() << " bytes" << std::endl;
        
        // Print first 20 bytes as hex (for demonstration)
        std::cout << "First bytes (hex): ";
        size_t print_count = std::min(binary_data.size(), size_t(20));
        for (size_t i = 0; i < print_count; ++i) {
            printf("%02x ", binary_data[i]);
        }
        if (binary_data.size() > print_count) {
            std::cout << "...";
        }
        std::cout << std::endl;
    }

    return 0;
}