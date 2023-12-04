#pragma once

enum fullscreen_startup_option : uint
{
    start_windowed,      // start up windowed
    start_fullscreen,    // start up fullscreen
    start_remember       // start up in whatever mode (fullscreen or windowed) it was in last time the app was exited
};

// whether to remember the window position or not
enum window_position_option : uint
{
    window_pos_remember,    // restore last window position
    window_pos_default      // reset window position to default each time
};

// how to show the filename overlay
enum show_filename_option : uint
{
    show_filename_always,
    show_filename_briefly,
    show_filename_never
};

// what to do about exif rotation/flip data
enum exif_option : uint
{
    exif_option_ignore,    // always ignore it
    exif_option_apply,     // always apply it
    exif_option_prompt     // prompt if it's anything other than default 0 rotation
};

// what should zoom be at startup
enum startup_zoom_mode : uint
{
    startup_zoom_one_to_one,
    startup_zoom_fit_to_window,
    startup_zoom_shrink_to_fit,
    startup_zoom_remember
};

//////////////////////////////////////////////////////////////////////

// what should reset_zoom do
enum class zoom_mode_t : uint
{
    one_to_one,
    fit_to_window,
    shrink_to_fit
};

enum mouse_button_t : int
{
    btn_left = 0,
    btn_middle = 1,
    btn_right = 2,
    btn_count = 3
};

//////////////////////////////////////////////////////////////////////

struct settings_t
{

#define DECL_SETTING_BOOL(name, string_id, default_value) bool name{ default_value };

#define DECL_SETTING_COLOR(name, string_id, r, g, b, a) vec4 name{ r, g, b, a };

#define DECL_SETTING_ENUM(type, name, string_id, value) type name{ value };

#define DECL_SETTING_RANGED(type, name, string_id, value, min, max) type name{ value };

#define DECL_SETTING_INTERNAL(type, name, ...) type name{ __VA_ARGS__ };

#include "settings_fields.h"

    //////////////////////////////////////////////////////////////////////

    HRESULT save();
    HRESULT load();

    // where in the registry to put the settings. this does not need to be localized... right?
    static char constexpr settings_key_name[] = "Software\\ImageView";

    enum class serialize_action
    {
        save,
        load
    };

    HRESULT serialize(serialize_action action, char const *save_key_name);

    // write or read a settings field to or from the registry - helper for serialize()
    HRESULT serialize_setting(
        settings_t::serialize_action action, char const *key_name, char const *name, byte *var, DWORD size);
};

//////////////////////////////////////////////////////////////////////
// settings get serialized/deserialized to/from the registry

extern settings_t settings;

extern settings_t default_settings;
