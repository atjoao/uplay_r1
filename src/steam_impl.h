#ifndef STEAM_IMPL_H
#define STEAM_IMPL_H

#include <cstdint>
#include <windows.h>

// Initialize Steam API (loads DLL, calls SteamAPI_Init)
void InitSteamApi();

// Initialize Steam UserStats interface for achievements
void InitSteamUserStats();

// Check if Steam API is initialized and ready
bool IsSteamApiInitialized();
bool IsSteamStatsReady();

// Achievement functions
bool Steam_UnlockAchievement(const char* achievementId);
bool Steam_GetAchievement(const char* achievementId, bool* unlocked);
bool Steam_ClearAchievement(const char* achievementId);
uint32_t Steam_GetNumAchievements();
const char* Steam_GetAchievementName(uint32_t index);
const char* Steam_GetAchievementDisplayAttribute(const char* name, const char* key);
void RunCallbacks();

// Achievement mapping (Uplay ID <-> Steam Name)
void LoadAchievementMappings();
const char* GetSteamAchievementName(DWORD uplayId);
DWORD GetUplayAchievementId(const char* steamName);

#endif // STEAM_IMPL_H
