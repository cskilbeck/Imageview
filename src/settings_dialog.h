#pragma once

namespace imageview::settings_ui
{
    HRESULT show_settings_dialog(HWND app_hwnd, uint tab_id);

    void new_settings_update();

    void update();
}
