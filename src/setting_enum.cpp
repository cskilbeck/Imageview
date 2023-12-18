//////////////////////////////////////////////////////////////////////
// An enum controller which has a combo box

#include "pch.h"

namespace
{
    using namespace imageview::settings_ui;

    //////////////////////////////////////////////////////////////////////
    // ENUM setting \ WM_COMMAND

    void on_command_setting_enum(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

        case IDC_COMBO_SETTING_ENUM: {

            HWND combo_box = GetDlgItem(hwnd, IDC_COMBO_SETTING_ENUM);
            int sel = ComboBox_GetCurSel(combo_box);
            uint v = static_cast<uint>(ComboBox_GetItemData(combo_box, sel));
            get_controller<enum_setting>(hwnd).value = v;
            post_new_settings();
        } break;
        }
    }
}

namespace imageview::settings_ui
{
    // big sigh - these are for mapping enums to strings for the combo boxes
    // need to be visible to settings_page

    // fullscreen_startup_option

    enum_id_map enum_fullscreen_startup_map = {
        { fullscreen_startup_option::start_fullscreen, IDS_ENUM_FULLSCREEN_STARTUP_FULLSCREEN },
        { fullscreen_startup_option::start_windowed, IDS_ENUM_FULLSCREEN_STARTUP_WINDOWED },
        { fullscreen_startup_option::start_remember, IDS_ENUM_FULLSCREEN_STARTUP_REMEMBER },
    };

    // mouse_button

    enum_id_map enum_mouse_buttons_map = {
        { mouse_button_t::btn_left, IDS_ENUM_MOUSE_BUTTON_LEFT },
        { mouse_button_t::btn_right, IDS_ENUM_MOUSE_BUTTON_RIGHT },
        { mouse_button_t::btn_middle, IDS_ENUM_MOUSE_BUTTON_MIDDLE },
    };

    // show_filename_option

    enum_id_map enum_show_filename_map = {
        { show_filename_option::show_filename_always, IDS_ENUM_SHOW_FILENAME_ALWAYS },
        { show_filename_option::show_filename_briefly, IDS_ENUM_SHOW_FILENAME_BRIEFLY },
        { show_filename_option::show_filename_never, IDS_ENUM_SHOW_FILENAME_NEVER },
    };

    // exif_option

    enum_id_map enum_exif_map = {
        { exif_option::exif_option_apply, IDS_ENUM_EXIF_APPLY },
        { exif_option::exif_option_ignore, IDS_ENUM_EXIF_IGNORE },
        { exif_option::exif_option_prompt, IDS_ENUM_EXIF_PROMPT },
    };

    // zoom_mode

    enum_id_map enum_zoom_mode_map = {
        { zoom_mode_t::shrink_to_fit, IDS_ENUM_ZOOM_MODE_SHRINK_TO_FIT },
        { zoom_mode_t::fit_to_window, IDS_ENUM_ZOOM_MODE_FIT_TO_WINDOW },
        { zoom_mode_t::one_to_one, IDS_ENUM_ZOOM_MODE_ONE_TO_ONE },
    };

    // startup_zoom_mode

    enum_id_map enum_startup_zoom_mode_map = {
        { startup_zoom_mode_option::startup_zoom_shrink_to_fit, IDS_ENUM_ZOOM_MODE_SHRINK_TO_FIT },
        { startup_zoom_mode_option::startup_zoom_fit_to_window, IDS_ENUM_ZOOM_MODE_FIT_TO_WINDOW },
        { startup_zoom_mode_option::startup_zoom_one_to_one, IDS_ENUM_ZOOM_MODE_ONE_TO_ONE },
        { startup_zoom_mode_option::startup_zoom_remember, IDS_ENUM_ZOOM_MODE_REMEMBER },
    };

    //////////////////////////////////////////////////////////////////////

    void enum_setting::setup_controls(HWND hwnd)
    {
        HWND combo_box = GetDlgItem(hwnd, IDC_COMBO_SETTING_ENUM);
        int index = 0;
        for(auto const &name : enum_names) {
            ComboBox_AddString(combo_box, localize(name.second).c_str());
            ComboBox_SetItemData(combo_box, index, name.first);
            index += 1;
        }
        setting_controller::setup_controls(hwnd);
    }

    //////////////////////////////////////////////////////////////////////

    void enum_setting::update_controls()
    {
        int index = 0;
        for(auto const &name : enum_names) {
            if(name.first == value) {
                HWND combo_box = GetDlgItem(window, IDC_COMBO_SETTING_ENUM);
                ComboBox_SetCurSel(combo_box, index);
                break;
            }
            index += 1;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // ENUM setting \ DLGPROC

    INT_PTR setting_enum_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_COMMAND, on_command_setting_enum);
        }
        return setting_base_dlgproc(dlg, msg, wParam, lParam);
    }
}
