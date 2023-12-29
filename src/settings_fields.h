// don't warn about structure padding in here

// DECL_SETTING_SECTION(name, string_id)
//
// creates a new settings section in the UI
// the first DECL_SETTING_xxx must be a DECL_SETTING_SECTION. i.e. every setting must be in a section

// DECL_SETTING_BOOL(name, string_id, default_value)
//
// a boolean setting

// DECL_SETTING_COLOR24(name, string_id, default_rgb_value)
//
// 24 bit color with no alpha slider
// alpha is fixed at 255

// DECL_SETTING_COLOR32(name, string_id, default_rgba_value)
//
// 32 bit color with alpha slider

// DECL_SETTING_ENUM(name, string_id, type, enum_names, default_value)
//
// ugh, an enum with a reference to a map for the names, see setting_enum.cpp

// DECL_SETTING_RANGED(name, string_id, default_value, min, max)
//
// ranged uint

// DECL_SETTING_UINT(name, string_id, default_value)
//
// some binary data, serialized as hex

// DECL_SETTING_BINARY(name, string_id, type, default_value...)
//
// some binary data of `type`, serialized as hex


//////////////////////////////////////////////////////////////////////
// background

DECL_SETTING_SECTION(background_section, IDS_SETTING_SECTION_BACKGROUND);

// border color beyond image bounds

DECL_SETTING_COLOR24(border_color, IDS_SETTING_NAME_BORDER_COLOR, 0x200000);

// bg color shows through image alpha

DECL_SETTING_COLOR24(background_color, IDS_SETTING_NAME_BACKGROUND_COLOR, 0xff00ffu);

// grid on or off

DECL_SETTING_BOOL(checkerboard_enabled, IDS_SETTING_NAME_CHECKERBOARD_ENABLED, true);

// grid colors

DECL_SETTING_COLOR24(grid_color_1, IDS_SETTING_NAME_CHECKERBOARD_COLOR1, 0x101010);
DECL_SETTING_COLOR24(grid_color_2, IDS_SETTING_NAME_CHECKERBOARD_COLOR2, 0x202020);

// grid size in pixels

DECL_SETTING_RANGED(grid_size, IDS_SETTING_NAME_CHECKERBOARD_SIZE, 16, 4, 128);

// grid origin is screen origin or image origin

DECL_SETTING_BOOL(fixed_checkerboard, IDS_SETTING_NAME_FIXED_CHECKERBOARD, true);

//////////////////////////////////////////////////////////////////////
// selection

DECL_SETTING_SECTION(selection_section, IDS_SETTING_SECTION_SELECT);

// selection rectangle fill color

DECL_SETTING_COLOR32(select_fill_color, IDS_SETTING_NAME_SELECT_FILL_COLOR, 0x80800000);

// selection outline colors (line alternates)

DECL_SETTING_COLOR32(select_outline_color1, IDS_SETTING_NAME_SELECT_OUTLINE_COLOR1, 0x80000000);
DECL_SETTING_COLOR32(select_outline_color2, IDS_SETTING_NAME_SELECT_OUTLINE_COLOR2, 0x80ffffff);

// selection border width (1 or maybe 2, anything bigger likely to cause problems)

DECL_SETTING_RANGED(select_border_width, IDS_SETTING_NAME_SELECT_BORDER_WIDTH, 2, 1, 16);

// selection line dash length

DECL_SETTING_RANGED(dash_length, IDS_SETTING_NAME_SELECT_DASH_LENGTH, 8, 4, 64);

// speed of selection outline dash

DECL_SETTING_RANGED(dash_anim_speed, IDS_SETTING_NAME_SELECT_DASH_ANIM_SPEED, 10, 0, 100);

//////////////////////////////////////////////////////////////////////
// crosshair

DECL_SETTING_SECTION(crosshair_section, IDS_SETTING_SECTION_CROSSHAIR);

// crosshair line colors

DECL_SETTING_COLOR32(crosshair_color1, IDS_SETTING_NAME_CROSSHAIR_COLOR1, 0x80000000);
DECL_SETTING_COLOR32(crosshair_color2, IDS_SETTING_NAME_CROSSHAIR_COLOR2, 0x80808080);

// crosshair dash length

DECL_SETTING_RANGED(crosshair_dash_length, IDS_SETTING_NAME_CROSSHAIR_DASH_LENGTH, 8, 4, 64);

// crosshair dash length

DECL_SETTING_RANGED(crosshair_width, IDS_SETTING_NAME_CROSSHAIR_WIDTH, 2, 1, 16);

// speed of crosshair dash

DECL_SETTING_RANGED(crosshair_dash_anim_speed, IDS_SETTING_NAME_CROSSHAIR_DASH_ANIM_SPEED, 10, 0, 100);

//////////////////////////////////////////////////////////////////////
// mouse buttons

DECL_SETTING_SECTION(mouse_section, IDS_SETTING_SECTION_MOUSE);

