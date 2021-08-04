//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

std::wstring windows_error_message(uint32_t err)
{
    if(err == 0) {
        err = GetLastError();
    }
    return _com_error(HRESULT_FROM_WIN32(err)).ErrorMessage();
}

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

#if defined(_DEBUG)
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

//////////////////////////////////////////////////////////////////////

HRESULT log_win32_error(DWORD err, TCHAR const *message, va_list v)
{
    TCHAR buffer[4096];
    _vsntprintf_s(buffer, _countof(buffer), message, v);
    HRESULT r = HRESULT_FROM_WIN32(err);
    std::wstring err_str = windows_error_message(r);
    Log(TEXT("ERROR %08x (%s) %s"), err, buffer, err_str.c_str());
    return r;
}

//////////////////////////////////////////////////////////////////////

HRESULT log_win32_error(TCHAR const *message, ...)
{
    va_list v;
    va_start(v, message);
    return log_win32_error(GetLastError(), message, v);
}

//////////////////////////////////////////////////////////////////////

HRESULT log_win32_error(DWORD err, TCHAR const *message, ...)
{
    va_list v;
    va_start(v, message);
    return log_win32_error(err, message, v);
}

//////////////////////////////////////////////////////////////////////

std::wstring format_v(wchar_t const *fmt, va_list v)
{
    size_t len = _vscwprintf(fmt, v);
    std::wstring s;
    s.resize(len + 1);
    _vsnwprintf_s(&s[0], len + 1, _TRUNCATE, fmt, v);
    return s;
}

//////////////////////////////////////////////////////////////////////

std::string format_v(char const *fmt, va_list v)
{
    size_t len = _vscprintf(fmt, v);
    std::string s;
    s.resize(len + 1);
    vsnprintf_s(&s[0], len + 1, _TRUNCATE, fmt, v);
    return s;
}

//////////////////////////////////////////////////////////////////////

std::string format(char const *fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    return format_v(fmt, v);
}

//////////////////////////////////////////////////////////////////////

std::wstring format(wchar_t const *fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    return format_v(fmt, v);
}

