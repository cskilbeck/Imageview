#pragma once

//////////////////////////////////////////////////////////////////////

#include <DirectXMath.h>

using namespace DirectX;

struct vec2;

using Microsoft::WRL::ComPtr;

// remove some nonsense

constexpr nullptr_t null = nullptr;

using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

using wchar = wchar_t;

// share some types with the HLSL header

using matrix = XMMATRIX;
using vec4 = XMVECTOR;
using uint = uint32;
using float4 = vec4;
using float2 = vec2;
using int4 = int[4];

//////////////////////////////////////////////////////////////////////
// get the name of a WM_ windows message

#if defined(_DEBUG)
wchar const *get_wm_name(uint32 uMsg);
#endif

//////////////////////////////////////////////////////////////////////
// defer / scoped

template <typename F> class defer_finalizer
{
    F f;
    bool moved;

public:
    template <typename T> defer_finalizer(T &&f_) : f(std::forward<T>(f_)), moved(false)
    {
    }

    defer_finalizer(const defer_finalizer &) = delete;

    defer_finalizer(defer_finalizer &&other) : f(std::move(other.f)), moved(other.moved)
    {
        other.moved = true;
    }

    void cancel()
    {
        moved = true;
    }

    ~defer_finalizer()
    {
        if(!moved) {
            f();
        }
    }
};

template <typename F> defer_finalizer<F> deferred(F &&f)
{
    return defer_finalizer<F>(std::forward<F>(f));
}

static struct
{
    template <typename F> defer_finalizer<F> operator<<(F &&f)
    {
        return defer_finalizer<F>(std::forward<F>(f));
    }
} deferrer;

#define DEFER_TOKENPASTE(x, y) x##y
#define DEFER_TOKENPASTE2(x, y) DEFER_TOKENPASTE(x, y)
#define scoped auto DEFER_TOKENPASTE2(__deferred_lambda_call, __COUNTER__) = deferrer <<
#define defer(X) \
    scoped[=]    \
    {            \
        X;       \
    };

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

HRESULT load_file(std::wstring filename, std::vector<byte> &buffer, HANDLE cancel_event = null);

HRESULT append_clipboard_to_buffer(std::vector<byte> &buffer, UINT format);

HRESULT load_resource(DWORD id, wchar const *type, void **buffer, size_t *size);
HRESULT load_bitmap(wchar const *filename, IWICBitmapFrameDecode **decoder);

HRESULT create_shell_item_from_object(IUnknown *punk, REFIID riid, void **ppv);

HRESULT select_file_dialog(std::wstring &path);
HRESULT select_color_dialog(HWND window, uint32 &color, wchar const *dialog_title);

HRESULT file_get_full_path(wchar const *filename, std::wstring &fullpath);
HRESULT file_get_path(wchar const *filename, std::wstring &path);
HRESULT file_get_filename(wchar const *filename, std::wstring &name);
HRESULT file_get_extension(wchar const *filename, std::wstring &extension);
BOOL file_exists(wchar const *name);

RECT center_rect_on_default_monitor(RECT const &r);
uint32 color_to_uint32(vec4 color);
vec4 uint32_to_color(uint32 color);

std::wstring strip_quotes(wchar const *s);

//////////////////////////////////////////////////////////////////////
// localization

wchar const *localize(uint id);
std::wstring const &str_local(uint id);

//////////////////////////////////////////////////////////////////////
// folder scanner

enum class scan_folder_sort_field
{
    name,
    date,
};

enum class scan_folder_sort_order
{
    ascending,
    descending
};

struct file_info
{
    file_info(std::wstring const &n, uint64 d) throw() : name(n), date(d)
    {
    }

    std::wstring name;
    uint64 date;
};

struct folder_scan_result
{
    std::wstring path;
    std::vector<file_info> files;
};

HRESULT scan_folder2(wchar const *path, std::vector<wchar const *> extensions, scan_folder_sort_field sort_field, scan_folder_sort_order order, folder_scan_result **result,
                     HANDLE cancel_event);
HRESULT scan_folder(wchar const *path, std::vector<wchar const *> extensions, scan_folder_sort_field sort_field, scan_folder_sort_order order, std::vector<file_info> &files);

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
// COM helpers for some external code

template <class T> void SafeRelease(T **ppT)
{
    if(*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

template <class T> HRESULT SetInterface(T **ppT, IUnknown *punk)
{
    SafeRelease(ppT);
    return punk ? punk->QueryInterface(ppT) : E_NOINTERFACE;
}

//////////////////////////////////////////////////////////////////////

inline void display_error(wchar const *message, HRESULT hr)
{
    MessageBoxW(null, format(L"Error:\n\n%s\n\n%s", message, windows_error_message(hr).c_str()).c_str(), localize(IDS_AppName), MB_ICONEXCLAMATION);
    log_win32_error(hr, message);
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

// if(FAILED(x)) { messagebox(error); return hresult; }
#define CHECK(x) _DO_CHECK(x, __COUNTER__)

// if(FAILED(x)) { return hresult; }
#define CHK_HR(x) _DO_HR(x, __COUNTER__)

// if(!x) { return HRESULT_FROM_WIN32(GetLastError()); }
#define CHK_BOOL(x) _DO_BOOL(x, __COUNTER__)

// if(x == null) { return HRESULT_FROM_WIN32(GetLastError()); }
#define CHK_NULL(x) _DO_NULL(x, __COUNTER__)
