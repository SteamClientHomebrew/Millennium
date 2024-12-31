#include <sys/stat.h>
#include <sys/types.h>
#include <direct.h>
#include <windows.h> 
#include <minizip/unzip.h>
#include <iostream>

#define WRITE_BUFFER_SIZE 8192

int is_symlink(const char *path) {
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return 0;
    }
    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0; 
#else
    struct stat path_stat;
    if (lstat(path, &path_stat) == -1) {
        return 0; 
    }
    return S_ISLNK(path_stat.st_mode);
#endif
}

int is_any_parent_symlink(const char *path) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);

    char *slash = strrchr(tmp, '/');
    while (slash) {
        *slash = '\0';
        if (is_symlink(tmp)) {
            return 1;
        }
        slash = strrchr(tmp, '/');
    }

    return 0;
}

void create_directories(const char *path) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);

    if (tmp[len - 1] == '/' || tmp[len - 1] == '\\') {
        tmp[len - 1] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            _mkdir(tmp);
            *p = '/';
        }
    }
    _mkdir(tmp);
}

void extract_file(unzFile zipfile, const char *filename) {
    if (is_symlink(filename)) {
        printf("Skipping symlink: %s\n", filename);
        return;
    }

    if (strcmp(filename, "user32.dll") == 0) {
        printf("Skipping file: %s\n", filename);
        return;
    }

    char buffer[WRITE_BUFFER_SIZE];
    FILE *outfile = fopen(filename, "wb");
    if (!outfile) {
        fprintf(stderr, "Error opening file %s for writing\n", filename);
        return;
    }

    int bytes_read;
    do {
        bytes_read = unzReadCurrentFile(zipfile, buffer, WRITE_BUFFER_SIZE);
        if (bytes_read < 0) {
            fprintf(stderr, "Error reading file %s from zip archive\n", filename);
            fclose(outfile);
            return;
        }
        if (bytes_read > 0) {
            fwrite(buffer, 1, bytes_read, outfile);
        }
    }
    while (bytes_read > 0);
    fclose(outfile);
}

void extract_zip(const char *zip_filename, const char *output_dir) {
    unzFile zipfile = unzOpen(zip_filename);
    if (!zipfile) {
        fprintf(stderr, "Error: Cannot open zip file %s\n", zip_filename);
        return;
    }

    if (unzGoToFirstFile(zipfile) != UNZ_OK) {
        fprintf(stderr, "Error: Cannot find the first file in %s\n", zip_filename);
        unzClose(zipfile);
        return;
    }

    do {
        char filename[256];
        unz_file_info file_info;

        if (unzGetCurrentFileInfo(zipfile, &file_info, filename, sizeof(filename), NULL, 0, NULL, 0) != UNZ_OK) {
            fprintf(stderr, "Error reading file info in zip archive\n");
            break;
        }

        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, filename);

        if (strcmp(filename, "user32.dll") == 0) {
            // rename it user32.queue.dll
            snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, "user32.queue.dll");
            snprintf(filename, sizeof(filename), "%s", "user32.queue.dll");
        }

        if (is_any_parent_symlink(output_path)) {
            // printf("Skipping file due to symlink in parent directory: %s\n", output_path);
            continue;
        }

        if (filename[strlen(filename) - 1] == '/' || filename[strlen(filename) - 1] == '\\') {
            create_directories(output_path);
            continue;
        }

        char *slash = strrchr(output_path, '/');
        if (slash) {
            *slash = '\0';
            create_directories(output_path);
            *slash = '/';
        }

        if (is_symlink(output_path)) {
            printf("Skipping symlink: %s\n", output_path);
            continue;
        }

        if (unzOpenCurrentFile(zipfile) != UNZ_OK) {
            fprintf(stderr, "Error opening file %s in zip archive\n", filename);
            break;
        }

        extract_file(zipfile, output_path);
        unzCloseCurrentFile(zipfile);
    } 
    while (unzGoToNextFile(zipfile) == UNZ_OK);
    unzClose(zipfile);
}