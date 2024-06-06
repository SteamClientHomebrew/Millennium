#include "log.hpp"
#include <chrono>
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <fmt/core.h>
#include <sstream>
#include <thread>
#ifdef _WIN32
#include <_win32/cmd.h>
#endif
#include <sys/locals.h>
#include <_win32/thread.h>

OutputLogger Logger;

std::string OutputLogger::GetLocalTime()
{
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::stringstream bufferStream;
	bufferStream << std::put_time(std::localtime(&time), "%H:%M:%S");
	bufferStream << fmt::format(".{:03}", ms.count());
	return fmt::format("[{}]", bufferStream.str());
}

void OutputLogger::PrintMessage(std::string type, const std::string& message)
{
	std::lock_guard<std::mutex> lock(logMutex);

	std::cout << GetLocalTime() << type << message << "\n";
	*outputLogStream << GetLocalTime() << type << message << "\n";
	outputLogStream->flush();
}

OutputLogger::OutputLogger()
{
	if (!IsSteamApplication())
	{
		return;
	}

	#ifdef _WIN32
	{
		std::unique_ptr<StartupParameters> startupParams = std::make_unique<StartupParameters>();

		if (startupParams->HasArgument("-dev") && static_cast<bool>(AllocConsole()))
		{
			void(freopen("CONOUT$", "w", stdout));
			void(freopen("CONOUT$", "w", stderr));
		}

		SetConsoleOutputCP(CP_UTF8);
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	}
	#endif

	const auto fileName = SystemIO::GetSteamPath() / ".millennium" / "debug.log";

	try
	{
		std::filesystem::create_directories(fileName.parent_path());
	}
	catch (const std::exception& e)
	{
		std::cout << "Error: " << e.what() << std::endl;
	}

	outputLogStream = std::make_shared<std::ofstream>(fileName);

    // Check if the file stream is open
    if (!outputLogStream->is_open()) 
	{
        std::cerr << "Failed to open the file." << std::endl;
        return;
    }
}

OutputLogger::~OutputLogger() 
{
	outputLogStream->close();
}

void OutputLogger::LogPluginMessage(std::string pluginName, std::string strMessage)
{
	#ifdef _WIN32
	{
		SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
	}
	#endif
	std::cout << GetLocalTime() << " [" << pluginName << "] ";
	#ifdef _WIN32
	{
		SetConsoleTextAttribute(hConsole, COLOR_WHITE);
	}
	#endif
	std::cout << strMessage << "\n";

	*outputLogStream << GetLocalTime() << " [" << pluginName << "] " << strMessage << "\n";
	outputLogStream->flush();
}

void OutputLogger::LogHead(std::string strHeadTitle) 
{
	const auto message = fmt::format("\n[┬] {}", strHeadTitle);

	#ifdef _WIN32
	{
		SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
	}
	#endif
    std::cout << "\n[┬] ";
	#ifdef _WIN32
	{
		SetConsoleTextAttribute(hConsole, COLOR_WHITE);
	}
	#endif
	std::cout << strHeadTitle << "\n";
	*outputLogStream << message << "\n";
	outputLogStream->flush();
}

void OutputLogger::LogItem(std::string pluginName, std::string strMessage, bool end) 
{
	std::string connectorPiece = end ? "└" : "├";
	const auto message = fmt::format(" {}──[{}]: {}", connectorPiece, pluginName, strMessage);
	
	#ifdef _WIN32
	{
		SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
	}
	#endif
    std::cout << " " << connectorPiece << "──[" <<pluginName << "]: ";
	#ifdef _WIN32
	{
		SetConsoleTextAttribute(hConsole, COLOR_WHITE);
	}
	#endif

	std::cout << strMessage << "\n";
	*outputLogStream << message << "\n";
	outputLogStream->flush();
 }
