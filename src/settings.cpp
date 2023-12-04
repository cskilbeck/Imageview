#include "pch.h"

settings_t settings;

settings_t default_settings;

//////////////////////////////////////////////////////////////////////
// for declaring dialog handlers

struct setting_base
{
    // internal name of the setting
    char const *name;

    // user friendly descriptive name for the dialog
    uint string_id;

    setting_base(char const *n, uint s) : name(n), string_id(s)
    {
    }

    // name of the type of this setting
    virtual uint type_string_id() = 0;

    // create dialog controls for editing this setting
    virtual void setup_controls() = 0;

    // update the dialog controls with current value of this setting
    virtual void update_controls() = 0;
};

//////////////////////////////////////////////////////////////////////

template <typename T> struct setting : virtual setting_base
{
    setting(T *v) : value(v)
    {
    }

    T *value;
};

//////////////////////////////////////////////////////////////////////

struct bool_setting : setting<bool>
{
    bool_setting(char const *n, uint s, bool *b) : setting_base(n, s), setting<bool>(b)
    {
    }

    uint type_string_id() override
    {
        return IDS_SETTING_TYPE_BOOL;
    }

    void setup_controls() override
    {
    }

    void update_controls() override
    {
    }
};

//////////////////////////////////////////////////////////////////////

template <typename T> struct enum_setting : setting<T>
{
    enum_setting(char const *n, uint s, T *b) : setting_base(n, s), setting<T>(b)
    {
    }

    uint type_string_id() override
    {
        return IDS_SETTING_TYPE_ENUM;
    }

    void setup_controls() override
    {
    }

    void update_controls() override
    {
    }

    std::map<uint, uint> enum_names;
};

//////////////////////////////////////////////////////////////////////

struct color_setting : setting<vec4>
{
    color_setting(char const *n, uint s, vec4 *b) : setting_base(n, s), setting<vec4>(b)
    {
    }

    uint type_string_id() override
    {
        return IDS_SETTING_TYPE_COLOR;
    }

    void setup_controls() override
    {
    }

    void update_controls() override
    {
    }
};

//////////////////////////////////////////////////////////////////////

template <typename T> struct ranged_setting : setting<T>
{
    ranged_setting(char const *n, uint s, T *b, T minval, T maxval)
        : setting_base(n, s), setting<T>(b), min_value(minval), max_value(maxval)
    {
    }

    uint type_string_id() override
    {
        return IDS_SETTING_TYPE_RANGED;
    }

    void setup_controls() override
    {
    }

    void update_controls() override
    {
    }

    T min_value;
    T max_value;
};

//////////////////////////////////////////////////////////////////////

void show_settings()
{
    settings_t dialog_settings;

    LOG_CONTEXT("SHOW");

#undef DECL_SETTING_BOOL
#undef DECL_SETTING_COLOR
#undef DECL_SETTING_ENUM
#undef DECL_SETTING_RANGED
#undef DECL_SETTING_INTERNAL

    std::list<setting_base *> dialog_controllers;

#define DECL_SETTING_BOOL(name, string_id, value) \
    dialog_controllers.push_back(new bool_setting(#name, string_id, &dialog_settings.name));

#define DECL_SETTING_COLOR(name, string_id, r, g, b, a) \
    dialog_controllers.push_back(new color_setting(#name, string_id, &dialog_settings.name));

#define DECL_SETTING_ENUM(type, name, string_id, value) \
    dialog_controllers.push_back(new enum_setting<type>(#name, string_id, &dialog_settings.name));

#define DECL_SETTING_RANGED(type, name, string_id, value, min, max) \
    dialog_controllers.push_back(new ranged_setting<type>(#name, string_id, &dialog_settings.name, min, max));

#define DECL_SETTING_INTERNAL(setting_type, name, ...)

#include "settings_fields.h"

    for(auto const s : dialog_controllers) {
        std::string desc = imageview::localize(s->string_id);
        std::string type_desc = imageview::localize(s->type_string_id());
        LOG_DEBUG("SETTING [{}] is a {} \"{}\"", s->name, type_desc, desc);
    }
}

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

#undef DECL_SETTING_BOOL
#undef DECL_SETTING_COLOR
#undef DECL_SETTING_ENUM
#undef DECL_SETTING_RANGED
#undef DECL_SETTING_INTERNAL

#define SERIALIZE_SETTING(name) \
    CHK_HR(serialize_setting(action, key, #name, reinterpret_cast<byte *>(&name), static_cast<DWORD>(sizeof(name))))

#define DECL_SETTING_BOOL(name, string_id, value) SERIALIZE_SETTING(name)

#define DECL_SETTING_COLOR(name, string_id, r, g, b, a) SERIALIZE_SETTING(name)

#define DECL_SETTING_ENUM(type, name, string_id, value) SERIALIZE_SETTING(name)

#define DECL_SETTING_RANGED(type, name, string_id, value, min, max) SERIALIZE_SETTING(name)

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
