#pragma once

//////////////////////////////////////////////////////////////////////
// a separator 'setting' is just a label

namespace imageview::settings_ui
{
    INT_PTR section_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

    struct section_setting : setting_controller
    {
        section_setting(wchar const *n, uint s, bool &v)
            : setting_controller(n, s, IDD_DIALOG_SETTING_SECTION, section_dlgproc)
            , expanded_height(0)
            , banner_height(0)
            , current_height(0)
            , target_height(0)
            , expanded(v)
        {
        }

        void setup_controls(HWND hwnd) override;

        void update_controls() override
        {
        }

        bool is_section_header() const override
        {
            return true;
        }

        int expanded_height;
        int banner_height;
        int current_height;
        int target_height;
        bool &expanded;

        static std::list<section_setting *> sections;
    };
}