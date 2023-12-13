#pragma once

namespace imageview::settings_ui
{
    void on_reset_default_settings();
    void on_save_current_settings();
    void on_load_settings();
    void on_revert_settings();
    void on_new_settings(settings_t const *new_settings);

    void post_new_settings();

    INT_PTR settings_dlgproc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
}
