//////////////////////////////////////////////////////////////////////

#include "pch.h"

namespace imageview
{
    //////////////////////////////////////////////////////////////////////

    HRESULT load_resource(DWORD id, char const *type, void **buffer, size_t *size)
    {
        if(buffer == null || size == null || type == null || id == 0) {
            return E_INVALIDARG;
        }

        HINSTANCE instance = GetModuleHandle(null);

        HRSRC rsrc;
        CHK_NULL(rsrc = FindResourceA(instance, MAKEINTRESOURCEA(id), type));

        size_t len;
        CHK_ZERO(len = SizeofResource(instance, rsrc));

        HGLOBAL mem;
        CHK_NULL(mem = LoadResource(instance, rsrc));

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

    uint32 color_to_uint32(vec4 color)
    {
        float *f = reinterpret_cast<float *>(&color);
        int a = (int)(f[0] * 255.0f) & 0xff;
        int b = (int)(f[1] * 255.0f) & 0xff;
        int g = (int)(f[2] * 255.0f) & 0xff;
        int r = (int)(f[3] * 255.0f) & 0xff;
        return (r << 24) | (g << 16) | (b << 8) | a;
    }

    //////////////////////////////////////////////////////////////////////

    vec4 uint32_to_color(uint32 color)
    {
        float a = ((color >> 24) & 0xff) / 255.0f;
        float b = ((color >> 16) & 0xff) / 255.0f;
        float g = ((color >> 8) & 0xff) / 255.0f;
        float r = ((color >> 0) & 0xff) / 255.0f;
        return { r, g, b, a };
    }

    //////////////////////////////////////////////////////////////////////
    // get a localized string by id

    std::string const &localize(uint64 id)
    {
        static std::unordered_map<uint64, std::string> localized_strings;
        static std::string const unknown;

        auto f = localized_strings.find(id);

        if(f != localized_strings.end()) {
            return f->second;
        }

        // For some reason LoadStringA doesn't work...?

        wchar *str;
        int len = LoadStringW(GetModuleHandle(null), static_cast<uint>(id), reinterpret_cast<wchar *>(&str), 0);

        if(len <= 0) {
            return unknown;
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
}
