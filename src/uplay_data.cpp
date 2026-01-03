#include "pch.h"
#include <vector>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <cstdarg>
#include <shlobj.h>

#define UPLAY_EXPORT extern "C" __declspec(dllexport)

static FILE* g_LogFile = NULL;
static FILE* g_ConsoleOut = NULL;
static bool g_LogInitialized = false;
static bool g_LoggingEnabled = false;
static bool g_ConsoleEnabled = false;
static char g_LogPath[MAX_PATH] = {0};

void ReadLoggingConfig()
{
	CHAR INI[MAX_PATH] = {0};
	GetModuleFileNameA(UplayModule, INI, MAX_PATH);
	char* p = strrchr(INI, '\\');
	if (p) strcpy(p + 1, "Uplay.ini");
	else strcpy(INI, "Uplay.ini");
	
	if (GetFileAttributesA(INI) != INVALID_FILE_ATTRIBUTES) {
		g_LoggingEnabled = GetPrivateProfileIntA("Uplay", "Logging", 0, INI) == TRUE;
		g_ConsoleEnabled = GetPrivateProfileIntA("Uplay", "EnableConsole", 0, INI) == TRUE;
	}
}

void InitConsole()
{
	if (!g_ConsoleEnabled || g_ConsoleOut) return;
	
	if (AllocConsole()) {
		SetConsoleTitleA("Uplay Emu Console");
		
		freopen_s(&g_ConsoleOut, "CONOUT$", "w", stdout);
		
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	}
}

void InitLog()
{
	if (!g_LogInitialized)
	{
		g_LogInitialized = true;
		
		if (!g_LoggingEnabled && !g_ConsoleEnabled) {
			ReadLoggingConfig();
		}
		
		if (g_ConsoleEnabled) {
			InitConsole();
		}
		
		if (!g_LoggingEnabled) return;
		
		GetModuleFileNameA(UplayModule, g_LogPath, MAX_PATH);
		char* p = strrchr(g_LogPath, '\\');
		if (p) strcpy(p + 1, "uplay_emu.log");
		else strcpy(g_LogPath, "uplay_emu.log");
		
		g_LogFile = fopen(g_LogPath, "w+");
		if (g_LogFile) {
			time_t now = time(NULL);
			struct tm* t = localtime(&now);
			fflush(g_LogFile);
		}
	}
}

