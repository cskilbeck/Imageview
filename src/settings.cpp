//////////////////////////////////////////////////////////////////////

#include "pch.h"

LOG_CONTEXT("settings");

namespace imageview
{
    settings_t settings;

    settings_t default_settings;
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

    HRESULT parse_ranged(std::wstring const &value, settings_t::ranged_t &setting)
    {
        wchar *ep;
        uint x = std::wcstol(value.c_str(), &ep, 10);
        if(ep != value.c_str() || *ep != 0) {
            setting = x;
            return S_OK;
        }
        return E_INVALIDARG;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT parse_color(std::wstring const &value, settings_t::color_t &setting)
    {
        CHK_HR(color_from_string(value.c_str(), setting));
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> HRESULT parse_enum(std::wstring const &value, T &setting)
    {
        wchar *ep;
        uint x = std::wcstol(value.c_str(), &ep, 10);
        if(ep != value.c_str() || *ep != 0) {
            setting = static_cast<T>(x);
            return S_OK;
        }
        return E_INVALIDARG;
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

    HRESULT to_string_ranged(settings_t::ranged_t const &setting, std::wstring &result)
    {
        result = std::format(L"{:d}", setting);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT to_string_color(settings_t::color_t const &setting, std::wstring &result, bool alpha)
    {
        if(alpha) {
            result = color32_to_string(setting);
        } else {
            result = color24_to_string(setting);
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT to_string_enum(uint const &setting, std::wstring &result)
    {
        result = std::format(L"{:d}", setting);
        return S_OK;
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
}

namespace imageview
{
    //////////////////////////////////////////////////////////////////////
    // load the settings

    HRESULT settings_t::load()
    {
#undef DECL_SETTING_SECTION
#undef DECL_SETTING_BOOL
#undef DECL_SETTING_COLOR
#undef DECL_SETTING_ENUM
#undef DECL_SETTING_RANGED
#undef DECL_SETTING_INTERNAL

#define LOAD_SETTING(name, type)                 \
    {                                                                \
        std::wstring s;                                              \
        CHK_HR(load_registry_string(L#name, s));                     \
        if(FAILED((parse_##type(s, name)))) {                        \
            LOG_ERROR(L"Can't load setting {} (got {})", L#name, s); \
        }                                                            \
    }

#define LOAD_SETTING_V(name, type, ...)                              \
    {                                                                \
        std::wstring s;                                              \
        CHK_HR(load_registry_string(L#name, s));                     \
        if(FAILED((parse_##type(s, name, __VA_ARGS__)))) {           \
            LOG_ERROR(L"Can't load setting {} (got {})", L#name, s); \
        }                                                            \
    }

#define DECL_SETTING_SECTION(name, string_id) LOAD_SETTING(name, section)

#define DECL_SETTING_BOOL(name, string_id, value) LOAD_SETTING(name, bool)

#define DECL_SETTING_COLOR(name, string_id, rgba, alpha) LOAD_SETTING(name, color)

#define DECL_SETTING_ENUM(name, string_id, type, enum_names, value) LOAD_SETTING(name, enum)

#define DECL_SETTING_RANGED(name, string_id, value, min, max) LOAD_SETTING(name, ranged)

#define DECL_SETTING_INTERNAL(name, type, ...) LOAD_SETTING(name, binary)

#include "settings_fields.h"

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // save the settings

    HRESULT settings_t::save()
    {
#undef DECL_SETTING_SECTION
#undef DECL_SETTING_BOOL
#undef DECL_SETTING_COLOR
#undef DECL_SETTING_ENUM
#undef DECL_SETTING_RANGED
#undef DECL_SETTING_INTERNAL

#define SAVE_SETTING(name, type)                 \
    {                                            \
        std::wstring s;                          \
        CHK_HR(to_string_##type(name, s));       \
        CHK_HR(save_registry_string(L#name, s)); \
    }

#define SAVE_SETTING_V(name, type, ...)                 \
    {                                                   \
        std::wstring s;                                 \
        CHK_HR(to_string_##type(name, s, __VA_ARGS__)); \
        CHK_HR(save_registry_string(L#name, s));        \
    }

#define DECL_SETTING_SECTION(name, string_id) SAVE_SETTING(name, section)

#define DECL_SETTING_BOOL(name, string_id, value) SAVE_SETTING(name, bool)

#define DECL_SETTING_COLOR(name, string_id, rgba, alpha) SAVE_SETTING_V(name, color, alpha)

#define DECL_SETTING_ENUM(name, string_id, type, enum_names, value) SAVE_SETTING(name, enum)

#define DECL_SETTING_RANGED(name, string_id, value, min, max) SAVE_SETTING(name, ranged)

#define DECL_SETTING_INTERNAL(name, type, ...) SAVE_SETTING(name, binary)

#include "settings_fields.h"
        return S_OK;
    }
}