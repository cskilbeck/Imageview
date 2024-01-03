//////////////////////////////////////////////////////////////////////
// settings page

#include "pch.h"

LOG_CONTEXT("Settings");

namespace
{
    using namespace imageview;
    using namespace imageview::settings_ui;

    //////////////////////////////////////////////////////////////////////
    // scroll info for the settings page

    scroll_pos settings_scrollpos;

    //////////////////////////////////////////////////////////////////////
    // all the settings on the settings page

    std::list<setting_controller *> controllers;

    //////////////////////////////////////////////////////////////////////
    // which one is expanding (have to make sure it's visible)

    section_setting const *active_section;

    //////////////////////////////////////////////////////////////////////
    // suppress sending new settings to main window when bulk update in progress

    bool settings_should_update{ false };

    //////////////////////////////////////////////////////////////////////
    // copy of settings currently being edited

    settings_t dialog_settings;

    //////////////////////////////////////////////////////////////////////
    // copy of settings from when the dialog was shown (for 'cancel')

    settings_t previous_settings;

    bool sections_should_update{ false };

    //////////////////////////////////////////////////////////////////////
    // setup all the controls when the settings have changed outside
    // of the dialog handlers (i.e. from the main window hotkeys or menu)

    void update_all_settings_controls()
    {
        settings_should_update = false;

        for(auto s : controllers) {
            s->update_controls();
        }

        settings_should_update = true;
    }

    //////////////////////////////////////////////////////////////////////
    // animate any expanding/contracting sections
    // update the scroll bars
    // position the sections based on the scrollbar position

    void update_sections(HWND hwnd)
    {
        if(sections_should_update) {

            HWND parent = GetParent(hwnd);
            HWND tab_ctrl = GetDlgItem(parent, IDC_SETTINGS_TAB_CONTROL);

            RECT tab_rect;
            GetWindowRect(tab_ctrl, &tab_rect);
            TabCtrl_AdjustRect(tab_ctrl, false, &tab_rect);

            // update and get new heights of the sections

            int total_height = 0;

            // speed based on size of tab rect so dpi taken into account
            int expand_contract_speed = std::max(3, rect_height(tab_rect) / 20);

            sections_should_update = false;

            // expand/contract sections, get total height and check if we're done animating
            for(auto const s : section_setting::sections) {

                int diff = sgn(s->target_height - s->current_height);

                if(diff != 0) {

                    diff *= expand_contract_speed;
                    s->current_height = std::clamp(s->current_height + diff, s->banner_height, s->expanded_height);
                    sections_should_update |= s->current_height != s->target_height;
                }
                total_height += s->current_height;
            }

            // set scrollbars based on total height

            int tab_width = rect_width(tab_rect);

            update_scrollbars(settings_scrollpos, hwnd, tab_rect, SIZE{ tab_width, total_height });

            // scroll so required section is within view

            if(active_section != null) {

                // currently visible portion of the whole window
                int v_top = settings_scrollpos[SB_VERT];
                int v_bottom = v_top + rect_height(tab_rect);

                // vertical extent of the required section
                RECT rc;
                GetWindowRect(active_section->window, &rc);
                MapWindowPoints(HWND_DESKTOP, hwnd, reinterpret_cast<LPPOINT>(&rc), 2);

                // scroll active section (most recently expanded) up into view if necessary
                int top = rc.top + v_top;
                int bottom = top + active_section->current_height;
                int diff = bottom - v_bottom;
                if(diff > 0) {
                    v_top += diff;
                    SetScrollPos(hwnd, SB_VERT, v_top, true);
                    settings_scrollpos[SB_VERT] = GetScrollPos(hwnd, SB_VERT);
                }
            }

            // move all the sections based on scrollbar position

            HDWP dwp = BeginDeferWindowPos(static_cast<int>(section_setting::sections.size()));

            int ypos = -settings_scrollpos[SB_VERT];
            int xpos = -settings_scrollpos[SB_HORZ];

            for(auto const s : section_setting::sections) {

                int height = s->current_height;

                DeferWindowPos(dwp, s->window, null, xpos, ypos, tab_width, height, SWP_NOZORDER | SWP_SHOWWINDOW);

                ypos += height;
            }

            EndDeferWindowPos(dwp);

            // if still need more expanding/contracting, set a timer to get it done

            if(!sections_should_update) {
                active_section = null;
            }
        }
    }

