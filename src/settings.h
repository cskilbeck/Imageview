#ifndef DECL_SETTING
#error Please define DECL_SETTING
#endif

// DECL_SETTING(type, name, default)

DECL_SETTING(vec4, grid_color_1, 0.125f, 0.125f, 0.125f, 1);
DECL_SETTING(vec4, grid_color_2, 0.25f, 0.25f, 0.25f, 1);
DECL_SETTING(vec4, select_fill_color, 0.1f, 0.2f, 0.5f, 0.5f);
DECL_SETTING(vec4, select_outline_color1, 1, 1, 1, 0.2f);
DECL_SETTING(vec4, select_outline_color2, 0, 0, 0, 0.4f);
DECL_SETTING(vec4, crosshair_color1, 0, 0, 0, 0.25f);
DECL_SETTING(vec4, crosshair_color2, 1, 1, 1, 0.25f);
DECL_SETTING(vec4, background_color, 0.05f, 0.1f, 0.3f, 1);
DECL_SETTING(uint, dash_length, 8);
DECL_SETTING(float, grid_size, 16.0f);
DECL_SETTING(bool, fixed_grid, true);
DECL_SETTING(bool, fullscreen, false);
DECL_SETTING(int, select_button, btn_left);
DECL_SETTING(int, zoom_button, btn_middle);
DECL_SETTING(int, drag_button, btn_right);
DECL_SETTING(int, select_border_width, 1);
DECL_SETTING(reset_zoom_mode, zoom_mode, reset_zoom_mode::shrink_to_fit);
DECL_SETTING(bool, reuse_window, true);
DECL_SETTING(show_filename_option, show_filename, show_filename_option::briefly);
DECL_SETTING(fullscreen_startup_option, fullscreen_mode, fullscreen_startup_option::start_remember);
DECL_SETTING(bool, first_run, true);
DECL_SETTING(WINDOWPLACEMENT, window_placement, sizeof(WINDOWPLACEMENT));
DECL_SETTING(rect, fullscreen_rect, 0, 0, 0, 0);
DECL_SETTING(exif_option, image_rotation_option, exif_option::apply);
DECL_SETTING(bool, show_full_filename_in_titlebar, true);
DECL_SETTING(float, select_border_grab_size, 8.0f);