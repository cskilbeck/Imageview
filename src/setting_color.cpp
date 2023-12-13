//////////////////////////////////////////////////////////////////////
// A color controller which has a colored button and an edit box (for the hex)

#include "pch.h"

LOG_CONTEXT("COLOR");

namespace
{
    using namespace imageview;
    using namespace imageview::settings_ui;

    //////////////////////////////////////////////////////////////////////
    // COLOR setting \ WM_DRAWITEM

    void on_drawitem_setting_color(HWND hwnd, const DRAWITEMSTRUCT *lpDrawItem)
    {
        // make the color button the right color

        if(lpDrawItem->CtlID == IDC_BUTTON_SETTING_COLOR) {

            color_setting &setting = setting_controller::get<color_setting>(hwnd);

            SetDCBrushColor(lpDrawItem->hDC, setting.value & 0xffffff);

            SelectObject(lpDrawItem->hDC, GetStockObject(DC_BRUSH));

            Rectangle(lpDrawItem->hDC,
                      lpDrawItem->rcItem.left,
                      lpDrawItem->rcItem.top,
                      lpDrawItem->rcItem.right,
                      lpDrawItem->rcItem.bottom);
        }
    }

    //////////////////////////////////////////////////////////////////////
    // COLOR setting \ WM_COMMAND

    void on_command_setting_color(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        color_setting &setting = setting_controller::get<color_setting>(hwnd);

        switch(id) {

            // clicked the color button

        case IDC_BUTTON_SETTINGS_BACKGROUND_COLOR: {

            std::string title = localize(setting.string_resource_id);
            if(dialog::select_color(GetParent(hwnd), setting.value, title.c_str()) == S_OK) {
                setting.update_controls();
            }
        } break;

            // edited the hex text (or new text from clicking the color button)

        case IDC_EDIT_SETTING_COLOR: {

            switch(codeNotify) {

            case EN_CHANGE: {

                HWND edit_control = GetDlgItem(hwnd, IDC_EDIT_SETTING_COLOR);
                int len = GetWindowTextLengthA(edit_control);
                if(len > 0) {
                    std::string txt;
                    txt.resize(len + 1llu);
                    GetWindowTextA(edit_control, txt.data(), len + 1);
                    txt.pop_back();
                    uint32 new_color{};
                    if(SUCCEEDED(color_from_string(txt, new_color))) {
                        setting.value = new_color;
                        InvalidateRect(hwnd, null, true);
                        post_new_settings();
                    }
                }
            } break;
            }
        } break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // COLOR setting \ WM_HSCROLL

    void on_hscroll_setting_color(HWND hwnd, HWND hwndCtl, UINT code, int pos)
    {
        color_setting &setting = setting_controller::get<color_setting>(hwnd);
        if(setting.alpha) {
            HWND slider = GetDlgItem(hwnd, IDC_SLIDER_SETTING_COLOR);
            uint new_alpha = static_cast<uint>(SendMessage(slider, TBM_GETPOS, 0, 0));
            setting.value = (setting.value & 0xffffff) | (new_alpha << 24);
            setting.update_controls();
        }
    }
}

namespace imageview::settings_ui
{
    //////////////////////////////////////////////////////////////////////

    void color_setting::setup_controls(HWND hwnd)
    {
        HWND slider = GetDlgItem(hwnd, IDC_SLIDER_SETTING_COLOR);
        if(!alpha) {
            ShowWindow(slider, SW_HIDE);
        } else {
            SendMessage(slider, TBM_SETRANGEMAX, false, 255);
            SendMessage(slider, TBM_SETRANGEMIN, false, 0);

            uint cur_alpha = value >> 24;

            uint current = static_cast<uint>(SendMessage(slider, TBM_GETPOS, 0, 0));

            if(current != cur_alpha) {
                SendMessage(slider, TBM_SETPOS, true, cur_alpha);
            }
        }

        setting_controller::setup_controls(hwnd);
    }

    //////////////////////////////////////////////////////////////////////

    void color_setting::update_controls()
    {
        std::string hex;
        if(alpha) {
            hex = color32_to_string(value);
        } else {
            hex = color24_to_string(value);
        }
        hex = make_uppercase(hex);
        SetWindowTextA(GetDlgItem(window, IDC_EDIT_SETTING_COLOR), hex.c_str());
    }

    //////////////////////////////////////////////////////////////////////
    // COLOR setting \ DLGPROC

    INT_PTR setting_color_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_DRAWITEM, on_drawitem_setting_color);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_setting_color);
            HANDLE_MSG(dlg, WM_HSCROLL, on_hscroll_setting_color);
        }
        return setting_base_dlgproc(dlg, msg, wParam, lParam);
    }
}