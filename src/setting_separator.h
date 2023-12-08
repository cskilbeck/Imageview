#pragma once

//////////////////////////////////////////////////////////////////////
// a separator 'setting' is just a label

namespace imageview::settings_dialog
{
    INT_PTR setting_separator_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

    struct separator_setting : setting_controller
    {
        separator_setting(uint s)
            : setting_controller("separator", s, IDD_DIALOG_SETTING_SEPARATOR, setting_separator_dlgproc)
        {
        }

        void update_controls() override
        {
        }
    };
}