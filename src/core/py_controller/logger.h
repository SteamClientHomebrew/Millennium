#pragma once
#include <Python.h>
#include <memory>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <fmt/core.h>
#include <fmt/color.h>
#include <sys/locals.h>
#include <sys/log.h>
#include <vector>

#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[38;5;214m"
#define BLUE "\x1b[94m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"
#define RESET "\033[0m"

class BackendLogger 
{
public:
    enum LogLevel {
        _INFO,
        _WARN,
        _ERROR
    };

    struct LogEntry {
        std::string message;
        LogLevel level;
    };

private:
    std::ofstream file;
    std::string filename;
    std::string pluginName;

    std::vector<LogEntry> logBuffer;

    std::string GetLocalTime(bool withHours = false)
    {
        std::stringstream bufferStream;
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        if (withHours)
        {
            bufferStream << std::put_time(std::localtime(&time), "%H:%M:%S");
        }
        else
        {
            bufferStream << std::put_time(std::localtime(&time), "%M:%S");
        }
        bufferStream << fmt::format(".{:03}", ms.count());
        return fmt::format("[{}]", bufferStream.str());
    }

    std::string GetLocalDateStr()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm = *std::localtime(&now_c);

        int year = local_tm.tm_year + 1900;
        int month = local_tm.tm_mon + 1;
        int day = local_tm.tm_mday;
        int hour = local_tm.tm_hour;
        int min = local_tm.tm_min;
        int sec = local_tm.tm_sec;

        return fmt::format("[{}-{}-{} @ {}:{}:{}]", year, month, day, hour, min, sec);
    }

public:

    BackendLogger(const std::string& pluginName) : pluginName(pluginName)
    {
        this->filename = (SystemIO::GetInstallPath() / "ext" / "data" / "logs" / fmt::format("{}_log.txt", pluginName)).generic_string();
        file.open(filename, std::ios::app);

        file << fmt::format("\n\n\n--------------------------------- [{}] ---------------------------------\n", GetLocalDateStr());
        file.flush();
    }

    ~BackendLogger() 
    {
        file.close();
    }

    void Log(const std::string& message, bool onlyBuffer = false) 
    {        
        std::string formatted = fmt::format("{} ", GetPluginName());

        if (!onlyBuffer) 
        {
            fmt::print("{} ", GetLocalTime());
    
            fmt::print("\033[1m\033[34m{}\033[0m\033[0m", formatted);
            fmt::print("{}\n", message.c_str());
            file << formatted << message << "\n";
            file.flush();
        }

        logBuffer.push_back({ WHITE + GetLocalTime(true) + RESET + " " + BLUE + formatted + RESET + message + "\n", _INFO });
    }

    void Warn(const std::string& message, bool onlyBuffer = false) 
    {
        if (!onlyBuffer)
        {
            std::string formatted = fmt::format("{}{}{}\n", GetLocalTime(), fmt::format(" {} ", GetPluginName()), message.c_str());

            fmt::print(fg(fmt::color::orange), formatted, "\n");
            file << formatted;
            file.flush();
        }

        logBuffer.push_back({ WHITE + GetLocalTime(true) + YELLOW + " " + GetPluginName() + " " + message + RESET + "\n", _WARN });
    }

    void Error(const std::string& message, bool onlyBuffer = false) 
    {
        if (!onlyBuffer) 
        {
            std::string formatted = fmt::format("{}{}{}\n", GetLocalTime(), fmt::format(" {} ", GetPluginName()), message.c_str());

            fmt::print(fg(fmt::color::red), formatted, "\n");
            file << formatted;
            file.flush();
        }

        logBuffer.push_back({ WHITE + GetLocalTime(true) + RED + " " + GetPluginName() + " " + message + RESET + "\n", _ERROR });
    }

    std::vector<LogEntry> CollectLogs() 
    {
        return logBuffer;
    }

    std::string GetPluginName(bool upperCase = true) 
    { 
        const auto toUpper = [](const std::string& str) {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(), ::toupper);
            return result;
        };

        return upperCase ? toUpper(pluginName) : pluginName;    
    }
};

extern std::vector<BackendLogger*> g_loggerList;

static void AddLoggerMessage(const std::string pluginName, const std::string message, BackendLogger::LogLevel level) 
{
    for (auto logger : g_loggerList) 
    {
        if (logger->GetPluginName(false) == pluginName) 
        {
            switch (level) 
            {
                case BackendLogger::_INFO:  logger->Log(message, true);   break;
                case BackendLogger::_WARN:  logger->Warn(message, true);  break;
                case BackendLogger::_ERROR: logger->Error(message, true); break;
            }
            return;
        }
    }

    // Make a new logger for the plugin
    BackendLogger* newLogger = new BackendLogger(pluginName);

    switch (level) 
    {
        case BackendLogger::_INFO:  newLogger->Log(message, true);   break;
        case BackendLogger::_WARN:  newLogger->Warn(message, true);  break;
        case BackendLogger::_ERROR: newLogger->Error(message, true); break;
    }

    g_loggerList.push_back(newLogger);
}

static const void InfoToLogger(const std::string pluginNme, const std::string message) {
    AddLoggerMessage(pluginNme, message, BackendLogger::_INFO);    
}

static const void WarnToLogger(const std::string pluginNme, const std::string message) {
    AddLoggerMessage(pluginNme, message, BackendLogger::_WARN);    
}

static const void ErrorToLogger(const std::string pluginNme, const std::string message) {
    AddLoggerMessage(pluginNme, message, BackendLogger::_ERROR);    
}

typedef struct 
{
    PyObject ob_base;
    char *prefix;
    BackendLogger* m_loggerPtr;
} 
LoggerObject;

extern PyTypeObject LoggerType;
PyObject* PyInit_Logger(void);
