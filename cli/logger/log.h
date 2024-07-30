#pragma once
#include <iostream>
#include <util/ansi.h>

#define LOG_LEVEL_WARN 1
#define LOG_LEVEL_FAIL 2
#define LOG_LEVEL_LOG  3
#define LOG_LEVEL LOG_LEVEL_LOG

#define LOG_WARN(msg) if (LOG_LEVEL >= LOG_LEVEL_WARN) std::cout << BOLD << YELLOW << "WARN " << RESET << msg << std::endl; 
#define LOG_FAIL(msg) if (LOG_LEVEL >= LOG_LEVEL_FAIL) std::cerr << BOLD << RED << "FAIL " << RESET << msg << std::endl; 
#define LOG_INFO(msg) if (LOG_LEVEL >= LOG_LEVEL_LOG) std::cout << BOLD << GREEN << "INFO " << RESET << msg << std::endl; 