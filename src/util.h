#pragma once

namespace imageview
{
    //////////////////////////////////////////////////////////////////////
    // get the name of a WM_ windows message

#if defined(_DEBUG)
    std::string get_wm_name(uint32 uMsg);
#endif

    //////////////////////////////////////////////////////////////////////
    // error reporting

    std::string windows_error_message(uint32 err = 0);
    HRESULT log_win32_error(DWORD err, char const *message, ...);
    HRESULT log_win32_error(char const *message, ...);
    void display_error(std::string const &message, HRESULT hr = 0);
    int message_box(HWND hwnd, std::string const &text, uint buttons);

    //////////////////////////////////////////////////////////////////////
    // color admin

    uint32 color_to_uint32(vec4 color);
    vec4 color_from_uint32(uint32 color);
    HRESULT color_from_string(std::string const &hex_text, uint32 &color);
    std::string color32_to_string(uint32 color);
    std::string color24_to_string(uint32 color);

    //////////////////////////////////////////////////////////////////////
    // app admin

    HRESULT get_is_process_elevated(bool &is_elevated);
    float get_window_dpi(HWND w);
    std::string get_app_filename();
    HRESULT get_app_version(std::string &version);

    //////////////////////////////////////////////////////////////////////

    HRESULT load_resource(DWORD id, char const *type, void **buffer, size_t *size);

    //////////////////////////////////////////////////////////////////////
    // string

    std::string localize(uint id);
    std::string strip_quotes(std::string const &s);

    std::wstring unicode(std::string s);
    std::wstring unicode(char const *s, size_t len);
    std::wstring unicode(char const *s);

    std::string utf8(std::wstring s);
    std::string utf8(wchar const *s, size_t len);
    std::string utf8(wchar const *s);

    std::string ascii(std::wstring s);
    std::string ascii(wchar const *s, size_t len);
    std::string ascii(wchar const *s);

    //////////////////////////////////////////////////////////////////////
    // upper/lower case - these are not valid for many unicode pages!

