//////////////////////////////////////////////////////////////////////
// settings page

#include "pch.h"

LOG_CONTEXT("Settings");

namespace
{
    using namespace imageview;
    using namespace imageview::settings_ui;

    //////////////////////////////////////////////////////////////////////
    // container for the settings

    HWND settings_container;

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
    // animate any expanding/contracting sections
    // update the scroll bars
    // position the sections based on the scrollbar position

    void update_sections(HWND hwnd)
    {
        if(sections_should_update) {

            RECT tab_rect;

            // HWND page = GetParent(hwnd);
            // HWND parent = GetParent(page);
            // HWND tab_ctrl = GetDlgItem(parent, IDC_SETTINGS_TAB_CONTROL);
            // GetWindowRect(tab_ctrl, &tab_rect);
            // TabCtrl_AdjustRect(tab_ctrl, false, &tab_rect);

            GetClientRect(hwnd, &tab_rect);

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
    // setup all the controls when the settings have changed outside
    // of the dialog handlers (i.e. from the main window hotkeys or menu)

    void update_all_settings_controls()
    {
        settings_should_update = false;
        sections_should_update = true;

        for(auto s : controllers) {
            s->update_controls();
        }

        settings_should_update = true;
    }

    //////////////////////////////////////////////////////////////////////
    // force settings controllers to update

    void refresh_sections(HWND hwnd)
    {
        sections_should_update = true;
        update_sections(hwnd);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS container \ WM_VSCROLL

    void on_vscroll_settings_container(HWND hwnd, HWND hwndCtl, UINT code, int pos)
    {
        on_scroll(settings_scrollpos, hwnd, SB_VERT, code);
        refresh_sections(hwnd);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS container \ WM_MOUSEWHEEL

    void on_mousewheel_settings_container(HWND hwnd, int xPos, int yPos, int zDelta, UINT fwKeys)
    {
        scroll_window(settings_scrollpos, hwnd, SB_VERT, -zDelta / WHEEL_DELTA);
        refresh_sections(hwnd);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS container \ WM_USER
    // WM_USER toggles section expand/collapse

    void on_user_settings_container(HWND hwnd, WPARAM wparam, LPARAM lparam)
    {
        section_setting *section = reinterpret_cast<section_setting *>(lparam);

        if(section->expanded) {
            active_section = section;
        }

        refresh_sections(hwnd);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS container \ WM_INITDIALOG

    BOOL on_initdialog_settings_container(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
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

        // we need the size of the tab page

        HWND page = GetParent(hwnd);
        HWND main_dialog = GetParent(page);

        HWND placeholder = GetDlgItem(page, IDC_STATIC_CONTAINER_PLACEHOLDER);

        RECT settings_rect;
        GetWindowRect(placeholder, &settings_rect);
        MapWindowPoints(HWND_DESKTOP, page, reinterpret_cast<LPPOINT>(&settings_rect), 2);

        // placeholder was just for placement, destroy it

        DestroyWindow(placeholder);
        placeholder = null;

        // add all the settings

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
        // We need the +1 because the dialog editor won't let us size
        // the window to fill the area exactly

        SetWindowPos(hwnd, null, 0, 0, rect_width(settings_rect) + 1, rect_height(settings_rect) + 1, SWP_NOZORDER);

        settings_scrollpos[SB_HORZ] = 0;
        settings_scrollpos[SB_VERT] = 0;

        refresh_sections(hwnd);

        InvalidateRect(main_dialog, null, false);

        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS container \ DLGPROC

    INT_PTR settings_container_dlgproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(hWnd, WM_VSCROLL, on_vscroll_settings_container);
            HANDLE_MSG(hWnd, WM_INITDIALOG, on_initdialog_settings_container);
            HANDLE_MSG(hWnd, WM_MOUSEWHEEL, on_mousewheel_settings_container);
            HANDLE_MSG(hWnd, WM_USER, on_user_settings_container);
        }
        return ctlcolor_base(hWnd, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_INITDIALOG

    BOOL on_initdialog_settings(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        previous_settings = settings;    // for reverting
        dialog_settings = settings;      // currently editing

        settings_should_update = false;

        // create the settings container

        settings_container = CreateDialogW(
            app::instance, MAKEINTRESOURCEW(IDD_DIALOG_SETTINGS_CONTAINER), hwnd, settings_container_dlgproc);

        // uncork the settings notifier
        settings_should_update = true;

        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_PAINT
    // draw a grey separator line between the settings and the controls underneath

    void on_paint_settings(HWND hwnd)
    {
        RECT settings_rect;
        GetClientRect(settings_container, &settings_rect);

        RECT client_rect;
        GetClientRect(hwnd, &client_rect);

        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);

        SetDCPenColor(ps.hdc, GetSysColor(COLOR_3DSHADOW));
        HGDIOBJ old_pen = SelectObject(ps.hdc, GetStockObject(DC_PEN));

        MoveToEx(ps.hdc, client_rect.left, settings_rect.bottom, null);
        LineTo(ps.hdc, client_rect.right, settings_rect.bottom);

        SelectObject(ps.hdc, old_pen);

        EndPaint(hwnd, &ps);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_NOTIFY

    int on_notify_settings(HWND hwnd, int idFrom, LPNMHDR nmhdr)
    {
        switch(idFrom) {

            // clicked the little arrow on the split button

        case IDC_SPLIT_BUTTON_SETTINGS: {

#pragma warning(suppress : 26454)
            if(nmhdr->code == (uint)(BCN_DROPDOWN)) {

                RECT rc;
                GetWindowRect(GetDlgItem(hwnd, IDC_SPLIT_BUTTON_SETTINGS), &rc);
                HMENU menu = LoadMenuW(app::instance, MAKEINTRESOURCEW(IDR_MENU_POPUP_SETTINGS_SPLIT_BUTTON));
                HMENU popup = GetSubMenu(menu, 0);
                TPMPARAMS tpm;
                tpm.cbSize = sizeof(TPMPARAMS);
                tpm.rcExclude = rc;
                uint choice = TrackPopupMenuEx(popup,
                                               TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL | TPM_RETURNCMD,
                                               rc.left,
                                               rc.bottom,
                                               hwnd,
                                               &tpm);
                DestroyMenu(menu);

                switch(choice) {

                case ID_POPUP_SETTINGS_RESET_DEFAULT:
                    reset_settings_to_defaults();
                    break;

                case ID_POPUP_SETTINGS_SAVE:
                    settings_ui::save_current_settings();
                    break;

                case ID_POPUP_SETTINGS_LOAD_SAVED:
                    settings_ui::load_saved_settings();
                    break;
                }
            }
        } break;
        }
        return 0;
    }


    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_COMMAND

    void on_command_settings(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

            // clicked the split button

        case IDC_SPLIT_BUTTON_SETTINGS:
            settings_ui::revert_settings();
            break;
        }
    }
}

namespace imageview::settings_ui
{
    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ DLGPROC

    INT_PTR settings_dlgproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(hWnd, WM_INITDIALOG, on_initdialog_settings);
            HANDLE_MSG(hWnd, WM_PAINT, on_paint_settings);
            HANDLE_MSG(hWnd, WM_COMMAND, on_command_settings);
            HANDLE_MSG(hWnd, WM_NOTIFY, on_notify_settings);
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
        settings_changed = true;

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
        update_sections(settings_container);
    }
}
