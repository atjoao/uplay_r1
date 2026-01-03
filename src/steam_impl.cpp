#include "pch.h"
#include "logging.h"
#include "uplay_config.h"
#include "steam_impl.h"
#include <cstdint>
#include <vector>
#include <winnt.h>

// steam api vars
static bool g_SteamApiInitialized = false;
static HMODULE g_SteamApiModule = nullptr;
static HMODULE g_SteamClientModule = nullptr;

static void* g_SteamUserStats = nullptr;
static bool g_SteamStatsReady = false;

// Getter functions for external access
bool IsSteamApiInitialized() { return g_SteamApiInitialized; }
bool IsSteamStatsReady() { return g_SteamStatsReady; }

// Steam API Function Pointers
typedef void* (__cdecl *SteamAPI_SteamUserStats_v012_t)();
typedef bool (__cdecl *SteamAPI_ISteamUserStats_RequestCurrentStats_t)(void*);
typedef bool (__cdecl *SteamAPI_ISteamUserStats_GetAchievement_t)(void*, const char*, bool*);
typedef bool (__cdecl *SteamAPI_ISteamUserStats_SetAchievement_t)(void*, const char*);
typedef bool (__cdecl *SteamAPI_ISteamUserStats_ClearAchievement_t)(void*, const char*);
typedef bool (__cdecl *SteamAPI_ISteamUserStats_StoreStats_t)(void*);
typedef uint32_t (__cdecl *SteamAPI_ISteamUserStats_GetNumAchievements_t)(void*);
typedef const char* (__cdecl *SteamAPI_ISteamUserStats_GetAchievementName_t)(void*, uint32_t);
typedef const char* (__cdecl *SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute_t)(void*, const char*, const char*);
typedef void (__cdecl *SteamAPI_RunCallbacks_t)();
static SteamAPI_RunCallbacks_t pfn_RunCallbacks = nullptr;

// Function pointer storage
static SteamAPI_SteamUserStats_v012_t pfn_SteamUserStats = nullptr;
static SteamAPI_ISteamUserStats_RequestCurrentStats_t pfn_RequestCurrentStats = nullptr;
static SteamAPI_ISteamUserStats_GetAchievement_t pfn_GetAchievement = nullptr;
static SteamAPI_ISteamUserStats_SetAchievement_t pfn_SetAchievement = nullptr;
static SteamAPI_ISteamUserStats_ClearAchievement_t pfn_ClearAchievement = nullptr;
static SteamAPI_ISteamUserStats_StoreStats_t pfn_StoreStats = nullptr;
static SteamAPI_ISteamUserStats_GetNumAchievements_t pfn_GetNumAchievements = nullptr;
static SteamAPI_ISteamUserStats_GetAchievementName_t pfn_GetAchievementName = nullptr;
static SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute_t pfn_GetAchievementDisplayAttribute = nullptr;

typedef int32_t HSteamUser;
typedef int32_t HSteamPipe;

typedef HSteamUser (__cdecl *SteamAPI_GetHSteamUser_t)();
typedef HSteamPipe (__cdecl *SteamAPI_GetHSteamPipe_t)();
typedef void* (__cdecl *SteamInternal_FindOrCreateUserInterface_t)(HSteamUser, const char*);

static SteamAPI_GetHSteamUser_t pfn_GetHSteamUser = nullptr;
static SteamAPI_GetHSteamPipe_t pfn_GetHSteamPipe = nullptr;
static SteamInternal_FindOrCreateUserInterface_t pfn_FindOrCreateUserInterface = nullptr;


// Forward declarations for new Steam functions
void InitSteamUserStats();
bool Steam_UnlockAchievement(const char* achievementId);
bool Steam_GetAchievement(const char* achievementId, bool* unlocked);
bool Steam_ClearAchievement(const char* achievementId);
uint32_t Steam_GetNumAchievements();
const char* Steam_GetAchievementName(uint32_t index);
const char* Steam_GetAchievementDisplayAttribute(const char* name, const char* key);

