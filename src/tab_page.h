#pragma once

namespace imageview::settings_ui
{
    //////////////////////////////////////////////////////////////////////

    enum tab_flags_t : uint
    {
        dont_care = 0,
        hide_if_elevated = (1 << 0),
        hide_if_not_elevated = (1 << 1),
    };

    //////////////////////////////////////////////////////////////////////

    struct tab_page_t
    {
        // dialog resource id
        uint resource_id;

        // dialog handler function
        DLGPROC dlg_proc;

        // see tab_flags_t
        uint flags;

        // position in the tab control
        int index;

        // dialog to show when tab is selected (not a child of the tab control!)
        HWND hwnd;

        bool should_hide() const
        {
            if((flags & tab_flags_t::hide_if_elevated) != 0) {
                return app::is_elevated;
            }

            if((flags & tab_flags_t::hide_if_not_elevated) != 0) {
                return !app::is_elevated;
            }

            return false;
        }
    };
}
