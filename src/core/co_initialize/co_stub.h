#pragma once
#include <locals/locals.h>

const void InjectFrontendShims(void);

const void ReInjectFrontendShims(void);

void BackendStartCallback(SettingsStore::PluginTypeSchema& plugin);