void LogWrite(const char* format, ...)
{
	if (!g_LogFile && !g_ConsoleOut) return;
	
	// Timestamp
	time_t now = time(NULL);
	struct tm* t = localtime(&now);
	char timestamp[32];
	sprintf(timestamp, "[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
	
	// Format message
	char buffer[2048];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	if (g_LogFile) {
		fprintf(g_LogFile, "%s%s\n", timestamp, buffer);
		fflush(g_LogFile);
	}
	
	if (g_ConsoleOut) {
		printf("%s%s\n", timestamp, buffer);
	}
}

#define LOG_FUNC() do { InitLog(); if (g_LogFile || g_ConsoleOut) LogWrite("[Uplay Emu] %s called", __FUNCTION__); } while(0)

#define SAVE_HEADER_SIZE 552

struct SaveSlot {
    DWORD mode;           // 0 = read, 1 = write
    DWORD slotId;
    HANDLE fileHandle;
    char saveName[512];
    bool inUse;
};

static SaveSlot g_SaveSlots[256] = {0};
static char g_SavePath[MAX_PATH] = {0};
static bool g_SavePathInit = false;

// Achievement system
static char g_AchievementPath[MAX_PATH] = {0};
static bool g_AchievementPathInit = false;

#pragma pack(push, 8)
struct UPLAY_ACH_Achievement
{
    uint32_t id;
    const char* nameUtf8;
    const char* descriptionUtf8;
    bool earned;
};
#pragma pack(pop)

#ifdef _WIN64
	#pragma pack(push, 8)
	struct SaveGameEntry {
		uint64_t id;
		char* nameUtf8;
		uint64_t size;
	};

	struct SaveListHeader {
		uint64_t count;
		void** entries;
	};
	#pragma pack(pop)
#else
	#pragma pack(push, 4)
	struct SaveGameEntry {
		uint32_t id;            // 4 bytes - slot ID
		char* nameUtf8;         // 4 bytes on x32 - name string
		uint32_t size;          // 4 bytes - file size
	};  // Total: 12 bytes

	struct SaveListHeader {
		uint32_t count;         // 4 bytes
		void** entries;         // 4 bytes on x32
	};  // Total: 8 bytes on x32
	#pragma pack(pop)
#endif

struct AchievementListHeader {
    ULONG_PTR count;
    UPLAY_ACH_Achievement* achievements;
};

// Forward declarations for achievement functions
void InitAchievementPath(const char* userName, DWORD appId);
void GetAchievementFilePath(DWORD achId, char* outPath);
bool ReadAchievement(DWORD achId, char* outName, char* outDesc, bool* outEarned);
bool WriteAchievement(DWORD achId, const char* name, const char* desc, bool earned);
bool UnlockAchievement(DWORD achId);
std::vector<DWORD> GetAllAchievementIds();
// Forward declaration - will be set in UPLAY_Start
void InitSavePath(const char* userName, DWORD appId);
void GetSaveFilePath(DWORD slotId, char* outPath);
void InitAchievementPath(const char* userName, DWORD appId);
void GetAchievementFilePath(DWORD achId, char* outPath);

HANDLE fileuplay = 0;
void* DirectoryBuffer = 0;

bool created = false;

int val = 0;

struct Chunks
{
	ULONG_PTR d1;
	void* d2;
};
struct Overmapped
{
	signed char pd[4];
	int32_t f4;
	int32_t f8;
};
struct FileOpen
{
	DWORD d1;
};
struct FileOpenTwo
{
	DWORD addr1;
	DWORD addr2;
	DWORD addr3;
};
struct FileRead
{
	DWORD addr1;
	DWORD addr2;
	DWORD addr3;
};
struct FileList
{
	DWORD num;
	void* bufferstring;
	DWORD pointer;
};
struct MyFileRef
{
	HANDLE hFile;
	int numfile;
	char nameoffile[4024];
};
std::vector<MyFileRef> FileVector;

namespace Uplay_Configuration
{
	char UserName[0x200] = { "Rat" };
	char UserEmail[0x200] = { "UplayEmu@rat43.com" };
	char password[0x200] = { "UplayPassword74" };
	char GameLanguage[0x200] = { "en-US" };
	char UserId[1024] = { "c91c91c9-1c91-c91c-91c9-1c91c91c91c9" };
	char CdKey[1024] = { "1111-2222-3333-4444" };
	char TickedId[1024] = { "noT456umPqRt" };
	bool Offline = false;
	bool appowned = true;
	bool logging = false;
	bool friends = false;
	bool party = false;

	int cdkey1 = 0;
	int cdkey2 = 0;
	uint32_t gameAppId = 0;
}

DWORD    GetFilePointer(HANDLE hFile) {
	return SetFilePointer(hFile, 0, NULL, FILE_CURRENT);
}
void CreatePath(const char* Path)
{
	if (!created)
	{
		CHAR data[MAX_PATH] = { 0 };
		lstrcpyA(&data[0], Path);
		int size = lstrlenA(data) + 1;
		CHAR out[MAX_PATH] = { 0 };
		int i = 0;
		for (;;)
		{
			if (!data[i]) break;

			memcpy(&out[i], &data[i], 1);
			if (out[i] == '\\')
			{
				if (GetFileAttributesA(out) == INVALID_FILE_ATTRIBUTES)
				{
					CreateDirectoryA(out, NULL);
				}
			}
			i++;
		}
		if (GetFileAttributesA(out) == INVALID_FILE_ATTRIBUTES)
		{
			CreateDirectoryA(out, NULL);
		}
		created = true;
	}
	return;
}
static char datapp[1024];
const char* AttachDirFile(const char* Path, const char* file)
{
	memset(datapp, 0, sizeof(datapp));
	sprintf_s(datapp, sizeof(datapp), "%s\\%s", Path, file);
	return datapp;
}
bool IsTargetExist(LPCSTR path)
{
	if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
		return false;
	return true;
}

UPLAY_EXPORT int UPLAY_ACH_EarnAchievement(DWORD achievementId, void* overlapped)
{
	LOG_FUNC();
	LogWrite("[Uplay Emu] EarnAchievement: id=%lu", achievementId);
	
	UnlockAchievement(achievementId);
	
	// Set overlapped result
	if (overlapped) {
		FileRead* ovr = (FileRead*)overlapped;
		ovr->addr1++;
		ovr->addr2 = 1;
		ovr->addr3 = 0;
	}
	
	return 1;
}
UPLAY_EXPORT int UPLAY_ACH_GetAchievementImage()
{
	LOG_FUNC();
	return 0;
}

// Static storage for achievement list to keep strings alive
static std::vector<char*> g_AchievementStrings;
static UPLAY_ACH_Achievement* g_AchievementList = nullptr;
static AchievementListHeader* g_AchievementListHeader = nullptr;

UPLAY_EXPORT int UPLAY_ACH_GetAchievements(DWORD filter, const char* accountIdUtf8, void* outAchievementList, void* overlapped)
{
	LOG_FUNC();
	LogWrite("[Uplay Emu] GetAchievements: filter=%lu", filter);
	
	// Get all achievement IDs
	std::vector<DWORD> achIds = GetAllAchievementIds();
	DWORD count = (DWORD)achIds.size();
	
	LogWrite("[Uplay Emu] Found %lu achievements", count);
	
	// Allocate list header
	g_AchievementListHeader = (AchievementListHeader*)VirtualAlloc(NULL, sizeof(AchievementListHeader), MEM_COMMIT, PAGE_READWRITE);
	g_AchievementListHeader->count = count;
	
	if (count > 0) {
		// Allocate achievements array
		g_AchievementList = (UPLAY_ACH_Achievement*)VirtualAlloc(NULL, sizeof(UPLAY_ACH_Achievement) * count, MEM_COMMIT, PAGE_READWRITE);
		g_AchievementListHeader->achievements = g_AchievementList;
		
		// Clear old strings
		for (char* str : g_AchievementStrings) {
			VirtualFree(str, 0, MEM_RELEASE);
		}
		g_AchievementStrings.clear();
		
		// Fill achievements
		for (DWORD i = 0; i < count; i++) {
			DWORD achId = achIds[i];
			
			char name[256] = {0};
			char desc[512] = {0};
			bool earned = false;
			
			ReadAchievement(achId, name, desc, &earned);
			
			// Allocate and copy strings
			char* nameCopy = (char*)VirtualAlloc(NULL, 256, MEM_COMMIT, PAGE_READWRITE);
			char* descCopy = (char*)VirtualAlloc(NULL, 512, MEM_COMMIT, PAGE_READWRITE);
			strcpy(nameCopy, name);
			strcpy(descCopy, desc);
			g_AchievementStrings.push_back(nameCopy);
			g_AchievementStrings.push_back(descCopy);
			
			g_AchievementList[i].id = achId;
			g_AchievementList[i].nameUtf8 = nameCopy;
			g_AchievementList[i].descriptionUtf8 = descCopy;
			g_AchievementList[i].earned = earned;
			
			LogWrite("[Uplay Emu] Achievement %lu: %s (earned=%d)", achId, name, earned);
		}
	} else {
		g_AchievementListHeader->achievements = nullptr;
	}
	
	// Write list pointer to output
	memcpy(outAchievementList, &g_AchievementListHeader, sizeof(void*));
	
	// Set overlapped result
	if (overlapped) {
		FileRead* ovr = (FileRead*)overlapped;
		ovr->addr1++;
		ovr->addr2 = 1;
		ovr->addr3 = 0;
	}
	
	return 1;
}
UPLAY_EXPORT int UPLAY_ACH_ReleaseAchievementImage()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_ACH_ReleaseAchievementList(void* list)
{
	LOG_FUNC();
	
	// Free strings
	for (char* str : g_AchievementStrings) {
		VirtualFree(str, 0, MEM_RELEASE);
	}
	g_AchievementStrings.clear();
	
	// Free achievement array
	if (g_AchievementList) {
		VirtualFree(g_AchievementList, 0, MEM_RELEASE);
		g_AchievementList = nullptr;
	}
	
	// Free header
	if (g_AchievementListHeader) {
		VirtualFree(g_AchievementListHeader, 0, MEM_RELEASE);
		g_AchievementListHeader = nullptr;
	}
	
	return 1;
}
UPLAY_EXPORT int UPLAY_ACH_Write()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_AVATAR_Get(void* buf1)
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_AVATAR_GetAvatarIdForCurrentUser()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_AVATAR_GetBitmap()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_AVATAR_Release()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_ClearGameSession()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_FRIENDS_AddPlayedWith()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_FRIENDS_AddToBlackList()
{
	LOG_FUNC();
	return 0;
}

UPLAY_EXPORT int UPLAY_FRIENDS_DisableFriendMenuItem()
{
	LOG_FUNC();
	return 0;
}

UPLAY_EXPORT int UPLAY_FRIENDS_EnableFriendMenuItem()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_FRIENDS_GetFriendList()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_FRIENDS_Init()
{
	LOG_FUNC();
	return Uplay_Configuration::friends;
}
UPLAY_EXPORT int UPLAY_FRIENDS_InviteToGame()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_FRIENDS_IsBlackListed()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_FRIENDS_IsFriend()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_FRIENDS_RemoveFriendship()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_FRIENDS_RemoveFromBlackList()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_FRIENDS_RequestFriendship()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_FRIENDS_RespondToGameInvite()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_FRIENDS_ShowFriendSelectionUI()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_FRIENDS_ShowInviteFriendsToGameUI()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_GetLastError()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_GetNextEvent()
{
	return 0;
}
UPLAY_EXPORT int UPLAY_GetOverlappedOperationResult(void* buf1, int* buf2)
{
	LOG_FUNC();
	Overmapped* ovr = (Overmapped*)buf1;
	if (!ovr->f4) {
		return 0;
	}
	else {
		*buf2 = ovr->f8;
		return 1;
	}
}
UPLAY_EXPORT int UPLAY_HasOverlappedOperationCompleted(void* buf1)
{
	LOG_FUNC();
	Overmapped* ovr = (Overmapped*)buf1;
	return ovr->f4;
}
UPLAY_EXPORT int UPLAY_INSTALLER_AreChunksInstalled(void* chunkIds, int chunkCount, void* outResult)
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_INSTALLER_GetChunkIdsFromTag()
{
	LOG_FUNC();
	return 0;
}
Chunks* chunks = 0;
UPLAY_EXPORT int UPLAY_INSTALLER_GetChunks(void* buf1)
{
	LOG_FUNC();
	chunks = (Chunks*)VirtualAlloc(0, 10, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	chunks->d1 = 1;
	chunks->d2 = VirtualAlloc(0, 1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#ifdef _WIN64
	memcpy(buf1, &chunks, 8);
#else
	memcpy(buf1, &chunks, 4);
#endif 
	return 1;

}
UPLAY_EXPORT const char* UPLAY_INSTALLER_GetLanguageUtf8()
{
	LOG_FUNC();
	return Uplay_Configuration::GameLanguage;
}
UPLAY_EXPORT int UPLAY_INSTALLER_Init()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_INSTALLER_ReleaseChunkIdList()
{
	LOG_FUNC();
	VirtualFree((void*)chunks, 0, MEM_DECOMMIT);
	return 1;
}
UPLAY_EXPORT int UPLAY_INSTALLER_UpdateInstallOrder()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_Init()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_METADATA_ClearContinuousTag()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_METADATA_SetContinuousTag()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_METADATA_SetSingleEventTag()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_OPTIONS_Apply()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_OPTIONS_Close()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_OPTIONS_Enumerate()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_OPTIONS_Get()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_OPTIONS_Open()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_OPTIONS_ReleaseKeyValueList()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_OPTIONS_Set()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_OPTIONS_SetInGameState()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_OVERLAY_SetShopUrl()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_OVERLAY_Show()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_OVERLAY_ShowBrowserUrl()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_OVERLAY_ShowFacebookAuthentication()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_OVERLAY_ShowNotification()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_OVERLAY_ShowShopUrl()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PARTY_DisablePartyMemberMenuItem()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PARTY_EnablePartyMemberMenuItem()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PARTY_GetFullMemberList()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PARTY_GetId()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PARTY_GetInGameMemberList()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PARTY_Init()
{
	LOG_FUNC();
	return Uplay_Configuration::party;
}
UPLAY_EXPORT int UPLAY_PARTY_InvitePartyToGame()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PARTY_InviteToParty()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PARTY_IsInParty()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PARTY_IsPartyLeader()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PARTY_PromoteToLeader()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PARTY_RespondToGameInvite()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PARTY_SetGuest()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PARTY_SetUserData()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_PARTY_ShowGameInviteOverlayUI()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_PRESENCE_SetPresence()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_PeekNextEvent()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_Quit()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_Release()
{
	LOG_FUNC();
	return 1;
}

void InitSavePath(const char* userName, DWORD appId) {
    if (g_SavePathInit) return;
    
    // Get AppData folder
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        // Build save path: %APPDATA%\UplayEmu\{userId}\{appId}\saves
        sprintf(g_SavePath, "%s\\UplayEmu\\%s\\%lu\\saves", appDataPath, userName, appId);
    } else {
        // Fallback to game directory
        char basePath[MAX_PATH];
        GetModuleFileNameA(UplayModule, basePath, MAX_PATH);
        char* p = strrchr(basePath, '\\');
        if (p) *p = 0;
        sprintf(g_SavePath, "%s\\UplayEmu\\%s\\%lu\\saves", basePath, userName, appId);
    }
    
    // Create directory structure
    char createPath[MAX_PATH];
    strcpy(createPath, g_SavePath);
    for (char* cp = createPath; *cp; cp++) {
        if (*cp == '\\') {
            *cp = 0;
            CreateDirectoryA(createPath, NULL);
            *cp = '\\';
        }
    }
    CreateDirectoryA(createPath, NULL);
    
    g_SavePathInit = true;
    LogWrite("[Uplay Emu] Save path initialized: %s", g_SavePath);
}

void GetSaveFilePath(DWORD slotId, char* outPath) {
    sprintf(outPath, "%s\\%lu.save", g_SavePath, slotId);
}

// Achievement system functions
void InitAchievementPath(const char* userName, DWORD appId) {
    if (g_AchievementPathInit) return;
    
    // Get AppData folder
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        // Build achievement path: %APPDATA%\UplayEmu\{userId}\{appId}\achievements
        sprintf(g_AchievementPath, "%s\\UplayEmu\\%s\\%lu\\achievements", appDataPath, userName, appId);
    } else {
        // Fallback to game directory
        char basePath[MAX_PATH];
        GetModuleFileNameA(UplayModule, basePath, MAX_PATH);
        char* p = strrchr(basePath, '\\');
        if (p) *p = 0;
        sprintf(g_AchievementPath, "%s\\UplayEmu\\%s\\%lu\\achievements", basePath, userName, appId);
    }
    
    // Create directory structure
    char createPath[MAX_PATH];
    strcpy(createPath, g_AchievementPath);
    for (char* cp = createPath; *cp; cp++) {
        if (*cp == '\\') {
            *cp = 0;
            CreateDirectoryA(createPath, NULL);
            *cp = '\\';
        }
    }
    CreateDirectoryA(createPath, NULL);
    
    g_AchievementPathInit = true;
    LogWrite("[Uplay Emu] Achievement path initialized: %s", g_AchievementPath);
}

void GetAchievementFilePath(DWORD achId, char* outPath) {
    sprintf(outPath, "%s\\%lu.ini", g_AchievementPath, achId);
}

bool ReadAchievement(DWORD achId, char* outName, char* outDesc, bool* outEarned) {
    char iniPath[MAX_PATH];
    GetAchievementFilePath(achId, iniPath);
    
    if (GetFileAttributesA(iniPath) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    
    GetPrivateProfileStringA("Achievement", "Name", "Unknown Achievement", outName, 256, iniPath);
    GetPrivateProfileStringA("Achievement", "Description", "", outDesc, 512, iniPath);
    *outEarned = GetPrivateProfileIntA("Achievement", "Unlocked", 0, iniPath) != 0;
    
    return true;
}

bool WriteAchievement(DWORD achId, const char* name, const char* desc, bool earned) {
    char iniPath[MAX_PATH];
    GetAchievementFilePath(achId, iniPath);
    
    WritePrivateProfileStringA("Achievement", "Name", name, iniPath);
    WritePrivateProfileStringA("Achievement", "Description", desc, iniPath);
    WritePrivateProfileStringA("Achievement", "Unlocked", earned ? "1" : "0", iniPath);
    
    return true;
}

bool UnlockAchievement(DWORD achId) {
    char iniPath[MAX_PATH];
    GetAchievementFilePath(achId, iniPath);
    
    // If file doesn't exist, create it with default values
    if (GetFileAttributesA(iniPath) == INVALID_FILE_ATTRIBUTES) {
        char defaultName[64];
        sprintf(defaultName, "Achievement %lu", achId);
        WritePrivateProfileStringA("Achievement", "Name", defaultName, iniPath);
        WritePrivateProfileStringA("Achievement", "Description", "", iniPath);
    }
    
    WritePrivateProfileStringA("Achievement", "Unlocked", "1", iniPath);
    LogWrite("[Uplay Emu] Achievement %lu unlocked", achId);
    return true;
}

std::vector<DWORD> GetAllAchievementIds() {
    std::vector<DWORD> ids;
    char searchPath[MAX_PATH];
    sprintf(searchPath, "%s\\*.ini", g_AchievementPath);
    
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPath, &fd);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                DWORD achId = strtoul(fd.cFileName, NULL, 10);
                ids.push_back(achId);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    
    return ids;
}

UPLAY_EXPORT int UPLAY_SAVE_Close(DWORD slotId)
{
	LOG_FUNC();
	LogWrite("[Uplay Emu] SAVE_Close: slotId=%lu", slotId);
	
	if (slotId >= 256 || !g_SaveSlots[slotId].inUse) {
		LogWrite("[Uplay Emu] SAVE_Close: Invalid or unused slot");
		return 0;
	}
	
	SaveSlot* slot = &g_SaveSlots[slotId];
	
	// If mode was 1 (write), update the header
	if (slot->mode == 1) {
		if (slot->saveName[0] == 0)
			strcpy(slot->saveName, "Unnamed");
		
		char savePath[MAX_PATH];
		GetSaveFilePath(slotId, savePath);
		
		// Verify file exists
		if (GetFileAttributesA(savePath) == INVALID_FILE_ATTRIBUTES) {
			LogWrite("[Uplay Emu] SAVE_Close: Save file doesn't exist:  %s", savePath);
			// Clear slot anyway
			memset(slot, 0, sizeof(SaveSlot));
			return 0;
		}
		
		HANDLE hFile = CreateFileA(savePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, 
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		
		if (hFile == INVALID_HANDLE_VALUE) {
			LogWrite("[Uplay Emu] SAVE_Close: Failed to open file for header update (Error: %lu)", GetLastError());
			memset(slot, 0, sizeof(SaveSlot));
			return 0;
		}
		
		// Create 552-byte header
		BYTE header[SAVE_HEADER_SIZE] = {0};
		
		// Write header size - 4 at offset 0
		DWORD headerSizeValue = SAVE_HEADER_SIZE - 4;
		memcpy(header, &headerSizeValue, 4);
		
		// Write save name as Unicode at offset 40
		int nameLen = strlen(slot->saveName);
		for (int i = 0; i < nameLen && (40 + i*2 + 1) < SAVE_HEADER_SIZE; i++) {
			header[40 + i*2] = (BYTE)slot->saveName[i];
			header[40 + i*2 + 1] = 0;
		}
		
		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		DWORD written = 0;
		
		if (!WriteFile(hFile, header, SAVE_HEADER_SIZE, &written, NULL) || written != SAVE_HEADER_SIZE) {
			LogWrite("[Uplay Emu] SAVE_Close: Failed to write header (Error: %lu, Written: %lu)", GetLastError(), written);
			CloseHandle(hFile);
			memset(slot, 0, sizeof(SaveSlot));
			return 0;
		}
		
		FlushFileBuffers(hFile);
		CloseHandle(hFile);
		LogWrite("[Uplay Emu] SAVE_Close: Header written for slot %lu", slotId);
	}
	
	// Close file handle if exists (for read mode)
	if (slot->fileHandle && slot->fileHandle != INVALID_HANDLE_VALUE) {
		CloseHandle(slot->fileHandle);
	}
	
	// Clear slot
	memset(slot, 0, sizeof(SaveSlot));
	return 1;
}

UPLAY_EXPORT int UPLAY_SAVE_GetSavegames(void* outListPtr, void* overlapped)
{
	LOG_FUNC();
	
	// Find all .save files in save directory
	char searchPath[MAX_PATH];
	sprintf(searchPath, "%s\\*.save", g_SavePath);
	
	// Count files first
	WIN32_FIND_DATAA fd;
	HANDLE hFind = FindFirstFileA(searchPath, &fd);
	DWORD fileCount = 0;
	
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				fileCount++;
		} while (FindNextFileA(hFind, &fd));
		FindClose(hFind);
	}
	
	LogWrite("[Uplay Emu] Found %lu save files", fileCount);
	
	// Allocate list structure using ULONG_PTR for cross-platform compatibility
	void* listBuffer = VirtualAlloc(NULL, 0x1000, MEM_COMMIT, PAGE_READWRITE);
	void* entriesBuffer = VirtualAlloc(NULL, 0x10000, MEM_COMMIT, PAGE_READWRITE);
	
	ULONG_PTR* listHeader = (ULONG_PTR*)listBuffer;
	listHeader[0] = fileCount;  // count
	listHeader[1] = (ULONG_PTR)entriesBuffer;  // pointer to entries
	
	// Fill entries
	hFind = FindFirstFileA(searchPath, &fd);
	DWORD entryIndex = 0;
	
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			
			// Parse slot ID from filename
			DWORD slotId = strtoul(fd.cFileName, NULL, 10);
			
			// Build full path
			char fullPath[MAX_PATH];
			sprintf(fullPath, "%s\\%s", g_SavePath, fd.cFileName);
			
			// Get file size
			HANDLE hFile = CreateFileA(fullPath, GENERIC_READ, FILE_SHARE_READ, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			DWORD fileSize = 0;
			char saveName[256] = "Unnamed";
			
			if (hFile != INVALID_HANDLE_VALUE) {
				fileSize = GetFileSize(hFile, NULL);
				if (fileSize > SAVE_HEADER_SIZE)
					fileSize -= SAVE_HEADER_SIZE;
				else
					fileSize = 0;
				
				// Read save name from header
				BYTE header[SAVE_HEADER_SIZE];
				DWORD bytesRead;
				if (ReadFile(hFile, header, SAVE_HEADER_SIZE, &bytesRead, NULL) && bytesRead >= SAVE_HEADER_SIZE) {
					// Extract Unicode name from offset 40
					int nameIdx = 0;
					for (int i = 40; i < SAVE_HEADER_SIZE - 1 && nameIdx < 255; i += 2) {
						if (header[i] == 0 && header[i+1] == 0) break;
						saveName[nameIdx++] = (char)header[i];
					}
					saveName[nameIdx] = 0;
				}
				CloseHandle(hFile);
			}
			
			// Allocate entry structure using ULONG_PTR for proper alignment
			void* entryBuffer = VirtualAlloc(NULL, 0x100, MEM_COMMIT, PAGE_READWRITE);
			
			// Entry structure: { ULONG_PTR id, ULONG_PTR namePtr, ULONG_PTR size }
			ULONG_PTR* entry = (ULONG_PTR*)entryBuffer;
			char* nameBuffer = (char*)VirtualAlloc(NULL, 0x200, MEM_COMMIT, PAGE_READWRITE);
			strcpy(nameBuffer, saveName);
			
			entry[0] = slotId;              // id
			entry[1] = (ULONG_PTR)nameBuffer; // name pointer
			entry[2] = fileSize;            // size
			
			// Store entry pointer in entries array
			((void**)entriesBuffer)[entryIndex++] = entryBuffer;
			
		} while (FindNextFileA(hFind, &fd));
		FindClose(hFind);
	}
	
	// Write list pointer
	memcpy(outListPtr, &listBuffer, sizeof(void*));
	
	// Set overlapped result
	FileRead* ovr = (FileRead*)overlapped;
	ovr->addr1++;
	ovr->addr2 = 1;
	ovr->addr3 = 0;
	
	return 1;
}

