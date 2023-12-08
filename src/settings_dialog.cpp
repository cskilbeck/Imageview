//////////////////////////////////////////////////////////////////////

#include "pch.h"

#pragma warning(disable : 4100)

LOG_CONTEXT("settings_dlg");

//////////////////////////////////////////////////////////////////////

namespace
{
    using namespace imageview;
    using namespace imageview::settings_dialog;

    struct tab_page_t;

    //////////////////////////////////////////////////////////////////////

    HWND main_dialog = null;

    HWND app_window = null;

    //////////////////////////////////////////////////////////////////////
    // scroll info for the settings page

    scroll_info settings_scroll;

    //////////////////////////////////////////////////////////////////////
    // all the settings on the settings page

    std::list<setting_controller *> setting_controllers;

    //////////////////////////////////////////////////////////////////////
    // copy of settings currently being edited

    settings_t dialog_settings;

    //////////////////////////////////////////////////////////////////////
    // copy of settings from when the dialog was shown (for 'cancel')

    settings_t revert_settings;

    //////////////////////////////////////////////////////////////////////
    // suppress sending new settings to main window when bulk update in progress

    bool settings_should_update{ false };

    //////////////////////////////////////////////////////////////////////
    // which tabs will be shown (some might be hidden)

    std::vector<tab_page_t *> active_tabs;

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

    //////////////////////////////////////////////////////////////////////
    // all the tabs that can be created

    tab_page_t all_tabs[] = {
        { IDD_DIALOG_SETTINGS_MAIN, settings_dlgproc, dont_care, -1, null },
        { IDD_DIALOG_SETTINGS_HOTKEYS, hotkeys_dlgproc, dont_care, -1, null },
        { IDD_DIALOG_SETTINGS_EXPLORER, explorer_dlgproc, hide_if_not_elevated, -1, null },
        { IDD_DIALOG_SETTINGS_RELAUNCH, relaunch_dlgproc, hide_if_elevated, -1, null },
        { IDD_DIALOG_SETTINGS_ABOUT, about_dlgproc, dont_care, -1, null },
    };

    //////////////////////////////////////////////////////////////////////

