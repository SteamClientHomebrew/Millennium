#pragma once
#include <string>
#include <fmt/core.h>

inline void Print(const std::string& msg) 
{
    fmt::print("++ {}\n", msg);
}

template <typename... Args>
inline void Print(const std::string& msg, Args&&... args) 
{
    fmt::print("++ " + msg + '\n', std::forward<Args>(args)...);
}

inline void Error(const std::string& msg) 
{
    fmt::print("!! {}\n", msg);
}

template <typename... Args>
inline void Error(const std::string& msg, Args&&... args) 
{
    fmt::print("!! " + msg + '\n', std::forward<Args>(args)...);
}