    template <typename T> T make_lowercase(T const &str)
    {
        T n{ str };
        std::transform(
            n.begin(), n.end(), n.begin(), [](T::value_type x) { return static_cast<T::value_type>(::tolower(x)); });
        return n;
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> T make_uppercase(T const &str)
    {
        T n{ str };
        std::transform(
            n.begin(), n.end(), n.begin(), [](T::value_type x) { return static_cast<T::value_type>(::toupper(x)); });
        return n;
    }

    //////////////////////////////////////////////////////////////////////
    // helpers

    inline int get_x(LPARAM lp)
    {
        return static_cast<int>(LOWORD(lp));
    }

    inline int get_y(LPARAM lp)
    {
        return static_cast<int>(HIWORD(lp));
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> void mem_clear(T *o)
    {
        memset(o, 0, sizeof(T));
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> T sub_point(T a, T b)
    {
        using Tx = decltype(a.x);
        return { static_cast<Tx>(a.x - b.x), static_cast<Tx>(a.y - b.y) };
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> T add_point(T a, T b)
    {
        using Tx = decltype(a.x);
        return { static_cast<Tx>(a.x + b.x), static_cast<Tx>(a.y + b.y) };
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> T mul_point(T a, T b)
    {
        using Tx = decltype(a.x);
        return { static_cast<Tx>(a.x * b.x), static_cast<Tx>(a.y * b.y) };
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> T div_point(T a, T b)
    {
        using Tx = decltype(a.x);
        return { static_cast<Tx>(a.x / b.x), static_cast<Tx>(a.y / b.y) };
    }

    //////////////////////////////////////////////////////////////////////

    inline int rect_width(RECT const &r)
    {
        return r.right - r.left;
    }

    //////////////////////////////////////////////////////////////////////

    inline int rect_height(RECT const &r)
    {
        return r.bottom - r.top;
    }

    //////////////////////////////////////////////////////////////////////

    inline POINT rect_top_left(RECT const &r)
    {
        return { r.left, r.top };
    }

    //////////////////////////////////////////////////////////////////////

    inline SIZE rect_size(RECT const &r)
    {
        return { rect_width(r), rect_height(r) };
    }

    //////////////////////////////////////////////////////////////////////

    inline std::string rect_to_string(RECT const &r)
    {
        return std::format("{},{} ({}x{})", r.left, r.top, rect_width(r), rect_height(r));
    }
}

//////////////////////////////////////////////////////////////////////
// error checking macros

#define _DO_HR(x, y)                                   \
    {                                                  \
        do {                                           \
            HRESULT hr##y = (x);                       \
            if(FAILED(hr##y)) {                        \
                imageview::log_win32_error(hr##y, #x); \
                return hr##y;                          \
            }                                          \
        } while(false);                                \
    }

#define _DO_BOOL(x, y)                                  \
    {                                                   \
        do {                                            \
            bool hr##y = (x);                           \
            if(!(hr##y)) {                              \
                DWORD gle##y = GetLastError();          \
                imageview::log_win32_error(gle##y, #x); \
                return HRESULT_FROM_WIN32(gle##y);      \
            }                                           \
        } while(false);                                 \
    }

#define _DO_NULL(x, y)                                \
    {                                                 \
        do {                                          \
            auto hr##y = (x);                         \
            if(hr##y == (decltype(hr##y))null) {      \
                DWORD gle##y = GetLastError();        \
                imageview::display_error(#x, gle##y); \
                return HRESULT_FROM_WIN32(gle##y);    \
            }                                         \
        } while(false);                               \
    }

// if(FAILED(x)) { return hresult; }
#define CHK_HR(x) _DO_HR(x, __COUNTER__)

// if(!x) { return HRESULT_FROM_WIN32(GetLastError()); }
#define CHK_BOOL(x) _DO_BOOL(x, __COUNTER__)

// if(x == 0) { return HRESULT_FROM_WIN32(GetLastError()); }
#define CHK_ZERO(x) _DO_BOOL(x, __COUNTER__)

// if(x == null) { return HRESULT_FROM_WIN32(GetLastError()); }
#define CHK_NULL(x) _DO_NULL(x, __COUNTER__)

//////////////////////////////////////////////////////////////////////
// COM helpers for font_loader and drag_drop helper

template <class T> void SafeRelease(T **ppT)
{
    if(*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

//////////////////////////////////////////////////////////////////////

template <class T> HRESULT SetInterface(T **ppT, IUnknown *punk)
{
    SafeRelease(ppT);

    if(punk == null) {
        return E_NOINTERFACE;
    }

    return punk->QueryInterface(ppT);
}

//////////////////////////////////////////////////////////////////////

template <typename InterfaceType> InterfaceType *SafeAcquire(InterfaceType *newObject)
{
    if(newObject != NULL)
        newObject->AddRef();

    return newObject;
}

//////////////////////////////////////////////////////////////////////

template <typename InterfaceType> void SafeSet(InterfaceType **currentObject, InterfaceType *newObject)
{
    SafeAcquire(newObject);
    SafeRelease(&currentObject);
    currentObject = newObject;
}

//////////////////////////////////////////////////////////////////////
// These are missing from windowsx.h

#define HANDLE_WM_DPICHANGED(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (UINT)LOWORD(wParam), (UINT)HIWORD(wParam), (RECT const *)(lParam)), 0L)

#define HANDLE_WM_INPUT(hwnd, wParam, lParam, fn) ((fn)((hwnd), (HRAWINPUT)(lParam)), 0L)

#define HANDLE_WM_ENTERSIZEMOVE(hwnd, wParam, lParam, fn) ((fn)((hwnd)), 0L)

#define HANDLE_WM_EXITSIZEMOVE(hwnd, wParam, lParam, fn) ((fn)((hwnd)), 0L)

#define HANDLE_WM_POWERBROADCAST(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (UINT)(wParam), (PPOWERBROADCAST_SETTING)(lParam)))
