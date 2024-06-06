#pragma once
#include <sys/locals.h>

const void InjectFrontendShims(void);

const void ReInjectFrontendShims(void);

void BackendStartCallback(SettingsStore::PluginTypeSchema& plugin);
