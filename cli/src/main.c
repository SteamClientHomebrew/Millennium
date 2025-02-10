#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdbool.h>
#include "incbin.h"
#include <pwd.h>

INCTXT(PATCHED_START_SCRIPT, "../../scripts/posix/start.sh");

#define START_SCRIPT_PATH "/usr/bin/steam-runtime"
#define BACKUP_PATH "/usr/bin/steam-runtime.millennium.bak"

void check_sudo() {
    if (getuid() != 0) {
        printf("Insufficient permissions. Please run this script with sudo.\n");
        exit(1);
    }
}

void __patch() {
    printf("resolving permissions...\n");
    check_sudo();

    if (access(START_SCRIPT_PATH, F_OK) == -1) {
        printf("error: Steam start script not found. Ensure you're Steam installation is not flatpak, or snap.\n");
        exit(1);
    }
    
    if (access(BACKUP_PATH, F_OK) == -1) {
        printf("Backing up the original start script...\n");
        rename(START_SCRIPT_PATH, BACKUP_PATH);
        printf("Backup made at: %s\n", BACKUP_PATH);
    } else {
        char response[10]; 

        printf(":: Backup already exists, overwrite? [y/N] ");
        fgets(response, sizeof(response), stdin);

        if (response[0] == 'y' || response[0] == 'Y') {
            remove(BACKUP_PATH);
            rename(START_SCRIPT_PATH, BACKUP_PATH);
        } else {
            printf("skipping backup...\n");
        }
    }
    
    printf("Patching the Steam start script...\n");
    FILE *file = fopen(START_SCRIPT_PATH, "w");
    if (!file) {
        perror("Failed to open script for writing");
        exit(1);
    }
    fputs(PATCHED_START_SCRIPT_data, file);
    fclose(file);
    chmod(START_SCRIPT_PATH, 0755);
    printf("Successfully wrote: %s\n", START_SCRIPT_PATH);
}

void __status() {
    check_sudo();

    if (access(START_SCRIPT_PATH, F_OK) == -1) {
        printf("Steam start script not found.\n");
        exit(1);
    }
    
    FILE *file = fopen(START_SCRIPT_PATH, "r");
    if (!file) {
        perror("Failed to open script");
        exit(1);
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        printf("File is empty or could not determine file size.\n");
        fclose(file);
        exit(1);
    }
    
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        perror("Failed to allocate memory for buffer");
        fclose(file);
        exit(1);
    }
    
    fread(buffer, 1, file_size, file);
    fclose(file);
    buffer[file_size] = '\0';
    
    bool is_patched = (strcmp(buffer, PATCHED_START_SCRIPT_data) == 0);
    printf("\033[%dm●\033[0m Steam bootstrapper status\n", is_patched ? 92 : 91);
    printf("  Patched: \033[%dm%s\033[0m\n", is_patched ? 92 : 91, is_patched ? "true" : "false");
    printf("  Path: %s\n", START_SCRIPT_PATH);
    
    free(buffer);
    exit(is_patched ? 0 : 1);
}

void __python() {
    const char *sudo_user = getenv("SUDO_USER");
    const char *home;

    if (sudo_user) {
        struct passwd *pw = getpwnam(sudo_user);
        if (pw) {
            home = pw->pw_dir;
        } else {
            home = getenv("HOME");
        }
    } else {
        home = getenv("HOME");
    }
    printf("%s/.local/share/millennium/lib/cache/bin/python3.11\n", home);
}

int __version() {
    printf("%s\n", MILLENNIUM_VERSION);

    char str[] = MILLENNIUM_VERSION;
    char *token;

    token = strtok(str, ".");
    token++;

    while (token != NULL) {
        printf("%s\n", token);
        token = strtok(NULL, ".");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s [patch|status|version|python]\n", argv[0]);
        return 1;
    }

    #ifdef __linux__
    if (strcmp(argv[1], "patch") == 0) {
        __patch();
    } else if (strcmp(argv[1], "status") == 0) {
        __status();
    } 
    else 
    #endif

    if (strcmp(argv[1], "version") == 0) {
        __version();
    } else if (strcmp(argv[1], "python") == 0) {
        __python();
    } else {
        printf("Unknown command: %s\n", argv[1]);
        return 1;
    }
    return 0;
}
