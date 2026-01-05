#include "steam_impl.h"
#include "logging.h"
#include "uplay_config.h"
#include <winnt.h>

static bool g_SteamApiInitialized = false;
static HMODULE g_SteamApiModule = nullptr;
static HMODULE g_SteamClientModule = nullptr;

static void *g_SteamUserStats = nullptr;
static bool g_SteamStatsReady = false;

bool IsSteamApiInitialized() { return g_SteamApiInitialized; }
bool IsSteamStatsReady() { return g_SteamStatsReady; }

void InitSteamApi() {
  if (g_SteamApiInitialized)
    return;

#ifdef _WIN64
  const char *steamApiName = "steam_api64.dll";
  const char *steamClientName = "steamclient64.dll";
#else
  const char *steamApiName = "steam_api.dll";
  const char *steamClientName = "steamclient.dll";
#endif

  g_SteamApiModule = GetModuleHandleA(steamApiName);
  g_SteamClientModule = GetModuleHandleA(steamClientName);
  if (g_SteamApiModule || g_SteamClientModule) {
    LogWrite("[Uplay Emu] steam_api already loaded at 0x%p", g_SteamApiModule);
  }

  if (!g_SteamApiModule) {
    g_SteamApiModule = LoadLibraryA(steamApiName);
    g_SteamClientModule = LoadLibraryA(steamClientName);
    if (g_SteamApiModule && g_SteamClientModule) {
      LogWrite("[Uplay Emu] Loaded steam_api globally:  %s", steamApiName);
    }
  }

  // Set SteamAppId and SteamGameId environment variables if not already set
  char envBuffer[64] = {0};
  char appIdStr[32] = {0};
  sprintf(appIdStr, "%u", Uplay_Configuration::steamId);

  if (GetEnvironmentVariableA("SteamAppId", envBuffer, sizeof(envBuffer)) ==
      0) {
    SetEnvironmentVariableA("SteamAppId", appIdStr);
    LogWrite("[Uplay Emu] Set SteamAppId=%s", appIdStr);
  } else {
    LogWrite("[Uplay Emu] SteamAppId already set: %s", envBuffer);
  }

  if (GetEnvironmentVariableA("SteamGameId", envBuffer, sizeof(envBuffer)) ==
      0) {
    SetEnvironmentVariableA("SteamGameId", appIdStr);
    LogWrite("[Uplay Emu] Set SteamGameId=%s", appIdStr);
  } else {
    LogWrite("[Uplay Emu] SteamGameId already set: %s", envBuffer);
  }

  typedef int(__cdecl * SteamAPI_InitFlat_t)(char *pszErrMsg);
  typedef bool(__cdecl * SteamAPI_Init_t)();

  SteamAPI_InitFlat_t SteamAPI_InitFlat = (SteamAPI_InitFlat_t)GetProcAddress(
      g_SteamApiModule, "SteamAPI_InitFlat");
  SteamAPI_Init_t SteamAPI_Init =
      (SteamAPI_Init_t)GetProcAddress(g_SteamApiModule, "SteamAPI_Init");

  bool initSuccess = false;

  if (SteamAPI_InitFlat) {
    LogWrite("[Uplay Emu] Found init function:  SteamAPI_InitFlat");
    char errBuffer[1024] = {0};
    int res = SteamAPI_InitFlat(errBuffer);
    if (res == 0) {
      LogWrite("[Uplay Emu] SteamAPI_InitFlat() succeeded");
      initSuccess = true;
    } else {
      LogWrite("[Uplay Emu] SteamAPI_InitFlat() failed with error %d:  %s", res,
               errBuffer[0] ? errBuffer : "(no message)");
    }
  }

  // Manual dispatch init (optional)
  typedef void(__cdecl * SteamAPI_ManualDispatch_Init_t)();
  SteamAPI_ManualDispatch_Init_t SteamAPI_ManualDispatch_Init =
      (SteamAPI_ManualDispatch_Init_t)GetProcAddress(
          g_SteamApiModule, "SteamAPI_ManualDispatch_Init");
  if (SteamAPI_ManualDispatch_Init) {
    LogWrite("[Uplay Emu] Calling SteamAPI_ManualDispatch_Init");
    SteamAPI_ManualDispatch_Init();
  }

  LogWrite("[Uplay Emu] Steam API initialized successfully");

  g_SteamApiInitialized = true;
}