void InitSteamUserStats()
{
    if (! g_SteamApiInitialized || ! g_SteamApiModule) {
        LogWrite("[Steam] Cannot init UserStats - Steam API not initialized");
        return;
    }
    
    if (g_SteamUserStats) {
        LogWrite("[Steam] UserStats already initialized");
        return;
    }


    
    // Get helper functions
    pfn_GetHSteamUser = (SteamAPI_GetHSteamUser_t)GetProcAddress(g_SteamApiModule, "SteamAPI_GetHSteamUser");
    pfn_GetHSteamPipe = (SteamAPI_GetHSteamPipe_t)GetProcAddress(g_SteamApiModule, "SteamAPI_GetHSteamPipe");
    pfn_FindOrCreateUserInterface = (SteamInternal_FindOrCreateUserInterface_t)
        GetProcAddress(g_SteamApiModule, "SteamInternal_FindOrCreateUserInterface");
    
    LogWrite("[Steam] GetHSteamUser = 0x%p", pfn_GetHSteamUser);
    LogWrite("[Steam] GetHSteamPipe = 0x%p", pfn_GetHSteamPipe);
    LogWrite("[Steam] FindOrCreateUserInterface = 0x%p", pfn_FindOrCreateUserInterface);
    
    if (!pfn_GetHSteamUser || !pfn_GetHSteamPipe) {
        LogWrite("[Steam] Failed to get Steam handle functions");
        return;
    }
    
    HSteamUser hUser = pfn_GetHSteamUser();
    HSteamPipe hPipe = pfn_GetHSteamPipe();
    LogWrite("[Steam] HSteamUser = %d, HSteamPipe = %d", hUser, hPipe);
    
    if (hPipe == 0) {
        LogWrite("[Steam] Invalid pipe!");
        return;
    }
    
    // Get the interface the PROPER way - same as Steamworks.NET
    if (pfn_FindOrCreateUserInterface) {
        g_SteamUserStats = pfn_FindOrCreateUserInterface(hUser, "STEAMUSERSTATS_INTERFACE_VERSION013");
        LogWrite("[Steam] Got ISteamUserStats via FindOrCreateUserInterface:  0x%p", g_SteamUserStats);
    }
    
    // Fallback to global accessor if needed
    if (! g_SteamUserStats) {
        pfn_SteamUserStats = (SteamAPI_SteamUserStats_v012_t)GetProcAddress(g_SteamApiModule, "SteamAPI_SteamUserStats_v012");
        if (! pfn_SteamUserStats) {
            pfn_SteamUserStats = (SteamAPI_SteamUserStats_v012_t)GetProcAddress(g_SteamApiModule, "SteamAPI_SteamUserStats");
        }
        if (pfn_SteamUserStats) {
            g_SteamUserStats = pfn_SteamUserStats();
            LogWrite("[Steam] Got ISteamUserStats via global accessor: 0x%p", g_SteamUserStats);
        }
    }
    
    if (!g_SteamUserStats) {
        LogWrite("[Steam] Failed to get ISteamUserStats interface!");
        return;
    }
    
    // Get flat API function pointers
    pfn_RequestCurrentStats = (SteamAPI_ISteamUserStats_RequestCurrentStats_t)
        GetProcAddress(g_SteamApiModule, "SteamAPI_ISteamUserStats_RequestCurrentStats");
    pfn_GetAchievement = (SteamAPI_ISteamUserStats_GetAchievement_t)
        GetProcAddress(g_SteamApiModule, "SteamAPI_ISteamUserStats_GetAchievement");
    pfn_SetAchievement = (SteamAPI_ISteamUserStats_SetAchievement_t)
        GetProcAddress(g_SteamApiModule, "SteamAPI_ISteamUserStats_SetAchievement");
    pfn_ClearAchievement = (SteamAPI_ISteamUserStats_ClearAchievement_t)
        GetProcAddress(g_SteamApiModule, "SteamAPI_ISteamUserStats_ClearAchievement");
    pfn_StoreStats = (SteamAPI_ISteamUserStats_StoreStats_t)
        GetProcAddress(g_SteamApiModule, "SteamAPI_ISteamUserStats_StoreStats");
    pfn_GetNumAchievements = (SteamAPI_ISteamUserStats_GetNumAchievements_t)
        GetProcAddress(g_SteamApiModule, "SteamAPI_ISteamUserStats_GetNumAchievements");
    pfn_GetAchievementName = (SteamAPI_ISteamUserStats_GetAchievementName_t)
        GetProcAddress(g_SteamApiModule, "SteamAPI_ISteamUserStats_GetAchievementName");
    pfn_GetAchievementDisplayAttribute = (SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute_t)
        GetProcAddress(g_SteamApiModule, "SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute");
    pfn_RunCallbacks = (SteamAPI_RunCallbacks_t)
        GetProcAddress(g_SteamApiModule, "SteamAPI_RunCallbacks");
    
    LogWrite("[Steam] Function pointers loaded:");
    LogWrite("[Steam]   RequestCurrentStats: 0x%p", pfn_RequestCurrentStats);
    LogWrite("[Steam]   GetAchievement:  0x%p", pfn_GetAchievement);
    LogWrite("[Steam]   SetAchievement: 0x%p", pfn_SetAchievement);
    LogWrite("[Steam]   StoreStats: 0x%p", pfn_StoreStats);
    LogWrite("[Steam]   GetNumAchievements: 0x%p", pfn_GetNumAchievements);
    LogWrite("[Steam]   RunCallbacks: 0x%p", pfn_RunCallbacks);

    g_SteamStatsReady = true;

}

