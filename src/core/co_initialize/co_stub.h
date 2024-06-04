#pragma once
#include <generic/stream_parser.h>

const void InjectFrontendShims(void);

const void ReInjectFrontendShims(void);

void BackendStartCallback(SettingsStore::PluginTypeSchema& plugin);
