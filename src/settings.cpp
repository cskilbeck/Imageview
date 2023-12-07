#include "pch.h"

settings_t settings;

settings_t default_settings;

//////////////////////////////////////////////////////////////////////
// save or load a setting

HRESULT settings_t::serialize_setting(
    settings_t::serialize_action action, char const *key_name, char const *name, byte *var, DWORD size)
{
    switch(action) {

    case settings_t::serialize_action::save: {
        HKEY key;
        CHK_HR(RegCreateKeyExA(HKEY_CURRENT_USER, key_name, 0, null, 0, KEY_WRITE, null, &key, null));
        DEFER(RegCloseKey(key));
        CHK_HR(RegSetValueExA(key, name, 0, REG_BINARY, var, size));
    } break;

    case settings_t::serialize_action::load: {
        HKEY key;
        CHK_HR(RegCreateKeyExA(HKEY_CURRENT_USER, key_name, 0, null, 0, KEY_READ | KEY_QUERY_VALUE, null, &key, null));
        DEFER(RegCloseKey(key));
        DWORD cbsize = 0;
        if(FAILED(RegQueryValueExA(key, name, null, null, null, &cbsize)) || cbsize != size) {
            return S_FALSE;
        }
        CHK_HR(RegGetValue(
            HKEY_CURRENT_USER, key_name, name, RRF_RT_REG_BINARY, null, reinterpret_cast<DWORD *>(var), &cbsize));
    } break;
    }

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// save or load all the settings

HRESULT settings_t::serialize(serialize_action action, char const *key)
{
    if(key == null) {
        return E_INVALIDARG;
    }

#undef DECL_SETTING_SEPARATOR
#undef DECL_SETTING_BOOL
#undef DECL_SETTING_COLOR
#undef DECL_SETTING_ENUM
#undef DECL_SETTING_RANGED
#undef DECL_SETTING_INTERNAL

#define SERIALIZE_SETTING(name) \
    CHK_HR(serialize_setting(action, key, #name, reinterpret_cast<byte *>(&name), static_cast<DWORD>(sizeof(name))))

#define DECL_SETTING_SEPARATOR(string_id)

#define DECL_SETTING_BOOL(name, string_id, value) SERIALIZE_SETTING(name)

#define DECL_SETTING_COLOR(name, string_id, r, g, b, a) SERIALIZE_SETTING(name)

#define DECL_SETTING_ENUM(type, name, string_id, enum_names, value) SERIALIZE_SETTING(name)

#define DECL_SETTING_RANGED(name, string_id, value, min, max) SERIALIZE_SETTING(name)

#define DECL_SETTING_INTERNAL(type, name, ...) SERIALIZE_SETTING(name)

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
