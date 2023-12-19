#pragma once

namespace imageview
{
    //////////////////////////////////////////////////////////////////////
    // get the name of a WM_ windows message

#if defined(_DEBUG)
    std::wstring get_wm_name(uint32 uMsg);
#endif

    //////////////////////////////////////////////////////////////////////
    // error reporting

    std::wstring windows_error_message(uint32 err = 0);
    HRESULT log_win32_error(wchar const *message, DWORD err = 0);
    HRESULT display_error(std::wstring const &message, HRESULT hr = (HRESULT)0);
    int message_box(HWND hwnd, std::wstring const &text, uint buttons);

    //////////////////////////////////////////////////////////////////////
    // color admin

    uint32 color_to_uint32(vec4 color);
    vec4 color_from_uint32(uint32 color);
    HRESULT color_from_string(std::wstring const &hex_text, uint32 &color);
    std::wstring color32_to_string(uint32 color);
    std::wstring color24_to_string(uint32 color);
    uint32 color_lerp(uint32 ca, uint32 cb, int x);

    HRESULT nibble_from_char(int c, int &x);

    //////////////////////////////////////////////////////////////////////
    // app admin

    HRESULT get_is_process_elevated(bool &is_elevated);
    float get_window_dpi(HWND w);
    std::wstring get_app_filename();
    HRESULT get_app_version(std::wstring &version);
    HRESULT copy_string_to_clipboard(std::wstring const &string);

    //////////////////////////////////////////////////////////////////////

    HRESULT load_resource(DWORD id, wchar const *type, void **buffer, size_t *size);

    //////////////////////////////////////////////////////////////////////
    // string

    std::wstring localize(uint id);
    std::wstring strip_quotes(std::wstring const &s);

    std::wstring unicode(std::string s);
    std::wstring unicode(char const *s, size_t len);
    std::wstring unicode(char const *s);

    std::string utf8(std::wstring s);
    std::string utf8(wchar const *s, size_t len);
    std::string utf8(wchar const *s);

    std::string ascii(std::wstring s);
    std::string ascii(wchar const *s, size_t len);
    std::string ascii(wchar const *s);

    std::wstring hex_string_from_bytes(byte const *data, size_t len);

    //////////////////////////////////////////////////////////////////////
    // upper/lower case - these are not valid for many unicode pages!

    template <typename T> T make_lowercase(T const &str)
    {
        std::locale l("");
        T r;
        r.reserve(str.size());
        for(auto c : str) {
            r.push_back(static_cast<decltype(c)>(std::tolower(c, l)));
        }
        return r;
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> T make_uppercase(T const &str)
    {
        std::locale l("");
        T r;
        r.reserve(str.size());
        for(auto c : str) {
            r.push_back(static_cast<decltype(c)>(std::toupper(c, l)));
        }
        return r;
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

    inline POINT rect_midpoint(RECT const &r)
    {
        return { (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
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

    inline std::wstring rect_to_string(RECT const &r)
    {
        return std::format(L"{},{} ({}x{})", r.left, r.top, rect_width(r), rect_height(r));
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> int sgn(T x)
    {
        return (T(0) < x) - (x < T(0));
    }

    //////////////////////////////////////////////////////////////////////

    enum tokenize_option
    {
        discard_empty = false,
        keep_empty = true
    };

    template <class container_t, class string_t, class char_t>
    void tokenize(string_t const &str,
                  container_t &tokens,
                  char_t const *delimiters,
                  tokenize_option option = keep_empty)
    {
        typename string_t::size_type end = 0, start = 0, len = str.size();
        while(end < len) {
            end = str.find_first_of(delimiters, start);
            if(end == string_t::npos) {
                end = len;
            }
            if(end != start || option == keep_empty) {
                tokens.push_back(container_t::value_type(str.data() + start, end - start));
            }
            start = end + 1;
        }
    }
}

//////////////////////////////////////////////////////////////////////
// error checking macros

#define _DO_HR(x, y)                                       \
    do {                                                   \
        HRESULT hr##y = (x);                               \
        if(FAILED(hr##y)) {                                \
            return imageview::log_win32_error(L#x, hr##y); \
        }                                                  \
    } while(false)

#define _DO_BOOL(x, y)                                      \
    do {                                                    \
        bool hr##y = (x);                                   \
        if(!(hr##y)) {                                      \
            DWORD gle##y = GetLastError();                  \
            return imageview::log_win32_error(L#x, gle##y); \
        }                                                   \
    } while(false)

#define _DO_NULL(x, y)                            \
    do {                                          \
        auto hr##y = (x);                         \
        if(hr##y == (decltype(hr##y))null) {      \
            return imageview::display_error(L#x); \
        }                                         \
    } while(false)

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
// These window message crackers are missing from windowsx.h

#define HANDLE_WM_DPICHANGED(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (UINT)LOWORD(wParam), (UINT)HIWORD(wParam), (RECT const *)(lParam)), 0L)

#define HANDLE_WM_INPUT(hwnd, wParam, lParam, fn) ((fn)((hwnd), (HRAWINPUT)(lParam)), 0L)

#define HANDLE_WM_ENTERSIZEMOVE(hwnd, wParam, lParam, fn) ((fn)((hwnd)), 0L)

#define HANDLE_WM_EXITSIZEMOVE(hwnd, wParam, lParam, fn) ((fn)((hwnd)), 0L)

#define HANDLE_WM_POWERBROADCAST(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (UINT)(wParam), (PPOWERBROADCAST_SETTING)(lParam)))

#define HANDLE_WM_USER(hwnd, wParam, lParam, fn) ((fn)((hwnd), wParam, lParam), 0L)
