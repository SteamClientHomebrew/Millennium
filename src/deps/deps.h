#pragma once
#include <string>

namespace dependencies {
    bool audit_package(std::string common_name, std::string package_path, std::string remote_object);
    bool embed_python();
}