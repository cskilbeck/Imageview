#pragma once

namespace imageview::settings_dialog
{
    //////////////////////////////////////////////////////////////////////

    enum tab_flags_t
    {
        dont_care = 0,
        hide_if_elevated = (1 << 0),
        hide_if_not_elevated = (1 << 1),
    };

    //////////////////////////////////////////////////////////////////////

    struct tab_page_t
    {
        uint resource_id;
        DLGPROC dlg_proc;
        int flags;
        int index;
        HWND hwnd;
    };

    //////////////////////////////////////////////////////////////////////

    INT_PTR settings_dlgproc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR hotkeys_dlgproc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR explorer_dlgproc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR relaunch_dlgproc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR about_dlgproc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);

    void on_reset_default_settings();
    void on_save_current_settings();
    void on_load_settings();
    void on_revert_settings();
    void on_new_settings(settings_t const *new_settings);

    void post_new_settings();
}
