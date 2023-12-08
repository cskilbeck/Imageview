#pragma once

//////////////////////////////////////////////////////////////////////
// ranged setting is... more complicated

namespace imageview::settings_dialog
{
    INT_PTR setting_ranged_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

    struct ranged_setting : setting_controller
    {
        ranged_setting(char const *n, uint s, uint dlg_id, DLGPROC dlg_proc, uint &b, uint minval, uint maxval)
            : setting_controller(n, s, dlg_id, dlg_proc), value(b), min_value(minval), max_value(maxval)
        {
        }

        void setup_controls(HWND hwnd) override;
        void update_controls() override;

        uint &value;
        uint min_value;
        uint max_value;
    };
}
