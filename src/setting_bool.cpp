//////////////////////////////////////////////////////////////////////
// A bool controller which has a checkbox

#include "pch.h"

namespace
{
    using namespace imageview::settings_ui;

    //////////////////////////////////////////////////////////////////////
    // BOOL setting \ WM_COMMAND

    void on_command_setting_bool(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

        case IDC_CHECK_SETTING_BOOL: {

            bool_setting &setting = get_controller<bool_setting>(hwnd);
            HWND checkbox = GetDlgItem(hwnd, IDC_CHECK_SETTING_BOOL);
            setting.value = Button_GetCheck(checkbox) == BST_CHECKED;
            post_new_settings();
        } break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // BOOL setting \ WM_LBUTTONDOWN, WM_LBUTTONDBLCLK

    void on_lbuttondown_setting_bool(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
    {
        // forward all mouse clicks to the checkbox so you can click anywhere on the setting
        // but remove that ugly dotted line outline thing
        SendMessage(GetDlgItem(hwnd, IDC_CHECK_SETTING_BOOL), WM_LBUTTONDOWN, 0, 0);
    }

    //////////////////////////////////////////////////////////////////////
    // BOOL setting \ WM_LBUTTONUP

    void on_lbuttonup_setting_bool(HWND hwnd, int x, int y, UINT keyFlags)
    {
        SendMessage(GetDlgItem(hwnd, IDC_CHECK_SETTING_BOOL), WM_LBUTTONUP, 0, 0);
    }
}

namespace imageview::settings_ui
{
    //////////////////////////////////////////////////////////////////////

    void bool_setting::update_controls()
    {
        HWND checkbox = GetDlgItem(window, IDC_CHECK_SETTING_BOOL);
        Button_SetCheck(checkbox, value ? BST_CHECKED : BST_UNCHECKED);
    }

    //////////////////////////////////////////////////////////////////////
    // BOOL setting \ DLGPROC

    INT_PTR setting_bool_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_LBUTTONDOWN, on_lbuttondown_setting_bool);
            HANDLE_MSG(dlg, WM_LBUTTONDBLCLK, on_lbuttondown_setting_bool);
            HANDLE_MSG(dlg, WM_LBUTTONUP, on_lbuttonup_setting_bool);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_setting_bool);
        }
        return setting_base_dlgproc(dlg, msg, wParam, lParam);
    }
}