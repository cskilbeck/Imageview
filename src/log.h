#pragma once

std::wstring windows_error_message(uint32_t err);
void write_line_to_file(char const *txt, uint32_t len);
void write_line_to_file(wchar_t const *txt, uint32_t len);

#if defined(_DEBUG)
void Log(char const *fmt, ...);
void Log(wchar_t const *fmt, ...);
#else
#define Log(...) \
    do {         \
    } while(0)

#endif

HRESULT log_win32_error(DWORD err, char const *message, va_list v);
HRESULT log_win32_error(DWORD err, char const *message, ...);
HRESULT log_win32_error(DWORD err, wchar_t const *message, ...);
HRESULT log_win32_error(wchar_t const *message, ...);
HRESULT log_win32_error(char const *message, ...);

std::wstring format_v(wchar_t const *fmt, va_list v);
std::string format_v(char const *fmt, va_list v);

std::wstring format(wchar_t const *fmt, ...);
std::string format(char const *fmt, ...);