// which mouse button for interactive zoom

DECL_SETTING_ENUM(zoom_button, IDS_SETTING_NAME_ZOOM_BUTTON, mouse_button_t, enum_mouse_buttons_map, btn_middle);

// which mouse button for dragging image

DECL_SETTING_ENUM(drag_button, IDS_SETTING_NAME_DRAG_BUTTON, mouse_button_t, enum_mouse_buttons_map, btn_right);

// which mouse button for selection

DECL_SETTING_ENUM(select_button, IDS_SETTING_NAME_SELECT_BUTTON, mouse_button_t, enum_mouse_buttons_map, btn_left);

// which mouse button for popup menu

DECL_SETTING_ENUM(menu_button, IDS_SETTING_NAME_MENU_BUTTON, mouse_button_t, enum_mouse_buttons_map, btn_right);

//////////////////////////////////////////////////////////////////////
// window

DECL_SETTING_SECTION(window_section, IDS_SETTING_SECTION_WINDOW);

// single instance mode

DECL_SETTING_BOOL(reuse_window, IDS_SETTING_NAME_REUSE_WINDOW, true);

// either remember fullscreen mode or always revert to: windowed or fullscreen

DECL_SETTING_ENUM(fullscreen_startup_mode,
                  IDS_SETTING_NAME_STARTUP_FULLSCREEN,
                  fullscreen_startup_option,
                  enum_fullscreen_startup_map,
                  fullscreen_startup_option::start_remember);

// current zoom mode

DECL_SETTING_ENUM(zoom_mode, IDS_SETTING_NAME_ZOOM_MODE, zoom_mode_t, enum_zoom_mode_map, zoom_mode_t::shrink_to_fit);

// default zoom mode at startup

DECL_SETTING_ENUM(startup_zoom_mode,
                  IDS_SETTING_NAME_STARTUP_ZOOM_MODE,
                  startup_zoom_mode_option,
                  enum_startup_zoom_mode_map,
                  startup_zoom_mode_option::startup_zoom_shrink_to_fit);

//////////////////////////////////////////////////////////////////////
// files

DECL_SETTING_SECTION(files_section, IDS_SETTING_SECTION_FILES);

// show either just filename or full path in window titlebar

DECL_SETTING_BOOL(show_full_filename_in_titlebar, IDS_SETTING_NAME_SHOW_FULL_FILENAME, false);

// how to show filename overlay

DECL_SETTING_ENUM(show_filename,
                  IDS_SETTING_NAME_SHOW_FILENAME,
                  show_filename_option,
                  enum_show_filename_map,
                  show_filename_option::show_filename_briefly);

// if there's an image in the clipboard at startup, paste it in

DECL_SETTING_BOOL(auto_paste, IDS_SETTING_NAME_AUTO_PASTE, true);

// try and load the last file that was viewed

DECL_SETTING_BOOL(reload_last_file, IDS_SETTING_NAME_RELOAD_LAST_FILE, true);

DECL_SETTING_RANGED(recent_files_count, IDS_SETTING_RECENT_FILES_COUNT, 10, 3, 20);

// what to do about exif metadata

DECL_SETTING_ENUM(
    image_rotation_option, IDS_SETTING_NAME_EXIF_OPTION, exif_option, enum_exif_map, exif_option::exif_option_apply);

// how much memory to use for caching files and decoded images
// TODO (chs): separate settings for file and uncompressed image caches?
// or... ditch the uncompressed cache? help with exif prompt problem

DECL_SETTING_RANGED(cache_size_mb, IDS_SETTING_NAME_CACHE_SIZE_MB, 128, 16, 4096);    // 1GB memory cache by default

//////////////////////////////////////////////////////////////////////
// internal settings, not exposed in the UI

// grid size multiplier, checkerboard is multiplied by 2^grid_multiplier

DECL_SETTING_UINT(grid_multiplier, SETTING_HIDDEN, 1);

// how far mouse has to move after clicking select button to consider a selection active

DECL_SETTING_UINT(select_start_distance, SETTING_HIDDEN, 4);

// how close in pixels the mouse must be to grab the selection edge

DECL_SETTING_UINT(select_border_grab_size, SETTING_HIDDEN, 8);

// has it ever been run before? if not, use some sensible defaults for window pos

DECL_SETTING_BOOL(first_run, SETTING_HIDDEN, true);

// windowed or fullscreen

DECL_SETTING_BOOL(fullscreen, SETTING_HIDDEN, false);

// non-fullscreen window placement

DECL_SETTING_BINARY(window_placement, SETTING_HIDDEN, WINDOWPLACEMENT, 0);

// last fullscreen rect

DECL_SETTING_BINARY(fullscreen_rect, SETTING_HIDDEN, RECT, 0, 0, 0, 0);

// last file loaded

DECL_SETTING_BINARY(last_file_loaded, SETTING_HIDDEN, std::wstring, L"");
