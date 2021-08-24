#pragma once

void write_line_to_file(char const *txt, uint32 len);
void write_line_to_file(wchar const *txt, uint32 len);

#if LOG_ENABLED
void Log(char const *fmt, ...);
void Log(wchar const *fmt, ...);
#else
#define Log(...) \
    do {         \
    } while(0)

#endif