    bool should_hide_tab(tab_page_t const *t)
    {
        return (t->flags & hide_if_elevated) != 0 && app::is_elevated ||
               (t->flags & hide_if_not_elevated) != 0 && !app::is_elevated;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT copy_window_text_to_clipboard(HWND hwnd)
    {
        SetLastError(0);

        int len;
        CHK_ZERO(len = GetWindowTextLengthW(hwnd));

        HANDLE handle;
        CHK_NULL(handle = GlobalAlloc(GHND | GMEM_SHARE, static_cast<size_t>(len * sizeof(wchar)) + 1));

        wchar *buffer;
        CHK_NULL(buffer = reinterpret_cast<wchar *>(GlobalLock(handle)));
        DEFER(GlobalUnlock(handle));

        CHK_ZERO(GetWindowTextW(hwnd, buffer, len + 1));

        CHK_BOOL(OpenClipboard(null));
        DEFER(CloseClipboard());

        CHK_BOOL(EmptyClipboard());
        CHK_BOOL(SetClipboardData(CF_UNICODETEXT, handle));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // switch to a tab page

    HRESULT show_current_page(int show)
    {
        HWND tab_ctrl = GetDlgItem(main_dialog, IDC_SETTINGS_TAB_CONTROL);
        uint tab = TabCtrl_GetCurSel(tab_ctrl);
        if(tab >= active_tabs.size()) {
            LOG_ERROR("!? Tab {} is out of range (there are {} tabs)", tab, active_tabs.size());
            return HRESULT_FROM_WIN32(ERROR_BAD_ARGUMENTS);
        }
        ShowWindow(active_tabs[tab]->hwnd, show);
        return S_OK;
    }

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
    // brutally suppress the text caret in the 'about' text box

    LRESULT CALLBACK suppress_caret_subclass(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR)
    {
        HideCaret(dlg);
        return DefSubclassProc(dlg, msg, wparam, lparam);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page
    //////////////////////////////////////////////////////////////////////

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

#define DECL_SETTING_COLOR(name, string_id, r, g, b, a) \
    setting_controllers.push_back(                      \
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
        HWND tab_ctrl = GetDlgItem(main_dialog, IDC_SETTINGS_TAB_CONTROL);

        RECT tab_rect;
        GetWindowRect(tab_ctrl, &tab_rect);
        MapWindowPoints(null, main_dialog, reinterpret_cast<LPPOINT>(&tab_rect), 2);
        TabCtrl_AdjustRect(tab_ctrl, false, &tab_rect);

        float dpi = get_window_dpi(main_dialog);

        auto dpi_scale = [=](int x) { return static_cast<int>(x * dpi / 96.0f); };

        int const top_margin = dpi_scale(12);
        int const inner_margin = dpi_scale(2);
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

        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_MOUSEWHEEL

    void on_mousewheel_settings(HWND hwnd, int xPos, int yPos, int zDelta, UINT fwKeys)
    {
        scroll_window(settings_scroll, SB_VERT, -zDelta / WHEEL_DELTA);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ DLGPROC

    INT_PTR settings_dlgproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(hWnd, WM_VSCROLL, on_vscroll_settings);
            HANDLE_MSG(hWnd, WM_INITDIALOG, on_initdialog_settings);
            HANDLE_MSG(hWnd, WM_MOUSEWHEEL, on_mousewheel_settings);
        }
        return setting_ctlcolor_base(hWnd, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // RELAUNCH (explorer) page
    //////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    // RELAUNCH (explorer) page \ WM_COMMAND

    void on_command_relaunch(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

        case IDC_BUTTON_SETTINGS_RELAUNCH: {

            main_dialog = null;
            EndDialog(hwnd, 0);
            PostMessage(app_window, app::WM_RELAUNCH_AS_ADMIN, 0, 0);
        } break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // RELAUNCH (explorer) page \ DLGPROC

    INT_PTR relaunch_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_COMMAND, on_command_relaunch);
        }
        return setting_ctlcolor_base(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // EXPLORER page
    //////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    // EXPLORER page \ DLGPROC

    INT_PTR explorer_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return setting_ctlcolor_base(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // ABOUT page
    //////////////////////////////////////////////////////////////////////

    // ABOUT page \ WM_INITDIALOG

    BOOL on_initdialog_about(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        // get rid of the text caret in the edit box
        HWND about = GetDlgItem(hwnd, IDC_SETTINGS_EDIT_ABOUT);
        SetWindowSubclass(about, suppress_caret_subclass, 0, 0);

        SendMessage(about, EM_SETREADONLY, 1, 0);

        // populate the about box text
        std::string version{ "Version?" };
        get_app_version(version);
        SetWindowTextA(about,
                       std::format("{}\r\nv{}\r\nBuilt {}\r\nRunning as admin: {}\r\nSystem Memory {} GB\r\n",
                                   localize(IDS_AppName),
                                   version,
                                   __TIMESTAMP__,
                                   app::is_elevated,
                                   app::system_memory_gb)
                           .c_str());
        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // ABOUT page \ WM_COMMAND

    void on_command_about(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

            // copy 'about' text to clipboard

        case IDC_BUTTON_ABOUT_COPY: {

            copy_window_text_to_clipboard(GetDlgItem(hwnd, IDC_SETTINGS_EDIT_ABOUT));
            SetWindowTextW(GetDlgItem(hwnd, IDC_BUTTON_ABOUT_COPY), unicode(localize(IDS_COPIED)).c_str());

        } break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // ABOUT page \ DLGPROC

    INT_PTR about_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_INITDIALOG, on_initdialog_about);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_about);
        }
        return setting_ctlcolor_base(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page
    //////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page \ WM_INITDIALOG

    BOOL on_initdialog_hotkeys(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        // populate the listview with all the hotkeys

        HWND listview = GetDlgItem(hwnd, IDC_LIST_HOTKEYS);

        RECT listview_rect;
        GetWindowRect(hwnd, &listview_rect);

        int inner_width = rect_width(listview_rect) - GetSystemMetrics(SM_CXVSCROLL);

        LVCOLUMNA column;
        mem_clear(&column);
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.fmt = LVCFMT_LEFT;
        column.cx = inner_width * 70 / 100;
        column.pszText = const_cast<LPSTR>("Action");
        ListView_InsertColumn(listview, 0, &column);
        column.cx = inner_width * 30 / 100;
        column.pszText = const_cast<LPSTR>("Hotkey");
        ListView_InsertColumn(listview, 1, &column);
        ListView_SetExtendedListViewStyle(listview, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_FLATSB);
        ListView_SetView(listview, LV_VIEW_DETAILS);

        // get all the hotkey descriptions in order

        std::map<std::string, uint> descriptions;

        for(auto const &a : hotkeys::hotkey_text) {
            descriptions[localize(a.first)] = a.first;
        }

        // populate the listview

        int index = 0;
        for(auto const &a : descriptions) {

            LVITEMA item;
            mem_clear(&item);
            item.mask = LVIF_TEXT;

            // there should be a string corresponding to the command id
            std::string action_text = a.first;

            std::string key_text;
            if(SUCCEEDED(hotkeys::get_hotkey_text(a.second, key_text))) {

                item.iItem = index;
                item.iSubItem = 0;
                item.pszText = action_text.data();
                ListView_InsertItem(listview, &item);

                item.mask = LVIF_TEXT;
                item.iSubItem = 1;
                item.pszText = key_text.data();
                ListView_SetItem(listview, &item);

                index += 1;
            }
        }

        return false;
    }

    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page \ WM_SHOWWINDOW

    void on_showwindow_hotkeys(HWND hwnd, BOOL fShow, UINT status)
    {
        // if being hidden, deselect listview item and hide change button

        if(!fShow) {
            HWND listview = GetDlgItem(hwnd, IDC_LIST_HOTKEYS);
            int selected_item_index = ListView_GetSelectionMark(listview);
            int clear_state = 0;
            ListView_SetItemState(listview, selected_item_index, clear_state, LVIS_SELECTED | LVIS_FOCUSED);
        }
    }

    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page \ WM_NOTIFY

    int on_notify_hotkeys(HWND hwnd, int idFrom, LPNMHDR nmhdr)
    {
        switch(nmhdr->idFrom) {

        case IDC_LIST_HOTKEYS: {

            switch(nmhdr->code) {

            case LVN_ITEMCHANGED: {

                LPNMLISTVIEW nm = reinterpret_cast<LPNMLISTVIEW>(nmhdr);
                if((nm->uNewState & LVIS_FOCUSED) != 0) {
                    // popupmenu: Edit\Reset
                }

            } break;
            }

        } break;
        }

        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page \ DLGPROC

    INT_PTR hotkeys_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_INITDIALOG, on_initdialog_hotkeys);
            HANDLE_MSG(dlg, WM_SHOWWINDOW, on_showwindow_hotkeys);
            HANDLE_MSG(dlg, WM_NOTIFY, on_notify_hotkeys);
        }
        return setting_ctlcolor_base(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // MAIN dialog
    //////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    // MAIN dialog \ WM_INITDIALOG

    BOOL on_initdialog_main(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        main_dialog = hwnd;
        settings_should_update = false;

        // setup the tab pages

        uint requested_tab_resource_id = static_cast<uint>(lParam);
        int active_tab_index{ 0 };

        HWND tab_ctrl = GetDlgItem(hwnd, IDC_SETTINGS_TAB_CONTROL);

        active_tabs.clear();

        int index = 0;
        for(int i = 0; i < _countof(all_tabs); ++i) {

            tab_page_t *tab = all_tabs + i;

            if(!should_hide_tab(tab)) {

                std::string tab_text = localize((uint64)tab->resource_id);
                TCITEMA tci;
                tci.mask = TCIF_TEXT;
                tci.pszText = const_cast<char *>(tab_text.c_str());
                TabCtrl_InsertItem(tab_ctrl, index, &tci);
                tab->index = index;
                index += 1;
                active_tabs.push_back(tab);
            }
        }

        // now the tabs are added, get the inner size of the tab control for adding dialogs for the pages
        RECT tab_rect;
        GetWindowRect(tab_ctrl, &tab_rect);
        MapWindowPoints(null, hwnd, reinterpret_cast<LPPOINT>(&tab_rect), 2);
        TabCtrl_AdjustRect(tab_ctrl, false, &tab_rect);

        // create the dialog pages

        for(auto t : active_tabs) {

            if(!should_hide_tab(t)) {

                if(t->resource_id == requested_tab_resource_id) {
                    active_tab_index = t->index;
                }

                CHK_NULL(t->hwnd = CreateDialogA(app::instance, MAKEINTRESOURCE(t->resource_id), hwnd, t->dlg_proc));

                SetWindowPos(
                    t->hwnd, HWND_TOP, tab_rect.left, tab_rect.top, rect_width(tab_rect), rect_height(tab_rect), 0);
            }
        }

        TabCtrl_SetCurSel(tab_ctrl, active_tab_index);

        // center dialog in main window rect
        HWND app_win = app_window;
        if(app_win == NULL) {
            app_win = GetDesktopWindow();
        }

        RECT parent_rect;
        GetWindowRect(app_win, &parent_rect);

        RECT dlg_rect;
        GetWindowRect(hwnd, &dlg_rect);

        int x = parent_rect.left + ((rect_width(parent_rect) - rect_width(dlg_rect)) / 2);
        int y = parent_rect.top + ((rect_height(parent_rect) - rect_height(dlg_rect)) / 2);

        SetWindowPos(hwnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);

        // show whichever page is active
        show_current_page(SW_SHOW);

        // uncork the settings notifier for main window
        settings_should_update = true;

        return false;
    }

    //////////////////////////////////////////////////////////////////////
    // MAIN dialog \ WM_NOTIFY

    int on_notify_main(HWND hwnd, int idFrom, LPNMHDR nmhdr)
    {
        switch(idFrom) {

            // clicked the little arrow on the split button

        case IDC_SPLIT_BUTTON_SETTINGS: {

            if(nmhdr->code == BCN_DROPDOWN) {

                RECT rc;
                GetWindowRect(GetDlgItem(hwnd, IDC_SPLIT_BUTTON_SETTINGS), &rc);
                HMENU menu = LoadMenuA(app::instance, MAKEINTRESOURCEA(IDR_MENU_POPUP_SETTINGS_SPLIT_BUTTON));
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
                    dialog_settings = default_settings;
                    update_all_settings_controls();
                    post_new_settings();
                    break;

                case ID_POPUP_SETTINGS_SAVE:
                    dialog_settings.save();
                    break;

                case ID_POPUP_SETTINGS_LOAD_SAVED:
                    dialog_settings.load();
                    update_all_settings_controls();
                    post_new_settings();
                    break;
                }
            }
        } break;

            // chose a tab

        case IDC_SETTINGS_TAB_CONTROL:

            switch(nmhdr->code) {
            case TCN_SELCHANGE: {
                show_current_page(SW_SHOW);
            } break;

            case TCN_SELCHANGING: {
                show_current_page(SW_HIDE);
            } break;
            }
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // MAIN dialog \ WM_COMMAND

    void on_command_main(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

            // clicked the split button

        case IDC_SPLIT_BUTTON_SETTINGS:
            dialog_settings = revert_settings;
            update_all_settings_controls();
            post_new_settings();
            break;

            // close button clicked (IDCLOSE) or window closed (IDCANCEL from DefDlgProc)

        case IDCANCEL:
        case IDCLOSE:
            main_dialog = null;
            EndDialog(hwnd, 0);
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // MAIN dialog \ DLGPROC

    INT_PTR main_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_INITDIALOG, on_initdialog_main);
            HANDLE_MSG(dlg, WM_NOTIFY, on_notify_main);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_main);

            // new settings from the app (from a hotkey or popup menu)

        case app::WM_NEW_SETTINGS: {
            settings_t *new_settings = reinterpret_cast<settings_t *>(lParam);
            memcpy(&dialog_settings, new_settings, sizeof(settings_t));
            delete new_settings;
            update_all_settings_controls();
        } break;
        }
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////

namespace imageview::settings_dialog
{
    //////////////////////////////////////////////////////////////////////
    // set name text and update controls for a setting_controller

    void setting_controller::setup_controls(HWND hwnd)
    {
        window = hwnd;
        SetWindowTextW(GetDlgItem(hwnd, IDC_STATIC_SETTING_NAME), unicode(localize(string_resource_id)).c_str());
        update_controls();
    }

    //////////////////////////////////////////////////////////////////////
    // common initdialog base for setting_controllers

    BOOL on_initdialog_setting(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, lParam);
        setting_controller *s = reinterpret_cast<setting_controller *>(lParam);
        s->setup_controls(hwnd);
        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // base dialog proc for setting_controllers

    INT_PTR setting_base_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_INITDIALOG, on_initdialog_setting);
        }
        return setting_ctlcolor_base(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // tab pages have white backgrounds - this sets the background of all controls to white

    HBRUSH on_ctl_color_base(HWND hwnd, HDC hdc, HWND hwndChild, int type)
    {
        return GetSysColorBrush(COLOR_WINDOW);
    }

    //////////////////////////////////////////////////////////////////////
    // default ctl color for most controls

    INT_PTR setting_ctlcolor_base(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_CTLCOLORDLG, on_ctl_color_base);
            HANDLE_MSG(dlg, WM_CTLCOLORSTATIC, on_ctl_color_base);
            HANDLE_MSG(dlg, WM_CTLCOLORBTN, on_ctl_color_base);
            HANDLE_MSG(dlg, WM_CTLCOLOREDIT, on_ctl_color_base);
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // send new settings to the main window from the settings_dialog

    void post_new_settings()
    {
        if(settings_should_update) {

            // main window is responsible for freeing this copy of the settings
            settings_t *settings_copy = new settings_t();
            memcpy(settings_copy, &dialog_settings, sizeof(settings_t));
            PostMessage(app_window, app::WM_NEW_SETTINGS, 0, reinterpret_cast<LPARAM>(settings_copy));
        }
    }

    //////////////////////////////////////////////////////////////////////
    // show the settings dialog and activate a tab

    HRESULT show_settings_dialog(HWND app_hwnd, uint tab_id)
    {
        if(main_dialog == null) {
            app_window = app_hwnd;
            CHK_NULL(
                CreateDialogParamA(app::instance, MAKEINTRESOURCEA(IDD_DIALOG_SETTINGS), null, main_dlgproc, tab_id));
        }
        ShowWindow(main_dialog, SW_SHOW);
        BringWindowToTop(main_dialog);
        SwitchToThisWindow(main_dialog, true);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // call this from the app whenever settings are changed

    void update_settings_dialog()
    {
        if(main_dialog != null) {
            settings_t *settings_copy = new settings_t();
            memcpy(settings_copy, &settings, sizeof(settings_t));
            PostMessage(main_dialog, app::WM_NEW_SETTINGS, 0, reinterpret_cast<LPARAM>(settings_copy));
        }
    }
}
