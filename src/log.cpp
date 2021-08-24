//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

void write_line_to_file(char const *txt, uint32_t len)
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> lockguard(mutex);

    HANDLE h = CreateFile(TEXT("log.txt"), FILE_APPEND_DATA, FILE_SHARE_READ, null, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, null);
    if(h != INVALID_HANDLE_VALUE) {
        DWORD wrote;
        WriteFile(h, txt, len, &wrote, null);
        WriteFile(h, "\r\n", 2, &wrote, null);
        CloseHandle(h);
    }
}

//////////////////////////////////////////////////////////////////////

void write_line_to_file(wchar_t const *txt, uint32_t len)
{
    std::string s = str_from_wide(txt, len);
    write_line_to_file(s.c_str(), static_cast<uint32_t>(s.length()));
}

//////////////////////////////////////////////////////////////////////

#if LOG_ENABLED
void Log(char const *fmt, ...)
{
    char buffer[4096];
    va_list v;
    va_start(v, fmt);
    time_t t;
    time(&t);
    struct tm tm;
    localtime_s(&tm, &t);
    uint32_t len = static_cast<uint32_t>(strftime(buffer, _countof(buffer), "%d/%m/%Y,%H:%M:%S ", &tm));
    len += _vsnprintf_s(buffer + len, _countof(buffer) - len - 1, _TRUNCATE, fmt, v);
    write_line_to_file(buffer, len);
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
}

//////////////////////////////////////////////////////////////////////

void Log(wchar_t const *fmt, ...)
{
    wchar_t buffer[4096];
    va_list v;
    va_start(v, fmt);
    time_t t;
    time(&t);
    struct tm tm;
    localtime_s(&tm, &t);
    uint32_t len = uint32_t(wcsftime(buffer, _countof(buffer), L"%d/%m/%Y,%H:%M:%S ", &tm));
    len += _vsnwprintf_s(buffer + len, _countof(buffer) - len, _countof(buffer) - len, fmt, v);
    write_line_to_file(buffer, len);
    OutputDebugStringW(buffer);
    OutputDebugStringW(L"\n");
}
#endif

