#pragma once
#include <sys/types.h>

// Call on_launch for every running app with this bundle id, then for each one
// that launches later. Never returns; runs the main run loop.
void watch_app_launches(const char* bundle_id, void (*on_launch)(pid_t pid));
