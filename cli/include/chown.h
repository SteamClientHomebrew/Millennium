#ifndef CHOWN_H
#define CHOWN_H
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

void change_ownership(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) == -1) {
        perror("stat");
        return;
    }

    // Get current user's UID and GID
    uid_t uid = getuid();
    gid_t gid = getgid();

    // Change ownership of the file or directory
    if (chown(path, uid, gid) == -1) {
        perror("chown");
        return;
    }

    // If it's a directory, recursively change ownership of its contents
    if (S_ISDIR(statbuf.st_mode)) {
        DIR *dir = opendir(path);
        if (dir == NULL) {
            perror("opendir");
            return;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            // Skip the "." and ".." entries
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char newpath[1024];
            snprintf(newpath, sizeof(newpath), "%s/%s", path, entry->d_name);

            // Recursively change ownership of the directory contents
            change_ownership(newpath);
        }

        closedir(dir);
    }
}

#endif // CHOWN_H