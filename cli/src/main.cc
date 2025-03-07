#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE

#include <CLI/CLI.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pwd.h>
#include <iostream>

#include "incbin.h"
#include "sock_serv.h"
#include "chown.h"

#include <fmt/core.h>

INCTXT(PATCHED_START_SCRIPT, "../../scripts/posix/start.sh");

#define START_SCRIPT_PATH "/usr/bin/steam-runtime"
#define BACKUP_PATH "/usr/bin/steam-runtime.millennium.bak"

void check_sudo() {
    if (getuid() != 0) {
        std::cerr << "Insufficient permissions. Please run this script with sudo.\n";
        exit(1);
    }
}

const void patch_steam() 
{
    std::cout << "Resolving permissions...\n";
    check_sudo();

    if (access(START_SCRIPT_PATH, F_OK) == -1) 
    {
        std::cerr << "Error: Steam start script not found. Ensure you're Steam installation is not flatpak, or snap.\n";
        return;
    }

    if (access(BACKUP_PATH, F_OK) == -1) 
    {
        std::cout << "Backing up the original start script...\n";
        rename(START_SCRIPT_PATH, BACKUP_PATH);
        std::cout << "Backup made at: " << BACKUP_PATH << "\n";
    } 
    else 
    {
        char response[10];
        std::cout << ":: Backup already exists, overwrite? [y/N] ";
        std::cin.getline(response, sizeof(response));

        if (response[0] == 'y' || response[0] == 'Y') 
        {
            remove(BACKUP_PATH);
            rename(START_SCRIPT_PATH, BACKUP_PATH);
        } 
        else {
            std::cout << "Skipping backup...\n";
        }
    }

    std::cout << "Patching the Steam start script...\n";
    FILE *file = fopen(START_SCRIPT_PATH, "w");
    if (!file) 
    {
        perror("Failed to open script for writing");
        return;
    }
    fputs(PATCHED_START_SCRIPT_data, file);
    fclose(file);

    chmod(START_SCRIPT_PATH, 0555);
    std::cout << "Successfully wrote: " << START_SCRIPT_PATH << "\n";
    return;
};

const void check_patch_status() 
{
    check_sudo();

    if (access(START_SCRIPT_PATH, F_OK) == -1) 
    {
        std::cerr << "Steam start script not found.\n";
        return;
    }

    FILE *file = fopen(START_SCRIPT_PATH, "r");
    if (!file) 
    {
        perror("Failed to open script");
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) 
    {
        std::cerr << "File is empty or could not determine file size.\n";
        fclose(file);
        return;
    }

    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) 
    {
        perror("Failed to allocate memory for buffer");
        fclose(file);
        return;
    }

    fread(buffer, 1, file_size, file);
    fclose(file);
    buffer[file_size] = '\0';

    int is_patched = (strcmp(buffer, PATCHED_START_SCRIPT_data) == 0);
    std::cout << "\n\033[32m>\e[0m  \033[1mPatched:\033[0m \033[" << (is_patched ? 92 : 91) << "m" << (is_patched ? "yes" : "no") << "\033[0m\n";
    std::cout << "\033[32m>\e[0m  \033[1mPath:\033[0m    \033[92m" << START_SCRIPT_PATH << "\033[0m\n";

    free(buffer);
};

const void get_python_path() 
{
    const char *sudo_user = getenv("SUDO_USER");
    const char *home;

    if (sudo_user) 
    {
        struct passwd *pw = getpwnam(sudo_user);
        if (pw) 
        {
            home = pw->pw_dir;
        } 
        else 
        {
            home = getenv("HOME");
        }
    } 
    else 
    {
        home = getenv("HOME");
    }

    std::cout << home << "/.local/share/millennium/lib/cache/bin/python3.11" << std::endl;
};

const void setup_millennium()
{
    check_sudo();
    const char *dir = getenv("HOME");
    if (dir == NULL) 
    {
        std::cerr << "HOME environment variable not set\n";
        return;
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/.local/share/millennium", dir);
    change_ownership(path);
    return;
};

const std::string get_millennium_version() 
{
    return 
    "\nmillennium@" MILLENNIUM_VERSION "\n"
    "Copyright (C) 2025 Project-Millennium, FOSS.\n"
    "This software is licensed under the MIT License. See more at <https://opensource.org/license/mit> \n"
    "This is free software; see the source for copying conditions. There is NO\n"
    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
    "Written by shdwmtr.";
};

int main(int argc, char* argv[]) 
{
    CLI::App app{fmt::format("Millennium@{}", MILLENNIUM_VERSION)};

    app.add_subcommand("setup",   "Setup Millennium after a fresh install"     )->callback(setup_millennium);
    app.add_subcommand("patch",   "Patch the Steam runtime to load Millennium" )->callback(patch_steam);
    app.add_subcommand("status",  "Check the status of the Steam runtime patch")->callback(check_patch_status);
    app.add_subcommand("python",  "Get the path to the Python binary"          )->callback(get_python_path);
    // app.add_subcommand("logs",    "View the instance logs of Millennium"       )->callback([]() { send_message(Millennium::GET_PLUGIN_LOGS, nullptr); });

    // auto* plugins = app.add_subcommand("plugins", "Manage installed user-plugins");

    // plugins->require_subcommand(1);
    // plugins->add_subcommand("list", "List all the installed plugins"  )->callback([]() { send_message(Millennium::LIST_PLUGINS, nullptr); });
    // plugins->add_subcommand("status", "Get the status of a current plugin")->callback([]() { send_message(Millennium::GET_PLUGIN_STATUS, nullptr); });

    app.set_version_flag("-v,--version", get_millennium_version());

    app.footer("If you have any issues, please report them at: <https://github.com/shdwmtr/millennium/issues>");

    CLI11_PARSE(app, argc, argv);
}
