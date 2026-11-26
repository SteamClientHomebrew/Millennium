#pragma once
// Foundation and AppKit services for the `millennium` command.
#include <sys/types.h>

// Call on_launch for every running app with this bundle id, then for each one
// that launches later. Never returns; runs the main run loop.
void watch_app_launches(const char* bundle_id, void (*on_launch)(pid_t pid));

// Write the autostart LaunchAgent: `program watch`, kept alive in the GUI session.
int write_agent_plist(const char* path, const char* label, const char* program);

// Write an entitlements file granting com.apple.security.cs.debugger.
int write_debugger_entitlements(const char* path);
