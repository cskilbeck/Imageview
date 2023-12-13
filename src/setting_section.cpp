//////////////////////////////////////////////////////////////////////
// A separator controller which does nothing

#include "pch.h"

LOG_CONTEXT("SECTION");

namespace
{
    using namespace imageview::settings_ui;

    //////////////////////////////////////////////////////////////////////
    // SECTION setting \ WM_CTLCOLOR

    HBRUSH on_ctl_color_section(HWND hwnd, HDC hdc, HWND hwndChild, int type)
    {
        SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
        return GetSysColorBrush(COLOR_3DFACE);
    }

    //////////////////////////////////////////////////////////////////////
    // SECTION setting \ WM_LBUTTONDOWN/DBLCLK

    void on_lbuttondown_setting_section(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
    {
        section_setting &setting = setting_controller::get<section_setting>(hwnd);

        setting.expanded = !setting.expanded;

        if(setting.expanded) {
            setting.target_height = setting.expanded_height;
        } else {
            setting.target_height = setting.banner_height;
        }

        HWND tab = GetParent(hwnd);
        PostMessage(tab, WM_USER, setting.expanded, reinterpret_cast<LPARAM>(&setting));
    }
}

namespace imageview::settings_ui
{
    std::list<section_setting *> section_setting::sections;

    //////////////////////////////////////////////////////////////////////

    void section_setting::setup_controls(HWND hwnd)
    {
        setting_controller::setup_controls(hwnd);

        sections.push_back(this);
    }

    //////////////////////////////////////////////////////////////////////
    // SECTION setting \ DLGPROC

    INT_PTR section_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_CTLCOLORSTATIC, on_ctl_color_section);
            HANDLE_MSG(dlg, WM_INITDIALOG, on_initdialog_setting);
            HANDLE_MSG(dlg, WM_LBUTTONDOWN, on_lbuttondown_setting_section);
            HANDLE_MSG(dlg, WM_LBUTTONDBLCLK, on_lbuttondown_setting_section);
        }
        return setting_base_dlgproc(dlg, msg, wParam, lParam);
    }
}
