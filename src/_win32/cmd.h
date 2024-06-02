#pragma once  
#include <string>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>
#include <Windows.h>
#include <shellapi.h>
#endif

class steam {
public:
    class parameters {
    public:
        parameters() {
            parse_outlet();
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
        void parse_outlet() {
#ifdef _WIN32
            int argc = 0;
            wchar_t** argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
            if (!argvW) return;

            for (int i = 0; i < argc; ++i) {
                int len = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
                if (len <= 0) return;

                std::vector<char> buffer(len);
                WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, buffer.data(), len, NULL, NULL);

                args.emplace_back(buffer.data());
            }
            LocalFree(argvW);
#endif
        }
        std::vector<std::string> args; 
    };
    static steam& get() {
        static steam instance;
        return instance;
    }

    parameters params; 
    steam() : params() {}
    steam(const steam&) = delete; 
    steam& operator=(const steam&) = delete;
};