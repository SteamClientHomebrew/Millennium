#pragma once

struct cdp_test_logger
{
    template <typename... Args> void log(Args&&...) {}
};

inline cdp_test_logger logger;
#define LOG_ERROR(...) logger.log(__VA_ARGS__)
