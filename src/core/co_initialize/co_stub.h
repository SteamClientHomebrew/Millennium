#pragma once
#include <sys/locals.h>

namespace CoInitializer
{
	const void InjectFrontendShims(void);
	const void ReInjectFrontendShims(void);
	const void BackendStartCallback(SettingsStore::PluginTypeSchema& plugin);
}