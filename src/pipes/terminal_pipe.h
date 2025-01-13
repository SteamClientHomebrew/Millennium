#pragma once
#ifdef _WIN32
#include <winbase.h>
#include <io.h>
#endif

#include <sys/log.h>
#include <fcntl.h>
#include <core/py_controller/logger.h>

#ifdef _WIN32
static const wchar_t* PIPE_NAME = LR"(\\.\pipe\MillenniumStdoutPipe)"; 

extern "C" {

    const int CreateTerminalPipe(HANDLE hConsolehandle) 
    {
        std::string filename = "stdout.txt";
        std::ofstream outFile;
        outFile.open(filename, std::ios::trunc);

        HANDLE hNamedPipe = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 0, 0, 0, nullptr);

        if (hNamedPipe == INVALID_HANDLE_VALUE) 
        {
            std::cout << "Failed to create named pipe. Error: " << GetLastError() << std::endl;
            return 1;
        }
        
        if (!ConnectNamedPipe(hNamedPipe, nullptr)) 
        {
            std::cout << "Failed to connect named pipe. Error: " << GetLastError() << std::endl;
            CloseHandle(hNamedPipe);
            return 1;
        }

        char buffer[256];
        unsigned long bytesRead;

        SetConsoleOutputCP(CP_UTF8);

        while (true) 
        {
            bool success = ReadFile(hNamedPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);

            if (!success || bytesRead == 0) 
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            buffer[bytesRead] = '\0';

            unsigned long charsWritten;
            std::string strData = std::string(buffer, bytesRead);

            std::vector<wchar_t> wideBuffer(strData.size());
            int wideLength = MultiByteToWideChar(CP_UTF8, 0, strData.c_str(), strData.size(), wideBuffer.data(), wideBuffer.size());

            if (wideLength > 0) 
            {
                DWORD charsWritten;
                WriteConsoleW(hConsolehandle, wideBuffer.data(), wideLength, &charsWritten, nullptr);
            }

            if (outFile.is_open()) 
            {
                outFile << strData;
                outFile.flush();
            }
            
            RawToLogger("Standard Output", std::string(buffer, bytesRead));
        }

        CloseHandle(hNamedPipe);
        FreeConsole();

        return 0;
    }

    const int RedirectToPipe()
    {
        HANDLE hPipe = CreateFileW(PIPE_NAME, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

        while (hPipe == INVALID_HANDLE_VALUE)
        {
            hPipe = CreateFileW(PIPE_NAME, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        int pipeDescriptor = _open_osfhandle((intptr_t)hPipe, _O_WRONLY);
        if (pipeDescriptor == -1)
        {
            std::cout << "Failed to get pipe file descriptor. Error: " << errno << std::endl;
            CloseHandle(hPipe);
            return 1;
        }

        FILE* pipeFile = _fdopen(pipeDescriptor, "w");
        if (!pipeFile)
        {
            std::cout << "Failed to open pipe file descriptor as FILE*. Error: " << errno << std::endl;
            CloseHandle(hPipe);
            return 1;
        }

        if (_dup2(_fileno(pipeFile), _fileno(stdout)) == -1)
        {
            std::cout << "Failed to redirect stdout to pipe. Error: " << errno << std::endl;
            fclose(pipeFile);
            CloseHandle(hPipe);
            return 1;
        }

        setvbuf(stdout, NULL, _IONBF, 0);

        std::cout << "Redirected stdout to pipe." << std::endl;
        return 0;
    }
}
#endif
