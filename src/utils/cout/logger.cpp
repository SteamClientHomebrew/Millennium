#pragma once
#include <utils/cout/logger.hpp>

#include <chrono>
#include <string>
#include <iostream>

std::vector<std::string> output_log = {};
std::vector<std::string> socket_log = {};

std::string output_console::get_time()
{
	std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	std::stringstream ss;
	ss << std::put_time(&*std::localtime(&time), "%H:%M:%S");
	return std::format("[{}]", ss.str());
}

void output_console::log(std::string type, const std::string& message)
{
	std::lock_guard<std::mutex> lock(logMutex);

	output_log.push_back(std::format("{}{}{}", get_time(), type, message));

	fileStream << get_time() << type << message << std::endl;
	std::cout << get_time() << type << message << std::endl;
}

void output_console::log_socket(std::string val)
{
	socket_log.push_back(std::format("{} {}", get_time(), val));
}

output_console::output_console()
{
	fileStream.open("millennium.log", std::ios::trunc);
}
output_console::~output_console()
{
	fileStream.close();
}

void output_console::log(std::string val) 
{
	log(" [info] ", val);
}

void output_console::err(std::string val) 
{
	SetConsoleTextAttribute(handle, FOREGROUND_RED);
	log(" [fail] ", val);
	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void output_console::warn(std::string val) 
{
	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	log(" [warn] ", val);
	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void output_console::succ(std::string val) 
{
	SetConsoleTextAttribute(handle, FOREGROUND_GREEN);
	log(" [okay] ", val);
	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void output_console::imp(std::string val) 
{
	log(" [+] ", val);
}

void output_console::log_patch(std::string type, std::string what_patched, std::string regex) 
{
	std::lock_guard<std::mutex> lock(logMutex);

	output_log.push_back(std::format("{} [{}] match -> [{}] selector: [{}]", get_time(), type, what_patched, regex));


	fileStream << get_time() << type << std::format("[{}] match -> [{}] selector: [{}]", type, what_patched, regex) << std::endl;

	std::cout << get_time();

	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	std::cout << " [patch] ";
	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

	std::cout << "[" << type << "] ";
	std::cout << "match";

	SetConsoleTextAttribute(handle, FOREGROUND_GREEN);
	std::cout << " -> ";
	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	std::cout << "[";
	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);


	SetConsoleTextAttribute(handle, FOREGROUND_INTENSITY);
	std::cout << what_patched;
	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);


	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	std::cout << "]";
	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

	std::cout << " selector: ";

	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	std::cout << "[";
	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);


	SetConsoleTextAttribute(handle, FOREGROUND_INTENSITY);
	std::cout << regex;
	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);


	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	std::cout << "]" << std::endl;
	SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}