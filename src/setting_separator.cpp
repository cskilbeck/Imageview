//////////////////////////////////////////////////////////////////////
// A separator controller which does nothing

#include "pch.h"

namespace imageview::settings_dialog
{
    //////////////////////////////////////////////////////////////////////

    HBRUSH on_ctl_color_separator(HWND hwnd, HDC hdc, HWND hwndChild, int type)
    {
        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
        return GetSysColorBrush(COLOR_3DFACE);
    }

    //////////////////////////////////////////////////////////////////////

    INT_PTR setting_separator_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_CTLCOLORSTATIC, on_ctl_color_separator);
            HANDLE_MSG(dlg, WM_INITDIALOG, on_initdialog_setting);
        }
        return setting_base_dlgproc(dlg, msg, wParam, lParam);
    }
}
