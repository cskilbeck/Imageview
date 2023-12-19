#pragma once

//////////////////////////////////////////////////////////////////////
// enum setting is a combobox

namespace imageview::settings_ui
{
    using enum_id_map = std::map<uint, uint>;

    extern enum_id_map enum_mouse_buttons_map;
    extern enum_id_map enum_fullscreen_startup_map;
    extern enum_id_map enum_show_filename_map;
    extern enum_id_map enum_exif_map;
    extern enum_id_map enum_zoom_mode_map;
    extern enum_id_map enum_startup_zoom_mode_map;

    INT_PTR setting_enum_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

    struct enum_setting : setting_controller
    {
        template <typename T>
        enum_setting(wchar const *n, uint s, T &b, enum_id_map const &names)
            : setting_controller(n, s, IDD_DIALOG_SETTING_ENUM, setting_enum_dlgproc)
            , enum_names(names)
            , value(reinterpret_cast<uint &>(b))
        {
        }

        void setup_controls(HWND hwnd) override;
        void update_controls() override;

        uint &value;
        std::map<uint, uint> const &enum_names;    // map<enum_value, string_id>
    };
}
