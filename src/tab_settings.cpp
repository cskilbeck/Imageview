//////////////////////////////////////////////////////////////////////
// settings page

#include "pch.h"

namespace
{
    using namespace imageview;
    using namespace imageview::settings_dialog;

    //////////////////////////////////////////////////////////////////////
    // scroll info for the settings page

    imageview::scroll_info settings_scroll;

    //////////////////////////////////////////////////////////////////////
    // all the settings on the settings page

    std::list<setting_controller *> setting_controllers;

    //////////////////////////////////////////////////////////////////////
    // suppress sending new settings to main window when bulk update in progress

    bool settings_should_update{ false };

    //////////////////////////////////////////////////////////////////////
    // copy of settings currently being edited

    settings_t dialog_settings;

    //////////////////////////////////////////////////////////////////////
    // copy of settings from when the dialog was shown (for 'cancel')

    settings_t revert_settings;

    //////////////////////////////////////////////////////////////////////
    // setup all the controls when the settings have changed outside
    // of the dialog handlers (i.e. from the main window hotkeys or menu)

    void update_all_settings_controls()
    {
        settings_should_update = false;

        for(auto s : setting_controllers) {
            s->update_controls();
        }

        settings_should_update = true;
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_VSCROLL

    void on_vscroll_settings(HWND hwnd, HWND hwndCtl, UINT code, int pos)
    {
        on_scroll(settings_scroll, SB_VERT, code);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_INITDIALOG

    BOOL on_initdialog_settings(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        revert_settings = settings;    // for reverting
        dialog_settings = settings;    // currently editing

        settings_should_update = false;

        // add all the settings controller child dialogs to the page

        if(setting_controllers.empty()) {

#undef DECL_SETTING_SEPARATOR
#undef DECL_SETTING_BOOL
#undef DECL_SETTING_COLOR
#undef DECL_SETTING_ENUM
#undef DECL_SETTING_RANGED
#undef DECL_SETTING_INTERNAL

#define DECL_SETTING_SEPARATOR(string_id) setting_controllers.push_back(new separator_setting(string_id))

#define DECL_SETTING_BOOL(name, string_id, value) \
    setting_controllers.push_back(                \
        new bool_setting(#name, string_id, IDD_DIALOG_SETTING_BOOL, setting_bool_dlgproc, dialog_settings.name));

#define DECL_SETTING_COLOR(name, string_id, argb) \
    setting_controllers.push_back(                \
        new color_setting(#name, string_id, IDD_DIALOG_SETTING_COLOR, setting_color_dlgproc, dialog_settings.name));

#define DECL_SETTING_ENUM(type, name, string_id, enum_names, value)         \
    setting_controllers.push_back(new enum_setting(#name,                   \
                                                   string_id,               \
                                                   IDD_DIALOG_SETTING_ENUM, \
                                                   setting_enum_dlgproc,    \
                                                   enum_names,              \
                                                   reinterpret_cast<uint &>(dialog_settings.name)));

#define DECL_SETTING_RANGED(name, string_id, value, min, max) \
    setting_controllers.push_back(new ranged_setting(         \
        #name, string_id, IDD_DIALOG_SETTING_RANGED, setting_ranged_dlgproc, dialog_settings.name, min, max));

#define DECL_SETTING_INTERNAL(setting_type, name, ...)

#include "settings_fields.h"
        }

        // get parent tab window rect
        HWND main_dialog = GetParent(hwnd);
        HWND tab_ctrl = GetDlgItem(main_dialog, IDC_SETTINGS_TAB_CONTROL);

        RECT tab_rect;
        GetWindowRect(tab_ctrl, &tab_rect);
        MapWindowPoints(null, main_dialog, reinterpret_cast<LPPOINT>(&tab_rect), 2);
        TabCtrl_AdjustRect(tab_ctrl, false, &tab_rect);

        float dpi = get_window_dpi(main_dialog);

        auto dpi_scale = [=](int x) { return static_cast<int>(x * dpi / 96.0f); };

        int const top_margin = dpi_scale(12);
        int const inner_margin = dpi_scale(3);
        int const bottom_margin = dpi_scale(12);

        int cur_height = top_margin;

        for(auto const s : setting_controllers) {
            HWND setting = CreateDialogParamA(
                app::instance, MAKEINTRESOURCE(s->dialog_resource_id), hwnd, s->dlg_proc, reinterpret_cast<LPARAM>(s));
            RECT r;
            GetWindowRect(setting, &r);
            SetWindowPos(
                setting, null, 0, cur_height, rect_width(tab_rect), rect_height(r), SWP_NOZORDER | SWP_SHOWWINDOW);
            cur_height += rect_height(r) + inner_margin;
        }

        cur_height += bottom_margin;

        // make this window big enough to contain the settings
        SetWindowPos(hwnd, null, 0, 0, rect_width(tab_rect), cur_height, SWP_NOZORDER | SWP_NOMOVE);
        update_scrollbars(settings_scroll, hwnd, tab_rect);

        // uncork the settings notifier
        settings_should_update = true;

        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_MOUSEWHEEL

    void on_mousewheel_settings(HWND hwnd, int xPos, int yPos, int zDelta, UINT fwKeys)
    {
        scroll_window(settings_scroll, SB_VERT, -zDelta / WHEEL_DELTA);
    }
}

namespace imageview::settings_dialog
{
    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ DLGPROC

    INT_PTR settings_dlgproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(hWnd, WM_VSCROLL, on_vscroll_settings);
            HANDLE_MSG(hWnd, WM_INITDIALOG, on_initdialog_settings);
            HANDLE_MSG(hWnd, WM_MOUSEWHEEL, on_mousewheel_settings);
        }
        return ctlcolor_base(hWnd, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////

    void on_reset_default_settings()
    {
        dialog_settings = default_settings;
        update_all_settings_controls();
        post_new_settings();
    }

    //////////////////////////////////////////////////////////////////////

    void on_save_current_settings()
    {
        dialog_settings.save();
    }

    //////////////////////////////////////////////////////////////////////

    void on_load_settings()
    {
        dialog_settings.load();
        update_all_settings_controls();
        post_new_settings();
    }

    //////////////////////////////////////////////////////////////////////

    void on_revert_settings()
    {
        dialog_settings = revert_settings;
        update_all_settings_controls();
        post_new_settings();
    }

    //////////////////////////////////////////////////////////////////////

    void on_new_settings(settings_t const *new_settings)
    {
        memcpy(&dialog_settings, new_settings, sizeof(settings_t));
        update_all_settings_controls();
    }

    //////////////////////////////////////////////////////////////////////
    // send new settings to the main window from the settings_dialog

    void post_new_settings()
    {
        if(settings_should_update) {

            // main window is responsible for freeing this copy of the settings
            settings_t *settings_copy = new settings_t();
            memcpy(settings_copy, &dialog_settings, sizeof(settings_t));
            PostMessage(app::window, app::WM_NEW_SETTINGS, 0, reinterpret_cast<LPARAM>(settings_copy));
        }
    }
}