    //////////////////////////////////////////////////////////////////////
    // force settings controllers to update

    void refresh_sections(HWND hwnd)
    {
        sections_should_update = true;
        update_sections(hwnd);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_VSCROLL

    void on_vscroll_settings(HWND hwnd, HWND hwndCtl, UINT code, int pos)
    {
        on_scroll(settings_scrollpos, hwnd, SB_VERT, code);
        refresh_sections(hwnd);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_INITDIALOG

    BOOL on_initdialog_settings(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        previous_settings = settings;    // for reverting
        dialog_settings = settings;      // currently editing

        settings_should_update = false;

        // create the list of settings controllers
        controllers.clear();
        section_setting::sections.clear();

#include "settings_reset_decls.h"

        // MSVC suppresses trailing comma when VA_ARGS is empty...
        // (__VA_OPT__ not supported without the new preprocessor which causes problems)

#define ADD_SETTING(name, str, type, ...)                                                    \
    do {                                                                                     \
        if constexpr(str != SETTING_HIDDEN) {                                                \
            controllers.push_back(new type(L#name, str, dialog_settings.name, __VA_ARGS__)); \
        }                                                                                    \
    } while(false);

#define DECL_SETTING_SECTION(name, str) ADD_SETTING(name, str, section_setting)

#define DECL_SETTING_BOOL(name, str, value) ADD_SETTING(name, str, bool_setting)

#define DECL_SETTING_COLOR24(name, str, bgr) ADD_SETTING(name, str, color_setting, false)

#define DECL_SETTING_COLOR32(name, str, abgr) ADD_SETTING(name, str, color_setting, true)

#define DECL_SETTING_ENUM(name, str, type, enum_names, value) ADD_SETTING(name, str, enum_setting, enum_names)

#define DECL_SETTING_RANGED(name, str, value, min, max) ADD_SETTING(name, str, ranged_setting, min, max)

        // binary and uint settings are all internal (hidden from UI) so no controllers for them

#define DECL_SETTING_UINT(name, string_id, value)

#define DECL_SETTING_BINARY(name, string_id, type, ...)

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

        int const top_margin = dpi_scale(5);
        int const inner_margin = dpi_scale(3);
        int const bottom_margin = dpi_scale(5);

        // create the sections and populate them

        for(auto const s : controllers) {

            // if it's a new section, parent should be hwnd, else parent should be the current section
            if(s->is_section_header()) {

                if(!section_setting::sections.empty()) {
                    section_setting::sections.back()->expanded_height += bottom_margin;
                }

                section_setting *cur_section = reinterpret_cast<section_setting *>(s);

                HWND section_window = CreateDialogParamW(app::instance,
                                                         MAKEINTRESOURCEW(s->dialog_resource_id),
                                                         hwnd,
                                                         s->dlg_proc,
                                                         reinterpret_cast<LPARAM>(s));

                RECT rc;
                GetWindowRect(section_window, &rc);

                cur_section->banner_height = rect_height(rc);

                int height = cur_section->banner_height;

                cur_section->expanded_height = height;
                cur_section->current_height = height;
                cur_section->target_height = height;

            } else {

                section_setting *cur_section = section_setting::sections.back();

                HWND controller_window = CreateDialogParamW(app::instance,
                                                            MAKEINTRESOURCEW(s->dialog_resource_id),
                                                            cur_section->window,
                                                            s->dlg_proc,
                                                            reinterpret_cast<LPARAM>(s));

                if(cur_section->banner_height == cur_section->expanded_height) {
                    cur_section->expanded_height += top_margin;
                }

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

        if(!section_setting::sections.empty()) {
            section_setting::sections.back()->expanded_height += bottom_margin;
        }

        // expand sections which were expanded last time

        for(auto const s : controllers) {
            if(s->is_section_header()) {
                section_setting *ss = reinterpret_cast<section_setting *>(s);
                if(ss->expanded) {
                    ss->target_height = ss->expanded_height;
                    ss->current_height = ss->expanded_height;
                }
            }
        }

        // size the window to fit within the tab control exactly

        int tab_width = rect_width(tab_rect);

        SetWindowPos(hwnd, null, 0, 0, tab_width, rect_height(tab_rect), SWP_NOZORDER | SWP_NOMOVE);

        settings_scrollpos[SB_HORZ] = 0;
        settings_scrollpos[SB_VERT] = 0;

        refresh_sections(hwnd);

        // uncork the settings notifier
        settings_should_update = true;

        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_MOUSEWHEEL

    void on_mousewheel_settings(HWND hwnd, int xPos, int yPos, int zDelta, UINT fwKeys)
    {
        scroll_window(settings_scrollpos, hwnd, SB_VERT, -zDelta / WHEEL_DELTA);
        refresh_sections(hwnd);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_USER
    // WM_USER toggles section expand/collapse

    void on_user_settings(HWND hwnd, WPARAM wparam, LPARAM lparam)
    {
        section_setting *section = reinterpret_cast<section_setting *>(lparam);

        if(section->expanded) {
            active_section = section;
        }

        refresh_sections(hwnd);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_SHOWWINDOW
    // show/hide the Revert button

    void on_showwindow_settings(HWND hwnd, BOOL fShow, UINT status)
    {
        HWND main_dialog = GetParent(hwnd);
        HWND revert_button = GetDlgItem(main_dialog, IDC_SPLIT_BUTTON_SETTINGS);
        ShowWindow(revert_button, fShow ? SW_SHOW : SW_HIDE);
    }
}

namespace imageview::settings_ui
{
    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ DLGPROC

    INT_PTR settings_dlgproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(hWnd, WM_VSCROLL, on_vscroll_settings);
            HANDLE_MSG(hWnd, WM_INITDIALOG, on_initdialog_settings);
            HANDLE_MSG(hWnd, WM_MOUSEWHEEL, on_mousewheel_settings);
            HANDLE_MSG(hWnd, WM_USER, on_user_settings);
            HANDLE_MSG(hWnd, WM_SHOWWINDOW, on_showwindow_settings);
        }
        return ctlcolor_base(hWnd, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////

    void reset_settings_to_defaults()
    {
        dialog_settings = default_settings;
        update_all_settings_controls();
        post_new_settings();
    }

    //////////////////////////////////////////////////////////////////////

    void save_current_settings()
    {
        dialog_settings.save();
    }

    //////////////////////////////////////////////////////////////////////

    void load_saved_settings()
    {
        dialog_settings.load();
        update_all_settings_controls();
        post_new_settings();
    }

    //////////////////////////////////////////////////////////////////////

    void revert_settings()
    {
        dialog_settings = previous_settings;
        update_all_settings_controls();
        post_new_settings();
    }

    //////////////////////////////////////////////////////////////////////

    void on_new_settings(settings_t const *new_settings)
    {
        dialog_settings = *new_settings;
        update_all_settings_controls();
    }

    //////////////////////////////////////////////////////////////////////
    // send new settings to the main window from the settings_dialog

    void post_new_settings()
    {
        if(settings_should_update) {

            // main window is responsible for freeing this copy of the settings
            settings_t *settings_copy = new settings_t();
            *settings_copy = dialog_settings;
            PostMessage(app::window, app::WM_NEW_SETTINGS, 0, reinterpret_cast<LPARAM>(settings_copy));
        }
    }

    //////////////////////////////////////////////////////////////////////

    void animate_settings_page(HWND hwnd)
    {
        update_sections(hwnd);
    }
}
