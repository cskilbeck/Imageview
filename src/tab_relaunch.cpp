#include "pch.h"

namespace
{
    using namespace imageview;

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
}

namespace imageview::settings_dialog
{
    //////////////////////////////////////////////////////////////////////
    // RELAUNCH (explorer) page \ DLGPROC

    INT_PTR relaunch_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_COMMAND, on_command_relaunch);
        }
        return ctlcolor_base(dlg, msg, wParam, lParam);
    }
}
