#pragma once
#ifdef _WIN32
#include <winbase.h>
#include <io.h>
#endif

#include <sys/log.h>
#include <fcntl.h>

#ifdef _WIN32
extern "C" {
    const int CreateTerminalPipe()
    {
        try {
            HANDLE hPipe = CreateFileA(R"(\\.\pipe\MillenniumStdoutPipe)", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

            if (hPipe == INVALID_HANDLE_VALUE)
            {
                std::cerr << "Failed to connect to pipe. Error: " << GetLastError() << std::endl;
                return 1;
            }

            int pipeDescriptor = _open_osfhandle((intptr_t)hPipe, _O_WRONLY);
            if (pipeDescriptor == -1)
            {
                std::cerr << "Failed to get pipe file descriptor. Error: " << errno << std::endl;
                CloseHandle(hPipe);
                return 1;
            }

            FILE* pipeFile = _fdopen(pipeDescriptor, "w");
            if (!pipeFile)
            {
                std::cerr << "Failed to open pipe file descriptor as FILE*. Error: " << errno << std::endl;
                CloseHandle(hPipe);
                return 1;
            }

            if (_dup2(_fileno(pipeFile), _fileno(stdout)) == -1)
            {
                std::cerr << "Failed to redirect stdout to pipe. Error: " << errno << std::endl;
                fclose(pipeFile);
                CloseHandle(hPipe);
                return 1;
            }

            setvbuf(stdout, NULL, _IONBF, 0);
            return 0;
        }
        catch (std::system_error& ex) {
            std::cerr << "Failed to create terminal pipe: " << ex.what() << std::endl;
            return 1;
        }
    }
}
#endif
