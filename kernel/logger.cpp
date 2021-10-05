#include "logger.hpp"

#include <cstdio>

#include "console.hpp"

// Default log level
namespace {
	LogLevel log_level = kWarn;
}

// Output log by the console
extern Console* console;

void SetLogLevel(LogLevel level) {
	log_level = level;
}

int Log(LogLevel level, const char* format, ...) {
	if (level > log_level) {
		return 0;
	}

	va_list ap;
	int result;
	char s[1024];

	va_start(ap, format);
	result = vsprintf(s, format, ap);
	va_end(ap);

	console->PutString(s);
	return result;
}