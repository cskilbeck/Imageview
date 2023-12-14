//////////////////////////////////////////////////////////////////////
// The main settings dialog

#include "pch.h"

LOG_CONTEXT("settings_dlg");

//////////////////////////////////////////////////////////////////////

namespace
{
    using namespace imageview;
    using namespace imageview::settings_ui;

    //////////////////////////////////////////////////////////////////////

    HWND settings_dlg = null;

    //////////////////////////////////////////////////////////////////////
    // which tabs will be shown (some might be hidden)

    std::vector<tab_page_t *> active_tabs;

    //////////////////////////////////////////////////////////////////////
    // all the tabs that can be created

    tab_page_t all_tabs[] = {
        { IDD_DIALOG_SETTINGS_MAIN, settings_dlgproc, tab_flags_t::dont_care, -1, null },
        { IDD_DIALOG_SETTINGS_HOTKEYS, hotkeys_dlgproc, tab_flags_t::dont_care, -1, null },
        { IDD_DIALOG_SETTINGS_EXPLORER, explorer_dlgproc, tab_flags_t::hide_if_not_elevated, -1, null },
        { IDD_DIALOG_SETTINGS_RELAUNCH, relaunch_dlgproc, tab_flags_t::hide_if_elevated, -1, null },
        { IDD_DIALOG_SETTINGS_ABOUT, about_dlgproc, tab_flags_t::dont_care, -1, null },
    };

    //////////////////////////////////////////////////////////////////////
    // make the current page shown (after it's activated) or hidden (before it's inactivated)

    HRESULT show_current_page(HWND hwnd, int show)
    {
        HWND tab_ctrl = GetDlgItem(hwnd, IDC_SETTINGS_TAB_CONTROL);
        uint tab = TabCtrl_GetCurSel(tab_ctrl);
        if(tab >= active_tabs.size()) {
            LOG_ERROR(L"!? Tab {} is out of range (there are {} tabs)", tab, active_tabs.size());
            return HRESULT_FROM_WIN32(ERROR_BAD_ARGUMENTS);
        }
        ShowWindow(active_tabs[tab]->hwnd, show);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // MAIN dialog \ WM_INITDIALOG

    BOOL on_initdialog_main(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        // setup the tab pages

        uint requested_tab_resource_id = static_cast<uint>(lParam);
        int active_tab_index{ 0 };

        HWND tab_ctrl = GetDlgItem(hwnd, IDC_SETTINGS_TAB_CONTROL);

        active_tabs.clear();

        int index = 0;
        for(int i = 0; i < _countof(all_tabs); ++i) {

            tab_page_t *tab = all_tabs + i;

            if(!tab->should_hide()) {

                std::wstring tab_text = localize((uint64)tab->resource_id);
                TCITEMW tci;
                tci.mask = TCIF_TEXT;
                tci.pszText = const_cast<wchar *>(tab_text.c_str());
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

            if(!t->should_hide()) {

                if(t->resource_id == requested_tab_resource_id) {
                    active_tab_index = t->index;
                }

                t->hwnd =
                    CreateDialogParamW((HMODULE)app::instance, MAKEINTRESOURCEW(t->resource_id), hwnd, t->dlg_proc, 0);

                if(t->hwnd == null) {
                    imageview::display_error(L"CreateDialogParamW failed!?");
                    return false;
                }

                SetWindowPos(
                    t->hwnd, HWND_TOP, tab_rect.left, tab_rect.top, rect_width(tab_rect), rect_height(tab_rect), 0);
            }
        }

        TabCtrl_SetCurSel(tab_ctrl, active_tab_index);

        // center dialog in main window rect
        HWND app_win = app::window;
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
        show_current_page(hwnd, SW_SHOW);

        return false;
    }

    //////////////////////////////////////////////////////////////////////
    // MAIN dialog \ WM_NOTIFY

    int on_notify_main(HWND hwnd, int idFrom, LPNMHDR nmhdr)
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
                    save_current_settings();
                    break;

                case ID_POPUP_SETTINGS_LOAD_SAVED:
                    load_saved_settings();
                    break;
                }
            }
        } break;

            // chose a tab

        case IDC_SETTINGS_TAB_CONTROL:

            switch(nmhdr->code) {
            case TCN_SELCHANGE: {
                show_current_page(hwnd, SW_SHOW);
            } break;

            case TCN_SELCHANGING: {
                show_current_page(hwnd, SW_HIDE);
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
            revert_settings();
            break;

            // close button clicked (IDCLOSE) or window closed (IDCANCEL from DefDlgProc)

        case IDCANCEL:
        case IDCLOSE:
            settings_dlg = null;
            post_new_settings();    // save the sections expanded states
            DestroyWindow(hwnd);
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
            on_new_settings(new_settings);
            delete new_settings;
        } break;
        }
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////

namespace imageview::settings_ui
{
    //////////////////////////////////////////////////////////////////////
    // show the settings dialog and activate a tab

    HRESULT show_settings_dialog(HWND app_hwnd, uint tab_id)
    {
        if(settings_dlg == null) {
            CHK_NULL(settings_dlg = CreateDialogParamW(
                         app::instance, MAKEINTRESOURCEW(IDD_DIALOG_SETTINGS), null, main_dlgproc, tab_id));
        }
        ShowWindow(settings_dlg, SW_SHOW);
        BringWindowToTop(settings_dlg);
        SwitchToThisWindow(settings_dlg, true);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // call this from the app whenever settings are changed

    void update_settings_dialog()
    {
        if(settings_dlg != null) {
            settings_t *settings_copy = new settings_t();
            memcpy(settings_copy, &settings, sizeof(settings_t));
            PostMessage(settings_dlg, app::WM_NEW_SETTINGS, 0, reinterpret_cast<LPARAM>(settings_copy));
        }
    }
}
