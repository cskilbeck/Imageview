#pragma once

enum class fullscreen_startup_option : uint
{
    start_windowed,      // start up windowed
    start_fullscreen,    // start up fullscreen
    start_remember       // start up in whatever mode (fullscreen or windowed) it was in last time the app was exited
};

// whether to remember the window position or not
enum class window_position_option : uint
{
    window_pos_remember,    // restore last window position
    window_pos_default      // reset window position to default each time
};

// how to show the filename overlay
enum class show_filename_option : uint
{
    always,
    briefly,
    never
};

// what to do about exif rotation/flip data
enum class exif_option : uint
{
    ignore,    // always ignore it
    apply,     // always apply it
    prompt     // prompt if it's anything other than default 0 rotation
};

// what should reset_zoom do
enum class zoom_mode_t : uint
{
    one_to_one,
    fit_to_window,
    shrink_to_fit
};

//////////////////////////////////////////////////////////////////////
// mouse buttons

enum mouse_button_t : int
{
    btn_left = 0,
    btn_middle = 1,
    btn_right = 2,
    btn_count = 3
};

// settings get serialized/deserialized to/from the registry
struct settings_t
{
    // use a header so we can implement the serializer more easily
#define DECL_SETTING(type, name, ...) type name{ __VA_ARGS__ };
#include "settings_fields.h"

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

extern settings_t settings;

extern settings_t default_settings;
