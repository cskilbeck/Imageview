#pragma once

//////////////////////////////////////////////////////////////////////
// bool setting is a checkbox

namespace imageview::settings_dialog
{
    INT_PTR setting_bool_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

    struct bool_setting : setting_controller
    {
        bool_setting(char const *n, uint s, uint dlg_id, DLGPROC dlg_proc, bool &b)
            : setting_controller(n, s, dlg_id, dlg_proc), value(b)
        {
        }

        void update_controls() override;

        bool &value;
    };
}