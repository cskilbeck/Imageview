//////////////////////////////////////////////////////////////////////

#include "pch.h"

LOG_CONTEXT("settings");

namespace imageview
{
    settings_t settings;

    settings_t default_settings;

    bool settings_purged{ false };
}

//////////////////////////////////////////////////////////////////////

namespace
{
    using namespace imageview;

    static wchar constexpr key[] = L"Software\\ImageView";

    //////////////////////////////////////////////////////////////////////

    HRESULT load_registry_string(wchar_t const *name, std::wstring &result)
    {
        HKEY hk;
        CHK_HR(RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, null, 0, KEY_READ | KEY_QUERY_VALUE, null, &hk, null));
        DEFER(RegCloseKey(hk));
        DWORD cbsize = 0;
        if(FAILED(RegQueryValueExW(hk, name, null, null, null, &cbsize))) {
            LOG_DEBUG(L"Can't RegQueryValueExW({}", name);
            return S_FALSE;
        }
        result.resize(cbsize / sizeof(wchar));
        CHK_HR(RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_SZ, null, result.data(), &cbsize));
        if(!result.empty() && result.back() == 0) {
            result.pop_back();
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT save_registry_string(wchar_t const *name, std::wstring const &s)
    {
        HKEY hk;
        CHK_HR(RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, null, 0, KEY_WRITE, null, &hk, null));
        DEFER(RegCloseKey(hk));
        CHK_HR(RegSetValueExW(hk,
                              name,
                              0,
                              REG_SZ,
                              reinterpret_cast<byte const *>(s.c_str()),
                              static_cast<DWORD>(s.size() * sizeof(wchar))));
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT delete_registry_key(std::wstring const &path)
    {
        CHK_HR(RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()));
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT parse_section(std::wstring const &value, settings_t::section_t &setting)
    {
        if(_wcsicmp(value.c_str(), L"expanded") == 0) {
            setting = true;
            return S_OK;
        } else if(_wcsicmp(value.c_str(), L"collapsed") == 0) {
            setting = false;
            return S_OK;
        }
        return E_INVALIDARG;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT parse_bool(std::wstring const &value, bool &setting)
    {
        if(_wcsicmp(value.c_str(), L"true") == 0) {
            setting = true;
            return S_OK;
        } else if(_wcsicmp(value.c_str(), L"false") == 0) {
            setting = false;
            return S_OK;
        }
        return E_INVALIDARG;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT parse_uint(std::wstring const &value, uint &setting)
    {
        wchar *ep;
        uint x = std::wcstoul(value.c_str(), &ep, 10);
        if(ep != value.c_str() || *ep != 0) {
            setting = x;
            return S_OK;
        }
        return E_INVALIDARG;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT parse_int(std::wstring const &value, int &setting)
    {
        wchar *ep;
        int x = std::wcstol(value.c_str(), &ep, 10);
        if(ep != value.c_str() || *ep != 0) {
            setting = x;
            return S_OK;
        }
        return E_INVALIDARG;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT parse_ranged(std::wstring const &value, settings_t::ranged_t &setting)
    {
        return parse_uint(value, setting);
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT parse_color24(std::wstring const &value, settings_t::color_t &setting)
    {
        CHK_HR(color_from_string(value.c_str(), setting));
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT parse_color32(std::wstring const &value, settings_t::color_t &setting)
    {
        CHK_HR(color_from_string(value.c_str(), setting));
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> HRESULT parse_enum(std::wstring const &value, T &setting)
    {
        uint x;
        CHK_HR(parse_uint(value, x));
        setting = static_cast<T>(x);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> HRESULT parse_binary(std::wstring const &value, T &setting)
    {
        if(value.size() != sizeof(T) * 2) {
            return E_INVALIDARG;
        }
        byte *b = reinterpret_cast<byte *>(&setting);
        for(size_t i = 0; i < value.size(); i += 2) {
            int h;
            int l;
            CHK_HR(nibble_from_char(value[i], h));
            CHK_HR(nibble_from_char(value[i + 1], l));
            *b++ = static_cast<byte>((h << 4) | l);
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    template <> HRESULT parse_binary(std::wstring const &value, std::wstring &setting)
    {
        setting = value;
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    template <> HRESULT parse_binary(std::wstring const &value, RECT &setting)
    {
        std::vector<std::wstring> tokens;
        tokenize(value, tokens, L",", tokenize_option::discard_empty);
        if(tokens.size() != 4) {
            return E_INVALIDARG;
        }
        int *l = reinterpret_cast<int *>(&setting);
        for(size_t i = 0; i < 4; ++i) {
            CHK_HR(parse_int(tokens[i], *l));
            l += 1;
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT to_string_section(settings_t::section_t const &setting, std::wstring &result)
    {
        if(setting) {
            result = std::wstring{ L"expanded" };
        } else {
            result = std::wstring{ L"collapsed" };
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT to_string_bool(bool const &setting, std::wstring &result)
    {
        if(setting) {
            result = std::wstring{ L"true" };
        } else {
            result = std::wstring{ L"false" };
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT to_string_uint(uint const &setting, std::wstring &result)
    {
        result = std::format(L"{:d}", setting);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT to_string_ranged(settings_t::ranged_t const &setting, std::wstring &result)
    {
        return to_string_uint(static_cast<uint const &>(setting), result);
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT to_string_color24(settings_t::color_t const &setting, std::wstring &result)
    {
        result = color24_to_string(setting);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT to_string_color32(settings_t::color_t const &setting, std::wstring &result)
    {
        result = color32_to_string(setting);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT to_string_enum(uint const &setting, std::wstring &result)
    {
        return to_string_uint(static_cast<uint const &>(setting), result);
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> HRESULT to_string_binary(T const &setting, std::wstring &result)
    {
        size_t size = sizeof(T);
        result.resize(size * 2);
        byte const *b = reinterpret_cast<byte const *>(&setting);
        auto it = result.begin();
        for(size_t i = 0; i < size; ++i) {
            std::format_to_n(it, 2, L"{:02X}", b[i]);
            it += 2;
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    template <> HRESULT to_string_binary(std::wstring const &setting, std::wstring &result)
    {
        result = setting;
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    template <> HRESULT to_string_binary(RECT const &setting, std::wstring &result)
    {
        result = std::format(L"{:d},{:d},{:d},{:d}", setting.left, setting.top, setting.right, setting.bottom);
        return S_OK;
    }
}

namespace imageview
{
    //////////////////////////////////////////////////////////////////////

    HRESULT delete_settings_from_registry()
    {
        CHK_HR(delete_registry_key(key));
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // load the settings

    HRESULT settings_t::load()
    {
#include "settings_reset_decls.h"

#define LOAD_SETTING(name, type)                                     \
    {                                                                \
        std::wstring s;                                              \
        CHK_HR(load_registry_string(L#name, s));                     \
        if(FAILED((parse_##type(s, name)))) {                        \
            LOG_ERROR(L"Can't load setting {} (got {})", L#name, s); \
        }                                                            \
    }

#define DECL_SETTING_SECTION(name, string_id) LOAD_SETTING(name, section)

#define DECL_SETTING_BOOL(name, string_id, value) LOAD_SETTING(name, bool)

#define DECL_SETTING_UINT(name, string_id, value) LOAD_SETTING(name, uint)

#define DECL_SETTING_COLOR24(name, string_id, rgba) LOAD_SETTING(name, color24)

#define DECL_SETTING_COLOR32(name, string_id, rgba) LOAD_SETTING(name, color32)

#define DECL_SETTING_ENUM(name, string_id, type, enum_names, value) LOAD_SETTING(name, enum)

#define DECL_SETTING_RANGED(name, string_id, value, min, max) LOAD_SETTING(name, ranged)

#define DECL_SETTING_BINARY(name, string_id, type, ...) LOAD_SETTING(name, binary)

#include "settings_fields.h"

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // save the settings

    HRESULT settings_t::save()
    {
#include "settings_reset_decls.h"

#define SAVE_SETTING(name, type)                 \
    {                                            \
        std::wstring s;                          \
        CHK_HR(to_string_##type(name, s));       \
        CHK_HR(save_registry_string(L#name, s)); \
    }

#define DECL_SETTING_SECTION(name, string_id) SAVE_SETTING(name, section)

#define DECL_SETTING_BOOL(name, string_id, value) SAVE_SETTING(name, bool)

#define DECL_SETTING_UINT(name, string_id, value) SAVE_SETTING(name, uint)

#define DECL_SETTING_COLOR24(name, string_id, bgr) SAVE_SETTING(name, color24)

#define DECL_SETTING_COLOR32(name, string_id, abgr) SAVE_SETTING(name, color32)

#define DECL_SETTING_ENUM(name, string_id, type, enum_names, value) SAVE_SETTING(name, enum)

#define DECL_SETTING_RANGED(name, string_id, value, min, max) SAVE_SETTING(name, ranged)

#define DECL_SETTING_BINARY(name, string_id, type, ...) SAVE_SETTING(name, binary)

#include "settings_fields.h"

        return S_OK;
    }
}