#pragma once
#include <nlohmann/json.hpp>

extern "C" {
	namespace ipc_pipe {
		enum type_t {
			call_server_method,
			front_end_loaded
		};

		const int _create();
	}
}