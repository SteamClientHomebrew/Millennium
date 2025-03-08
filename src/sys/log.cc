/**
 * ==================================================
 *   _____ _ _ _             _                     
 *  |     |_| | |___ ___ ___|_|_ _ _____           
 *  | | | | | | | -_|   |   | | | |     |          
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
 * 
 * ==================================================
 * 
 * Copyright (c) 2025 Project Millennium
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "log.h"
#include <chrono>
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#ifdef _WIN32
#include "cmd.h"
#endif
#include "locals.h"
#include <env.h>

OutputLogger Logger;

std::string OutputLogger::GetLocalTime()
{
	std::stringstream bufferStream;
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	bufferStream << std::put_time(std::localtime(&time), "%M:%S");
	bufferStream << fmt::format(".{:03}", ms.count());
	return fmt::format("[{}]", bufferStream.str());
}

void OutputLogger::PrintMessage(std::string type, const std::string& message, std::string color)
{
	std::lock_guard<std::mutex> lock(logMutex);

	if (m_bIsVersbose || m_bIsConsoleEnabled)
	{
		fmt::print("{}\033[1m{}{}{}\033[0m{}\n", GetLocalTime(), color, type, COL_RESET, message);
	}

	if (m_bIsVersbose)
		return;

	*outputLogStream << GetLocalTime() << type << message << "\n";
	outputLogStream->flush();
}

OutputLogger::OutputLogger()
{
	#ifdef _WIN32
	{
		std::unique_ptr<StartupParameters> startupParams = std::make_unique<StartupParameters>();

		m_bIsConsoleEnabled = ((GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState('M') & 0x8000)) || startupParams->HasArgument("-dev");
		m_bIsVersbose = startupParams->HasArgument("-verbose");
	}
	#elif __linux__
	{
		this->m_bIsConsoleEnabled = true;
		this->m_bIsVersbose = true;
	}
	#endif

	const auto fileName = std::filesystem::path(GetEnv("MILLENNIUM__LOGS_PATH")) / "debug.log";

	try
	{
		std::filesystem::create_directories(fileName.parent_path());
	}
	catch (const std::exception& exception)
	{
		LOG_ERROR("An error occurred creating debug log directories -> {}", exception.what());
	}

	outputLogStream = std::make_shared<std::ofstream>(fileName, std::ios::out | std::ios::trunc);

    if (!outputLogStream->is_open()) 
	{
		LOG_ERROR("Couldn't open output log stream.");
        return;
    }

	//fmt::print("[+] Bootstrapping Millennium@{}\n", MILLENNIUM_VERSION);                                    
}

OutputLogger::~OutputLogger() 
{
	outputLogStream->close();
}

void OutputLogger::LogPluginMessage(std::string pluginName, std::string strMessage)
{
	const auto toUpper = [](const std::string& str) {
		std::string result = str;
		std::transform(result.begin(), result.end(), result.begin(), ::toupper);
		return result;
	};

	if (m_bIsVersbose || m_bIsConsoleEnabled)
	{
		fmt::print("{} ", GetLocalTime());
        fmt::print("\033[1m\033[34m{} \033[0m\033[0m", toUpper(pluginName));
		fmt::print("{}\n", strMessage);
	}

	if (m_bIsVersbose)
		return;

	*outputLogStream << GetLocalTime() << " [" << pluginName << "] " << strMessage << "\n";
	outputLogStream->flush();
}

void OutputLogger::LogHead(std::string strHeadTitle, fmt::text_style color) 
{
	const auto message = fmt::format("\n(┬) {}", strHeadTitle);

	if (m_bIsVersbose || m_bIsConsoleEnabled)
	{
		fmt::print(color, "\n(┬) ");
		fmt::print("{}\n", strHeadTitle);
	}

	if (m_bIsVersbose)
		return;

	*outputLogStream << message << "\n";
	outputLogStream->flush();
}

void OutputLogger::LogItem(std::string pluginName, std::string strMessage, bool end, fmt::text_style color) 
{
	std::string connectorPiece = end ? "╰" : "├";
	const auto message = fmt::format(" {}─({}) {}", connectorPiece, pluginName, strMessage);
	
	if (m_bIsVersbose || m_bIsConsoleEnabled)
	{
		fmt::print(color, " {}─({}) ", connectorPiece, pluginName);
		fmt::print("{}\n", strMessage);
	}

	if (m_bIsVersbose)
		return;

	*outputLogStream << message << "\n";
	outputLogStream->flush();
}
