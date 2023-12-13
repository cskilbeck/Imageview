#include "pch.h"

LOG_CONTEXT("settings");

settings_t settings;

settings_t default_settings;

//////////////////////////////////////////////////////////////////////
// save or load a setting

HRESULT settings_t::serialize_setting(
    serialize_action action, char const *key, char const *name, byte *var, size_t size)
{
    switch(action) {

    case serialize_action::save: {
        HKEY hk;
        CHK_HR(RegCreateKeyExA(HKEY_CURRENT_USER, key, 0, null, 0, KEY_WRITE, null, &hk, null));
        DEFER(RegCloseKey(hk));
        CHK_HR(RegSetValueExA(hk, name, 0, REG_BINARY, var, static_cast<DWORD>(size)));
    } break;

    case serialize_action::load: {
        HKEY hk;
        CHK_HR(RegCreateKeyExA(HKEY_CURRENT_USER, key, 0, null, 0, KEY_READ | KEY_QUERY_VALUE, null, &hk, null));
        DEFER(RegCloseKey(hk));
        DWORD cbsize = 0;
        if(FAILED(RegQueryValueExA(hk, name, null, null, null, &cbsize)) || cbsize != size) {
            return S_FALSE;
        }
        CHK_HR(RegGetValueA(HKEY_CURRENT_USER, key, name, RRF_RT_REG_BINARY, null, var, &cbsize));
    } break;
    }

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// save or load all the settings

HRESULT settings_t::serialize(serialize_action action, char const *key)
{
#undef DECL_SETTING_SECTION
#undef DECL_SETTING_BOOL
#undef DECL_SETTING_COLOR
#undef DECL_SETTING_ENUM
#undef DECL_SETTING_RANGED
#undef DECL_SETTING_INTERNAL

#define SERIALIZE_SETTING(name)                                                                                  \
    LOG_DEBUG("{}: {}", #name, imageview::hex_string_from_bytes(reinterpret_cast<byte *>(&name), sizeof(name))); \
    CHK_HR(serialize_setting(action, key, #name, reinterpret_cast<byte *>(&name), sizeof(name)))

#define DECL_SETTING_SECTION(name, string_id) SERIALIZE_SETTING(name)

#define DECL_SETTING_BOOL(name, string_id, value) SERIALIZE_SETTING(name)

#define DECL_SETTING_COLOR(name, string_id, rgba, alpha) SERIALIZE_SETTING(name)

#define DECL_SETTING_ENUM(name, string_id, type, enum_names, value) SERIALIZE_SETTING(name)

#define DECL_SETTING_RANGED(name, string_id, value, min, max) SERIALIZE_SETTING(name)

#define DECL_SETTING_INTERNAL(name, type, ...) SERIALIZE_SETTING(name)

#include "settings_fields.h"

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// load the settings

HRESULT settings_t::load()
{
    return serialize(serialize_action::load, settings_key_name);
}

//////////////////////////////////////////////////////////////////////
// save the settings

HRESULT settings_t::save()
{
    return serialize(serialize_action::save, settings_key_name);
}
