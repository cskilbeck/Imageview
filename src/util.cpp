//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

HRESULT load_resource(DWORD id, wchar const *type, void **buffer, size_t *size)
{
    if(buffer == null || size == null || type == null || id == 0) {
        return ERROR_BAD_ARGUMENTS;
    }

    HINSTANCE instance = GetModuleHandle(null);

    HRSRC rsrc = FindResource(instance, MAKEINTRESOURCE(id), type);
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

    defer(CloseClipboard());

    HANDLE c = GetClipboardData(format);
    if(c == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    void *data = GlobalLock(c);
    if(data == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    defer(GlobalUnlock(c));

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

std::wstring const &str_local(uint64 id)
{
    static std::unordered_map<uint64, std::wstring> localized_strings;
    static std::wstring const unknown{ L"?" };

    auto f = localized_strings.find(id);

    if(f != localized_strings.end()) {
        return f->second;
    }

    // SetThreadUILanguage(MAKELCID(LANG_FRENCH, SUBLANG_NEUTRAL));

    wchar *str;
    int len = LoadString(GetModuleHandle(null), static_cast<uint>(id), reinterpret_cast<wchar *>(&str), 0);

    if(len == 0) {
        return unknown;
    }
    return localized_strings.insert({ id, std::wstring(str, len) }).first->second;
}

//////////////////////////////////////////////////////////////////////
// get a null-terminated wchar pointer to the localized string

wchar const *localize(uint64 id)
{
    return str_local(id).c_str();
}

//////////////////////////////////////////////////////////////////////

std::wstring strip_quotes(wchar const *s)
{
    size_t len = wcslen(s);
    if(s[0] == '"' && s[len - 1] == '"') {
        len -= 2;
        s += 1;
    }
    return std::wstring(s, len);
}

//////////////////////////////////////////////////////////////////////

std::wstring windows_error_message(uint32 err)
{
    if(err == 0) {
        err = GetLastError();
    }
    return _com_error(HRESULT_FROM_WIN32(err)).ErrorMessage();
}

//////////////////////////////////////////////////////////////////////

std::wstring format_v(wchar const *fmt, va_list v)
{
    size_t len = _vscwprintf(fmt, v);
    std::wstring s;
    s.resize(len + 1);
    _vsnwprintf_s(&s[0], len + 1, _TRUNCATE, fmt, v);
    return s;
}

//////////////////////////////////////////////////////////////////////

std::string format_v(char const *fmt, va_list v)
{
    size_t len = _vscprintf(fmt, v);
    std::string s;
    s.resize(len + 1);
    vsnprintf_s(&s[0], len + 1, _TRUNCATE, fmt, v);
    return s;
}

//////////////////////////////////////////////////////////////////////

std::string format(char const *fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    return format_v(fmt, v);
}

//////////////////////////////////////////////////////////////////////

std::wstring format(wchar const *fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    return format_v(fmt, v);
}

//////////////////////////////////////////////////////////////////////

HRESULT log_win32_error(DWORD err, wchar const *message, va_list v)
{
    wchar buffer[4096];
    _vsntprintf_s(buffer, _countof(buffer), message, v);
    HRESULT r = HRESULT_FROM_WIN32(err);
    std::wstring err_str = windows_error_message(r);
    Log(TEXT("ERROR %08x (%s) %s"), err, buffer, err_str.c_str());
    return r;
}

//////////////////////////////////////////////////////////////////////

HRESULT log_win32_error(wchar const *message, ...)
{
    va_list v;
    va_start(v, message);
    return log_win32_error(GetLastError(), message, v);
}

//////////////////////////////////////////////////////////////////////

HRESULT log_win32_error(DWORD err, wchar const *message, ...)
{
    va_list v;
    va_start(v, message);
    return log_win32_error(err, message, v);
}

//////////////////////////////////////////////////////////////////////

HRESULT get_accelerator_hotkey_text(ACCEL const &accel, HKL layout, std::wstring &text)
{
    wchar key_name[256];
    switch(accel.key) {
    case VK_LEFT:
        wcsncpy_s(key_name, L"Left", 256);
        break;
    case VK_RIGHT:
        wcsncpy_s(key_name, L"Right", 256);
        break;
    case VK_UP:
        wcsncpy_s(key_name, L"Up", 256);
        break;
    case VK_DOWN:
        wcsncpy_s(key_name, L"Down", 256);
        break;
    case VK_PRIOR:
        wcsncpy_s(key_name, L"Page Up", 256);
        break;
    case VK_NEXT:
        wcsncpy_s(key_name, L"Page Down", 256);
        break;
    case VK_OEM_COMMA:
    case L',':
        wcsncpy_s(key_name, L"Comma", 256);
        break;
    case VK_OEM_PERIOD:
    case L'.':
        wcsncpy_s(key_name, L"Period", 256);
        break;

    default:
        uint scan_code = MapVirtualKeyEx(accel.key, MAPVK_VK_TO_VSC, layout);
        GetKeyNameText((scan_code & 0x7f) << 16, key_name, 256);
        break;
    }

    // build the label with modifier keys

    std::wstring key_label;

    auto append = [&](std::wstring &a, wchar const *b) {
        if(!a.empty()) {
            a.append(L"-");
        }
        a.append(b);
    };

    if(accel.fVirt & FCONTROL) {
        append(key_label, L"Ctrl");
    }
    if(accel.fVirt & FSHIFT) {
        append(key_label, L"Shift");
    }
    if(accel.fVirt & FALT) {
        append(key_label, L"Alt");
    }
    append(key_label, key_name);

    text = key_label;
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT copy_accelerator_table(HACCEL h, std::vector<ACCEL> &table)
{
    if(h == null) {
        return ERROR_INVALID_DATA;
    }

    UINT num_accelerators = CopyAcceleratorTable(h, null, 0);

    if(num_accelerators == 0) {
        return ERROR_NOT_FOUND;
    }

    table.resize(num_accelerators);

    UINT num_accelerators_got = CopyAcceleratorTable(h, table.data(), num_accelerators);

    if(num_accelerators_got != num_accelerators) {
        table.clear();
        return ERROR_NOT_ALL_ASSIGNED;
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT get_hotkey_description(ACCEL const &accel, std::wstring &text)
{
    wchar buffer[512];
    LoadString(GetModuleHandle(null), accel.cmd, buffer, _countof(buffer));
    text = buffer;
    return S_OK;
}
