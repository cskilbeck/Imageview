//////////////////////////////////////////////////////////////////////

#include "pch.h"

namespace
{
    //////////////////////////////////////////////////////////////////////

    std::string convert_wide_text_to_utf8_string(wchar const *text, size_t len)
    {
        std::string result;
        uint size = WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(len), null, 0, null, null);
        if(size != 0) {
            result.resize(size);
            WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(len), result.data(), size, null, null);
        }
        return result;
    }

    //////////////////////////////////////////////////////////////////////

    std::wstring convert_utf8_text_to_wide_string(char const *text, size_t len)
    {
        std::wstring result;
        uint size = MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>(len), null, 0);
        if(size != 0) {
            result.resize(size);
            MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>(len), result.data(), size);
        }
        return result;
    }

    //////////////////////////////////////////////////////////////////////

    std::string convert_wide_text_to_ascii_string(wchar const *text, size_t len)
    {
        std::string result;
        uint size = WideCharToMultiByte(CP_ACP, 0, text, static_cast<int>(len), null, 0, null, null);
        if(size != 0) {
            result.resize(size);
            WideCharToMultiByte(CP_ACP, 0, text, static_cast<int>(len), result.data(), size, null, null);
        }
        return result;
    }
}

namespace imageview
{
    //////////////////////////////////////////////////////////////////////

