//////////////////////////////////////////////////////////////////////
// All the tabbed pages in the setttings dialog

#include "pch.h"

namespace
{
    using namespace imageview;
    using namespace imageview::settings_ui;

    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page \ WM_INITDIALOG

    BOOL on_initdialog_hotkeys(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        // populate the listview with all the hotkeys

        HWND listview = GetDlgItem(hwnd, IDC_LIST_HOTKEYS);

        RECT listview_rect;
        GetWindowRect(hwnd, &listview_rect);

        int inner_width = rect_width(listview_rect) - GetSystemMetrics(SM_CXVSCROLL);

        LVCOLUMNW column;
        mem_clear(&column);
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.fmt = LVCFMT_LEFT;
        column.cx = inner_width * 70 / 100;
        column.pszText = const_cast<LPWSTR>(L"Action");
        ListView_InsertColumn(listview, 0, &column);
        column.cx = inner_width * 30 / 100;
        column.pszText = const_cast<LPWSTR>(L"Hotkey");
        ListView_InsertColumn(listview, 1, &column);
        ListView_SetExtendedListViewStyle(listview, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_FLATSB);
        ListView_SetView(listview, LV_VIEW_DETAILS);

        // get all the hotkey descriptions in order

        std::map<std::wstring, uint> descriptions;

        for(auto const &a : hotkeys::hotkey_text) {
            descriptions[localize(a.first)] = a.first;
        }

        // populate the listview

        int index = 0;
        for(auto const &a : descriptions) {

            LVITEMW item;
            mem_clear(&item);
            item.mask = LVIF_TEXT;

            // there should be a string corresponding to the command id
            std::wstring action_text = a.first;

            std::wstring key_text;
            if(SUCCEEDED(hotkeys::get_hotkey_text(a.second, key_text))) {

                item.iItem = index;
                item.iSubItem = 0;
                item.pszText = action_text.data();
                ListView_InsertItem(listview, &item);

                item.mask = LVIF_TEXT;
                item.iSubItem = 1;
                item.pszText = key_text.data();
                ListView_SetItem(listview, &item);

                index += 1;
            }
        }

        return false;
    }

    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page \ WM_SHOWWINDOW

    void on_showwindow_hotkeys(HWND hwnd, BOOL fShow, UINT status)
    {
        // if being hidden, deselect listview item and hide change button

        if(!fShow) {
            HWND listview = GetDlgItem(hwnd, IDC_LIST_HOTKEYS);
            int selected_item_index = ListView_GetSelectionMark(listview);
            int clear_state = 0;
            ListView_SetItemState(listview, selected_item_index, clear_state, LVIS_SELECTED | LVIS_FOCUSED);
        }
    }

    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page \ WM_NOTIFY

    int on_notify_hotkeys(HWND hwnd, int idFrom, LPNMHDR nmhdr)
    {
        switch(nmhdr->idFrom) {

        case IDC_LIST_HOTKEYS: {

            switch(nmhdr->code) {

            case LVN_ITEMCHANGED: {

                LPNMLISTVIEW nm = reinterpret_cast<LPNMLISTVIEW>(nmhdr);
                if((nm->uNewState & LVIS_FOCUSED) != 0) {
                    // popupmenu: Edit\Reset
                }

            } break;
            }

        } break;
        }

        return 0;
    }
}

namespace imageview::settings_ui
{
    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page \ DLGPROC

    INT_PTR hotkeys_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_INITDIALOG, on_initdialog_hotkeys);
            HANDLE_MSG(dlg, WM_SHOWWINDOW, on_showwindow_hotkeys);
            HANDLE_MSG(dlg, WM_NOTIFY, on_notify_hotkeys);
        }
        return ctlcolor_base(dlg, msg, wParam, lParam);
    }
}