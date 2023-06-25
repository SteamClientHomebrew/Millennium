#pragma once
/// <summary>
/// incredibly shitty logger class
/// </summary>
class output_console {
private:
	std::ofstream fileStream;

	std::string get_time()
	{
		std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

		std::stringstream ss;
		ss << std::put_time(&*std::localtime(&time), "%y-%m-%d %H:%M:%S");
		return "[" + ss.str() + "]";
	}

	void log(std::string type, const std::string& message) 
	{
		fileStream << get_time() << type << message << std::endl;
		std::cout  << get_time() << type << message <<  std::endl;
	}
public:

	output_console() 
	{
		fileStream.open("millennium.log", std::ios::trunc);
	}
	~output_console() 
	{
		fileStream.close();
	}

	void log (std::string val) { log(" [INFO] ", val); }
	void err (std::string val) { log(" [FAIL] ", val); }
	void warn(std::string val) { log(" [WARN] ", val); }
	void succ(std::string val) { log(" [OKAY] ", val); }
	void imp (std::string val) { log(" [Millennium] ", val); }
};