//////////////////////////////////////////////////////////////////////
// explorer page

#include "pch.h"

namespace imageview::settings_dialog
{
    //////////////////////////////////////////////////////////////////////
    // EXPLORER page \ DLGPROC

    INT_PTR explorer_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return ctlcolor_base(dlg, msg, wParam, lParam);
    }
}
