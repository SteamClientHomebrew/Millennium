#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*SteamWatchDogCallback)();

void* SteamWatchDog_Start(SteamWatchDogCallback onLaunch, SteamWatchDogCallback onQuit);
void  SteamWatchDog_Stop(void* instance);

#ifdef __cplusplus
}
#endif
