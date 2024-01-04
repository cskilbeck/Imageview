#pragma once

namespace imageview::settings_ui
{
    //////////////////////////////////////////////////////////////////////

    struct tab_page_t
    {
        // dialog resource id
        uint resource_id;

        // dialog handler function
        DLGPROC dlg_proc;

        // position in the tab control
        int index{ -1 };

        // dialog to show when tab is selected (not a child of the tab control!)
        HWND hwnd{ null };
    };
}
