//////////////////////////////////////////////////////////////////////
// settings page

#include "pch.h"

LOG_CONTEXT("Settings");

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

    void update_sections(HWND hwnd)
    {
        HWND tab_ctrl;
        HWND parent = GetParent(hwnd);
        tab_ctrl = GetDlgItem(parent, IDC_SETTINGS_TAB_CONTROL);

        RECT tab_rect;
        GetWindowRect(tab_ctrl, &tab_rect);
        TabCtrl_AdjustRect(tab_ctrl, false, &tab_rect);

        // get height of the sections

        int total_height = 0;

        for(auto const s : section_setting::sections) {

            int height = s->banner_height;

            if(s->expanded) {
                height = s->expanded_height;
            }

            total_height += height;
        }

        // set scrollbars based on total height

        update_scrollbars(settings_scroll, hwnd, tab_rect, SIZE{ rect_width(tab_rect), total_height });

        // move all the sections based on scrollbar position

        HDWP dwp = BeginDeferWindowPos(static_cast<int>(section_setting::sections.size() + 1u));

        int ypos = -settings_scroll.pos[SB_VERT];
        int xpos = -settings_scroll.pos[SB_HORZ];

        for(auto const s : section_setting::sections) {

            int height = s->banner_height;

            if(s->expanded) {
                height = s->expanded_height;
            }


            DeferWindowPos(
                dwp, s->window, null, xpos, ypos, rect_width(tab_rect), height, SWP_NOZORDER | SWP_SHOWWINDOW);

            ypos += height;
        }

        EndDeferWindowPos(dwp);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_VSCROLL

    void on_vscroll_settings(HWND hwnd, HWND hwndCtl, UINT code, int pos)
    {
        on_scroll(settings_scroll, hwnd, SB_VERT, code);
        update_sections(hwnd);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_INITDIALOG

    BOOL on_initdialog_settings(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        revert_settings = settings;    // for reverting
        dialog_settings = settings;    // currently editing

        settings_should_update = false;

        // create the list of settings controllers
        setting_controllers.clear();
        section_setting::sections.clear();

#undef DECL_SETTING_SECTION
#undef DECL_SETTING_BOOL
#undef DECL_SETTING_COLOR
#undef DECL_SETTING_ENUM
#undef DECL_SETTING_RANGED
#undef DECL_SETTING_INTERNAL

#define DECL_SETTING_SECTION(string_id)          \
    {                                            \
        auto s = new section_setting(string_id); \
        setting_controllers.push_back(s);        \
    }

#define DECL_SETTING_BOOL(name, string_id, value)                                                                    \
    {                                                                                                                \
        auto b =                                                                                                     \
            new bool_setting(#name, string_id, IDD_DIALOG_SETTING_BOOL, setting_bool_dlgproc, dialog_settings.name); \
        setting_controllers.push_back(b);                                                                            \
    }

#define DECL_SETTING_COLOR(name, string_id, argb, alpha)                                                     \
    {                                                                                                        \
        auto c = new color_setting(                                                                          \
            #name, string_id, IDD_DIALOG_SETTING_COLOR, setting_color_dlgproc, dialog_settings.name, alpha); \
        setting_controllers.push_back(c);                                                                    \
    }

#define DECL_SETTING_ENUM(type, name, string_id, enum_names, value)                \
    {                                                                              \
        auto e = new enum_setting(#name,                                           \
                                  string_id,                                       \
                                  IDD_DIALOG_SETTING_ENUM,                         \
                                  setting_enum_dlgproc,                            \
                                  enum_names,                                      \
                                  reinterpret_cast<uint &>(dialog_settings.name)); \
        setting_controllers.push_back(e);                                          \
    }

#define DECL_SETTING_RANGED(name, string_id, value, min, max)                                                     \
    {                                                                                                             \
        auto r = new ranged_setting(                                                                              \
            #name, string_id, IDD_DIALOG_SETTING_RANGED, setting_ranged_dlgproc, dialog_settings.name, min, max); \
        setting_controllers.push_back(r);                                                                         \
    }

#define DECL_SETTING_INTERNAL(setting_type, name, ...)

#include "settings_fields.h"

        // get parent tab window rect
        HWND main_dialog = GetParent(hwnd);
        HWND tab_ctrl = GetDlgItem(main_dialog, IDC_SETTINGS_TAB_CONTROL);

        RECT tab_rect;
        GetWindowRect(tab_ctrl, &tab_rect);
        MapWindowPoints(null, main_dialog, reinterpret_cast<LPPOINT>(&tab_rect), 2);
        TabCtrl_AdjustRect(tab_ctrl, false, &tab_rect);

        float dpi = get_window_dpi(main_dialog);

        auto dpi_scale = [=](int x) { return static_cast<int>(x * dpi / 96.0f); };

        int const inner_margin = dpi_scale(3);
        // int const bottom_margin = dpi_scale(12);

        // create the sections and populate them

        for(auto const s : setting_controllers) {

            // if it's a new section, parent should be hwnd, else parent should be the current section
            if(s->is_section_header()) {

                section_setting *cur_section = reinterpret_cast<section_setting *>(s);

                HWND section_window = CreateDialogParamA(app::instance,
                                                         MAKEINTRESOURCE(s->dialog_resource_id),
                                                         hwnd,
                                                         s->dlg_proc,
                                                         reinterpret_cast<LPARAM>(s));

                RECT rc;
                GetWindowRect(section_window, &rc);

                cur_section->banner_height = rect_height(rc);
                cur_section->expanded_height = cur_section->banner_height;

            } else {

                section_setting *cur_section = section_setting::sections.back();

                HWND controller_window = CreateDialogParamA(app::instance,
                                                            MAKEINTRESOURCE(s->dialog_resource_id),
                                                            cur_section->window,
                                                            s->dlg_proc,
                                                            reinterpret_cast<LPARAM>(s));

                // stack the setting_controller within the section
                SetWindowPos(controller_window,
                             null,
                             0,
                             cur_section->expanded_height,
                             0,
                             0,
                             SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW);

                RECT rc;
                GetWindowRect(controller_window, &rc);
                int height = rect_height(rc);

                cur_section->expanded_height += height + inner_margin;
            }
        }

        // size the window to fit within the tab control exactly

        SetWindowPos(hwnd, null, 0, 0, rect_width(tab_rect), rect_height(tab_rect), SWP_NOZORDER | SWP_NOMOVE);

        settings_scroll.pos[SB_HORZ] = 0;
        settings_scroll.pos[SB_VERT] = 0;

        update_sections(hwnd);

        // uncork the settings notifier
        settings_should_update = true;

        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_MOUSEWHEEL

    void on_mousewheel_settings(HWND hwnd, int xPos, int yPos, int zDelta, UINT fwKeys)
    {
        scroll_window(settings_scroll, hwnd, SB_VERT, -zDelta / WHEEL_DELTA);
        update_sections(hwnd);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_COMMAND (but faked from section button)

    void on_command_settings(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        update_sections(hwnd);
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
            HANDLE_MSG(hWnd, WM_COMMAND, on_command_settings);
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
