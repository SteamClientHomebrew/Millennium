#ifndef LOGGER
#define LOGGER

#include <fstream>

class Console {
private:
	std::ofstream fileStream;

	std::string get_time()
	{
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		std::time_t time = std::chrono::system_clock::to_time_t(now);

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

	Console() 
	{
		fileStream.open("millennium.log", std::ios::app);
	}
	~Console() 
	{
		fileStream.close();
	}

	void log (std::string val) { log(" [INFO] ", val); }
	void err (std::string val) { log(" [FAIL] ", val); }
	void warn(std::string val) { log(" [WARN] ", val); }
	void succ(std::string val) { log(" [OKAY] ", val); }
	void imp (std::string val) { log(" [Millennium] ", val); }
};

#endif // LOGGER