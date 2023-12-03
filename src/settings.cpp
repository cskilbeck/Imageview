#include "pch.h"

settings_t settings;

settings_t default_settings;

//////////////////////////////////////////////////////////////////////
// for declaring dialog handlers

// extern std::list<setting_base *> dialog_controllers;
//
//#define DECL_SETTING_BOOL(name, string_id, default_value) \
//    dialog_controllers.push_back(new bool_setting{ &settings::dialog_settings::name, string_id });
//
//#define DECL_SETTING_COLOR(name, string_id, default_r, default_g, default_b, default_a) \
//    dialog_controllers.push_back(new color_setting{ &settings::dialog_settings::name, string_id });
//
//#define DECL_SETTING_ENUM(name, string_id, default_value, enum_type) \
//    dialog_controllers.push_back(new enum_setting<enum_type>{ &settings::dialog_settings::name, string_id });
//
//#define DECL_SETTING_RANGED(name, string_id, default_value, type, min, max) \
//    dialog_controllers.push_back(new ranged_setting<type>{ &settings::dialog_settings::name, string_id, min, max });
//
//#define DECL_SETTING_INTERNAL(setting_type, name, ...)

// struct setting_base
//{
//    virtual void setup_controls() = 0;
//    virtual void update_controls() = 0;
//};
//
// template <typename T> struct setting : setting_base
//{
//    // the current value
//    T *value;
//
//    // name of the setting (e.g. "Show Filename")
//    uint string_id;
//};
//
// struct bool_setting : setting<bool>
//{
//    void setup_controls() override;
//    void update_controls() override;
//};
//
// template <typename T> struct enum_setting : setting<T>
//{
//    void setup_controls() override;
//    void update_controls() override;
//
//    std::map<uint, uint> enum_names;
//};
//
// struct color_setting : setting<vec4>
//{
//    void setup_controls() override;
//    void update_controls() override;
//};
//
// template <typename T> struct ranged_setting : setting<T>
//{
//    void setup_controls() override;
//    void update_controls() override;
//
//    T min_value;
//    T max_value;
//};
//


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

    //////////////////////////////////////////////////////////////////////
    // for declaring serializers

#undef DECL_SETTING_BOOL
#undef DECL_SETTING_COLOR
#undef DECL_SETTING_ENUM
#undef DECL_SETTING_RANGED
#undef DECL_SETTING_INTERNAL

#define DECL_SETTING_BOOL(name, string_id, default_value) \
    CHK_HR(serialize_setting(action, save_key_name, #name, reinterpret_cast<byte *>(&name), (DWORD)sizeof(name)))

#define DECL_SETTING_COLOR(name, string_id, default_r, default_g, default_b, default_a) \
    CHK_HR(serialize_setting(action, save_key_name, #name, reinterpret_cast<byte *>(&name), (DWORD)sizeof(name)))

#define DECL_SETTING_ENUM(enum_type, name, string_id, default_value) \
    CHK_HR(serialize_setting(action, save_key_name, #name, reinterpret_cast<byte *>(&name), (DWORD)sizeof(name)))

#define DECL_SETTING_RANGED(ranged_type, name, string_id, default_value, min, max) \
    CHK_HR(serialize_setting(action, save_key_name, #name, reinterpret_cast<byte *>(&name), (DWORD)sizeof(name)))

#define DECL_SETTING_INTERNAL(setting_type, name, ...) \
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
