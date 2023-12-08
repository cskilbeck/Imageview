//////////////////////////////////////////////////////////////////////
// All the tabbed pages in the setttings dialog

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
        HWND main_dialog = GetParent(hwnd);
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

    //////////////////////////////////////////////////////////////////////
    // RELAUNCH (explorer) page
    //////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    // RELAUNCH (explorer) page \ WM_COMMAND

    void on_command_relaunch(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

        case IDC_BUTTON_SETTINGS_RELAUNCH: {

            EndDialog(hwnd, 0);
            PostMessage(app::window, app::WM_RELAUNCH_AS_ADMIN, 0, 0);
        } break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // EXPLORER page
    //////////////////////////////////////////////////////////////////////

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
}

//////////////////////////////////////////////////////////////////////

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
    // RELAUNCH (explorer) page \ DLGPROC

    INT_PTR relaunch_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_COMMAND, on_command_relaunch);
        }
        return ctlcolor_base(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // EXPLORER page \ DLGPROC

    INT_PTR explorer_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return ctlcolor_base(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // ABOUT page \ DLGPROC

    INT_PTR about_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_INITDIALOG, on_initdialog_about);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_about);
        }
        return ctlcolor_base(dlg, msg, wParam, lParam);
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
        return ctlcolor_base(dlg, msg, wParam, lParam);
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