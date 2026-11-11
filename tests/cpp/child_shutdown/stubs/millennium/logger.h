#pragma once
#include <format>

struct child_shutdown_test_logger
{
    template <typename... Args> void log(Args&&...) {}
};

inline child_shutdown_test_logger logger;
#define LOG_ERROR(...) logger.log(__VA_ARGS__)
