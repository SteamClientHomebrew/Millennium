#include "log.h"
#include <chrono>
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <fmt/core.h>
#include <sstream>
#include <thread>
#ifdef _WIN32
#include <procmon/cmd.h>
#include <procmon/thread.h>
#endif
#include <sys/locals.h>

OutputLogger Logger;

std::string OutputLogger::GetLocalTime()
{
	std::stringstream bufferStream;
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

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
	#ifdef _WIN32
	//if (!IsSteamApplication())
	//{
	//	return;
	//}

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
	catch (const std::exception& exception)
	{
		LOG_ERROR("An error occurred creating debug log directories -> {}", exception.what());
	}

	outputLogStream = std::make_shared<std::ofstream>(fileName);

    if (!outputLogStream->is_open()) 
	{
		LOG_ERROR("Couldn't open output log stream.");
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

void OutputLogger::LogHead(std::string strHeadTitle, bool useColor) 
{
	const auto message = fmt::format("\n[┬] {}", strHeadTitle);

	#ifdef _WIN32
	{
		if (useColor) SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
	}
	#endif
	std::cout << "\n[┬] ";
	#ifdef _WIN32
	{
		if (useColor) SetConsoleTextAttribute(hConsole, COLOR_WHITE);
	}
	#endif
	std::cout << strHeadTitle << "\n";
	*outputLogStream << message << "\n";
	outputLogStream->flush();
}

void OutputLogger::LogItem(std::string pluginName, std::string strMessage, bool end, bool useColor) 
{
	std::string connectorPiece = end ? "└" : "├";
	const auto message = fmt::format(" {}──[{}]: {}", connectorPiece, pluginName, strMessage);
	
	#ifdef _WIN32
	{
		if (useColor) SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
	}
	#endif
	std::cout << " " << connectorPiece << "──[" <<pluginName << "]: ";
	#ifdef _WIN32
	{
		if (useColor) SetConsoleTextAttribute(hConsole, COLOR_WHITE);
	}
	#endif

	std::cout << strMessage << "\n";
	*outputLogStream << message << "\n";
	outputLogStream->flush();
 }
