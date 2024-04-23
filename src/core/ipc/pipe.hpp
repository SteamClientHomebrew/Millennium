#pragma once
#include <nlohmann/json.hpp>

extern "C" {
	namespace pipe {
		enum type_t {
			call_server_method,
			front_end_loaded
		};

		const void _create();
	}
}