bool Steam_UnlockAchievement(const char* achievementId)
{
    if (! g_SteamUserStats || !pfn_SetAchievement || !pfn_StoreStats) {
        LogWrite("[Steam] Cannot unlock - interface not ready");
        return false;
    }
    
    LogWrite("[Steam] Unlocking:  %s", achievementId);
    bool testUnlocked = false;
    if (pfn_GetAchievement && g_SteamUserStats) {
        bool canRead = pfn_GetAchievement(g_SteamUserStats, achievementId, &testUnlocked);
        LogWrite("[Steam] GetAchievement(%s) = %s, unlocked=%s", 
                 achievementId, canRead ? "OK" : "FAIL", testUnlocked ? "true" :  "false");
    }
    
    bool result = pfn_SetAchievement(g_SteamUserStats, achievementId);
    if (! result) {
        LogWrite("[Steam] SetAchievement failed:  %s", achievementId);
        return false;
    }
    
    result = pfn_StoreStats(g_SteamUserStats);
    if (!result) {
        LogWrite("[Steam] StoreStats failed");
        return false;
    }
    
    LogWrite("[Steam] Unlocked: %s", achievementId);
    pfn_StoreStats(g_SteamUserStats);
    return true;
}

bool Steam_GetAchievement(const char* achievementId, bool* unlocked)
{
    if (!g_SteamUserStats || !pfn_GetAchievement) {
        return false;
    }
    return pfn_GetAchievement(g_SteamUserStats, achievementId, unlocked);
}

bool Steam_ClearAchievement(const char* achievementId)
{
    if (! g_SteamUserStats || !pfn_ClearAchievement || !pfn_StoreStats) {
        return false;
    }
    
    bool result = pfn_ClearAchievement(g_SteamUserStats, achievementId);
    if (result) {
        pfn_StoreStats(g_SteamUserStats);
    }
    return result;
}

uint32_t Steam_GetNumAchievements()
{
    if (!g_SteamUserStats || !pfn_GetNumAchievements) {
        return 0;
    }
    return pfn_GetNumAchievements(g_SteamUserStats);
}

const char* Steam_GetAchievementName(uint32_t index)
{
    if (!g_SteamUserStats || ! pfn_GetAchievementName) {
        return nullptr;
    }
    return pfn_GetAchievementName(g_SteamUserStats, index);
}

