#pragma once

//////////////////////////////////////////////////////////////////////
// bool setting is a checkbox

namespace imageview::settings_ui
{
    INT_PTR setting_bool_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

    struct bool_setting : setting_controller
    {
        bool_setting(wchar const *n, uint s, bool &b)
            : setting_controller(n, s, IDD_DIALOG_SETTING_BOOL, setting_bool_dlgproc), value(b)
        {
        }

        void update_controls() override;

        bool &value;
    };
}