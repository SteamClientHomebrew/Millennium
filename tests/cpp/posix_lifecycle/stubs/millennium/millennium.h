#pragma once

#include <memory>

// Keep the real POSIX thread management without starting Steam hooks or plugins.
class millennium
{
  public:
    void entry();
};

extern std::unique_ptr<millennium> g_millennium;
