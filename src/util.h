#pragma once

namespace imageview
{
    //////////////////////////////////////////////////////////////////////
    // get the name of a WM_ windows message

#if defined(_DEBUG)
    char const *get_wm_name(uint32 uMsg);
#endif

    //////////////////////////////////////////////////////////////////////
    // win32 error messages

    std::string windows_error_message(uint32 err = 0);

    HRESULT log_win32_error(DWORD err, char const *message, ...);
    HRESULT log_win32_error(char const *message, ...);

    //////////////////////////////////////////////////////////////////////

    HRESULT append_clipboard_to_buffer(std::vector<byte> &buffer, UINT format);

    RECT center_rect_on_default_monitor(RECT const &r);
    uint32 color_to_uint32(vec4 color);
    vec4 uint32_to_color(uint32 color);

    std::string strip_quotes(std::string const &s);

    HRESULT load_resource(DWORD id, char const *type, void **buffer, size_t *size);

    HRESULT get_accelerator_hotkey_text(uint id, std::vector<ACCEL> const &accel_table, HKL layout, std::string &text);
    HRESULT copy_accelerator_table(HACCEL h, std::vector<ACCEL> &table);

    HRESULT get_is_process_elevated(bool &is_elevated);
    float get_window_dpi(HWND w);
    std::string const &localize(uint64 id);

    std::string get_app_filename();
    HRESULT get_app_version(std::string &version);

    //////////////////////////////////////////////////////////////////////

    inline int get_x(LPARAM lp)
    {
        return static_cast<int>(static_cast<short>(LOWORD(lp)));
    }

    inline int get_y(LPARAM lp)
    {
        return static_cast<int>(static_cast<short>(HIWORD(lp)));
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

    template <typename T> void make_uppercase(T &str)
    {
        std::transform(str.begin(), str.end(), str.begin(), [](T::value_type x) {
            return static_cast<T::value_type>(::toupper(x));
        });
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> void make_lowercase(T &str)
    {
        std::transform(str.begin(), str.end(), str.begin(), [](T::value_type x) {
            return static_cast<T::value_type>(::tolower(x));
        });
    }

    //////////////////////////////////////////////////////////////////////
    // string conversion

    std::string convert_wide_text_to_utf8_string(wchar const *text, size_t len);
    std::string convert_wide_text_to_ascii_string(wchar const *text, size_t len);
    std::wstring convert_utf8_text_to_wide_string(char const *text, size_t len);

    //////////////////////////////////////////////////////////////////////

    inline std::wstring unicode(std::string s)
    {
        return convert_utf8_text_to_wide_string(s.c_str(), s.size());
    }

    inline std::wstring unicode(char const *s, size_t len)
    {
        return convert_utf8_text_to_wide_string(s, len);
    }

    inline std::wstring unicode(char const *s)
    {
        return convert_utf8_text_to_wide_string(s, strlen(s));
    }

    //////////////////////////////////////////////////////////////////////

    inline std::string utf8(std::wstring s)
    {
        return convert_wide_text_to_utf8_string(s.c_str(), s.size());
    }

    inline std::string utf8(wchar const *s, size_t len)
    {
        return convert_wide_text_to_utf8_string(s, len);
    }

    inline std::string utf8(wchar const *s)
    {
        return convert_wide_text_to_utf8_string(s, wcslen(s));
    }

    //////////////////////////////////////////////////////////////////////

    inline std::string ascii(std::wstring s)
    {
        return convert_wide_text_to_ascii_string(s.c_str(), s.size());
    }

    inline std::string ascii(wchar const *s, size_t len)
    {
        return convert_wide_text_to_ascii_string(s, len);
    }

    inline std::string ascii(wchar const *s)
    {
        return convert_wide_text_to_ascii_string(s, wcslen(s));
    }

    //////////////////////////////////////////////////////////////////////

    inline void display_error(std::string const &message, HRESULT hr)
    {
        std::string err = std::format("Error:\n\n{}\n\n{}", message, windows_error_message(hr));
        MessageBoxA(null, err.c_str(), localize(IDS_AppName).c_str(), MB_ICONEXCLAMATION);
        log_win32_error(hr, err.c_str());
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
