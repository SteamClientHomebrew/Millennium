#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <argp.h>

#include "incbin.h"
#include "sock_serv.h"

INCTXT(PATCHED_START_SCRIPT, "../../scripts/posix/start.sh");

#define START_SCRIPT_PATH "/usr/bin/steam-runtime"
#define BACKUP_PATH "/usr/bin/steam-runtime.millennium.bak"

void check_sudo() {
    if (getuid() != 0) {
        printf("Insufficient permissions. Please run this script with sudo.\n");
        exit(1);
    }
}

int patch_steam() {
    printf("resolving permissions...\n");
    check_sudo();

    if (access(START_SCRIPT_PATH, F_OK) == -1) {
        printf("error: Steam start script not found. Ensure you're Steam installation is not flatpak, or snap.\n");
        return 1;
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
        return 1;
    }
    fputs(PATCHED_START_SCRIPT_data, file);
    fclose(file);
    chmod(START_SCRIPT_PATH, 0755);
    printf("Successfully wrote: %s\n", START_SCRIPT_PATH);

    return 0;
}

int check_patch_status() {
    check_sudo();

    if (access(START_SCRIPT_PATH, F_OK) == -1) {
        printf("Steam start script not found.\n");
        return 1;
    }
    
    FILE *file = fopen(START_SCRIPT_PATH, "r");
    if (!file) {
        perror("Failed to open script");
        return 1;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        printf("File is empty or could not determine file size.\n");
        fclose(file);
        return 1;
    }
    
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        perror("Failed to allocate memory for buffer");
        fclose(file);
        return 1;
    }
    
    fread(buffer, 1, file_size, file);
    fclose(file);
    buffer[file_size] = '\0';
    
    int is_patched = (strcmp(buffer, PATCHED_START_SCRIPT_data) == 0);
    printf("\033[%dm●\033[0m Steam bootstrapper status\n", is_patched ? 92 : 91);
    printf("  Patched: \033[%dm%s\033[0m\n", is_patched ? 92 : 91, is_patched ? "true" : "false");
    printf("  Path: %s\n", START_SCRIPT_PATH);
    
    free(buffer);
    return !is_patched;
}

int get_python_path() {
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
    return 0;
}

int get_millennium_version() {
    printf("%s\n", MILLENNIUM_VERSION);

    char str[] = MILLENNIUM_VERSION;
    char *token;

    token = strtok(str, ".");
    token++;

    while (token != NULL) {
        printf("%s\n", token);
        token = strtok(NULL, ".");
    }

    return 0;
}

struct argp_option options[] = {
    { "patch", 'p', 0, 0, "Patch the Steam runtime to load Millennium", 1 },
    { "status", 's', 0, 0, "Check if Steam runtime is patched", 1 },
    { "plugins", 'l', 0, 0, "Manage plugins", 1 },
    { "logs", 'g', 0, 0, "Manage themes", 1 },
    { "version", 'v', 0, 0, "Print the current version", 1 },
    { "python", 'y', 0, 0, "Get the path of the python interpreter", 1 },
    { 0 } // Null terminator
};

error_t parse_opt(int key, char *arg, struct argp_state *state) {
    switch (key) {
        case 'p': return patch_steam();
        case 's': return check_patch_status();
        case 'l': return send_message(LIST_PLUGINS,    0);
        case 'g': return send_message(GET_PLUGIN_LOGS, 0); 
        case 'v': return get_millennium_version();
        case 'y': return get_python_path();

        default: return ARGP_ERR_UNKNOWN;
    }
}

int main(int argc, char* argv[]) {

    char title[] = "Millennium@";
    char* __dest = malloc(strlen(title) + strlen(MILLENNIUM_VERSION) + 1);
    if (__dest == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    
    strcpy(__dest, title);
    strcat(__dest, MILLENNIUM_VERSION);

    struct argp argp = { options, parse_opt, NULL, __dest };
    return argp_parse(&argp, argc, argv, 0, 0, NULL);
}
