#pragma once

void write_line_to_file(char const *txt, uint32_t len);
void write_line_to_file(wchar_t const *txt, uint32_t len);

#if LOG_ENABLED
void Log(char const *fmt, ...);
void Log(wchar_t const *fmt, ...);
#else
#define Log(...) \
    do {         \
    } while(0)

#endif

