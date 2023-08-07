#pragma once
#define _WINSOCKAPI_   

#include <string>
#include <Windows.h>
#include <vector>
#include <shellapi.h>

class steam {
public:
    class parameters {
    public:
        parameters(LPSTR commandLine) {
            parse_outlet(commandLine);
        }

        bool has(const std::string& param) const {
            for (const std::string& arg : args) {
                if (arg == param) {
                    return true;
                }
            }
            return false;
        }

    private:
        void parse_outlet(LPSTR commandLine) {
            int argc = 0;
            LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);

            if (!argvW)
                return;

            for (int i = 0; i < argc; ++i) {
                int len = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);

                if (len <= 0)
                    return;

                std::vector<char> buffer(len);
                WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, buffer.data(), len, NULL, NULL);

                args.emplace_back(buffer.data());
            }
            LocalFree(argvW);
        }

        std::vector<std::string> args;
    };

    static steam& get_instance() {
        static steam instance;
        return instance;
    }

    parameters params;

private:
    steam() : params(GetCommandLineA()) {}
    steam(const steam&) = delete;
    steam& operator=(const steam&) = delete;
};
