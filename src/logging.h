#ifndef LOGGING_H
#define LOGGING_H

#include "pch.h"
#include <cstdio>
#include <ctime>
#include <cstdarg>

// Initialize the logging system (reads config, opens log file/console)
void InitLog();

// Write a formatted log message
void LogWrite(const char* format, ...);

// Read logging configuration from INI file
void ReadLoggingConfig();

// Enable/disable logging programmatically
void SetLoggingEnabled(bool enabled);
void SetConsoleEnabled(bool enabled);

// Check if logging is enabled
bool IsLoggingEnabled();
bool IsConsoleEnabled();

// Macro for logging function calls
#define LOG_FUNC() do { InitLog(); LogWrite("[Uplay Emu] %s called", __FUNCTION__); } while(0)

#endif // LOGGING_H