    HRESULT load_resource(DWORD id, char const *type, void **buffer, size_t *size)
    {
        if(buffer == null || size == null || type == null || id == 0) {
            return E_INVALIDARG;
        }


        HRSRC rsrc;
        CHK_NULL(rsrc = FindResourceA(app::instance, MAKEINTRESOURCEA(id), type));

        size_t len;
        CHK_ZERO(len = SizeofResource(app::instance, rsrc));

        HGLOBAL mem;
        CHK_NULL(mem = LoadResource(app::instance, rsrc));

        void *data;
        CHK_NULL(data = LockResource(mem));

        *buffer = data;
        *size = len;

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // append helps because then we can prepend a BITMAPFILEHEADER
    // when we're loading the clipboard and pretend it's a file

    HRESULT append_clipboard_to_buffer(std::vector<byte> &buffer, UINT format)
    {
        CHK_BOOL(OpenClipboard(null));

        DEFER(CloseClipboard());

        HANDLE c = GetClipboardData(format);
        if(c == null) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        void *data = GlobalLock(c);
        if(data == null) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        DEFER(GlobalUnlock(c));

        size_t size = GlobalSize(c);
        if(size == 0) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        size_t existing = buffer.size();
        buffer.resize(size + existing);
        memcpy(buffer.data() + existing, data, size);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    RECT center_rect_on_default_monitor(RECT const &r)
    {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        int ww = r.right - r.left;
        int wh = r.bottom - r.top;
        RECT rc;
        rc.left = (sw - ww) / 2;
        rc.top = (sh - wh) / 2;
        rc.right = rc.left + ww;
        rc.bottom = rc.top + wh;
        return rc;
    }

    //////////////////////////////////////////////////////////////////////

    std::wstring unicode(std::string s)
    {
        return convert_utf8_text_to_wide_string(s.c_str(), s.size());
    }

    std::wstring unicode(char const *s, size_t len)
    {
        return convert_utf8_text_to_wide_string(s, len);
    }

    std::wstring unicode(char const *s)
    {
        return convert_utf8_text_to_wide_string(s, strlen(s));
    }

    //////////////////////////////////////////////////////////////////////

    std::string utf8(std::wstring s)
    {
        return convert_wide_text_to_utf8_string(s.c_str(), s.size());
    }

    std::string utf8(wchar const *s, size_t len)
    {
        return convert_wide_text_to_utf8_string(s, len);
    }

    std::string utf8(wchar const *s)
    {
        return convert_wide_text_to_utf8_string(s, wcslen(s));
    }

    //////////////////////////////////////////////////////////////////////

    std::string ascii(std::wstring s)
    {
        return convert_wide_text_to_ascii_string(s.c_str(), s.size());
    }

    std::string ascii(wchar const *s, size_t len)
    {
        return convert_wide_text_to_ascii_string(s, len);
    }

    std::string ascii(wchar const *s)
    {
        return convert_wide_text_to_ascii_string(s, wcslen(s));
    }

    //////////////////////////////////////////////////////////////////////

    int message_box(HWND hwnd, std::string const &text, uint buttons)
    {
        std::wstring title = unicode(localize(IDS_AppName));
        return MessageBoxW(hwnd, unicode(text).c_str(), title.c_str(), buttons);
    }

    //////////////////////////////////////////////////////////////////////

    void display_error(std::string const &message, HRESULT hr)
    {
        if(hr == 0) {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
        std::string err = std::format("{}\r\n{}\r\n{}", localize(IDS_ERROR), message, windows_error_message(hr));
        message_box(null, err, MB_ICONEXCLAMATION);
        log_win32_error(hr, err.c_str());
    }

    //////////////////////////////////////////////////////////////////////

    uint32 color_to_uint32(vec4 color)
    {
        uint r = static_cast<uint>(XMVectorGetX(color) * 255.0f);
        uint g = static_cast<uint>(XMVectorGetY(color) * 255.0f);
        uint b = static_cast<uint>(XMVectorGetZ(color) * 255.0f);
        uint a = static_cast<uint>(XMVectorGetW(color) * 255.0f);
        return (a << 24) | (b << 16) | (g << 8) | r;
    }

    //////////////////////////////////////////////////////////////////////

    vec4 color_from_uint32(uint32 color)
    {
        uint a = (color >> 24) & 0xff;
        uint b = (color >> 16) & 0xff;
        uint g = (color >> 8) & 0xff;
        uint r = (color >> 0) & 0xff;
        return { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
    }

    //////////////////////////////////////////////////////////////////////

    static constexpr char const *hex_digits = "0123456789ABCDEF";

    std::string color32_to_string(uint32 color)
    {
        std::string c;
        c.reserve(8);
        for(uint i = 0; i < 4; ++i) {
            c.push_back(hex_digits[(color >> 4) & 0x0f]);
            c.push_back(hex_digits[color & 0x0f]);
            color >>= 8;
        }
        return c;
    }

    //////////////////////////////////////////////////////////////////////

    std::string color24_to_string(uint32 color)
    {
        std::string c;
        c.reserve(6);
        for(uint i = 0; i < 3; ++i) {
            c.push_back(hex_digits[(color >> 4) & 0x0f]);
            c.push_back(hex_digits[color & 0x0f]);
            color >>= 8;
        }
        return c;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT color_from_string(std::string const &text, uint32 &color)
    {
        auto nibble_from_char = [](int c, int &x) {
            if(c >= '0' && c <= '9') {
                x = c - '0';
            } else {
                c = tolower(c);
                if(c >= 'a' && c <= 'f') {
                    x = (c - 'a') + 10;
                } else {
                    return false;
                }
            }
            return true;
        };

        uint32 new_color = 0;
        uint32 alpha = 0;

        if(text.size() == 6) {
            alpha = 0xff000000;

        } else if(text.size() != 8) {
            return E_INVALIDARG;
        }

        uint shift[8] = { 4, 0, 12, 8, 20, 16, 28, 24 };

        for(size_t i = 0; i < text.size(); ++i) {
            int x;
            if(!nibble_from_char(text[i], x)) {
                return E_INVALIDARG;
            }
            new_color |= x << shift[i];
        }
        color = new_color | alpha;
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    std::string hex_string_from_bytes(byte const *data, size_t len)
    {
        std::string result;
        result.resize(len * 2);
        for(size_t i = 0; i < len; ++i) {
            std::format_to_n(result.begin() + i * 2, 2, "{:02x}", data[i]);
        }
        return result;
    }

    //////////////////////////////////////////////////////////////////////
    // get a localized string by id

    std::string localize(uint id)
    {
        static std::unordered_map<uint, std::string> localized_strings;

        auto f = localized_strings.find(id);

        if(f != localized_strings.end()) {
            return f->second;
        }

        // For some reason LoadStringA doesn't work...?

        wchar *str;
        int len = LoadStringW(app::instance, id, reinterpret_cast<wchar *>(&str), 0);

        if(len <= 0) {
            return std::format("UNLOCALIZED_{}", id);
        }
        return localized_strings.insert({ id, utf8(str, len) }).first->second;
    }

    //////////////////////////////////////////////////////////////////////

    std::string strip_quotes(std::string const &s)
    {
        if(s.front() != '"' || s.back() != '"') {
            return s;
        }
        return s.substr(1, s.size() - 2);
    }

    //////////////////////////////////////////////////////////////////////

    std::string windows_error_message(uint32 err)
    {
        if(err == 0) {
            err = GetLastError();
        }
        return std::string(_com_error(HRESULT_FROM_WIN32(err)).ErrorMessage());
    }

    //////////////////////////////////////////////////////////////////////

    static HRESULT log_win32_error_v(DWORD err, char const *message, va_list v)
    {
        LOG_CONTEXT("win32");

        char buffer[4096];
        _vsnprintf_s(buffer, _countof(buffer), message, v);
        HRESULT r = HRESULT_FROM_WIN32(err);
        std::string err_str = windows_error_message(r);
        LOG_ERROR("ERROR {:08x} ({}) {}", err, buffer, err_str);
        return r;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT log_win32_error(DWORD err, char const *message, ...)
    {
        va_list v;
        va_start(v, message);
        return log_win32_error_v(err, message, v);
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT log_win32_error(char const *message, ...)
    {
        va_list v;
        va_start(v, message);
        return log_win32_error_v(GetLastError(), message, v);
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_is_process_elevated(bool &is_elevated)
    {
        HANDLE hToken = NULL;
        TOKEN_ELEVATION elevation;
        DWORD dwSize;
        CHK_BOOL(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken));
        DEFER(CloseHandle(hToken));
        CHK_BOOL(GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize));
        is_elevated = elevation.TokenIsElevated;
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // for dpi, win8 and newer have a better way, use it if available

    float get_window_dpi(HWND w)
    {
        UINT dpi = 0;

        HMODULE h = LoadLibraryA("user32.dll");
        if(h != null) {

            typedef UINT (*GetDpiForWindowFN)(HWND w);
            GetDpiForWindowFN get_dpi_fn = reinterpret_cast<GetDpiForWindowFN>(GetProcAddress(h, "GetDpiForWindow"));
            if(get_dpi_fn != null) {
                dpi = get_dpi_fn(w);
            }
            FreeLibrary(h);
        }

        if(dpi == 0) {
            HDC dc = GetDC(null);
            dpi = (UINT)GetDeviceCaps(dc, LOGPIXELSX);
            ReleaseDC(null, dc);
        }

        return (float)dpi;
    }

    //////////////////////////////////////////////////////////////////////

    std::string get_app_filename()
    {
        std::string path;
        char exe_path[MAX_PATH * 2];
        uint32 got = GetModuleFileNameA(NULL, exe_path, _countof(exe_path));
        if(got != 0 && got != ERROR_INSUFFICIENT_BUFFER) {
            path = std::string(exe_path);
        }
        return path;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_app_version(std::string &version)
    {
        std::string app_filename = get_app_filename();

        DWORD dummy;
        uint32 len = GetFileVersionInfoSizeA(app_filename.c_str(), &dummy);

        if(len == 0) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        std::vector<byte> buffer;
        buffer.resize(len);

        CHK_BOOL(GetFileVersionInfoA(get_app_filename().c_str(), 0, len, buffer.data()));

        VS_FIXEDFILEINFO *version_info;
        uint version_size;
        CHK_BOOL(VerQueryValueA(buffer.data(), "\\", reinterpret_cast<void **>(&version_info), &version_size));

        uint32 vh = version_info->dwFileVersionMS;
        uint32 vl = version_info->dwFileVersionLS;

        version = std::format("{}.{}.{}.{}", (vh >> 16) & 0xffff, vh & 0xffff, (vl >> 16) & 0xffff, vl & 0xffff);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT copy_string_to_clipboard(std::string const &string)
    {
        SetLastError(0);

        HANDLE handle;
        CHK_NULL(handle = GlobalAlloc(GHND | GMEM_SHARE, string.size() + 1));

        char *buffer;
        CHK_NULL(buffer = reinterpret_cast<char *>(GlobalLock(handle)));
        DEFER(GlobalUnlock(handle));

        memcpy(buffer, string.c_str(), string.size() + 1);

        CHK_BOOL(OpenClipboard(null));
        DEFER(CloseClipboard());

        CHK_BOOL(EmptyClipboard());
        CHK_BOOL(SetClipboardData(CF_TEXT, handle));

        return S_OK;
    }
}
