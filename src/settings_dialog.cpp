//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

namespace
{
    //////////////////////////////////////////////////////////////////////

    HWND current_page = null;

    //////////////////////////////////////////////////////////////////////

    INT_PTR settings_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        UNREFERENCED_PARAMETER(dlg);
        UNREFERENCED_PARAMETER(msg);
        UNREFERENCED_PARAMETER(wparam);
        UNREFERENCED_PARAMETER(lparam);
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    INT_PTR about_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        UNREFERENCED_PARAMETER(wparam);
        UNREFERENCED_PARAMETER(lparam);

        switch(msg) {

        case WM_INITDIALOG: {
            HWND about = GetDlgItem(dlg, IDC_SETTINGS_EDIT_ABOUT);
            SendMessage(about, EM_SETREADONLY, 1, 0);
            SetWindowText(about, L"ImageView\r\n\r\nVersion x.y.z\r\n\r\nBuilt {at some point in time}");
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
        UNREFERENCED_PARAMETER(wparam);
        UNREFERENCED_PARAMETER(lparam);

        switch(msg) {

        case WM_INITDIALOG: {

            HWND listview = GetDlgItem(dlg, IDC_LIST_HOTKEYS);
            LVCOLUMN column;
            memset(&column, 0, sizeof(column));
            column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.fmt = LVCFMT_LEFT;
            column.cx = 250;
            column.pszText = const_cast<LPWSTR>(L"Action");
            ListView_InsertColumn(listview, 0, &column);
            column.cx = 100;
            column.pszText = const_cast<LPWSTR>(L"Hotkey");
            ListView_InsertColumn(listview, 1, &column);
            ListView_SetExtendedListViewStyle(listview, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_FLATSB);
            ListView_SetView(listview, LV_VIEW_DETAILS);

            HACCEL accelerator_table = LoadAccelerators(GetModuleHandle(null), MAKEINTRESOURCE(IDR_ACCELERATORS_EN_UK));

            std::vector<ACCEL> accelerators;
            copy_accelerator_table(accelerator_table, accelerators);

            HKL layout = GetKeyboardLayout(GetCurrentThreadId());

            LVITEM item;
            memset(&item, 0, sizeof(item));
            item.mask = LVIF_TEXT;

            int index = 0;
            for(auto const &a : accelerators) {

                std::wstring action_text;
                get_hotkey_description(a, action_text);

                std::wstring key_text;
                get_accelerator_hotkey_text(a, layout, key_text);

                if(!(action_text.empty() || key_text.empty())) {

                    Log(L"%s: %s", action_text.c_str(), key_text.c_str());

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
        }

        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    struct settings_tab_t
    {
        LPWSTR resource_id;
        DLGPROC handler;
    };

    settings_tab_t tabs[] = {
        { MAKEINTRESOURCE(IDD_DIALOG_SETTINGS_MAIN), settings_handler },
        { MAKEINTRESOURCE(IDD_DIALOG_SETTINGS_HOTKEYS), hotkeys_handler },
        { MAKEINTRESOURCE(IDD_DIALOG_SETTINGS_ABOUT), about_handler },
    };

    //////////////////////////////////////////////////////////////////////

    std::vector<HWND> pages;

    //////////////////////////////////////////////////////////////////////
    // for subclassed tab page dialogs
    // makes controls look transparent by using child dialog background color
    // makes the child dialog transparent by supressing wm_erasebkgnd
    // make sure to set WS_EX_TRANSPARENT on the child dialog

    LRESULT CALLBACK
    child_proc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
    {
        UNREFERENCED_PARAMETER(uIdSubclass);
        UNREFERENCED_PARAMETER(dwRefData);
        switch(msg) {

        case WM_CTLCOLORBTN:
        case WM_CTLCOLORSTATIC: {
            wchar_t class_name[100];
            GetClassName(dlg, class_name, _countof(class_name));
            WNDCLASS lpcls{};
            GetClassInfo(GetModuleHandle(null), class_name, &lpcls);
            return (LRESULT)lpcls.hbrBackground;
        }

        case WM_ERASEBKGND:
            return 1;
        }
        return DefSubclassProc(dlg, msg, wparam, lparam);
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT add_tab_pages(HWND dlg)
    {
        // create the tabbed dialog pages

        HWND tab_ctrl = GetDlgItem(dlg, IDC_SETTINGS_TAB_CONTROL);

        rect biggest_tab_dialog{ 0, 0, 0, 0 };

        for(int i = 0; i < _countof(tabs); ++i) {

            auto &tab = tabs[i];

            HRSRC hrsrc;
            CHK_NULL(hrsrc = FindResource(NULL, tab.resource_id, RT_DIALOG));

            HGLOBAL hglb;
            CHK_NULL(hglb = LoadResource(GetModuleHandle(nullptr), hrsrc));

            defer(FreeResource(hglb));

            DLGTEMPLATE *dlg_template;
            CHK_NULL(dlg_template = reinterpret_cast<DLGTEMPLATE *>(LockResource(hglb)));

            defer(UnlockResource(dlg_template));

            HWND page_dlg;
            CHK_NULL(page_dlg = CreateDialogIndirect(GetModuleHandle(nullptr), dlg_template, tab_ctrl, tab.handler));

            TCITEM tie{};
            tie.mask = TCIF_TEXT;
            tie.pszText = const_cast<wchar *>(localize((uint64)tab.resource_id));
            TabCtrl_InsertItem(tab_ctrl, i, &tie);

            SetWindowSubclass(page_dlg, child_proc, 0, 0);

            rect tab_rect;
            GetWindowRect(page_dlg, &tab_rect);
            biggest_tab_dialog.right = std::max(biggest_tab_dialog.right, static_cast<LONG>(tab_rect.w()));
            biggest_tab_dialog.bottom = std::max(biggest_tab_dialog.bottom, static_cast<LONG>(tab_rect.h()));

            pages.push_back(page_dlg);
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

        for(auto const page : pages) {
            SetWindowPos(page, null, tab_rect.x(), tab_rect.y(), tab_rect.w(), tab_rect.h(), SWP_NOZORDER);
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT show_settings_page(uint tab)
    {
        if(tab >= _countof(tabs)) {
            Log(L"!? Tab %d is out of range (there are %d tabs)", tab, _countof(tabs));
            return ERROR_BAD_ARGUMENTS;
        }
        if(current_page != null) {
            ShowWindow(current_page, SW_HIDE);
        }
        current_page = pages[tab];
        ShowWindow(current_page, SW_SHOW);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    INT_PTR settings_dialog_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        UNREFERENCED_PARAMETER(lparam);

        switch(msg) {

        case WM_INITDIALOG: {

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

            if(FAILED(add_tab_pages(dlg))) {
                // TODO (chs): report windows error
                return 0;
            }

            // show the first page because tab 0 is initially selected
            show_settings_page(0);

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

        case WM_COMMAND:

            switch(LOWORD(wparam)) {

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

LRESULT show_settings_dialog(HWND parent)
{
    current_page = null;
    pages.clear();
    return DialogBox(GetModuleHandle(null), MAKEINTRESOURCE(IDD_DIALOG_SETTINGS), parent, settings_dialog_handler);
}
