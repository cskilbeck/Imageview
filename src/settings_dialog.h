#pragma once

namespace imageview::settings_dialog
{
    void post_new_settings();

    HRESULT show_settings_dialog(HWND app_hwnd, uint tab_id);
    void update_settings_dialog();
}