const char* Steam_GetAchievementDisplayAttribute(const char* name, const char* key)
{
    if (!g_SteamUserStats || !pfn_GetAchievementDisplayAttribute) {
        return nullptr;
    }
    return pfn_GetAchievementDisplayAttribute(g_SteamUserStats, name, key);
}

void InitSteamApi() {
    if (g_SteamApiInitialized) return;
    
    #ifdef _WIN64
        const char* steamApiName = "steam_api64.dll";
		const char* steamClientName = "steamclient64.dll";
    #else
        const char* steamApiName = "steam_api.dll";
		const char* steamClientName = "steamclient.dll";
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
	
	if (GetEnvironmentVariableA("SteamAppId", envBuffer, sizeof(envBuffer)) == 0) {
		SetEnvironmentVariableA("SteamAppId", appIdStr);
		LogWrite("[Uplay Emu] Set SteamAppId=%s", appIdStr);
	} else {
		LogWrite("[Uplay Emu] SteamAppId already set: %s", envBuffer);
	}
	
	if (GetEnvironmentVariableA("SteamGameId", envBuffer, sizeof(envBuffer)) == 0) {
		SetEnvironmentVariableA("SteamGameId", appIdStr);
		LogWrite("[Uplay Emu] Set SteamGameId=%s", appIdStr);
	} else {
		LogWrite("[Uplay Emu] SteamGameId already set: %s", envBuffer);
	}
    
    typedef int (__cdecl *SteamAPI_InitFlat_t)(char* pszErrMsg);
    typedef bool (__cdecl *SteamAPI_Init_t)();
    
    SteamAPI_InitFlat_t SteamAPI_InitFlat = (SteamAPI_InitFlat_t)GetProcAddress(g_SteamApiModule, "SteamAPI_InitFlat");
    SteamAPI_Init_t SteamAPI_Init = (SteamAPI_Init_t)GetProcAddress(g_SteamApiModule, "SteamAPI_Init");
    
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
    typedef void (__cdecl *SteamAPI_ManualDispatch_Init_t)();
    SteamAPI_ManualDispatch_Init_t SteamAPI_ManualDispatch_Init = 
        (SteamAPI_ManualDispatch_Init_t)GetProcAddress(g_SteamApiModule, "SteamAPI_ManualDispatch_Init");
    if (SteamAPI_ManualDispatch_Init) {
        LogWrite("[Uplay Emu] Calling SteamAPI_ManualDispatch_Init");
        SteamAPI_ManualDispatch_Init();
    }

    LogWrite("[Uplay Emu] Steam API initialized successfully");

	// once loaded, get steam exports to use later...
    g_SteamApiInitialized = true;
    InitSteamUserStats();
	
}

void RunCallbacks() {
    if (pfn_RunCallbacks) {
        pfn_RunCallbacks();
    }
}

// ============================================================================
// Achievement ID to Name Mapping
// ============================================================================

struct AchievementMapping {
    DWORD uplayId;
    char steamName[128];
};

static std::vector<AchievementMapping> g_AchievementMappings;
static bool g_AchievementMappingsLoaded = false;

// Extract numeric ID from achievement name (e.g., "ACH_42" -> 42, "achievement_15" -> 15)
DWORD ExtractIdFromName(const char* name)
{
    if (!name || !*name) return 0;
    
    const char* p = name;
    const char* lastNumStart = nullptr;
    
    // Find the last sequence of digits in the string
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            if (! lastNumStart) {
                lastNumStart = p;
            }
        } else {
            lastNumStart = nullptr;
        }
        p++;
    }
    
    // If we found digits at the end or somewhere in the string
    if (lastNumStart) {
        return strtoul(lastNumStart, NULL, 10);
    }
    
    // Try to find any number in the string
    p = name;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            return strtoul(p, NULL, 10);
        }
        p++;
    }
    
    return 0;
}

