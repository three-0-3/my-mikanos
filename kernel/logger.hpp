#pragma once

enum LogLevel {
  kError = 3,
  kWarn  = 4,
  kInfo  = 6,
  kDebug = 7,
};

// Set the log level thredhold
// Hereafter, only the logs over the configured value will be input
void SetLogLevel(enum LogLevel level);

// Print the log
int Log(enum LogLevel level, const char* format, ...);