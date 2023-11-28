#pragma once

//////////////////////////////////////////////////////////////////////
// get the name of a WM_ windows message

#if defined(_DEBUG)
wchar const *get_wm_name(uint32 uMsg);
#endif

//////////////////////////////////////////////////////////////////////
// string formatting

std::wstring format_v(wchar const *fmt, va_list v);
std::string format_v(char const *fmt, va_list v);

std::wstring format(wchar const *fmt, ...);
std::string format(char const *fmt, ...);

//////////////////////////////////////////////////////////////////////
// win32 error messages

std::wstring windows_error_message(uint32 err = 0);

HRESULT log_win32_error(DWORD err, wchar const *message, ...);
HRESULT log_win32_error(wchar const *message, ...);
HRESULT log_win32_error(DWORD err, wchar const *message, va_list v);

//////////////////////////////////////////////////////////////////////
// general utility functions

HRESULT append_clipboard_to_buffer(std::vector<byte> &buffer, UINT format);

RECT center_rect_on_default_monitor(RECT const &r);
uint32 color_to_uint32(vec4 color);
vec4 uint32_to_color(uint32 color);

std::wstring strip_quotes(wchar const *s);

HRESULT load_resource(DWORD id, wchar const *type, void **buffer, size_t *size);

HRESULT get_accelerator_hotkey_text(ACCEL const &accel, HKL layout, std::wstring &text);
HRESULT copy_accelerator_table(HACCEL h, std::vector<ACCEL> &table);
HRESULT get_hotkey_description(ACCEL const &accel, std::wstring &text);

HRESULT get_is_process_elevated(bool &is_elevated);

//////////////////////////////////////////////////////////////////////
// localization

wchar const *localize(uint64 id);
std::wstring const &str_local(uint64 id);

//////////////////////////////////////////////////////////////////////

inline int get_x(LPARAM lp)
{
    return (int)(short)LOWORD(lp);
}

inline int get_y(LPARAM lp)
{
    return (int)(short)HIWORD(lp);
}

//////////////////////////////////////////////////////////////////////
// general

template <typename T> T clamp(T min, T a, T max)
{
    return std::max(min, std::min(a, max));
}

//////////////////////////////////////////////////////////////////////

template <typename T> T sub_point(T a, T b)
{
    return { a.x - b.x, a.y - b.y };
}

//////////////////////////////////////////////////////////////////////

template <typename T> T add_point(T a, T b)
{
    return { a.x + b.x, a.y + b.y };
}

//////////////////////////////////////////////////////////////////////

template <typename T> T mul_point(T a, T b)
{
    return { a.x * b.x, a.y * b.y };
}

//////////////////////////////////////////////////////////////////////

template <typename T> T div_point(T a, T b)
{
    return { a.x / b.x, a.y / b.y };
}

//////////////////////////////////////////////////////////////////////

template <typename T> void make_uppercase(T &str)
{
    std::transform(
        str.begin(), str.end(), str.begin(), [](T::value_type x) { return static_cast<T::value_type>(::toupper(x)); });
}

//////////////////////////////////////////////////////////////////////

template <typename T> void make_lowercase(T &str)
{
    std::transform(
        str.begin(), str.end(), str.begin(), [](T::value_type x) { return static_cast<T::value_type>(::tolower(x)); });
}

//////////////////////////////////////////////////////////////////////
// wide string conversion

__declspec(selectany) std::wstring_convert<std::codecvt_utf8<wchar>, wchar> wstr_converter;

inline std::wstring wide_from_str(std::string s)
{
    return wstr_converter.from_bytes(s);
}

inline std::string str_from_wide(std::wstring s)
{
    return wstr_converter.to_bytes(s);
}

inline std::wstring wide_from_str(char const *s, size_t len)
{
    return wstr_converter.from_bytes(s, s + len);
}

inline std::string str_from_wide(wchar const *s, size_t len)
{
    return wstr_converter.to_bytes(s, s + len);
}

inline std::wstring wide_from_str(char const *s)
{
    return wstr_converter.from_bytes(s, s + strlen(s));
}

inline std::string str_from_wide(wchar const *s)
{
    return wstr_converter.to_bytes(s, s + wcslen(s));
}

//////////////////////////////////////////////////////////////////////

inline void display_error(wchar const *message, HRESULT hr)
{
    std::wstring err = format(L"Error:\n\n%s\n\n%s", message, windows_error_message(hr).c_str());
    MessageBoxW(null, err.c_str(), localize(IDS_AppName), MB_ICONEXCLAMATION);
    log_win32_error(hr, err.c_str());
}

//////////////////////////////////////////////////////////////////////
// error checking macros

#define _DO_CHECK(x, y)                    \
    {                                      \
        do {                               \
            HRESULT hr##y = (x);           \
            if(FAILED(hr##y)) {            \
                display_error(L#x, hr##y); \
                return 1;                  \
            }                              \
        } while(false);                    \
    }

#define _DO_HR(x, y)                         \
    {                                        \
        do {                                 \
            HRESULT hr##y = (x);             \
            if(FAILED(hr##y)) {              \
                log_win32_error(hr##y, L#x); \
                return hr##y;                \
            }                                \
        } while(false);                      \
    }

#define _DO_BOOL(x, y)                             \
    {                                              \
        do {                                       \
            bool hr##y = (x);                      \
            if(!(hr##y)) {                         \
                DWORD gle##y = GetLastError();     \
                log_win32_error(gle##y, L#x);      \
                return HRESULT_FROM_WIN32(gle##y); \
            }                                      \
        } while(false);                            \
    }

#define _DO_NULL(x, y)                             \
    {                                              \
        do {                                       \
            auto hr##y = (x);                      \
            if(hr##y == (decltype(hr##y))null) {   \
                DWORD gle##y = GetLastError();     \
                display_error(L#x, gle##y);        \
                return HRESULT_FROM_WIN32(gle##y); \
            }                                      \
        } while(false);                            \
    }

// if(FAILED(x)) { messagebox(error); return 1; }
#define CHECK(x) _DO_CHECK(x, __COUNTER__)

// if(FAILED(x)) { return hresult; }
#define CHK_HR(x) _DO_HR(x, __COUNTER__)

// if(!x) { return HRESULT_FROM_WIN32(GetLastError()); }
#define CHK_BOOL(x) _DO_BOOL(x, __COUNTER__)

// if(x == null) { return HRESULT_FROM_WIN32(GetLastError()); }
#define CHK_NULL(x) _DO_NULL(x, __COUNTER__)
