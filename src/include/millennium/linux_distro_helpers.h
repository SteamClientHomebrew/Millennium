/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

#ifdef __linux__
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <cstdlib>
#include <fcntl.h>
#include <sys/auxv.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

inline std::string get_linux_distribution_id()
{
    std::string line, distro;
    std::ifstream file("/etc/os-release");

    while (std::getline(file, line)) {
        if (line.find("ID=") == 0) {
            distro = line.substr(3);
            break;
        }
    }

    return distro;
}

inline std::string get_linux_architecture()
{
    struct utsname info;
    uname(&info);

    return info.machine;
}

inline bool program_in_path(const std::string& name)
{
    const char* path_env = getenv("PATH");
    if (!path_env) return false;

    std::istringstream ss(path_env);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (access((dir + "/" + name).c_str(), X_OK) == 0) return true;
    }
    return false;
}

inline bool millennium_is_pacman_managed()
{
    pid_t pid = fork();
    if (pid < 0) return false;

    if (pid == 0) {
        int fd = open("/dev/null", O_WRONLY);
        if (fd >= 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        char* argv[] = { (char*)"pacman", (char*)"-Qi", (char*)"millennium", nullptr };
        execvp("pacman", argv);
        _exit(127); /** 127 means cmd not found */
    }

    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

inline std::string get_aur_update_command()
{
    static const std::vector<std::pair<std::string, std::string>> helpers = {
        { "yay",    "yay -Syu millennium"      },
        { "paru",   "paru -Syu millennium"     },
        { "aurman", "aurman -Syu millennium"   },
        { "pikaur", "pikaur -Syu millennium"   },
        { "pamac",  "pamac upgrade millennium" },
        { "trizen", "trizen -Syu millennium"   },
        { "pacaur", "pacaur -Syu millennium"   },
        { "aura",   "aura -Ayu millennium"     },
    };

    for (const auto& [helper, cmd] : helpers) {
        if (program_in_path(helper)) return cmd;
    }

    return "[failed to detect aur helper]";
}
#endif
