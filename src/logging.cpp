#include "logging.h"

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

static void InitConsole()
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

void SetLoggingEnabled(bool enabled)
{
	g_LoggingEnabled = enabled;
}

void SetConsoleEnabled(bool enabled)
{
	g_ConsoleEnabled = enabled;
}

bool IsLoggingEnabled()
{
	return g_LoggingEnabled;
}

bool IsConsoleEnabled()
{
	return g_ConsoleEnabled;
}
