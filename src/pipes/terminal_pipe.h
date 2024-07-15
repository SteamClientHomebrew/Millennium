#include <winbase.h>
#include <sys/log.h>
#include <io.h>
#include <fcntl.h>

const int CreateTerminalPipe()
{
    const std::string pipeName = R"(\\.\pipe\MillenniumStdoutPipe)";

    // Try to open a named pipe; wait for it, if necessary.
    HANDLE hPipe = CreateFileA(pipeName.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Failed to connect to pipe." << std::endl;
        return 1;
    }

    int pipeDescriptor = _open_osfhandle((intptr_t)hPipe, _O_WRONLY);
    if (pipeDescriptor == -1)
    {
        std::cerr << "Failed to get pipe file descriptor." << std::endl;
        CloseHandle(hPipe);
        return 1;
    }

    FILE* pipeFile = _fdopen(pipeDescriptor, "w");
    if (!pipeFile)
    {
        std::cerr << "Failed to open pipe file descriptor as FILE*." << std::endl;
        CloseHandle(hPipe);
        return 1;
    }

    // Redirect stdout to the named pipe
    if (dup2(_fileno(pipeFile), _fileno(stdout)) == -1)
    {
        std::cerr << "Failed to redirect stdout to pipe." << std::endl;
        CloseHandle(hPipe);
        fclose(pipeFile); // Close the FILE* on failure
        return 1;
    }
    setvbuf(stdout, NULL, _IONBF, 0);

    std::shared_ptr<FILE*> pipePtr = std::make_shared<FILE*>(pipeFile);
    return 0;
}