#include "pch.h"

#pragma warning(disable : 4100)

namespace imageview::settings_dialog
{
    //////////////////////////////////////////////////////////////////////

    void color_setting::update_controls()
    {
        uint32 color = color_swap_red_blue(color_to_uint32(value));
        std::string hex = std::format("{:06x}", color & 0xffffff);
        make_uppercase(hex);
        SetWindowTextA(GetDlgItem(window, IDC_EDIT_SETTING_COLOR), hex.c_str());
    }

    //////////////////////////////////////////////////////////////////////
    // COLOR setting \ WM_DRAWITEM

    void on_drawitem_setting_color(HWND hwnd, const DRAWITEMSTRUCT *lpDrawItem)
    {
        // make the color button the right color

        if(lpDrawItem->CtlID == IDC_BUTTON_SETTING_COLOR) {

            color_setting &setting = setting_controller::get<color_setting>(hwnd);

            uint color = color_to_uint32(setting.value) & 0xffffff;

            SetDCBrushColor(lpDrawItem->hDC, color);

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
            uint new_color = color_to_uint32(setting.value);
            if(dialog::select_color(GetParent(hwnd), new_color, title.c_str()) == S_OK) {
                setting.value = color_from_uint32(new_color);
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
                        setting.value = color_from_uint32(new_color);
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
    // COLOR setting \ DLGPROC

    INT_PTR setting_color_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_DRAWITEM, on_drawitem_setting_color);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_setting_color);
        }
        return setting_base_dlgproc(dlg, msg, wParam, lParam);
    }
}