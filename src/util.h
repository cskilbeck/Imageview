#pragma once

//////////////////////////////////////////////////////////////////////

#include <DirectXMath.h>

using namespace DirectX;

struct vec2;

// share some types with the HLSL header
using matrix = XMMATRIX;
using vec4 = XMVECTOR;
using uint = uint32_t;
using float4 = vec4;
using float2 = vec2;
using int4 = int[4];

using Microsoft::WRL::ComPtr;

constexpr nullptr_t null = nullptr;

//////////////////////////////////////////////////////////////////////

// get the name of a WM_ windows message
#if defined(_DEBUG)
wchar_t const *get_wm_name(uint32_t uMsg);
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

std::wstring format_v(wchar_t const *fmt, va_list v);
std::string format_v(char const *fmt, va_list v);

std::wstring format(wchar_t const *fmt, ...);
std::string format(char const *fmt, ...);

std::wstring windows_error_message(uint32_t err = 0);

HRESULT log_win32_error(DWORD err, wchar_t const *message, ...);
HRESULT log_win32_error(wchar_t const *message, ...);
HRESULT log_win32_error(DWORD err, wchar_t const *message, va_list v);

//////////////////////////////////////////////////////////////////////
// general utility functions

HRESULT load_file(std::wstring filename, std::vector<byte> &buffer, HANDLE cancel_event = null);
HRESULT append_clipboard_to_buffer(std::vector<byte> &buffer, UINT format);
HRESULT get_file_id(wchar_t const *filename, uint32_t *volume_id, uint64_t *id);
HRESULT load_resource(DWORD id, wchar_t const *type, void **buffer, size_t *size);
HRESULT load_bitmap(wchar_t const *filename, IWICBitmapFrameDecode **decoder);
HRESULT create_shell_item_from_object(IUnknown *punk, REFIID riid, void **ppv);
HRESULT select_file_dialog(std::wstring &path);
HRESULT select_color_dialog(HWND window, uint32_t &color, wchar_t const *dialog_title);
RECT center_rect_on_default_monitor(RECT const &r);

HRESULT file_get_full_path(wchar_t const *filename, std::wstring &fullpath);

HRESULT file_get_path(wchar_t const *filename, std::wstring &path);
HRESULT file_get_filename(wchar_t const *filename, std::wstring &name);
HRESULT file_get_extension(wchar_t const *filename, std::wstring &extension);

BOOL file_exists(wchar_t const *name);

uint32_t color_to_uint32(vec4 color);
vec4 uint32_to_color(uint32_t color);

std::wstring strip_quotes(wchar_t const *s);

//////////////////////////////////////////////////////////////////////
// localization

wchar_t const *localize(uint id);
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
    file_info(std::wstring const &n, uint64_t d) throw() : name(n), date(d)
    {
    }

    std::wstring name;
    uint64_t date;
};

struct folder_scan_result
{
    std::wstring path;
    std::vector<file_info> files;
};

HRESULT scan_folder2(wchar_t const *path, std::vector<wchar_t const *> extensions, scan_folder_sort_field sort_field, scan_folder_sort_order order, folder_scan_result **result,
                     HANDLE cancel_event);
HRESULT scan_folder(wchar_t const *path, std::vector<wchar_t const *> extensions, scan_folder_sort_field sort_field, scan_folder_sort_order order, std::vector<file_info> &files);

//////////////////////////////////////////////////////////////////////

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

__declspec(selectany) std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> wstr_converter;

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

inline std::string str_from_wide(wchar_t const *s, size_t len)
{
    return wstr_converter.to_bytes(s, s + len);
}

inline std::wstring wide_from_str(char const *s)
{
    return wstr_converter.from_bytes(s, s + strlen(s));
}

inline std::string str_from_wide(wchar_t const *s)
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
// error checking macros

inline void display_error(wchar_t const *message, HRESULT hr)
{
    MessageBoxW(null, format(L"Error:\n\n%s\n\n%s", message, windows_error_message(hr).c_str()).c_str(), localize(IDS_AppName), MB_ICONEXCLAMATION);
    log_win32_error(hr, message);
}

//////////////////////////////////////////////////////////////////////

#define DO_CHECK(x, y)                     \
    {                                      \
        do {                               \
            HRESULT hr##y = (x);           \
            if(FAILED(hr##y)) {            \
                display_error(L#x, hr##y); \
                return 1;                  \
            }                              \
        } while(false);                    \
    }

#define DO_HR(x, y)                          \
    {                                        \
        do {                                 \
            HRESULT hr##y = (x);             \
            if(FAILED(hr##y)) {              \
                log_win32_error(hr##y, L#x); \
                return hr##y;                \
            }                                \
        } while(false);                      \
    }

#define DO_BOOL(x, y)                              \
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

#define DO_NULL(x, y)                              \
    {                                              \
        do {                                       \
            auto hr##y = (x);                      \
            if(hr##y == (decltype(x))null) {       \
                DWORD gle##y = GetLastError();     \
                display_error(L#x, gle##y);        \
                return HRESULT_FROM_WIN32(gle##y); \
            }                                      \
        } while(false);                            \
    }

#define CHECK(x) DO_CHECK(x, __COUNTER__)
#define CHECK(x) DO_CHECK(x, __COUNTER__)
#define CHK_HR(x) DO_HR(x, __COUNTER__)
#define CHK_BOOL(x) DO_BOOL(x, __COUNTER__)
#define CHK_NULL(x) DO_NULL(x, __COUNTER__)