UPLAY_EXPORT int UPLAY_SAVE_Open(DWORD slotId, DWORD mode, void* outHandle, void* overlapped)
{
	LOG_FUNC();
	LogWrite("[Uplay Emu] SAVE_Open: slotId=%lu, mode=%lu", slotId, mode);
	
	if (slotId >= 256) {
		FileRead* ovr = (FileRead*)overlapped;
		ovr->addr1++;
		ovr->addr2 = 1;
		ovr->addr3 = 0;
		return 0;
	}
	
	// Initialize slot
	SaveSlot* slot = &g_SaveSlots[slotId];
	memset(slot, 0, sizeof(SaveSlot));
	slot->mode = mode;
	slot->slotId = slotId;
	slot->inUse = true;
	
	char savePath[MAX_PATH];
	GetSaveFilePath(slotId, savePath);
	
	if (mode == 0) { // Read mode
		HANDLE hFile = CreateFileA(savePath, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			LogWrite("[Uplay Emu] File not found: %s", savePath);
			slot->inUse = false;
			FileRead* ovr = (FileRead*)overlapped;
			ovr->addr1++;
			ovr->addr2 = 1;
			ovr->addr3 = 0;
			return 0;
		}
		slot->fileHandle = hFile;
	} else { // Write mode
		// Create file with header if it doesn't exist
		if (GetFileAttributesA(savePath) == INVALID_FILE_ATTRIBUTES) {
			HANDLE hFile = CreateFileA(savePath, GENERIC_WRITE, 0, NULL,
				CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE) {
				BYTE header[SAVE_HEADER_SIZE] = {0};
				DWORD written;
				WriteFile(hFile, header, SAVE_HEADER_SIZE, &written, NULL);
				CloseHandle(hFile);
			}
		}
		slot->fileHandle = NULL;
	}
	
	// Write slot ID to output handle
	*(DWORD*)outHandle = slotId;
	
	// Set overlapped result
	FileRead* ovr = (FileRead*)overlapped;
	ovr->addr1++;
	ovr->addr2 = 1;
	ovr->addr3 = 0;
	
	LogWrite("[Uplay Emu] SAVE_Open success");
	return 1;
}

