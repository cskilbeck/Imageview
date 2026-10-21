//////////////////////////////////////////////////////////////////////
// A ranged controller which has a slider and a textbox

#include "pch.h"

namespace
{
    using namespace imageview;
    using namespace imageview::settings_ui;

    //////////////////////////////////////////////////////////////////////
    // RANGED setting \ WM_COMMAND

    void on_command_setting_ranged(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        if(id == IDC_EDIT_SETTING_RANGED) {

            ranged_setting &ranged = get_controller<ranged_setting>(hwnd);
            HWND edit = GetDlgItem(hwnd, IDC_EDIT_SETTING_RANGED);
            HWND slider = GetDlgItem(hwnd, IDC_SLIDER_SETTING_RANGED);

            switch(codeNotify) {

            case EN_CHANGE: {
                std::wstring txt;
                if(SUCCEEDED(get_window_text(edit, txt)) && !txt.empty()) {
                    wchar *ep = null;
                    unsigned long parsed = std::wcstoul(txt.c_str(), &ep, 10);
                    if(ep != txt.c_str() && *ep == 0) {
                        uint clamped = std::clamp(static_cast<uint>(parsed), ranged.min_value, ranged.max_value);
                        if(static_cast<uint>(SendMessage(slider, TBM_GETPOS, 0, 0)) != clamped) {
                            SendMessage(slider, TBM_SETPOS, true, clamped);
                        }
                        if(ranged.value != clamped) {
                            ranged.value = clamped;
                            post_new_settings();
                        }
                    }
                }
            } break;

            case EN_KILLFOCUS: {
                std::wstring expected = std::format(L"{}", ranged.value);
                std::wstring current_txt;
                (void)get_window_text(edit, current_txt);
                if(current_txt != expected) {
                    Edit_SetText(edit, expected.c_str());
                }
            } break;
            }
        }
    }

    //////////////////////////////////////////////////////////////////////
    // RANGED setting \ WM_HSCROLL

    void on_hscroll_setting_ranged(HWND hwnd, HWND hwndCtl, UINT code, int pos)
    {
        HWND slider = GetDlgItem(hwnd, IDC_SLIDER_SETTING_RANGED);
        uint new_pos = static_cast<uint>(SendMessage(slider, TBM_GETPOS, 0, 0));
        ranged_setting &ranged = get_controller<ranged_setting>(hwnd);
        ranged.value = std::clamp(new_pos, ranged.min_value, ranged.max_value);
        HWND edit = GetDlgItem(hwnd, IDC_EDIT_SETTING_RANGED);
        Edit_SetText(edit, std::format(L"{}", ranged.value).c_str());
        post_new_settings();
    }
}

namespace imageview::settings_ui
{
    //////////////////////////////////////////////////////////////////////

    void ranged_setting::setup_controls(HWND hwnd)
    {
        HWND slider = GetDlgItem(hwnd, IDC_SLIDER_SETTING_RANGED);
        SendMessage(slider, TBM_SETRANGEMAX, false, max_value);
        SendMessage(slider, TBM_SETRANGEMIN, false, min_value);
        setting_controller::setup_controls(hwnd);
        SetWindowSubclass(slider, slider_subclass_handler, 0, 0);
    }

    //////////////////////////////////////////////////////////////////////

    void ranged_setting::update_controls()
    {
        HWND slider = GetDlgItem(window, IDC_SLIDER_SETTING_RANGED);
        HWND edit = GetDlgItem(window, IDC_EDIT_SETTING_RANGED);
        Edit_SetText(edit, std::format(L"{}", value).c_str());
        SendMessage(slider, TBM_SETPOS, true, value);
    }

    //////////////////////////////////////////////////////////////////////
    // RANGED setting \ DLGPROC

    INT_PTR setting_ranged_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {
            HANDLE_MSG(dlg, WM_COMMAND, on_command_setting_ranged);
            HANDLE_MSG(dlg, WM_HSCROLL, on_hscroll_setting_ranged);
        }
        return setting_base_dlgproc(dlg, msg, wParam, lParam);
    }
}
