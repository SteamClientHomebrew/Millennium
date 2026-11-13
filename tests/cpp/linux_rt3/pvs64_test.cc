#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace
{
const char* const k_fifos[] = { "cmd.fifo", "resp.fifo", "lb-cmd.fifo", "lb-resp.fifo" };
const int k_access_modes[] = { O_RDONLY, O_WRONLY, O_RDONLY, O_WRONLY };

void report(const std::string& message)
{
    std::fprintf(stderr, "pvs64_test: %s\n", message.c_str());
}

std::string read_file(const fs::path& path)
{
    std::ifstream stream(path);
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

bool child_check(int argc, char** argv)
{
    if (argc != 3 || std::strcmp(argv[1], "--pvs64-child") != 0 || std::strcmp(argv[2], "argument with spaces") != 0) {
        report("quoted argument was not preserved");
        return false;
    }

    bool valid = true;
    for (int index = 0; index < 4; ++index) {
        const int fd = 3 + index;
        struct stat status {};
        errno = 0;
        const bool is_fifo = fstat(fd, &status) == 0 ? S_ISFIFO(status.st_mode)
                                                     : (errno == ESTALE && fcntl(fd, F_GETPIPE_SZ) >= 0);
        if (!is_fifo) {
            report("fd " + std::to_string(fd) + " is not a FIFO");
            valid = false;
            continue;
        }

        const int flags = fcntl(fd, F_GETFL);
        if (flags < 0 || (flags & O_ACCMODE) != k_access_modes[index]) {
            report("fd " + std::to_string(fd) + " has the wrong access mode");
            valid = false;
        }

        const int descriptor_flags = fcntl(fd, F_GETFD);
        if (descriptor_flags < 0 || (descriptor_flags & FD_CLOEXEC) != 0) {
            report("fd " + std::to_string(fd) + " is not inheritable");
            valid = false;
        }
    }

    if (std::getenv("PRESSURE_VESSEL_PREFIX") != nullptr) {
        report("PRESSURE_VESSEL_PREFIX survived exec");
        valid = false;
    }
    if (std::getenv("MILLENNIUM_PVS64_DIRECT_EXEC") != nullptr) {
        report("MILLENNIUM_PVS64_DIRECT_EXEC survived exec");
        valid = false;
    }
    return valid;
}

struct fixture
{
    fs::path root;
    fs::path target;
    int holders[4] = { -1, -1, -1, -1 };
    pid_t child = -1;
    bool reaped = false;

    ~fixture()
    {
        if (child > 0 && !reaped) {
            kill(-child, SIGKILL);
            kill(child, SIGKILL);
            waitpid(child, nullptr, 0);
        }
        for (int& fd : holders) {
            if (fd >= 0) close(fd);
        }
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }

    bool create()
    {
        const char* temp = std::getenv("TMPDIR");
        std::string pattern = std::string(temp != nullptr && *temp != '\0' ? temp : "/tmp") + "/millennium-pvs64-XXXXXX";
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        const char* directory = mkdtemp(buffer.data());
        if (directory == nullptr) return false;
        root = fs::absolute(directory);

        std::error_code error;
        fs::create_directory(root / "bin", error);
        if (error) return false;

        for (int index = 0; index < 4; ++index) {
            const fs::path path = root / k_fifos[index];
            if (mkfifo(path.c_str(), 0600) != 0) return false;
            holders[index] = open(path.c_str(), O_RDWR | O_CLOEXEC);
            if (holders[index] < 0) return false;
        }

        char self[PATH_MAX];
        const ssize_t length = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (length <= 0) return false;
        self[length] = '\0';
        target = root / "bin" / "pvs-target";
        fs::copy_file(self, target, fs::copy_options::overwrite_existing, error);
        if (error) return false;
        fs::permissions(target, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, error);
        return !error;
    }

    bool spawn(const fs::path& pvs, const std::vector<std::string>& arguments)
    {
        std::vector<std::string> values;
        values.push_back(pvs.string());
        values.insert(values.end(), arguments.begin(), arguments.end());

        std::vector<char*> argv;
        for (std::string& value : values) argv.push_back(value.data());
        argv.push_back(nullptr);

        child = fork();
        if (child < 0) return false;
        if (child == 0) {
            setpgid(0, 0);
            const fs::path log = root / "child.log";
            const int log_fd = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (log_fd >= 0) {
                dup2(log_fd, STDERR_FILENO);
                close(log_fd);
            }
            execv(argv[0], argv.data());
            _exit(127);
        }
        setpgid(child, child);
        return true;
    }

    bool wait_for_child(int& status)
    {
        const auto deadline = std::chrono::steady_clock::now() + 10s;
        for (;;) {
            const pid_t result = waitpid(child, &status, WNOHANG);
            if (result == child) {
                reaped = true;
                return true;
            }
            if (result < 0 && errno != EINTR) {
                if (errno == ECHILD) reaped = true;
                return false;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                kill(-child, SIGKILL);
                kill(child, SIGKILL);
                waitpid(child, &status, 0);
                reaped = true;
                return false;
            }
            std::this_thread::sleep_for(10ms);
        }
    }

    bool fifos_removed() const
    {
        for (const char* name : k_fifos) {
            if (access((root / name).c_str(), F_OK) == 0) return false;
        }
        return true;
    }
};

bool run(const fs::path& pvs, const std::string& mode)
{
    fixture scratch;
    if (!scratch.create()) {
        report("failed to create scratch fixture");
        return false;
    }

    if (setenv("PRESSURE_VESSEL_PREFIX", scratch.root.c_str(), 1) != 0 || setenv("MILLENNIUM_PVS64_DIRECT_EXEC", "1", 1) != 0) {
        report("failed to configure environment");
        return false;
    }

    if (mode == "path") {
        const char* previous = std::getenv("PATH");
        std::string path = (scratch.root / "bin").string();
        if (previous != nullptr && *previous != '\0') path = path + ":" + previous;
        if (setenv("PATH", path.c_str(), 1) != 0) {
            report("failed to configure PATH");
            return false;
        }
    }

    std::vector<std::string> arguments;
    if (mode != "no-command") {
        arguments.push_back(mode == "absolute" ? scratch.target.string() : "pvs-target");
        arguments.push_back("--pvs64-child");
        arguments.push_back("argument with spaces");
    }

    if (!scratch.spawn(pvs, arguments)) {
        report("failed to spawn pvs64");
        return false;
    }

    int status = 0;
    if (!scratch.wait_for_child(status)) {
        report("pvs64 child did not exit within the deadline");
        return false;
    }

    const std::string output = read_file(scratch.root / "child.log");
    if (mode == "no-command") {
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 1) {
            report("no-command did not exit with status 1\n" + output);
            return false;
        }
        if (output.find("without a command") == std::string::npos) {
            report("no-command diagnostic was missing\n" + output);
            return false;
        }
    } else {
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            report("child self-check failed\n" + output);
            return false;
        }
        if (output.find("MILLENNIUM_PVS64_TEST_CHILD_OK") == std::string::npos) {
            report("child self-check did not run\n" + output);
            return false;
        }
    }

    if (!scratch.fifos_removed()) {
        report("pvs64 left FIFO paths behind");
        return false;
    }
    return true;
}
}

int main(int argc, char** argv)
{
    if (argc >= 2 && std::strcmp(argv[1], "--pvs64-child") == 0) {
        alarm(5);
        const bool valid = child_check(argc, argv);
        if (valid) std::fputs("MILLENNIUM_PVS64_TEST_CHILD_OK\n", stderr);
        return valid ? 0 : 1;
    }

    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <pvs64> absolute|path|no-command\n", argv[0]);
        return 2;
    }

    const std::string mode = argv[2];
    if (mode != "absolute" && mode != "path" && mode != "no-command") {
        std::fprintf(stderr, "pvs64_test: unknown mode %s\n", argv[2]);
        return 2;
    }
    return run(argv[1], mode) ? 0 : 1;
}