UPLAY_EXPORT int UPLAY_SAVE_Read(DWORD slotId, DWORD numBytes, DWORD offset, void* outBufferPtr, void* outBytesRead, void* overlapped)
{
	LOG_FUNC();
	LogWrite("[Uplay Emu] SAVE_Read: slotId=%lu, bytes=%lu, offset=%lu", slotId, numBytes, offset);
	
	char savePath[MAX_PATH];
	GetSaveFilePath(slotId, savePath);
	
	void* actualBuffer = *(void**)outBufferPtr;
	DWORD bytesRead = 0;
	
	HANDLE hFile = CreateFileA(savePath, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if (hFile != INVALID_HANDLE_VALUE) {
		SetFilePointer(hFile, SAVE_HEADER_SIZE + offset, NULL, FILE_BEGIN);
		ReadFile(hFile, actualBuffer, numBytes, &bytesRead, NULL);
		CloseHandle(hFile);
		LogWrite("[Uplay Emu] Read %lu bytes", bytesRead);
	}
	
	*(DWORD*)outBytesRead = bytesRead;
	
	FileRead* ovr = (FileRead*)overlapped;
	ovr->addr1++;
	ovr->addr2 = 1;
	ovr->addr3 = 0;
	
	return (bytesRead > 0) ? 1 : 0;
}

UPLAY_EXPORT int UPLAY_SAVE_ReleaseGameList(void* listPointer)
{
	LOG_FUNC();
	if (listPointer) {
		VirtualFree(listPointer, 0, MEM_RELEASE);
	}
	return 1;
}

UPLAY_EXPORT int UPLAY_SAVE_Remove(DWORD slotId, void* overlapped)
{
	LOG_FUNC();
	LogWrite("[Uplay Emu] SAVE_Remove: slotId=%lu", slotId);
	
	char savePath[MAX_PATH];
	GetSaveFilePath(slotId, savePath);
	
	DeleteFileA(savePath);
	
	FileRead* ovr = (FileRead*)overlapped;
	ovr->addr1++;
	ovr->addr2 = 1;
	ovr->addr3 = 0;
	
	return 1;
}

UPLAY_EXPORT int UPLAY_SAVE_SetName(DWORD slotId, const char* nameUtf8)
{
	LOG_FUNC();
	LogWrite("[Uplay Emu] SAVE_SetName: slotId=%lu, name=%s", slotId, nameUtf8 ? nameUtf8 : "(null)");
	
	if (slotId < 256 && g_SaveSlots[slotId].inUse && nameUtf8) {
		strncpy(g_SaveSlots[slotId].saveName, nameUtf8, 511);
		g_SaveSlots[slotId].saveName[511] = 0;
	}
	return 1;
}

UPLAY_EXPORT int UPLAY_SAVE_Write(DWORD slotId, DWORD numBytes, void* bufferPtr, void* overlapped)
{
	LOG_FUNC();
	LogWrite("[Uplay Emu] SAVE_Write: slotId=%lu, bytes=%lu", slotId, numBytes);
	
	// Validate slot
	if (slotId >= 256 || !g_SaveSlots[slotId].inUse) {
		LogWrite("[Uplay Emu] SAVE_Write: Invalid or unused slot");
		FileRead* ovr = (FileRead*)overlapped;
		ovr->addr1++;
		ovr->addr2 = 1;
		ovr->addr3 = 0;
		return 0;
	}
	
	SaveSlot* slot = &g_SaveSlots[slotId];
	
	// Verify slot is in write mode
	if (slot->mode != 1) {
		LogWrite("[Uplay Emu] SAVE_Write:  Slot not opened in write mode");
		FileRead* ovr = (FileRead*)overlapped;
		ovr->addr1++;
		ovr->addr2 = 1;
		ovr->addr3 = 0;
		return 0;
	}
	
	char savePath[MAX_PATH];
	GetSaveFilePath(slotId, savePath);
	
	// Get actual buffer from pointer
	void* actualBuffer = *(void**)bufferPtr;
	if (!actualBuffer) {
		LogWrite("[Uplay Emu] SAVE_Write: NULL buffer pointer");
		FileRead* ovr = (FileRead*)overlapped;
		ovr->addr1++;
		ovr->addr2 = 1;
		ovr->addr3 = 0;
		return 0;
	}
	
	// Verify file exists (should have been created in SAVE_Open)
	if (GetFileAttributesA(savePath) == INVALID_FILE_ATTRIBUTES) {
		LogWrite("[Uplay Emu] SAVE_Write: Save file doesn't exist, creating it");
		
		// Create file with header
		HANDLE hTemp = CreateFileA(savePath, GENERIC_WRITE, 0, NULL,
			CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		
		if (hTemp == INVALID_HANDLE_VALUE) {
			LogWrite("[Uplay Emu] SAVE_Write: Failed to create file (Error: %lu)", GetLastError());
			FileRead* ovr = (FileRead*)overlapped;
			ovr->addr1++;
			ovr->addr2 = 1;
			ovr->addr3 = 0;
			return 0;
		}
		
		BYTE header[SAVE_HEADER_SIZE] = {0};
		DWORD written;
		WriteFile(hTemp, header, SAVE_HEADER_SIZE, &written, NULL);
		CloseHandle(hTemp);
	}
	
	// Open file for writing with proper sharing mode
	HANDLE hFile = CreateFileA(savePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if (hFile == INVALID_HANDLE_VALUE) {
		LogWrite("[Uplay Emu] SAVE_Write: Failed to open file (Error: %lu)", GetLastError());
		FileRead* ovr = (FileRead*)overlapped;
		ovr->addr1++;
		ovr->addr2 = 1;
		ovr->addr3 = 0;
		return 0;
	}
	
	// Seek past header to data section
	DWORD seekResult = SetFilePointer(hFile, SAVE_HEADER_SIZE, NULL, FILE_BEGIN);
	if (seekResult == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
		LogWrite("[Uplay Emu] SAVE_Write: Failed to seek (Error: %lu)", GetLastError());
		CloseHandle(hFile);
		FileRead* ovr = (FileRead*)overlapped;
		ovr->addr1++;
		ovr->addr2 = 1;
		ovr->addr3 = 0;
		return 0;
	}
	
	// Write data
	DWORD written = 0;
	if (!WriteFile(hFile, actualBuffer, numBytes, &written, NULL)) {
		LogWrite("[Uplay Emu] SAVE_Write: WriteFile failed (Error: %lu)", GetLastError());
		CloseHandle(hFile);
		FileRead* ovr = (FileRead*)overlapped;
		ovr->addr1++;
		ovr->addr2 = 1;
		ovr->addr3 = 0;
		return 0;
	}
	
	// Verify all bytes were written
	if (written != numBytes) {
		LogWrite("[Uplay Emu] SAVE_Write: Partial write (Requested: %lu, Written: %lu)", numBytes, written);
		CloseHandle(hFile);
		FileRead* ovr = (FileRead*)overlapped;
		ovr->addr1++;
		ovr->addr2 = 1;
		ovr->addr3 = 0;
		return 0;
	}
	
	// Truncate file to exact size (header + data)
	seekResult = SetFilePointer(hFile, SAVE_HEADER_SIZE + numBytes, NULL, FILE_BEGIN);
	if (seekResult == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
		LogWrite("[Uplay Emu] SAVE_Write: Failed to seek for truncate (Error: %lu)", GetLastError());
		CloseHandle(hFile);
		FileRead* ovr = (FileRead*)overlapped;
		ovr->addr1++;
		ovr->addr2 = 1;
		ovr->addr3 = 0;
		return 0;
	}
	
	if (!SetEndOfFile(hFile)) {
		LogWrite("[Uplay Emu] SAVE_Write: Failed to truncate file (Error: %lu)", GetLastError());
		CloseHandle(hFile);
		FileRead* ovr = (FileRead*)overlapped;
		ovr->addr1++;
		ovr->addr2 = 1;
		ovr->addr3 = 0;
		return 0;
	}
	
	// Flush to disk
	FlushFileBuffers(hFile);
	CloseHandle(hFile);
	
	LogWrite("[Uplay Emu] SAVE_Write: Successfully wrote %lu bytes", written);
	
	// Set overlapped result to success
	FileRead* ovr = (FileRead*)overlapped;
	ovr->addr1++;
	ovr->addr2 = 1;
	ovr->addr3 = 0;
	
	return 1;
}

UPLAY_EXPORT int UPLAY_STORE_Checkout()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_STORE_GetPartner()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_STORE_GetProducts()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_STORE_ReleaseProductsList()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_STORE_ShowProductDetails()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_STORE_ShowProducts()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_SetGameSession()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_SetLanguage(const char* language)
{
	LOG_FUNC();
	strcpy(Uplay_Configuration::GameLanguage, language);
	return 1;
}
UPLAY_EXPORT int UPLAY_Start(unsigned int uplayId)
{
	LOG_FUNC();
	CHAR INI[MAX_PATH] = { 0 };					// Get ini directory
	GetModuleFileNameA(UplayModule, INI, MAX_PATH);
	int Size = lstrlenA(INI);
	for (int i = Size; i > 0; i--)
	{
		if (INI[i] == '\\')
		{
			lstrcpyA(&INI[i], "\\Uplay.ini");
			break;
		}
	}
	if (!IsTargetExist(INI))
	{
		FILE* iniFile = fopen(INI, "w");
		if (iniFile) {
			fprintf(iniFile, "[Uplay]\n");
			fprintf(iniFile, "; Application ownership status (0 = not owned, 1 = owned)\nIsAppOwned=1\n");
			fprintf(iniFile, "; Connection mode (0 = online, 1 = offline)\nUplayConnection=0\n");
			fprintf(iniFile, ";Application ID (change this to match your game's App ID)\nAppId=%d\n", uplayId);
			fprintf(iniFile, "; User credential\nUsername=Rat\n");
			fprintf(iniFile, "Email=UplayEmu@rat43.com\n");
			fprintf(iniFile, "Password=UplayPassword74\n");
			fprintf(iniFile, "; Game language (ISO language code)\nLanguage=en-US\n");
			fprintf(iniFile, "; CD Key for the game\nCdKey=1111-2222-3333-4444\n");
			fprintf(iniFile, "; User ID (UUID format)\nUserId=c91c91c9-1c91-c91c-91c9-1c91c91c91c9\n");
			fprintf(iniFile, "; Ticket ID for authentication\nTickedId=noT456umPqRt\n");
			fprintf(iniFile, "\n; Enable logging to uplay_emu.log or Console (0 = disabled, 1 = enabled)\nLogging=0\n");
			fprintf(iniFile, "EnableConsole=0\n");
			fprintf(iniFile, "\n; Enable Friends/Party features (0 = disabled, 1 = enabled, this is for returning values to the game in case of crashing)\nFriends=0\n");
			fprintf(iniFile, "Party=0\n");

			fclose(iniFile);
		} else {
			MessageBoxA(0, "Couldn't create Uplay.ini.", "Uplay", MB_ICONERROR);
			ExitProcess(0);
		}
	}
	Uplay_Configuration::appowned = GetPrivateProfileIntA("Uplay", "IsAppOwned", 0, INI) == TRUE;		// Read ini informations
	Uplay_Configuration::Offline = GetPrivateProfileIntA("Uplay", "UplayConnection", 0, INI) == TRUE;
	Uplay_Configuration::logging = GetPrivateProfileIntA("Uplay", "Logging", 0, INI) == TRUE;
	g_LoggingEnabled = Uplay_Configuration::logging;
	Uplay_Configuration::gameAppId = GetPrivateProfileIntA("Uplay", "AppId", 0, INI);
	GetPrivateProfileStringA("Uplay", "Username", 0, Uplay_Configuration::UserName, 0x200, INI);
	GetPrivateProfileStringA("Uplay", "Email", 0, Uplay_Configuration::UserEmail, 0x200, INI);
	GetPrivateProfileStringA("Uplay", "Password", 0, Uplay_Configuration::password, 0x200, INI);
	GetPrivateProfileStringA("Uplay", "Language", 0, Uplay_Configuration::GameLanguage, 0x200, INI);
	GetPrivateProfileStringA("Uplay", "CdKey", 0, Uplay_Configuration::CdKey, 0x200, INI);
	GetPrivateProfileStringA("Uplay", "UserId", 0, Uplay_Configuration::UserId, 0x200, INI);
	GetPrivateProfileStringA("Uplay", "TickedId", 0, Uplay_Configuration::TickedId, 0x200, INI);
	Uplay_Configuration::friends = GetPrivateProfileIntA("Uplay", "Friends", 0, INI) == TRUE;
	Uplay_Configuration::party = GetPrivateProfileIntA("Uplay", "Party", 0, INI) == TRUE;

	InitSavePath(Uplay_Configuration::UserName, Uplay_Configuration::gameAppId);
	InitAchievementPath(Uplay_Configuration::UserName, Uplay_Configuration::gameAppId);
	
	return 0;
}
UPLAY_EXPORT int UPLAY_StartCN()
{
	LOG_FUNC();
	return UPLAY_Start(0);
}
UPLAY_EXPORT int UPLAY_Startup(unsigned int uplayId, unsigned int gameVersion, const char* languageCountryCode)
{
	LOG_FUNC();
	LogWrite("[Uplay Emu] UPLAY_Startup(uplayId=%u, gameVersion=%u, language=%s)", 
		uplayId, gameVersion, languageCountryCode ? languageCountryCode : "null");
	
	// Use uplayId as appId if not set in INI
	if (Uplay_Configuration::gameAppId == 0) {
		Uplay_Configuration::gameAppId = uplayId;
	}
	
	// Use language if provided
	if (languageCountryCode && *languageCountryCode) {
		strcpy(Uplay_Configuration::GameLanguage, languageCountryCode);
	}
	
	return UPLAY_Start(uplayId);
}

UPLAY_EXPORT int UPLAY_USER_ClearGameSession()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_USER_ConsumeItem()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_USER_GetAccountId()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT const char* UPLAY_USER_GetAccountIdUtf8()
{
	LOG_FUNC();
	return Uplay_Configuration::UserId;
}
UPLAY_EXPORT int UPLAY_USER_GetCPUScore()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT const char* UPLAY_USER_GetCdKeyUtf8()
{
	LOG_FUNC();
	return Uplay_Configuration::CdKey;
}
UPLAY_EXPORT int UPLAY_USER_GetCdKeys()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_USER_GetConsumableItems(void* buf1)
{
	LOG_FUNC();
#ifdef _WIN64
	memset(buf1, 0, 8);
#else
	memset(buf1, 0, 4);
#endif
	return 1;
}
UPLAY_EXPORT int UPLAY_USER_GetCredentials()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_USER_GetEmail()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT const char* UPLAY_USER_GetEmailUtf8()
{
	LOG_FUNC();
	return Uplay_Configuration::UserEmail;
}
UPLAY_EXPORT int UPLAY_USER_GetGPUScore()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_USER_GetGPUScoreConfidenceLevel()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT const char* UPLAY_USER_GetNameUtf8()
{
	LOG_FUNC();
	return Uplay_Configuration::UserName;
}
UPLAY_EXPORT int UPLAY_USER_GetPassword()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT const char* UPLAY_USER_GetPasswordUtf8()
{
	LOG_FUNC();
	return Uplay_Configuration::password;
}
UPLAY_EXPORT int UPLAY_USER_GetProfile()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT const char* UPLAY_USER_GetTicketUtf8()
{
	LOG_FUNC();
	return Uplay_Configuration::TickedId;
}
UPLAY_EXPORT int UPLAY_USER_GetUsername()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT const char* UPLAY_USER_GetUsernameUtf8()
{
	LOG_FUNC();
	return Uplay_Configuration::UserName;
}
UPLAY_EXPORT int UPLAY_USER_IsConnected()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_USER_IsInOfflineMode()
{
	LOG_FUNC();
	return Uplay_Configuration::Offline;
}
UPLAY_EXPORT int UPLAY_USER_IsOwned(int data)
{
	LOG_FUNC();
	return Uplay_Configuration::appowned;
}
UPLAY_EXPORT int UPLAY_USER_ReleaseCdKeyList()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_USER_ReleaseConsumeItemResult()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_USER_ReleaseProfile()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_USER_SetGameSession()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_Update()
{
	return 1;
}
UPLAY_EXPORT int UPLAY_WIN_GetActions()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_WIN_GetRewards()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_WIN_GetUnitBalance()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_WIN_RefreshActions()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_WIN_ReleaseActionList()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_WIN_ReleaseRewardList()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_WIN_SetActionsCompleted()
{
	LOG_FUNC();
	return 1;
}
UPLAY_EXPORT int UPLAY_CHAT_GetHistory()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_CHAT_Init()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_CHAT_ReleaseHistoryList()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_CHAT_SendMessage()
{
	LOG_FUNC();
	return 0;
}
UPLAY_EXPORT int UPLAY_CHAT_SentMessagesRead()
{
	LOG_FUNC();
	return 0;
}

UPLAY_EXPORT int UPLAY_PRODUCT_GetProductList(DWORD a1, DWORD a2, DWORD a3)
{
	LOG_FUNC();
	return 1;
}

UPLAY_EXPORT int UPLAY_PRODUCT_ReleaseProductList(DWORD a1)
{
	LOG_FUNC();
	return 1;
}