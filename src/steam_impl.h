#ifndef STEAM_IMPL_H
#define STEAM_IMPL_H

#include <cstdint>
#include <windows.h>

void InitSteamApi();
bool IsSteamApiInitialized();
bool IsSteamStatsReady();

#endif // STEAM_IMPL_H
