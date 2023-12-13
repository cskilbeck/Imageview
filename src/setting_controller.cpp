//////////////////////////////////////////////////////////////////////
// Base of the setting controllers

#include "pch.h"

namespace imageview::settings_ui
{
    //////////////////////////////////////////////////////////////////////
    // erk - forward mousewheel messages for sliders to the tab page (parent of parent)

    LRESULT CALLBACK slider_subclass_handler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR)
    {
        switch(msg) {
        case WM_MOUSEWHEEL:
            // ack!
            SendMessage(GetParent(GetParent(hwnd)), msg, wparam, lparam);
            break;

        default:
            return DefSubclassProc(hwnd, msg, wparam, lparam);
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // tab pages have white backgrounds - this sets the background of all controls to white

    HBRUSH on_ctl_color_base(HWND hwnd, HDC hdc, HWND hwndChild, int type)
    {
        return GetSysColorBrush(COLOR_WINDOW);
    }

    //////////////////////////////////////////////////////////////////////
    // default ctl color for most controls

    INT_PTR ctlcolor_base(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
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
    // set name text and update controls for a setting_controller

    void setting_controller::setup_controls(HWND hwnd)
    {
        window = hwnd;
        SetWindowTextW(GetDlgItem(hwnd, IDC_STATIC_SETTING_NAME), unicode(name()).c_str());
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

    // Don't handle WM_INITDIALOG in any of the derived ones (or FORWARD_WM_INITDIALOG to here if you do)

    INT_PTR setting_base_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_INITDIALOG, on_initdialog_setting);
        }
        return ctlcolor_base(dlg, msg, wParam, lParam);
    }
}
