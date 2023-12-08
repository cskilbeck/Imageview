#pragma once

//////////////////////////////////////////////////////////////////////
// color setting is a button and edit control for the hex

namespace imageview::settings_dialog
{
    INT_PTR setting_color_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

    struct color_setting : setting_controller
    {
        color_setting(char const *n, uint s, uint dlg_id, DLGPROC dlg_proc, vec4 &b)
            : setting_controller(n, s, dlg_id, dlg_proc), value(b)
        {
        }

        void update_controls() override;

        vec4 &value;
    };
}
