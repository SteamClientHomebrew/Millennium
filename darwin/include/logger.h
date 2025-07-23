#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>

#define COLOR_RESET "\033[0m"
#define COLOR_INFO "\033[1;34m"
#define COLOR_ERROR "\033[1;31m"
#define COLOR_WARN "\033[1;33m"

#define GET_TIMESTAMP() ({ \
    struct timeval tv; \
    struct tm *tm_info; \
    gettimeofday(&tv, NULL); \
    tm_info = localtime(&tv.tv_sec); \
    static char timestamp_buf[16]; \
    snprintf(timestamp_buf, sizeof(timestamp_buf), "[%02d:%02d.%03d]", tm_info->tm_min, tm_info->tm_sec, (int)(tv.tv_usec / 10000)); \
    timestamp_buf; \
})

#define LOG_INFO(fmt, ...) fprintf(stdout, "%s " COLOR_INFO "OSX-INFO " COLOR_RESET fmt "\n", GET_TIMESTAMP(), ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "%s " COLOR_ERROR "OSX-ERROR " COLOR_RESET fmt "\n", GET_TIMESTAMP(), ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) fprintf(stderr, "%s " COLOR_WARN "OSX-WARN " COLOR_RESET fmt "\n", GET_TIMESTAMP(), ##__VA_ARGS__)

#endif // LOGGER_H