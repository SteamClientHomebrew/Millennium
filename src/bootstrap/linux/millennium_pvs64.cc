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

/**
 * libmillennium_pvs64 (Pressure-Vessel-Shim): pressure-vessel unruntime shim for CDP pipe forwarding.
 * Steam's CreateSimpleProcess() fn strips all fd's we pass to it (we need 3 & 4 for the CDP).
 * This shim wraps and executes the real pressure-vessel-unruntime with --pass-fd=3 --pass-fd=4
 * https://gitlab.steamos.cloud/steamrt/steam-runtime-tools/-/blob/main/pressure-vessel/wrap.1.md#:~:text=variable%2Ddir.-,%2D%2Dpass%2Dfd,-FD
 * documents that 0,1,2 (stdin, stdout and stderr) are the only fd's implicitly passed.
 */
#ifdef __linux__
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

static bool open_at_fd(const char* path, int flags, int target_fd)
{
    int fd = ::open(path, flags);
    if (fd < 0) {
        fprintf(stderr, "[pvs64] open %s failed: %s\n", path, strerror(errno));
        return false;
    }
    if (fd != target_fd) {
        if (dup2(fd, target_fd) < 0) {
            fprintf(stderr, "[pvs64] dup2 %d->%d failed: %s\n", fd, target_fd, strerror(errno));
            ::close(fd);
            return false;
        }
        ::close(fd);
    }
    return true;
}

int main(int argc, char* argv[])
{
    const char* prefix = getenv("PRESSURE_VESSEL_PREFIX");
    const char* runtime_base = getenv("PRESSURE_VESSEL_RUNTIME_BASE");

    if (prefix && *prefix) {
        std::string cmd_path = std::string(prefix) + "/cmd.fifo";
        std::string resp_path = std::string(prefix) + "/resp.fifo";

        open_at_fd(cmd_path.c_str(), O_RDONLY, 3);
        open_at_fd(resp_path.c_str(), O_WRONLY, 4);

        unlink(cmd_path.c_str());
        unlink(resp_path.c_str());
    } else {
        fprintf(stderr, "[pvs64] WARNING: PRESSURE_VESSEL_PREFIX not set\n");
    }

    unsetenv("PRESSURE_VESSEL_PREFIX");

    if (!runtime_base || !*runtime_base) {
        fprintf(stderr, "[pvs64] ERROR: PRESSURE_VESSEL_RUNTIME_BASE not set\n");
        return 1;
    }

    std::string real_unruntime = std::string(runtime_base) + "/pressure-vessel/bin/pressure-vessel-unruntime";

    std::vector<char*> new_argv;
    new_argv.push_back(const_cast<char*>(real_unruntime.c_str()));

    /* pass through our fd's so steamwebhelper can write to our fd */
    new_argv.push_back(const_cast<char*>("--pass-fd=3"));
    new_argv.push_back(const_cast<char*>("--pass-fd=4"));

    /* pass original arguments */
    for (int i = 1; i < argc; ++i)
        new_argv.push_back(argv[i]);

    /* sentinel */
    new_argv.push_back(nullptr);

    execv(real_unruntime.c_str(), new_argv.data());
    fprintf(stderr, "[pvs64] execv failed: %s\n", strerror(errno));
    return 1;
}
#endif
