#pragma once

struct lifecycle_test_logger
{
    template <typename... Args> void log(const char*, Args&&...)
    {
    }
};

inline lifecycle_test_logger logger;
