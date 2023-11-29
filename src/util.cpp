//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

HRESULT load_resource(DWORD id, char const *type, void **buffer, size_t *size)
{
    if(buffer == null || size == null || type == null || id == 0) {
        return HRESULT_FROM_WIN32(ERROR_BAD_ARGUMENTS);
    }

    HINSTANCE instance = GetModuleHandle(null);

    HRSRC rsrc = FindResourceA(instance, MAKEINTRESOURCEA(id), type);
    if(rsrc == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    size_t len = SizeofResource(instance, rsrc);
    if(len == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    HGLOBAL mem = LoadResource(instance, rsrc);
    if(mem == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    void *data = LockResource(mem);
    if(data == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

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

std::string const &str_local(uint64 id)
{
    static std::unordered_map<uint64, std::string> localized_strings;
    static std::string const unknown{ "?" };

    auto f = localized_strings.find(id);

    if(f != localized_strings.end()) {
        return f->second;
    }

    // SetThreadUILanguage(MAKELCID(LANG_FRENCH, SUBLANG_NEUTRAL));

    wchar *str;
    int len = LoadStringW(GetModuleHandle(null), static_cast<uint>(id), reinterpret_cast<wchar *>(&str), 0);

    if(len <= 0) {
        return unknown;
    }
    return localized_strings.insert({ id, utf8(str, len) }).first->second;
}

//////////////////////////////////////////////////////////////////////
// get a null-terminated char pointer to the localized string

char const *localize(uint64 id)
{
    return str_local(id).c_str();
}

//////////////////////////////////////////////////////////////////////

std::string strip_quotes(char const *s)
{
    size_t len = strlen(s);
    if(s[0] == '"' && s[len - 1] == '"') {
        len -= 2;
        s += 1;
    }
    return std::string(s, len);
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

HRESULT get_accelerator_hotkey_text(uint id, std::vector<ACCEL> const &accel_table, HKL layout, std::string &text)
{
    char const *separator = "";

    text.clear();

    for(auto const &a : accel_table) {

        if(a.cmd == id) {

            char key_name[256];
            switch(a.key) {
            case VK_LEFT:
                strncpy_s(key_name, "Left", 256);
                break;
            case VK_RIGHT:
                strncpy_s(key_name, "Right", 256);
                break;
            case VK_UP:
                strncpy_s(key_name, "Up", 256);
                break;
            case VK_DOWN:
                strncpy_s(key_name, "Down", 256);
                break;
            case VK_PRIOR:
                strncpy_s(key_name, "Page Up", 256);
                break;
            case VK_NEXT:
                strncpy_s(key_name, "Page Down", 256);
                break;
            case VK_OEM_COMMA:
            case ',':
                strncpy_s(key_name, "Comma", 256);
                break;
            case VK_OEM_PERIOD:
            case '.':
                strncpy_s(key_name, "Period", 256);
                break;

            default:
                uint scan_code = MapVirtualKeyEx(a.key, MAPVK_VK_TO_VSC, layout);
                GetKeyNameTextA((scan_code & 0x7f) << 16, key_name, 256);
                break;
            }

            // build the label with modifier keys

            std::string key_label;

            auto append = [&](std::string &a, char const *b) {
                if(!a.empty()) {
                    a.append("-");
                }
                a.append(b);
            };

            if(a.fVirt & FCONTROL) {
                append(key_label, "Ctrl");
            }
            if(a.fVirt & FSHIFT) {
                append(key_label, "Shift");
            }
            if(a.fVirt & FALT) {
                append(key_label, "Alt");
            }
            append(key_label, key_name);

            text = std::format("{}{}{}", text, separator, key_label);
            separator = ", ";
        }
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT copy_accelerator_table(HACCEL h, std::vector<ACCEL> &table)
{
    if(h == null) {
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }

    UINT num_accelerators = CopyAcceleratorTable(h, null, 0);

    if(num_accelerators == 0) {
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    }

    table.resize(num_accelerators);

    UINT num_accelerators_got = CopyAcceleratorTable(h, table.data(), num_accelerators);

    if(num_accelerators_got != num_accelerators) {
        table.clear();
        return HRESULT_FROM_WIN32(ERROR_NOT_ALL_ASSIGNED);
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT get_hotkey_description(ACCEL const &accel, std::string &text)
{
    char buffer[512];
    LoadStringA(GetModuleHandle(null), accel.cmd, buffer, _countof(buffer));
    text = buffer;
    return S_OK;
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
