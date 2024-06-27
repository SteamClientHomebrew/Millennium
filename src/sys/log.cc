#include "log.h"
#include <chrono>
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
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

#ifdef _WIN32
void EnableVirtualTerminalProcessing() 
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
    {
        return;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode))
    {
        return;
    }
}
#endif

void OutputLogger::PrintMessage(std::string type, const std::string& message, fmt::v10::text_style color)
{
	std::lock_guard<std::mutex> lock(logMutex);

	if (m_bIsConsoleEnabled)
	{
		fmt::print(color, "{}{}{}\n", GetLocalTime(), type, message);
	}

	*outputLogStream << GetLocalTime() << type << message << "\n";
	outputLogStream->flush();
}

OutputLogger::OutputLogger()
{
	#ifdef _WIN32
	{
		std::unique_ptr<StartupParameters> startupParams = std::make_unique<StartupParameters>();

		m_bIsConsoleEnabled = startupParams->HasArgument("-dev");

		if (m_bIsConsoleEnabled && static_cast<bool>(AllocConsole()))
		{
			void(freopen("CONOUT$", "w", stdout));
			void(freopen("CONOUT$", "w", stderr));

			EnableVirtualTerminalProcessing();
			SetConsoleOutputCP(CP_UTF8);
		}
	}
	#endif

	const auto fileName = SystemIO::GetSteamPath() / "ext" / "data" / "logs" / "debug.log";

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
	if (m_bIsConsoleEnabled)
	{
		fmt::print(DEFAULT_ACCENT_COL, "{} [{}] ", GetLocalTime(), pluginName);
		fmt::print("{}\n", strMessage);
	}

	*outputLogStream << GetLocalTime() << " [" << pluginName << "] " << strMessage << "\n";
	outputLogStream->flush();
}

void OutputLogger::LogHead(std::string strHeadTitle, fmt::v10::text_style color) 
{
	const auto message = fmt::format("\n[┬] {}", strHeadTitle);

	if (m_bIsConsoleEnabled)
	{
		fmt::print(color, "\n[┬] ");
		fmt::print("{}\n", strHeadTitle);
	}

	*outputLogStream << message << "\n";
	outputLogStream->flush();
}

void OutputLogger::LogItem(std::string pluginName, std::string strMessage, bool end, fmt::v10::text_style color) 
{
	std::string connectorPiece = end ? "└" : "├";
	const auto message = fmt::format(" {}──[{}]: {}", connectorPiece, pluginName, strMessage);
	
	if (m_bIsConsoleEnabled)
	{
		fmt::print(color, " {}──[{}]: ", connectorPiece, pluginName);
		fmt::print("{}\n", strMessage);
	}

	*outputLogStream << message << "\n";
	outputLogStream->flush();
 }
