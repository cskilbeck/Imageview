#pragma once

namespace imageview::settings_ui
{
    void reset_settings_to_defaults();
    void save_current_settings();
    void load_saved_settings();
    void revert_settings();
    void on_new_settings(settings_t const *new_settings);

    void post_new_settings();

    INT_PTR settings_dlgproc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
}
