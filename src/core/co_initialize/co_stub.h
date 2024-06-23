#pragma once
#include <sys/locals.h>

namespace CoInitializer
{
	const void InjectFrontendShims(uint16_t ftpPort = 0, uint16_t ipcPort = 0);
	const void ReInjectFrontendShims(void);
	const void BackendStartCallback(SettingsStore::PluginTypeSchema plugin);
}