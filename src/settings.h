#if !defined(DECL_SETTING)
#define DECL_SETTING(...) "Please define DECL_SETTING"
#endif

// don't warn about structure padding in this section

#pragma warning(push)
#pragma warning(disable : 4324)

// DECL_SETTING(type, name, default)

//////////////////////////////////////////////////////////////////////
// background colors

// bg color beyond image bounds

DECL_SETTING(vec4, border_color, 0.05f, 0.1f, 0.3f, 1);

// bg color shows through image alpha

DECL_SETTING(vec4, background_color, { 1, 1, 1, 1 });

//////////////////////////////////////////////////////////////////////
// grid

// grid on or off

DECL_SETTING(bool, grid_enabled, false);

// grid colors

DECL_SETTING(vec4, grid_color_1, 0.125f, 0.125f, 0.125f, 1);
DECL_SETTING(vec4, grid_color_2, 0.25f, 0.25f, 0.25f, 1);

// grid size in pixels

DECL_SETTING(float, grid_size, 16.0f);

// grid is fixed to screen coords (true) or floats (fixed to image origin)

DECL_SETTING(bool, fixed_grid, true);

// grid size multiplier, must be a power of 2

DECL_SETTING(int, grid_multiplier, 2);

//////////////////////////////////////////////////////////////////////
// select

// selection rectangle fill color

DECL_SETTING(vec4, select_fill_color, 0.1f, 0.2f, 0.5f, 0.5f);

// selection outline colors (line alternates)

DECL_SETTING(vec4, select_outline_color1, 1, 1, 1, 0.2f);
DECL_SETTING(vec4, select_outline_color2, 0, 0, 0, 0.4f);

// how far mouse has to move after clicking select button to consider a selection active

DECL_SETTING(float, select_start_distance, 4);

// how close in pixels the mouse must be to grab the selection edge

DECL_SETTING(float, select_border_grab_size, 8);

// selection border width (1 or maybe 2, anything bigger likely to cause problems)

DECL_SETTING(int, select_border_width, 2);

// selection line dash length

DECL_SETTING(uint, dash_length, 8);

// crosshair line colors

DECL_SETTING(vec4, crosshair_color1, 0, 0, 0, 0.25f);
DECL_SETTING(vec4, crosshair_color2, 1, 1, 1, 0.25f);

//////////////////////////////////////////////////////////////////////
// mouse buttons

// which mouse button for interactive zoom

DECL_SETTING(uint, zoom_button, btn_middle);

// which mouse button for dragging image

DECL_SETTING(uint, drag_button, btn_right);

// which mouse button for selection

DECL_SETTING(uint, select_button, btn_left);

//////////////////////////////////////////////////////////////////////
// window

// has it ever been run before? if not, use some sensible defaults for window pos

DECL_SETTING(bool, first_run, true);

// windowed or fullscreen

DECL_SETTING(bool, fullscreen, false);

// when new instance launched, if this is true then check for existing instance and use that instead ('single instance
// mode')

DECL_SETTING(bool, reuse_window, true);

// either remember fullscreen mode or always revert to: windowed or fullscreen

DECL_SETTING(fullscreen_startup_option, fullscreen_mode, fullscreen_startup_option::start_remember);

// non-fullscreen window placement

DECL_SETTING(WINDOWPLACEMENT, window_placement, sizeof(WINDOWPLACEMENT));

// last fullscreen rect

DECL_SETTING(rect, fullscreen_rect, 0, 0, 0, 0);

// show either just filename or full path in window titlebar

DECL_SETTING(bool, show_full_filename_in_titlebar, false);

// how to show filename overlay

DECL_SETTING(show_filename_option, show_filename, show_filename_option::briefly);

//////////////////////////////////////////////////////////////////////
// others

// what to do about exif metadata

DECL_SETTING(exif_option, image_rotation_option, exif_option::apply);

// how much memory to use for caching files and decoded images

DECL_SETTING(size_t, cache_size, 1048576llu * 1024);    // 1GB memory cache by default

// what happens when you press 'z'

DECL_SETTING(reset_zoom_mode, zoom_mode, reset_zoom_mode::shrink_to_fit);

#pragma warning(pop)
