#pragma once

//////////////////////////////////////////////////////////////////////
// ranged setting is... more complicated

namespace imageview::settings_ui
{
    INT_PTR setting_ranged_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

    struct ranged_setting : setting_controller
    {
        ranged_setting(wchar const *n, uint s, uint &b, uint minval, uint maxval)
            : setting_controller(n, s, IDD_DIALOG_SETTING_RANGED, setting_ranged_dlgproc)
            , value(b)
            , min_value(minval)
            , max_value(maxval)
        {
        }

        void setup_controls(HWND hwnd) override;
        void update_controls() override;

        uint &value;
        uint min_value;
        uint max_value;
    };
}
