#pragma once
#define _WINSOCKAPI_   

#include <string>
#include <Windows.h>
#include <vector>
#include <shellapi.h>

/**
 * @brief Class representing Steam application.
 */
class steam {
public:
    /**
     * @brief Class representing Steam parameters.
     */
    class parameters {
    public:
        /**
         * @brief Constructs parameters from command line.
         *
         * @param commandLine The command line string.
         */
        parameters(LPSTR commandLine) {
            parse_outlet(commandLine);
        }

        /**
         * @brief Checks if a parameter exists.
         *
         * @param param The parameter to check.
         * @return true if the parameter exists, false otherwise.
         */
        bool has(const std::string& param) const {
            for (const std::string& arg : args) {
                if (arg == param) {
                    return true;
                }
            }
            return false;
        }

    private:
        /**
         * @brief Parses command line arguments.
         *
         * @param commandLine The command line string.
         */
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

        std::vector<std::string> args; /**< Vector to store command line arguments. */
    };

    /**
     * @brief Returns the singleton instance of Steam.
     *
     * @return The singleton instance of Steam.
     */
    static steam& get() {
        static steam instance;
        return instance;
    }

    parameters params; /**< Parameters of the Steam instance. */

private:
    /**
     * @brief Constructs a Steam instance.
     *
     * @param commandLine The command line string.
     */
    steam() : params(GetCommandLineA()) {}

    steam(const steam&) = delete; /**< Deleted copy constructor. */
    steam& operator=(const steam&) = delete; /**< Deleted copy assignment operator. */
};