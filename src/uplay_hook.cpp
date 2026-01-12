/*
 * Uplay Hook ASI Plugin
 * 
 * Intercepts uplay_r1_loader64.dll exports and redirects to the emulator.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>
#include "minhook/include/MinHook.h"

#ifdef _WIN64
    #define EMULATOR_DLL_NAME "emu.upc_r1_loader64.dll"
#else
    #define EMULATOR_DLL_NAME "emu.upc_r1_loader.dll"
#endif

// known dll names
static const char* g_TargetDlls[] = {
    "uplay_r1_loader64.dll",
    "uplay_r1_loader.dll",
    "upc_r1_loader.dll",
    "upc_r1_loader64.dll",
    "uplay_r164.dll",
    "uplay_r1.dll",
    nullptr
};

static HMODULE g_OurModule = nullptr;
static HMODULE g_EmulatorModule = nullptr;
static HMODULE g_HookedUplayModule = nullptr;
static bool g_HooksInstalled = false;
static FILE* g_LogFile = nullptr;

void InitLog() {
    if (g_LogFile) return;
    
    char logPath[MAX_PATH];
    GetModuleFileNameA(g_OurModule, logPath, MAX_PATH);
    char* p = strrchr(logPath, '\\');
    if (p) strcpy(p + 1, "uplay_hook.log");
    else strcpy(logPath, "uplay_hook.log");
    
    g_LogFile = fopen(logPath, "w");
}

void Log(const char* fmt, ...) {
    if (!g_LogFile) return;
    
    va_list args;
    va_start(args, fmt);
    vfprintf(g_LogFile, fmt, args);
    va_end(args);
    fprintf(g_LogFile, "\n");
    fflush(g_LogFile);
}

bool IsTargetDll(const char* path) {
    if (!path) return false;
    
    const char* filename = strrchr(path, '\\');
    filename = filename ? filename + 1 : path;
    
    for (int i = 0; g_TargetDlls[i]; i++) {
        if (_stricmp(filename, g_TargetDlls[i]) == 0) return true;
    }
    return false;
}

void LoadEmulatorDll() {
    if (g_EmulatorModule) return;
    
    char emuPath[MAX_PATH];
    GetModuleFileNameA(g_OurModule, emuPath, MAX_PATH);
    char* p = strrchr(emuPath, '\\');
    if (p) strcpy(p + 1, EMULATOR_DLL_NAME);
    else strcpy(emuPath, EMULATOR_DLL_NAME);
    
    Log("[Uplay Hook] Loading emulator DLL: %s", emuPath);
    g_EmulatorModule = LoadLibraryA(emuPath);
    
    if (g_EmulatorModule) {
        Log("[Uplay Hook] Emulator DLL loaded at 0x%p", g_EmulatorModule);
    } else {
        Log("[Uplay Hook] FAILED to load emulator DLL! Error: %lu", GetLastError());
    }
}

void HookAllExports(HMODULE uplayModule) {
    if (!uplayModule || !g_EmulatorModule) {
        Log("[Uplay Hook] HookAllExports: Missing module");
        return;
    }
    
    Log("[Uplay Hook] Hooking exports");
    Log("[Uplay Hook] Uplay DLL: 0x%p", uplayModule);
    Log("[Uplay Hook] Emulator DLL: 0x%p", g_EmulatorModule);
    
    // Get Uplay export directory
    PIMAGE_DOS_HEADER uplayDos = (PIMAGE_DOS_HEADER)uplayModule;
    PIMAGE_NT_HEADERS uplayNt = (PIMAGE_NT_HEADERS)((BYTE*)uplayModule + uplayDos->e_lfanew);
    
    DWORD exportRVA = uplayNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!exportRVA) {
        Log("[ERROR] No exports in Uplay DLL!");
        return;
    }
    
    PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)uplayModule + exportRVA);
    DWORD* names = (DWORD*)((BYTE*)uplayModule + exports->AddressOfNames);
    DWORD* funcs = (DWORD*)((BYTE*)uplayModule + exports->AddressOfFunctions);
    WORD* ords = (WORD*)((BYTE*)uplayModule + exports->AddressOfNameOrdinals);
    
    int hookedCount = 0;
    int skippedCount = 0;
    
    Log("[Uplay Hook] Uplay DLL has %lu exports", exports->NumberOfNames);
    
    for (DWORD i = 0; i < exports->NumberOfNames; i++) {
        const char* funcName = (const char*)((BYTE*)uplayModule + names[i]);
        WORD ordinal = ords[i];
        FARPROC uplayFunc = (FARPROC)((BYTE*)uplayModule + funcs[ordinal]);
        
        FARPROC emuFunc = GetProcAddress(g_EmulatorModule, funcName);
        if (!emuFunc) {
            skippedCount++;
            continue;
        }
        
        MH_STATUS status = MH_CreateHook((LPVOID)uplayFunc, (LPVOID)emuFunc, nullptr);
        if (status == MH_OK) {
            Log("[Uplay Hook] Hooked: %s (0x%p -> 0x%p)", funcName, uplayFunc, emuFunc);
            hookedCount++;
        } else {
            Log("[Uplay Hook] FAILED to hook %s: %s", funcName, MH_StatusToString(status));
        }
    }
    
    MH_STATUS enableStatus = MH_EnableHook(MH_ALL_HOOKS);
    Log("[Uplay Hook] MH_EnableHook(MH_ALL_HOOKS): %s", MH_StatusToString(enableStatus));
    
    Log("[Uplay Hook] ========================================");
    Log("[Uplay Hook] Results: %d hooked, %d skipped", hookedCount, skippedCount);
    Log("[Uplay Hook] ========================================");
}

HMODULE FindUplayModule() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) return nullptr;
    
    MODULEENTRY32 me;
    me.dwSize = sizeof(me);
    
    HMODULE result = nullptr;
    if (Module32First(snapshot, &me)) {
        do {
            if (IsTargetDll(me.szExePath)) {
                Log("[Uplay Hook] Found loaded Uplay DLL: %s at 0x%p", me.szExePath, me.hModule);
                result = me.hModule;
                break;
            }
        } while (Module32Next(snapshot, &me));
    }
    
    CloseHandle(snapshot);
    return result;
}

void TryHookUplay() {
    if (g_HookedUplayModule) return;
    
    HMODULE uplay = FindUplayModule();
    if (uplay) {
        LoadEmulatorDll();
        if (g_EmulatorModule) {
            HookAllExports(uplay);
            g_HookedUplayModule = uplay;
        }
    }
}

void Initialize() {
    InitLog();
    MH_STATUS mhStatus = MH_Initialize();
    Log("[Uplay Hook] MH_Initialize: %s", MH_StatusToString(mhStatus));
    
    if (mhStatus != MH_OK && mhStatus != MH_ERROR_ALREADY_INITIALIZED) {
        Log("[Uplay Hook] FATAL: MinHook initialization failed!");
        return;
    }
    
    TryHookUplay();
    
    Log("[Uplay Hook] Initialization complete");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    (void)reserved;
    
    if (reason == DLL_PROCESS_ATTACH) {
        g_OurModule = hModule;
        DisableThreadLibraryCalls(hModule);
        Initialize();
    }
    else if (reason == DLL_PROCESS_DETACH) {
        MH_Uninitialize();
        if (g_LogFile) {
            Log("[Uplay Hook] Shutting down");
            fclose(g_LogFile);
        }
    }
    
    return TRUE;
}
