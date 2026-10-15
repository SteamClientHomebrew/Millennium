#pragma once

#define CONSTRUCTOR __attribute__((constructor))
#define DESTRUCTOR __attribute__((destructor))

namespace platform::environment
{
inline void setup()
{
}
} // namespace platform::environment
