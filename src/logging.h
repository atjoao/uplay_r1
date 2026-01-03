#ifndef LOGGING_H
#define LOGGING_H

#include "pch.h"
#include <cstdio>
#include <ctime>
#include <cstdarg>

void InitLog();

void LogWrite(const char* format, ...);

void ReadLoggingConfig();

void SetLoggingEnabled(bool enabled);
void SetConsoleEnabled(bool enabled);

// Check if logging is enabled
bool IsLoggingEnabled();
bool IsConsoleEnabled();

#define LOG_FUNC() do { InitLog(); LogWrite("[Uplay Emu] %s called", __FUNCTION__); } while(0)

#endif
