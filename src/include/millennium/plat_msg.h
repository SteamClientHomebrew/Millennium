#pragma once

#include "millennium/logger.h"
#include "millennium/filesystem.h"
#include <cstdio>
#include <cstdlib>
#ifdef __linux__
#include <linux/limits.h>
#include <unistd.h>
#elif _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace platform
{
namespace messagebox
{
typedef enum
{
    error,
    warn,
    info,
    question
} level;

[[maybe_unused]]
static int show(const std::string title, const std::string message, platform::messagebox::level level)
{
    if (message.empty()) {
        LOG_ERROR("No message provided to message box call");
        return -1;
    }

#ifdef _WIN32
    UINT type = MB_OK;

    switch (level) {
        case MessageLevel::Error:
            type |= MB_ICONERROR;
            break;
        case MessageLevel::Warning:
            type |= MB_ICONWARNING;
            break;
        case MessageLevel::Info:
            type |= MB_ICONINFORMATION;
            break;
        case MessageLevel::Question:
            type |= MB_YESNO | MB_ICONQUESTION;
            break;
    }

    return MessageBoxA(nullptr, message.data(), title.empty() ? "Message" : title.data(), type);

#elif __linux__
    const std::string steam_path = platform::get_steam_path().generic_string();

    char zenity_path[PATH_MAX];
    snprintf(zenity_path, sizeof(zenity_path), "%s%s", steam_path.c_str(), "ubuntu12_32/steam-runtime/i386/usr/bin/zenity");

    std::string cmd = zenity_path;

    switch (level) {
        case level::error:
            cmd += " --error";
            break;
        case level::warn:
            cmd += " --warning";
            break;
        case level::info:
            cmd += " --info";
            break;
        case level::question:
            cmd += " --question";
            break;
    }

    if (!title.empty()) {
        cmd += " --title=\"";
        cmd += title;
        cmd += "\"";
    }

    cmd += " --text=\"";
    cmd += message;
    cmd += "\"";

    logger.log("executing command: {}", cmd);
    int ret = std::system(cmd.c_str());

    if (level == level::question) return ret == 0 ? 1 : 0;

    return ret;

#else
#error "Unsupported Platform"
#endif
}
} // namespace messagebox
} // namespace platform
