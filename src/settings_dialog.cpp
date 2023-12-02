//////////////////////////////////////////////////////////////////////

#include "pch.h"

LOG_CONTEXT("settings_dialog");

//////////////////////////////////////////////////////////////////////

namespace
{
    //////////////////////////////////////////////////////////////////////

    HWND main_dialog = null;

    std::vector<HWND> tab_pages;

    HWND current_page = null;

    using namespace imageview;

    //////////////////////////////////////////////////////////////////////

    enum tab_flags_t
    {
        dont_care = 0,
        hide_if_elevated = 1,
        hide_if_not_elevated = 2,
        ignore_size = 4,
    };

    struct settings_tab_t
    {
        uint resource_id;
        DLGPROC handler;
        int flags;
    };

    INT_PTR main_settings_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR hotkeys_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR explorer_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR relaunch_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR about_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);

    //////////////////////////////////////////////////////////////////////

    settings_tab_t tabs[] = {
        { IDD_DIALOG_SETTINGS_MAIN, main_settings_handler, dont_care | ignore_size },
        { IDD_DIALOG_SETTINGS_HOTKEYS, hotkeys_handler, dont_care },
        { IDD_DIALOG_SETTINGS_EXPLORER, explorer_handler, hide_if_not_elevated },
        { IDD_DIALOG_SETTINGS_RELAUNCH, relaunch_handler, hide_if_elevated },
        { IDD_DIALOG_SETTINGS_ABOUT, about_handler, dont_care },
    };

    //////////////////////////////////////////////////////////////////////

    INT_PTR main_settings_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        UNREFERENCED_PARAMETER(dlg);
        UNREFERENCED_PARAMETER(msg);
        UNREFERENCED_PARAMETER(wparam);
        UNREFERENCED_PARAMETER(lparam);
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    INT_PTR relaunch_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        UNREFERENCED_PARAMETER(dlg);
        UNREFERENCED_PARAMETER(msg);
        UNREFERENCED_PARAMETER(wparam);
        UNREFERENCED_PARAMETER(lparam);
        switch(msg) {
        case WM_COMMAND: {
            switch(LOWORD(wparam)) {
            case IDC_BUTTON_SETTINGS_RELAUNCH: {
                EndDialog(main_dialog, app::LRESULT_LAUNCH_AS_ADMIN);
            } break;
            }
        } break;
        case WM_INITDIALOG: {
        } break;
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    INT_PTR explorer_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        UNREFERENCED_PARAMETER(dlg);
        UNREFERENCED_PARAMETER(msg);
        UNREFERENCED_PARAMETER(wparam);
        UNREFERENCED_PARAMETER(lparam);
        switch(msg) {
        case WM_INITDIALOG: {
        } break;
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    LRESULT CALLBACK suppress_cursor(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR)
    {
        HideCaret(dlg);
        return DefSubclassProc(dlg, msg, wparam, lparam);
    }

    //////////////////////////////////////////////////////////////////////

    INT_PTR about_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        UNREFERENCED_PARAMETER(wparam);
        UNREFERENCED_PARAMETER(lparam);

        switch(msg) {

        case WM_INITDIALOG: {
            HWND about = GetDlgItem(dlg, IDC_SETTINGS_EDIT_ABOUT);
            SetWindowSubclass(about, suppress_cursor, 0, 0);
            SendMessage(about, EM_SETREADONLY, 1, 0);
            std::string version{ "Version?" };
            get_app_version(version);
            SetWindowTextA(about, std::format("ImageView\r\nVersion {}\r\nBuilt {}", version, __TIMESTAMP__).c_str());
            return 0;
        }
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // Create the listview columns
    // Get the list of hotkeys
    // Populate listview rows
    // Allow user to edit them somehow

    INT_PTR hotkeys_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        HWND listview = GetDlgItem(dlg, IDC_LIST_HOTKEYS);

        switch(msg) {

        case WM_INITDIALOG: {

            rect listview_rect;
            GetWindowRect(listview, &listview_rect);

            int width = listview_rect.w() - GetSystemMetrics(SM_CXVSCROLL);

            LVCOLUMNA column;
            memset(&column, 0, sizeof(column));
            column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.fmt = LVCFMT_LEFT;
            column.cx = width * 70 / 100;
            column.pszText = const_cast<LPSTR>("Action");
            ListView_InsertColumn(listview, 0, &column);
            column.cx = width * 30 / 100;
            column.pszText = const_cast<LPSTR>("Hotkey");
            ListView_InsertColumn(listview, 1, &column);
            ListView_SetExtendedListViewStyle(listview, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_FLATSB);
            ListView_SetView(listview, LV_VIEW_DETAILS);

            HACCEL accelerator_table = LoadAccelerators(GetModuleHandle(null), MAKEINTRESOURCE(IDR_ACCELERATORS_EN_UK));

            std::vector<ACCEL> accelerators;
            copy_accelerator_table(accelerator_table, accelerators);

            HKL layout = GetKeyboardLayout(GetCurrentThreadId());

            LVITEMA item;
            memset(&item, 0, sizeof(item));
            item.mask = LVIF_TEXT;

            int index = 0;
            for(auto const &a : accelerators) {

                // there should be a string corresponding to the command id
                std::string action_text = localize(a.cmd);

                std::string key_text;
                get_accelerator_hotkey_text(a.cmd, accelerators, layout, key_text);

                if(!(action_text.empty() || key_text.empty())) {

                    LOG_DEBUG("{}: {}", action_text, key_text);

                    item.iItem = index++;
                    item.iSubItem = 0;
                    item.pszText = action_text.data();
                    ListView_InsertItem(listview, &item);

                    item.iSubItem = 1;
                    item.pszText = key_text.data();
                    ListView_SetItem(listview, &item);
                }
            }

        } break;

        // if being hidden, deselect listview item and hide change button
        case WM_SHOWWINDOW: {

            if(wparam == false) {
                int selected_item_index = ListView_GetSelectionMark(listview);
                int clear_state = 0;
                ListView_SetItemState(listview, selected_item_index, clear_state, LVIS_SELECTED | LVIS_FOCUSED);
            }
            EnableWindow(GetDlgItem(dlg, IDC_SETTINGS_BUTTON_EDIT_HOTKEY), false);
        } break;

        case WM_NOTIFY: {

            LPNMHDR const nmhdr = reinterpret_cast<LPNMHDR const>(lparam);

            switch(nmhdr->idFrom) {

            case IDC_LIST_HOTKEYS: {

                switch(nmhdr->code) {

                case LVN_ITEMCHANGED: {

                    LPNMLISTVIEW const nm = reinterpret_cast<LPNMLISTVIEW const>(lparam);
                    if((nm->uNewState & LVIS_FOCUSED) != 0) {
                        EnableWindow(GetDlgItem(dlg, IDC_SETTINGS_BUTTON_EDIT_HOTKEY), true);
                    }
                    break;
                } break;
                }
            }
            }
            break;
        } break;
        }

        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // for subclassed tab page dialogs
    // makes controls look transparent by using child dialog background color
    // makes the child dialog transparent by supressing wm_erasebkgnd
    // make sure to set WS_EX_TRANSPARENT on the child dialog

    LRESULT CALLBACK child_proc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR)
    {
        switch(msg) {

        case WM_CTLCOLORBTN:
        case WM_CTLCOLORSTATIC: {
            char class_name[100];
            GetClassNameA(dlg, class_name, _countof(class_name));
            WNDCLASSA lpcls{};
            GetClassInfoA(GetModuleHandle(null), class_name, &lpcls);
            return (LRESULT)lpcls.hbrBackground;
        }

        case WM_ERASEBKGND:
            return 1;
        }
        return DefSubclassProc(dlg, msg, wparam, lparam);
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT add_tab_pages(HWND dlg, uint show_tab_id, int &active_tab)
    {
        // create the tabbed dialog pages

        HWND tab_ctrl = GetDlgItem(dlg, IDC_SETTINGS_TAB_CONTROL);

        rect biggest_tab_dialog{ 0, 0, 0, 0 };

        for(int i = 0; i < _countof(tabs); ++i) {

            auto &tab = tabs[i];

            if((tab.flags & hide_if_not_elevated) != 0 && !app::is_elevated) {
                continue;
            }

            if((tab.flags & hide_if_elevated) != 0 && app::is_elevated) {
                continue;
            }

            if(tab.resource_id == show_tab_id) {
                active_tab = i;
            }

            HRSRC hrsrc;
            CHK_NULL(hrsrc = FindResource(NULL, MAKEINTRESOURCE(tab.resource_id), RT_DIALOG));

            HGLOBAL hglb;
            CHK_NULL(hglb = LoadResource(GetModuleHandle(nullptr), hrsrc));

            DEFER(FreeResource(hglb));

            DLGTEMPLATE *dlg_template;
            CHK_NULL(dlg_template = reinterpret_cast<DLGTEMPLATE *>(LockResource(hglb)));

            DEFER(UnlockResource(dlg_template));

            HWND page_dlg;
            CHK_NULL(page_dlg = CreateDialogIndirect(GetModuleHandle(nullptr), dlg_template, tab_ctrl, tab.handler));

            TCITEMA tci;
            tci.mask = TCIF_TEXT;
            tci.pszText = const_cast<char *>(localize((uint64)tab.resource_id).c_str());
            TabCtrl_InsertItem(tab_ctrl, i, &tci);

            SetWindowSubclass(page_dlg, child_proc, 0, 0);

            if((tab.flags & ignore_size) == 0) {
                rect tab_rect;
                GetWindowRect(page_dlg, &tab_rect);
                biggest_tab_dialog.right = std::max(biggest_tab_dialog.right, tab_rect.w());
                biggest_tab_dialog.bottom = std::max(biggest_tab_dialog.bottom, tab_rect.h());
            }

            tab_pages.push_back(page_dlg);
        }

        // resize the dialog to contain the largest tab page

        // get the original size of the tab control
        rect tab_rect;
        GetWindowRect(tab_ctrl, &tab_rect);

        // get the required size of the tab control
        TabCtrl_AdjustRect(tab_ctrl, true, &biggest_tab_dialog);

        // modify window size by the difference (which may be positive or negative)
        int x_diff = tab_rect.w() - biggest_tab_dialog.w();
        int y_diff = tab_rect.h() - biggest_tab_dialog.h();

        rect main_rect;
        GetWindowRect(dlg, &main_rect);

        main_rect.right -= x_diff;
        main_rect.bottom -= y_diff;
        SetWindowPos(dlg, null, 0, 0, main_rect.w(), main_rect.h(), SWP_NOZORDER | SWP_NOMOVE);

        tab_rect.right -= x_diff;
        tab_rect.bottom -= y_diff;
        SetWindowPos(tab_ctrl, null, 0, 0, tab_rect.w(), tab_rect.h(), SWP_NOMOVE | SWP_NOZORDER);

        // reposition all top-level child windows which are not the tab control
        // note - there's no concept of docking here - they're all assumed to be
        // relative to the dialog top left.

        HWND child = GetTopWindow(dlg);
        while(child != null) {
            if(GetDlgCtrlID(child) != IDC_SETTINGS_TAB_CONTROL) {
                rect child_rect;
                GetWindowRect(child, &child_rect);
                OffsetRect(&child_rect, -x_diff, -y_diff);
                MapWindowPoints(null, dlg, reinterpret_cast<LPPOINT>(&child_rect), 2);
                MoveWindow(child, child_rect.x(), child_rect.y(), child_rect.w(), child_rect.h(), false);
            }
            child = GetNextWindow(child, GW_HWNDNEXT);
        }

        // resize the tab dialogs to fit within the tab control

        GetClientRect(tab_ctrl, &tab_rect);
        TabCtrl_AdjustRect(tab_ctrl, false, &tab_rect);

        for(auto const page : tab_pages) {
            SetWindowPos(page, null, tab_rect.x(), tab_rect.y(), tab_rect.w(), tab_rect.h(), SWP_NOZORDER);
        }

        TabCtrl_SetCurSel(tab_ctrl, active_tab);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT show_settings_page(uint tab)
    {
        if(tab >= _countof(tabs)) {
            LOG_ERROR("!? Tab {} is out of range (there are {} tabs)", tab, _countof(tabs));
            return HRESULT_FROM_WIN32(ERROR_BAD_ARGUMENTS);
        }
        if(current_page != null) {
            ShowWindow(current_page, SW_HIDE);
        }
        current_page = tab_pages[tab];
        ShowWindow(current_page, SW_SHOW);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    INT_PTR settings_dialog_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        switch(msg) {

        case WM_INITDIALOG: {

            main_dialog = dlg;

            uint show_tab_id = static_cast<uint>(lparam);
            int active_tab{ 0 };

            if(FAILED(add_tab_pages(dlg, show_tab_id, active_tab))) {
                // TODO (chs): report windows error
                return 0;
            }

            // center dialog in main window rect

            HWND parent;
            if((parent = GetParent(dlg)) == NULL) {
                parent = GetDesktopWindow();
            }

            rect parent_rect;
            GetWindowRect(parent, &parent_rect);

            rect dlg_rect;
            GetWindowRect(dlg, &dlg_rect);

            int x = parent_rect.left + ((parent_rect.w() - dlg_rect.w()) / 2);
            int y = parent_rect.top + ((parent_rect.h() - dlg_rect.h()) / 2);

            SetWindowPos(dlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);

            show_settings_page(active_tab);

        } break;

        case WM_NOTIFY: {

            LPNMHDR n = reinterpret_cast<LPNMHDR>(lparam);

            switch(n->idFrom) {

            case IDC_SETTINGS_TAB_CONTROL:

                if(n->code == static_cast<UINT>(TCN_SELCHANGE)) {

                    HWND tab_ctrl = GetDlgItem(dlg, IDC_SETTINGS_TAB_CONTROL);
                    show_settings_page(TabCtrl_GetCurSel(tab_ctrl));
                }
                break;
            }
        } break;

        case WM_USER: {
        } break;

        case WM_COMMAND:

            switch(LOWORD(wparam)) {

            case app::LRESULT_LAUNCH_AS_ADMIN:
                EndDialog(dlg, app::LRESULT_LAUNCH_AS_ADMIN);
                break;

            case IDOK:
                EndDialog(dlg, 0);
                break;

            case IDCANCEL:
                EndDialog(dlg, 0);
                break;
            }
            break;
        }
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////

namespace imageview
{
    LRESULT show_settings_dialog(HWND parent, uint tab_id)
    {
        current_page = null;
        tab_pages.clear();
        return DialogBoxParamA(
            GetModuleHandle(null), MAKEINTRESOURCEA(IDD_DIALOG_SETTINGS), parent, settings_dialog_handler, tab_id);
    }
}