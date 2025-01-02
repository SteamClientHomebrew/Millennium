#pragma once
#include <nlohmann/json.hpp>

extern "C"
{
	namespace IPCMain
	{
		enum Builtins
		{
			CALL_SERVER_METHOD,
			FRONT_END_LOADED,
			GET_FRONTEND_SETTINGS
		};

		const uint16_t OpenConnection();
	}
}