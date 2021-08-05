#pragma once

//////////////////////////////////////////////////////////////////////

#include <DirectXMath.h>

using namespace DirectX;

// share some types with the HLSL header
using matrix = XMMATRIX;
using vec4 = XMVECTOR;
using uint = uint32_t;
using float4 = vec4;
using float2 = float[2];
using int4 = int[4];

using Microsoft::WRL::ComPtr;

constexpr nullptr_t null = nullptr;

//////////////////////////////////////////////////////////////////////

// get the name of a WM_ windows message
#if defined(_DEBUG)
TCHAR const *get_wm_name(uint32_t uMsg);
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
// error checking macros

#define _V(A, B) A##B

#define DO_CHECK(x, y)                                                                                                                                     \
    {                                                                                                                                                      \
        do {                                                                                                                                               \
            HRESULT _V(hr, y) = (x);                                                                                                                       \
            if(FAILED(_V(hr, y))) {                                                                                                                        \
                MessageBoxW(null, format(L"Error:\n\n%s\n\n%s", L#x, windows_error_message(_V(hr, y)).c_str()).c_str(), L"ImageView", MB_ICONEXCLAMATION); \
                log_win32_error(_V(hr, y), TEXT(#x));                                                                                                      \
                return 1;                                                                                                                                  \
            }                                                                                                                                              \
        } while(false);                                                                                                                                    \
    }

#define DO_HR(x, y)                                   \
    {                                                 \
        do {                                          \
            HRESULT _V(hr, y) = (x);                  \
            if(FAILED(_V(hr, y))) {                   \
                log_win32_error(_V(hr, y), TEXT(#x)); \
                return _V(hr, y);                     \
            }                                         \
        } while(false);                               \
    }

#define DO_BOOL(x, y)                                  \
    {                                                  \
        do {                                           \
            bool _V(hr, y) = (x);                      \
            if(!(_V(hr, y))) {                         \
                DWORD _V(gle, y) = GetLastError();     \
                log_win32_error(_V(gle, y), TEXT(#x)); \
                return HRESULT_FROM_WIN32(_V(gle, y)); \
            }                                          \
        } while(false);                                \
    }

#define DO_NULL(x, y)                                                                                                                                      \
    {                                                                                                                                                      \
        do {                                                                                                                                               \
            auto _V(hr, y) = (x);                                                                                                                          \
            if(_V(hr, y) == (decltype(x))null) {                                                                                                                        \
                DWORD _V(gle, y) = GetLastError();                                                                                                         \
                MessageBoxW(null, format(L"Error:\n\n%s\n\n%s", L#x, windows_error_message(_V(gle, y)).c_str()).c_str(), L"ImageView", MB_ICONEXCLAMATION); \
                log_win32_error(_V(gle, y), TEXT(#x));                                                                                                     \
                return HRESULT_FROM_WIN32(_V(gle, y));                                                                                                     \
            }                                                                                                                                              \
        } while(false);                                                                                                                                    \
    }

#define CHECK(x) DO_CHECK(x, __COUNTER__)
#define CHK_HR(x) DO_HR(x, __COUNTER__)
#define CHK_BOOL(x) DO_BOOL(x, __COUNTER__)
#define CHK_NULL(x) DO_NULL(x, __COUNTER__)

//////////////////////////////////////////////////////////////////////
// general utility functions

HRESULT load_file(std::wstring const &filename, std::vector<byte> &buffer, HANDLE cancel_event = null, HANDLE complete_event = null);
HRESULT append_clipboard_to_buffer(std::vector<byte> &buffer, UINT format);
HRESULT load_resource(DWORD id, wchar_t const *type, void **buffer, size_t *size);
HRESULT load_bitmap(wchar_t const *filename, IWICBitmapFrameDecode **decoder);
HRESULT create_shell_item_from_object(IUnknown *punk, REFIID riid, void **ppv);
HRESULT select_file_dialog(std::wstring &path);
RECT center_rect_on_default_monitor(RECT const &r);

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

HRESULT scan_folder(wchar_t const *path, std::vector<wchar_t const *> extensions, scan_folder_sort_field sort_field, scan_folder_sort_order order, std::vector<std::wstring> &results);

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