void LoadAchievementMappings()
{
    if (g_AchievementMappingsLoaded) return;
    g_AchievementMappingsLoaded = true;
    
    // Try to load mappings from achievements.ini
    CHAR INI[MAX_PATH] = {0};
    GetModuleFileNameA(UplayModule, INI, MAX_PATH);
    char* p = strrchr(INI, '\\');
    if (p) strcpy(p + 1, "achievements.ini");
    else strcpy(INI, "achievements.ini");
    
    if (GetFileAttributesA(INI) != INVALID_FILE_ATTRIBUTES) {
        // Read mappings from [Mappings] section
        // Format:  1=ACHIEVEMENT_NAME
        char buffer[4096] = {0};
        GetPrivateProfileSectionA("Mapping", buffer, sizeof(buffer), INI);
        
        char* entry = buffer;
        while (*entry) {
            char* equals = strchr(entry, '=');
            if (equals) {
                *equals = 0;
                DWORD uplayId = strtoul(entry, NULL, 10);
                const char* steamName = equals + 1;
                
                AchievementMapping mapping;
                mapping.uplayId = uplayId;
                strncpy(mapping.steamName, steamName, sizeof(mapping.steamName) - 1);
                mapping.steamName[sizeof(mapping. steamName) - 1] = 0;
                
                g_AchievementMappings. push_back(mapping);
                LogWrite("[Steam] Loaded mapping: %lu -> %s", uplayId, steamName);
            }
            entry += strlen(entry) + 1;
        }
    }
    
    // Auto-generate from Steam if no mappings and Steam is ready
    if (g_AchievementMappings.empty() && g_SteamStatsReady) {
        uint32_t numAch = Steam_GetNumAchievements();
        LogWrite("[Steam] Auto-generating %u achievement mappings", numAch);
        
        for (uint32_t i = 0; i < numAch; i++) {
            const char* name = Steam_GetAchievementName(i);
            if (name) {
                AchievementMapping mapping;
                
                // Try to extract ID from the achievement name first
                DWORD extractedId = ExtractIdFromName(name);
                
                if (extractedId > 0) {
                    // Use the ID found in the name
                    mapping.uplayId = extractedId;
                    LogWrite("[Steam] Extracted ID %lu from name: %s", extractedId, name);
                } else {
                    // Fallback to 1-based index
                    mapping. uplayId = i + 1;
                    LogWrite("[Steam] No ID in name, using index:  %lu for %s", mapping. uplayId, name);
                }
                
                strncpy(mapping.steamName, name, sizeof(mapping.steamName) - 1);
                mapping.steamName[sizeof(mapping.steamName) - 1] = 0;
                
                g_AchievementMappings.push_back(mapping);
                LogWrite("[Steam] Auto-mapped:  %lu -> %s", mapping.uplayId, name);
            }
        }
        
        // Save mappings to INI for future reference/editing
        if (!g_AchievementMappings.empty()) {
            for (const auto& m : g_AchievementMappings) {
                char key[32];
                sprintf(key, "%lu", m. uplayId);
                WritePrivateProfileStringA("Mappings", key, m.steamName, INI);
            }
            LogWrite("[Steam] Saved %zu mappings to %s", g_AchievementMappings.size(), INI);
        }
    }
}

const char* GetSteamAchievementName(DWORD uplayId)
{
    LoadAchievementMappings();
    
    for (const auto& m :  g_AchievementMappings) {
        if (m.uplayId == uplayId) {
            return m.steamName;
        }
    }
    
    // Fallback:  construct a name with the ID
    static char fallbackName[32];
    sprintf(fallbackName, "ACH_%lu", uplayId);
    return fallbackName;
}

DWORD GetUplayAchievementId(const char* steamName)
{
    LoadAchievementMappings();
    
    // First check existing mappings
    for (const auto& m : g_AchievementMappings) {
        if (_stricmp(m.steamName, steamName) == 0) {
            return m.uplayId;
        }
    }
    
    // If not found in mappings, try to extract from name
    DWORD extractedId = ExtractIdFromName(steamName);
    if (extractedId > 0) {
        return extractedId;
    }
    
    return 0;
}