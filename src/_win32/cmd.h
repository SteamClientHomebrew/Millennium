#pragma once  
#include <string>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>
#include <Windows.h>
#include <shellapi.h>
#endif

class StartupParameters 
{
public:

    StartupParameters() 
    {
        this->ParseCommandLine();
    }

    bool HasArgument(const std::string& targetArgument) const 
    {
        for (const std::string& argument : m_argumentList) 
        {
            if (argument == targetArgument) 
            {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> GetArgumentList()
    {
        return this->m_argumentList;
    }

private:

    void ParseCommandLine() 
    {
#ifdef _WIN32
        int argc = 0;
        wchar_t** argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argvW) return;

        for (int i = 0; i < argc; ++i) 
        {
            int len = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
            if (len <= 0) return;

            std::vector<char> buffer(len);
            WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, buffer.data(), len, NULL, NULL);

            m_argumentList.emplace_back(buffer.data());
        }
        LocalFree(argvW);
#endif
    }

    std::vector<std::string> m_argumentList;
};