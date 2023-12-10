// don't warn about structure padding in here

#pragma warning(push)
#pragma warning(disable : 4324)

//////////////////////////////////////////////////////////////////////
// background colors { r,g,b,a }

DECL_SETTING_SEPARATOR(IDS_SETTING_SEPARATOR_BACKGROUND);

// border color beyond image bounds

DECL_SETTING_COLOR(border_color, IDS_SETTING_NAME_BORDER_COLOR, 0x203040, false);

// bg color shows through image alpha

DECL_SETTING_COLOR(background_color, IDS_SETTING_NAME_BACKGROUND_COLOR, 0xff00ff, false);

//////////////////////////////////////////////////////////////////////
// grid

DECL_SETTING_SEPARATOR(IDS_SETTING_SEPARATOR_GRID);

// grid on or off

DECL_SETTING_BOOL(grid_enabled, IDS_SETTING_NAME_GRID_ENABLED, true);

// grid colors

DECL_SETTING_COLOR(grid_color_1, IDS_SETTING_NAME_GRID_COLOR1, 0x202020, false);
DECL_SETTING_COLOR(grid_color_2, IDS_SETTING_NAME_GRID_COLOR2, 0x505050, false);

// grid size in pixels

DECL_SETTING_RANGED(grid_size, IDS_SETTING_NAME_GRID_SIZE, 16, 1, 512);

// grid is fixed to screen coords (true) or floats (fixed to image origin)

DECL_SETTING_BOOL(fixed_grid, IDS_SETTING_NAME_FIXED_GRID, true);

// grid size multiplier, @NOTE must be a power of 2

DECL_SETTING_RANGED(grid_multiplier, IDS_SETTING_NAME_GRID_MULTIPLIER, 2, 1, 4);

//////////////////////////////////////////////////////////////////////
// select

DECL_SETTING_SEPARATOR(IDS_SETTING_SEPARATOR_SELECT);

// selection rectangle fill color

DECL_SETTING_COLOR(select_fill_color, IDS_SETTING_NAME_SELECT_FILL_COLOR, 0x80200080, true);

// selection outline colors (line alternates)

DECL_SETTING_COLOR(select_outline_color1, IDS_SETTING_NAME_SELECT_OUTLINE_COLOR1, 0xff000000, true);
DECL_SETTING_COLOR(select_outline_color2, IDS_SETTING_NAME_SELECT_OUTLINE_COLOR2, 0xffffffff, true);

// how far mouse has to move after clicking select button to consider a selection active

DECL_SETTING_INTERNAL(float, select_start_distance, 4);

// how close in pixels the mouse must be to grab the selection edge

DECL_SETTING_INTERNAL(float, select_border_grab_size, 8);

// selection border width (1 or maybe 2, anything bigger likely to cause problems)

DECL_SETTING_RANGED(select_border_width, IDS_SETTING_NAME_SELECT_BORDER_WIDTH, 2, 1, 16);

// selection line dash length

DECL_SETTING_RANGED(dash_length, IDS_SETTING_NAME_SELECT_DASH_LENGTH, 8, 4, 64);

// crosshair line colors

DECL_SETTING_COLOR(crosshair_color1, IDS_SETTING_NAME_CROSSHAIR_COLOR1, 0xff000000, true);
DECL_SETTING_COLOR(crosshair_color2, IDS_SETTING_NAME_CROSSHAIR_COLOR2, 0xffffffff, true);

//////////////////////////////////////////////////////////////////////
// mouse buttons

DECL_SETTING_SEPARATOR(IDS_SETTING_SEPARATOR_MOUSE);

// which mouse button for interactive zoom

DECL_SETTING_ENUM(mouse_button_t, zoom_button, IDS_SETTING_NAME_ZOOM_BUTTON, enum_mouse_buttons_map, btn_middle);

// which mouse button for dragging image

DECL_SETTING_ENUM(mouse_button_t, drag_button, IDS_SETTING_NAME_DRAG_BUTTON, enum_mouse_buttons_map, btn_right);

// which mouse button for selection

DECL_SETTING_ENUM(mouse_button_t, select_button, IDS_SETTING_NAME_SELECT_BUTTON, enum_mouse_buttons_map, btn_left);

// which mouse button for popup menu

DECL_SETTING_ENUM(mouse_button_t, menu_button, IDS_SETTING_NAME_MENU_BUTTON, enum_mouse_buttons_map, btn_right);

//////////////////////////////////////////////////////////////////////
// window

DECL_SETTING_SEPARATOR(IDS_SETTING_SEPARATOR_WINDOW);

// has it ever been run before? if not, use some sensible defaults for window pos

DECL_SETTING_INTERNAL(bool, first_run, true);

// windowed or fullscreen

DECL_SETTING_INTERNAL(bool, fullscreen, false);

// single instance mode

DECL_SETTING_BOOL(reuse_window, IDS_SETTING_NAME_REUSE_WINDOW, true);

// if there's an image in the clipboard at startup, paste it in

DECL_SETTING_BOOL(auto_paste, IDS_SETTING_NAME_AUTO_PASTE, true);

// either remember fullscreen mode or always revert to: windowed or fullscreen

DECL_SETTING_ENUM(fullscreen_startup_option,
                  fullscreen_mode,
                  IDS_SETTING_NAME_STARTUP_FULLSCREEN,
                  enum_fullscreen_startup_map,
                  fullscreen_startup_option::start_remember);

// non-fullscreen window placement

DECL_SETTING_INTERNAL(WINDOWPLACEMENT, window_placement, sizeof(WINDOWPLACEMENT));

// last fullscreen rect

DECL_SETTING_INTERNAL(RECT, fullscreen_rect, 0, 0, 0, 0);

// show either just filename or full path in window titlebar

DECL_SETTING_BOOL(show_full_filename_in_titlebar, IDS_SETTING_NAME_SHOW_FULL_FILENAME, false);

// how to show filename overlay

DECL_SETTING_ENUM(show_filename_option,
                  show_filename,
                  IDS_SETTING_NAME_SHOW_FILENAME,
                  enum_show_filename_map,
                  show_filename_option::show_filename_briefly);

//////////////////////////////////////////////////////////////////////
// others

// what to do about exif metadata

DECL_SETTING_ENUM(
    exif_option, image_rotation_option, IDS_SETTING_NAME_EXIF_OPTION, enum_exif_map, exif_option::exif_option_apply);

// how much memory to use for caching files and decoded images
// TODO (chs): separate settings for file and uncompressed image caches?
// or... ditch the uncompressed cache? help with exif prompt problem

DECL_SETTING_RANGED(cache_size_mb, IDS_SETTING_NAME_CACHE_SIZE_MB, 128, 16, 4096);    // 1GB memory cache by default

// what happens when you press 'z'

DECL_SETTING_ENUM(zoom_mode_t, zoom_mode, IDS_SETTING_NAME_ZOOM_MODE, enum_zoom_mode_map, zoom_mode_t::shrink_to_fit);

DECL_SETTING_ENUM(startup_zoom_mode_option,
                  startup_zoom_mode,
                  IDS_SETTING_NAME_STARTUP_ZOOM_MODE,
                  enum_startup_zoom_mode_map,
                  startup_zoom_mode_option::startup_zoom_shrink_to_fit);


#pragma warning(pop)
