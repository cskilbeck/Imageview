#pragma once

namespace imageview
{
    using enum_id_map = std::map<uint, uint>;

    // what mode (fullscreen or windowed) to start up in

    enum fullscreen_startup_option : uint
    {
        start_windowed,      // start up windowed
        start_fullscreen,    // start up fullscreen
        start_remember       // start up in whatever mode (fullscreen or windowed) it was in last time
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

    // what should zoom be at startup - the first three of these should line up with zoom_mode_t

    enum startup_zoom_mode_option : uint
    {
        startup_zoom_one_to_one,
        startup_zoom_fit_to_window,
        startup_zoom_shrink_to_fit,
        startup_zoom_remember
    };

    // what should reset_zoom do

    enum zoom_mode_t : uint
    {
        one_to_one,
        fit_to_window,
        shrink_to_fit
    };

    enum mouse_button_t : int
    {
        btn_min = 0,
        btn_left = 0,
        btn_middle = 1,
        btn_right = 2,
        btn_count = 3
    };

    //////////////////////////////////////////////////////////////////////

    struct settings_t
    {
        using section_t = bool;
        using color_t = uint32;
        using ranged_t = uint;
        // enums are special, just serialized as uints

#define SETTING_HIDDEN 0

#include "settings_reset_decls.h"

#define DECL_SETTING_SECTION(name, string_id) \
    section_t name                            \
    {                                         \
        false                                 \
    }

#define DECL_SETTING_BOOL(name, string_id, default_value) \
    bool name                                             \
    {                                                     \
        default_value                                     \
    }

#define DECL_SETTING_UINT(name, string_id, default_value) \
    uint name                                             \
    {                                                     \
        default_value                                     \
    }

#define DECL_SETTING_COLOR24(name, string_id, bgr) \
    color_t name                                   \
    {                                              \
        bgr | 0xff000000                           \
    }

#define DECL_SETTING_COLOR32(name, string_id, abgr) \
    color_t name                                    \
    {                                               \
        abgr                                        \
    }

#define DECL_SETTING_ENUM(name, string_id, type, enum_map, value) \
    type name                                                     \
    {                                                             \
        value                                                     \
    }

#define DECL_SETTING_RANGED(name, string_id, value, min, max) \
    ranged_t name                                             \
    {                                                         \
        value                                                 \
    }

#define DECL_SETTING_BINARY(name, string_id, type, ...) \
    type name                                           \
    {                                                   \
        __VA_ARGS__                                     \
    }

#include "settings_fields.h"

        //////////////////////////////////////////////////////////////////////

        HRESULT save();
        HRESULT load();
    };

    //////////////////////////////////////////////////////////////////////
    // settings get serialized/deserialized to/from the registry

    extern settings_t settings;
    extern settings_t default_settings;

    extern bool settings_purged;

    HRESULT delete_settings_from_registry();
}
