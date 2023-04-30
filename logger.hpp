#ifndef LOGGER
#define LOGGER

class Console {
private:
	std::string get_time()
	{
		// Get the current time
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		std::time_t time = std::chrono::system_clock::to_time_t(now);

		// Convert the time to a tm struct
		std::tm tm = *std::localtime(&time);

		std::stringstream ss;
		ss << std::put_time(&tm, "%y-%m-%d %H:%M:%S");
		return "[" + ss.str() + "]";
	}
public:
	void log(std::string val)
	{
		std::cout << get_time() << " [INFO] " << val << std::endl;
	}
	void err(std::string val)
	{
		std::cout << get_time() << " [FAIL] " << val << std::endl;
	}
	void warn(std::string val)
	{
		std::cout << get_time() << " [WARN] " << val << std::endl;
	}
	void succ(std::string val)
	{
		std::cout << get_time() << " [OKAY] " << val << std::endl;
	}
};

#endif // LOGGER