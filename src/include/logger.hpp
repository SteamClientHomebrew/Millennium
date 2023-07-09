#pragma once
/// <summary>
/// incredibly shitty logger class
/// </summary>
class output_console {
private:
	HANDLE consoleHandle;
	std::ofstream fileStream;

	std::string get_time()
	{
		std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

		std::stringstream ss;
		ss << std::put_time(&*std::localtime(&time), "%H:%M:%S");
		return std::format("[{}]", ss.str());
	}

	void log(std::string type, const std::string& message) 
	{
		fileStream << get_time() << type << message << std::endl;
		std::cout  << get_time() << type << message <<  std::endl;
	}
public:
	output_console()
	{
		consoleHandle=(GetStdHandle(STD_OUTPUT_HANDLE));
		fileStream.open("millennium.log", std::ios::trunc);
	}
	~output_console() 
	{
		fileStream.close();
	}

	void log (std::string val) { log(" [info] ", val); }
	void err (std::string val) { 
		SetConsoleTextAttribute(consoleHandle, FOREGROUND_RED | FOREGROUND_INTENSITY);
		log(" [fail] ", val);
		SetConsoleTextAttribute(consoleHandle, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);
	}
	void warn(std::string val) { log(" [warn] ", val); }
	void succ(std::string val) { log(" [okay] ", val); }
	void imp (std::string val) { log(" [+] ", val); }

	void log_patch(std::string type, std::string what_patched, std::string regex)	{

		SetConsoleTextAttribute(consoleHandle, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		log(" [patch] ", std::format("[{}] match -> [{}] selector: [{}]", type, what_patched, regex));
		SetConsoleTextAttribute(consoleHandle, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);
	}
};
static output_console console;