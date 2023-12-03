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

HRESULT settings_t::serialize(serialize_action action, char const *save_key_name)
{
    if(save_key_name == null) {
        return E_INVALIDARG;
    }

#undef DECL_SETTING
#define DECL_SETTING(type, name, ...) \
    CHK_HR(serialize_setting(action, save_key_name, #name, reinterpret_cast<byte *>(&name), (DWORD)sizeof(name)))
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
