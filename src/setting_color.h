#pragma once

//////////////////////////////////////////////////////////////////////
// color setting is a button and edit control for the hex

namespace imageview::settings_ui
{
    INT_PTR setting_color_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

    struct color_setting : setting_controller
    {
        color_setting(wchar const *n, uint s, uint32 &b, bool alpha_support)
            : setting_controller(n, s, IDD_DIALOG_SETTING_COLOR, setting_color_dlgproc), value(b), alpha(alpha_support)
        {
        }

        void setup_controls(HWND hwnd) override;
        void update_controls() override;

        uint32 &value;
        bool alpha;
    };
}
