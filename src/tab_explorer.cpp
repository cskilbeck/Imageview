//////////////////////////////////////////////////////////////////////
// explorer page

#include "pch.h"

namespace
{
    using namespace imageview;

    //////////////////////////////////////////////////////////////////////

    void clear_message(HWND hwnd)
    {
        SetWindowTextW(GetDlgItem(hwnd, IDC_STATIC_EXPLORER_STATUS), L"");
        KillTimer(hwnd, 1);
    }

    //////////////////////////////////////////////////////////////////////

    void show_message(HWND hwnd, uint timeout, std::wstring const &msg)
    {
        SetWindowTextW(GetDlgItem(hwnd, IDC_STATIC_EXPLORER_STATUS), msg.c_str());
        if(timeout != 0) {
            SetTimer(hwnd, 1, timeout, null);
        }
    }

    //////////////////////////////////////////////////////////////////////

    void update_controls(HWND dlg)
    {
        bool elevated = false;
        get_is_process_elevated(elevated);

        uint show_elevate_button = SW_HIDE;
        bool enable_add_remove = true;

        // this will fail if we're trying to check all users and we're not elevated, but that's ok
        bool is_installed = true;
        check_filetype_handler(is_installed);

        if(!elevated && ComboBox_GetCurSel(GetDlgItem(dlg, IDC_COMBO_EXPLORER_ALL_USERS)) == 1) {

            show_elevate_button = SW_SHOW;
            enable_add_remove = false;
        }

        EnableWindow(GetDlgItem(dlg, IDC_BUTTON_EXPLORER_ADD_FILE_TYPES), !is_installed && enable_add_remove);
        EnableWindow(GetDlgItem(dlg, IDC_BUTTON_EXPLORER_REMOVE_FILE_TYPES), is_installed && enable_add_remove);
        EnableWindow(GetDlgItem(dlg, IDC_BUTTON_EXPLORER_PURGE_REGISTRY), enable_add_remove);

        ShowWindow(GetDlgItem(dlg, IDC_STATIC_EXPLORER_REQUIRES_ADMIN), show_elevate_button);
        ShowWindow(GetDlgItem(dlg, IDC_BUTTON_EXPLORER_RESTART_AS_ADMIN), show_elevate_button);
    }

    //////////////////////////////////////////////////////////////////////

    BOOL on_initdialog_explorer(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        HWND all_users_combo = GetDlgItem(hwnd, IDC_COMBO_EXPLORER_ALL_USERS);
        ComboBox_AddString(all_users_combo, localize(IDS_CURRENT_USER).c_str());    // Current User is index 0
        ComboBox_AddString(all_users_combo, localize(IDS_ALL_USERS).c_str());       // All Users is index 1
        ComboBox_SetCurSel(all_users_combo, 0);
        update_controls(hwnd);
        return false;
    }

    //////////////////////////////////////////////////////////////////////
    // EXPLORER page \ WM_COMMAND

    void on_command_explorer(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

        case IDC_COMBO_EXPLORER_ALL_USERS: {
            update_controls(hwnd);
        } break;

        case IDC_BUTTON_EXPLORER_ADD_FILE_TYPES: {
            install_filetype_handler();
            update_controls(hwnd);
            show_message(hwnd, 2000, localize(IDS_ADDED_FILE_TYPES));
        } break;

        case IDC_BUTTON_EXPLORER_REMOVE_FILE_TYPES: {
            remove_filetype_handler();
            update_controls(hwnd);
            show_message(hwnd, 2000, localize(IDS_REMOVED_FILE_TYPES));
        } break;

        case IDC_BUTTON_EXPLORER_RESTART_AS_ADMIN: {
            PostMessageW(app::window, app::WM_RELAUNCH_AS_ADMIN, 0, 0);
            DestroyWindow(hwnd);
        } break;

        case IDC_BUTTON_EXPLORER_PURGE_REGISTRY: {
            remove_filetype_handler();
            delete_settings_from_registry();
            update_controls(hwnd);
            show_message(hwnd, 2000, localize(IDS_PURGED_EVERYTHING));
        } break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // EXPLORER page \ WM_TIMER

    void on_timer_explorer(HWND hwnd, UINT id)
    {
        clear_message(hwnd);
    }
}

namespace imageview::settings_ui
{
    //////////////////////////////////////////////////////////////////////
    // EXPLORER page \ DLGPROC

    INT_PTR explorer_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {
            HANDLE_MSG(dlg, WM_INITDIALOG, on_initdialog_explorer);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_explorer);
            HANDLE_MSG(dlg, WM_TIMER, on_timer_explorer);
        }
        return ctlcolor_base(dlg, msg, wParam, lParam);
    }
}
