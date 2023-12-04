// don't warn about structure padding in here

#pragma warning(push)
#pragma warning(disable : 4324)

//////////////////////////////////////////////////////////////////////
// background colors { r,g,b,a }

// border color beyond image bounds

DECL_SETTING_COLOR(border_color, IDS_SETTING_NAME_BORDER_COLOR, 0.05f, 0.1f, 0.3f, 1);

// bg color shows through image alpha

DECL_SETTING_COLOR(background_color, IDS_SETTING_NAME_BACKGROUND_COLOR, 1, 0, 0, 1);

//////////////////////////////////////////////////////////////////////
// grid

// grid on or off

DECL_SETTING_BOOL(grid_enabled, IDS_SETTING_NAME_GRID_ENABLED, false);

// grid colors

DECL_SETTING_COLOR(grid_color_1, IDS_SETTING_NAME_GRID_COLOR1, 0.125f, 0.125f, 0.125f, 1);
DECL_SETTING_COLOR(grid_color_2, IDS_SETTING_NAME_GRID_COLOR2, 0.25f, 0.25f, 0.25f, 1);

// grid size in pixels

DECL_SETTING_RANGED(float, grid_size, IDS_SETTING_NAME_GRID_SIZE, 16.0f, 4.0f, 32.0f);

// grid is fixed to screen coords (true) or floats (fixed to image origin)

DECL_SETTING_BOOL(fixed_grid, IDS_SETTING_NAME_FIXED_GRID, true);

// grid size multiplier, @NOTE must be a power of 2

DECL_SETTING_RANGED(int, grid_multiplier, IDS_SETTING_NAME_GRID_MULTIPLIER, 2, 1, 4);

//////////////////////////////////////////////////////////////////////
// select

// selection rectangle fill color

DECL_SETTING_COLOR(select_fill_color, IDS_SETTING_NAME_SELECT_FILL_COLOR, 0.1f, 0.2f, 0.5f, 0.5f);

// selection outline colors (line alternates)

DECL_SETTING_COLOR(select_outline_color1, IDS_SETTING_NAME_SELECT_OUTLINE_COLOR1, 1, 1, 1, 0.2f);
DECL_SETTING_COLOR(select_outline_color2, IDS_SETTING_NAME_SELECT_OUTLINE_COLOR2, 0, 0, 0, 0.4f);

// how far mouse has to move after clicking select button to consider a selection active

DECL_SETTING_INTERNAL(float, select_start_distance, 4);

// how close in pixels the mouse must be to grab the selection edge

DECL_SETTING_INTERNAL(float, select_border_grab_size, 8);

// selection border width (1 or maybe 2, anything bigger likely to cause problems)

DECL_SETTING_RANGED(int, select_border_width, IDS_SETTING_NAME_SELECT_BORDER_WIDTH, 2, 1, 4);

// selection line dash length

DECL_SETTING_RANGED(uint, dash_length, IDS_SETTING_NAME_SELECT_DASH_LENGTH, 8, 4, 32);

// crosshair line colors

DECL_SETTING_COLOR(crosshair_color1, IDS_SETTING_NAME_CROSSHAIR_COLOR1, 0, 0, 0, 0.25f);
DECL_SETTING_COLOR(crosshair_color2, IDS_SETTING_NAME_CROSSHAIR_COLOR2, 1, 1, 1, 0.25f);

//////////////////////////////////////////////////////////////////////
// mouse buttons

// which mouse button for interactive zoom

DECL_SETTING_ENUM(mouse_button_t, zoom_button, IDS_SETTING_NAME_ZOOM_BUTTON, btn_middle);

// which mouse button for dragging image

DECL_SETTING_ENUM(mouse_button_t, drag_button, IDS_SETTING_NAME_DRAG_BUTTON, btn_right);

// which mouse button for selection

DECL_SETTING_ENUM(mouse_button_t, select_button, IDS_SETTING_NAME_SELECT_BUTTON, btn_left);

//////////////////////////////////////////////////////////////////////
// window

// has it ever been run before? if not, use some sensible defaults for window pos

DECL_SETTING_INTERNAL(bool, first_run, true);

// windowed or fullscreen

DECL_SETTING_INTERNAL(bool, fullscreen, false);

// when new instance launched, if this is true then check for existing instance and use that instead ('single instance
// mode')

DECL_SETTING_BOOL(reuse_window, IDS_SETTING_NAME_REUSE_WINDOW, true);

// if there's an image in the clipboard at startup, paste it in

DECL_SETTING_BOOL(auto_paste, IDS_SETTING_NAME_AUTO_PASTE, true);

// either remember fullscreen mode or always revert to: windowed or fullscreen

DECL_SETTING_ENUM(fullscreen_startup_option,
                  fullscreen_mode,
                  IDS_SETTING_NAME_STARTUP_FULLSCREEN,
                  fullscreen_startup_option::start_remember);

// non-fullscreen window placement

DECL_SETTING_INTERNAL(WINDOWPLACEMENT, window_placement, sizeof(WINDOWPLACEMENT));

// last fullscreen rect

DECL_SETTING_INTERNAL(imageview::rect, fullscreen_rect, 0, 0, 0, 0);

// show either just filename or full path in window titlebar

DECL_SETTING_BOOL(show_full_filename_in_titlebar, IDS_SETTING_NAME_SHOW_FULL_FILENAME, false);

// how to show filename overlay

DECL_SETTING_ENUM(show_filename_option,
                  show_filename,
                  IDS_SETTING_NAME_SHOW_FILENAME,
                  show_filename_option::show_filename_briefly);

//////////////////////////////////////////////////////////////////////
// others

// what to do about exif metadata

DECL_SETTING_ENUM(exif_option, image_rotation_option, IDS_SETTING_NAME_EXIF_OPTION, exif_option::exif_option_apply);

// how much memory to use for caching files and decoded images

DECL_SETTING_RANGED(
    size_t, cache_size_mb, IDS_SETTING_NAME_CACHE_SIZE_MB, 1024, 64, 4096);    // 1GB memory cache by default

// what happens when you press 'z'

DECL_SETTING_ENUM(zoom_mode_t, zoom_mode, IDS_SETTING_NAME_ZOOM_MODE, zoom_mode_t::shrink_to_fit);

#pragma warning(pop)
