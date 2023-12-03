//////////////////////////////////////////////////////////////////////

#include "pch.h"

LOG_CONTEXT("settings_dlg");

//////////////////////////////////////////////////////////////////////

namespace
{
    using namespace imageview;

    //////////////////////////////////////////////////////////////////////

    HWND main_dialog = null;

    int current_page = -1;

    rect tab_rect;

    //////////////////////////////////////////////////////////////////////

    enum tab_flags_t
    {
        dont_care = 0,
        hide_if_elevated = 1,
        hide_if_not_elevated = 2,
    };

    struct settings_tab_t
    {
        uint resource_id;
        DLGPROC handler;
        int flags;
        int index;
        HWND hwnd;
    };

    //////////////////////////////////////////////////////////////////////

    INT_PTR main_settings_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR hotkeys_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR explorer_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR relaunch_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR about_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);

    //////////////////////////////////////////////////////////////////////

    settings_tab_t tabs[] = {
        { IDD_DIALOG_SETTINGS_MAIN, main_settings_handler, dont_care },
        { IDD_DIALOG_SETTINGS_HOTKEYS, hotkeys_handler, dont_care },
        { IDD_DIALOG_SETTINGS_EXPLORER, explorer_handler, hide_if_not_elevated },
        { IDD_DIALOG_SETTINGS_RELAUNCH, relaunch_handler, hide_if_elevated },
        { IDD_DIALOG_SETTINGS_ABOUT, about_handler, dont_care },
    };

    //////////////////////////////////////////////////////////////////////

    std::vector<settings_tab_t *> active_tabs;

    //////////////////////////////////////////////////////////////////////

    bool hide_tab(settings_tab_t const &t)
    {
        return (t.flags & hide_if_elevated) != 0 && app::is_elevated ||
               (t.flags & hide_if_not_elevated) != 0 && !app::is_elevated;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT show_settings_page(uint tab)
    {
        if(tab >= active_tabs.size()) {
            LOG_ERROR("!? Tab {} is out of range (there are {} tabs)", tab, active_tabs.size());
            return HRESULT_FROM_WIN32(ERROR_BAD_ARGUMENTS);
        }
        if(current_page != -1) {
            ShowWindow(active_tabs[current_page]->hwnd, SW_HIDE);
        }
        current_page = tab;
        ShowWindow(active_tabs[current_page]->hwnd, SW_SHOW);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

#pragma warning(push)
#pragma warning(disable : 4100)

    void update_scroll_info(HWND hWnd)
    {
        rect rc;
        GetClientRect(hWnd, &rc);

        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
        si.nPos = 0;
        si.nTrackPos = 0;
        si.nMin = 0;
        si.nMax = rc.w();
        si.nPage = tab_rect.w();
        SetScrollInfo(hWnd, SB_HORZ, &si, FALSE);

        si.nMax = rc.h();
        si.nPage = tab_rect.h();
        SetScrollInfo(hWnd, SB_VERT, &si, FALSE);
    }

    int get_scroll_pos(HWND hwnd, int bar, UINT code)
    {
        SCROLLINFO si = {};
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_TRACKPOS;
        GetScrollInfo(hwnd, bar, &si);

        const int minPos = si.nMin;
        const int maxPos = si.nMax - (si.nPage - 1);

        int result = -1;

        switch(code) {
        case SB_LINEUP /*SB_LINELEFT*/:
            result = std::max(si.nPos - 1, minPos);
            break;

        case SB_LINEDOWN /*SB_LINERIGHT*/:
            result = std::min(si.nPos + 1, maxPos);
            break;

        case SB_PAGEUP /*SB_PAGELEFT*/:
            result = std::max(si.nPos - (int)si.nPage, minPos);
            break;

        case SB_PAGEDOWN /*SB_PAGERIGHT*/:
            result = std::min(si.nPos + (int)si.nPage, maxPos);
            break;

        case SB_THUMBPOSITION:
            // do nothing
            break;

        case SB_THUMBTRACK:
            result = si.nTrackPos;
            break;

        case SB_TOP /*SB_LEFT*/:
            result = minPos;
            break;

        case SB_BOTTOM /*SB_RIGHT*/:
            result = maxPos;
            break;

        case SB_ENDSCROLL:
            // do nothing
            break;
        }

        return result;
    }

    void scroll_client(HWND hwnd, int bar, int pos)
    {
        static int s_prevx = 1;
        static int s_prevy = 1;

        int cx = 0;
        int cy = 0;

        int &delta = (bar == SB_HORZ ? cx : cy);
        int &prev = (bar == SB_HORZ ? s_prevx : s_prevy);

        delta = prev - pos;
        prev = pos;

        if(cx || cy) {
            ScrollWindow(hwnd, cx, cy, NULL, NULL);
        }
    }

    void on_scroll(HWND hwnd, int bar, UINT code)
    {
        const int scrollPos = get_scroll_pos(hwnd, bar, code);

        if(scrollPos == -1) {
            return;
        }

        SetScrollPos(hwnd, bar, scrollPos, TRUE);
        scroll_client(hwnd, bar, scrollPos);
    }

    void on_hscroll(HWND hwnd, HWND hwndCtl, UINT code, int pos)
    {
        on_scroll(hwnd, SB_HORZ, code);
    }

    void on_vscroll(HWND hwnd, HWND hwndCtl, UINT code, int pos)
    {
        on_scroll(hwnd, SB_VERT, code);
    }

    //////////////////////////////////////////////////////////////////////
    // dialogs which are in tab pages have white backgrounds

    HBRUSH on_ctl_color(HWND hwnd, HDC hdc, HWND hwndChild, int type)
    {
        return GetSysColorBrush(COLOR_WINDOW);
    }

    //////////////////////////////////////////////////////////////////////

    void on_command_relaunch_handler(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

        case IDC_BUTTON_SETTINGS_RELAUNCH: {
            EndDialog(main_dialog, app::LRESULT_LAUNCH_AS_ADMIN);
        } break;
        }
    }

    //////////////////////////////////////////////////////////////////////

    LRESULT CALLBACK suppress_cursor(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR)
    {
        HideCaret(dlg);
        return DefSubclassProc(dlg, msg, wparam, lparam);
    }

    //////////////////////////////////////////////////////////////////////

    BOOL on_init_about_handler(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        HWND about = GetDlgItem(hwnd, IDC_SETTINGS_EDIT_ABOUT);
        SetWindowSubclass(about, suppress_cursor, 0, 0);
        SendMessage(about, EM_SETREADONLY, 1, 0);
        std::string version{ "Version?" };
        get_app_version(version);
        SetWindowTextA(about,
                       std::format("ImageView V{}\r\nBuilt {}\r\nRunning as admin: {}\r\nSystem Memory {} GB\r\n",
                                   version,
                                   __TIMESTAMP__,
                                   app::is_elevated,
                                   app::system_memory_gb)
                           .c_str());
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void on_command_about_handler(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

        case IDC_BUTTON_ABOUT_COPY: {
            HWND edit_control = GetDlgItem(hwnd, IDC_SETTINGS_EDIT_ABOUT);

            SetLastError(0);
            int len = GetWindowTextLengthA(edit_control);
            if(len == 0) {
                LOG_ERROR("Can't GetWindowTextLength for clipboard: {}", windows_error_message());
                return;
            }
            HANDLE handle = GlobalAlloc(GHND | GMEM_SHARE, static_cast<size_t>(len) + 1);
            if(handle == null) {
                LOG_ERROR("Can't alloc {} for clipboard: {}", len, windows_error_message());
                return;
            }
            char *buffer = reinterpret_cast<char *>(GlobalLock(handle));
            if(buffer == null) {
                LOG_ERROR("Can't lock buffer clipboard: {}", windows_error_message());
                return;
            }

            if(GetWindowTextA(edit_control, buffer, len + 1) == 0) {
                LOG_ERROR("Can't GetWindowText for clipboard: {}", windows_error_message());
                return;
            }

            if(!OpenClipboard(null)) {
                LOG_ERROR("Can't open clipboard: {}", windows_error_message());
                return;
            }
            DEFER(CloseClipboard());

            if(!EmptyClipboard()) {
                LOG_ERROR("Can't empty clipboard: {}", windows_error_message());
                return;
            }

            if(!SetClipboardData(CF_TEXT, handle)) {
                LOG_ERROR("Can't SetClipboardData clipboard: {}", windows_error_message());
                return;
            }
            LOG_INFO("Copied 'About' text to clipboard");
            SetWindowTextA(GetDlgItem(hwnd, IDC_BUTTON_ABOUT_COPY), "Copied");    //@localize

        } break;
        }
    }

    //////////////////////////////////////////////////////////////////////

    std::string get_item_text(HWND listview, LPARAM item)
    {
        LVITEMA lvi;
        memset(&lvi, 0, sizeof(lvi));
        lvi.mask = LVIF_TEXT;

        std::string s;

        LRESULT len = 32;
        LRESULT got;

        do {
            len *= 2;
            s.resize(len);
            lvi.cchTextMax = static_cast<int>(len);
            lvi.pszText = s.data();
            got = SendMessageA(listview, LVM_GETITEMTEXT, static_cast<WPARAM>(item), reinterpret_cast<LPARAM>(&lvi));
        } while(got >= len - 1);

        return s.substr(0, got);
    }

    //////////////////////////////////////////////////////////////////////

    BOOL on_init_hotkeys_handler(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        HWND listview = GetDlgItem(hwnd, IDC_LIST_HOTKEYS);

        rect listview_rect;
        GetWindowRect(hwnd, &listview_rect);

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

        std::set<uint> got_hotkey;

        // get all the hotkeys descriptions in order

        std::map<std::string, uint> descriptions;

        for(auto const &a : hotkeys::hotkey_text) {
            std::string action_text = localize(a.first);
            descriptions[action_text] = a.first;
        }

        // populate the listview

        int index = 0;
        for(auto const &a : descriptions) {

            // there should be a string corresponding to the command id
            std::string action_text = localize(a.second);
            std::string key_text;
            if(hotkeys::get_hotkey_text(a.second, key_text) == S_OK) {
                LVITEMA item;
                memset(&item, 0, sizeof(item));
                item.mask = LVIF_TEXT;
                item.iItem = index;
                item.iSubItem = 0;
                item.pszText = action_text.data();
                ListView_InsertItem(listview, &item);
                index += 1;

                item.mask = LVIF_TEXT;
                item.iSubItem = 1;
                item.pszText = key_text.data();
                ListView_SetItem(listview, &item);
            }
        }

        return false;
    }

    //////////////////////////////////////////////////////////////////////

    void on_showwindow_hotkeys_handler(HWND hwnd, BOOL fShow, UINT status)
    {
        HWND listview = GetDlgItem(hwnd, IDC_LIST_HOTKEYS);

        // if being hidden, deselect listview item and hide change button
        if(!fShow) {
            int selected_item_index = ListView_GetSelectionMark(listview);
            int clear_state = 0;
            ListView_SetItemState(listview, selected_item_index, clear_state, LVIS_SELECTED | LVIS_FOCUSED);
        }
    }

    //////////////////////////////////////////////////////////////////////

    int on_notify_hotkeys_handler(HWND hwnd, int idFrom, LPNMHDR nmhdr)
    {
        switch(nmhdr->idFrom) {

        case IDC_LIST_HOTKEYS: {

            switch(nmhdr->code) {

            case LVN_ITEMCHANGED: {

                LPNMLISTVIEW nm = reinterpret_cast<LPNMLISTVIEW>(nmhdr);
                if((nm->uNewState & LVIS_FOCUSED) != 0) {
                    //
                }
                break;
            } break;
            }
        }
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    BOOL on_init_settings_handler(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        main_dialog = hwnd;

        uint requested_tab_resource_id = static_cast<uint>(lParam);
        int active_tab_index{ 0 };

        HWND tab_ctrl = GetDlgItem(hwnd, IDC_SETTINGS_TAB_CONTROL);

        active_tabs.clear();

        int index = 0;
        for(int i = 0; i < _countof(tabs); ++i) {

            settings_tab_t &tab = tabs[i];

            if(hide_tab(tab)) {
                continue;
            }

            TCITEMA tci;
            tci.mask = TCIF_TEXT;
            tci.pszText = const_cast<char *>(localize((uint64)tab.resource_id).c_str());
            TabCtrl_InsertItem(tab_ctrl, index, &tci);
            tab.index = index;
            index += 1;
            active_tabs.push_back(&tab);
        }

        // now the tabs are added, get the inner size of the tab control
        GetWindowRect(tab_ctrl, &tab_rect);
        TabCtrl_AdjustRect(tab_ctrl, false, &tab_rect);
        MapWindowPoints(null, hwnd, reinterpret_cast<LPPOINT>(&tab_rect), 2);

        // create the dialog pages

        for(auto t : active_tabs) {

            settings_tab_t &tab = *t;

            if(hide_tab(tab)) {
                continue;
            }

            if(tab.resource_id == requested_tab_resource_id) {
                active_tab_index = tab.index;
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
            CHK_NULL(page_dlg = CreateDialogIndirectA(GetModuleHandle(nullptr), dlg_template, hwnd, tab.handler));

            SetWindowPos(page_dlg, HWND_TOP, tab_rect.x(), tab_rect.y(), tab_rect.w(), tab_rect.h(), 0);

            // update_scroll_info(page_dlg);

            tab.hwnd = page_dlg;
        }

        TabCtrl_SetCurSel(tab_ctrl, active_tab_index);

        // center dialog in main window rect

        HWND parent;
        if((parent = GetParent(hwnd)) == NULL) {
            parent = GetDesktopWindow();
        }

        rect parent_rect;
        GetWindowRect(parent, &parent_rect);

        rect dlg_rect;
        GetWindowRect(hwnd, &dlg_rect);

        int x = parent_rect.left + ((parent_rect.w() - dlg_rect.w()) / 2);
        int y = parent_rect.top + ((parent_rect.h() - dlg_rect.h()) / 2);

        SetWindowPos(hwnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);

        show_settings_page(active_tab_index);

        return false;
    }

    //////////////////////////////////////////////////////////////////////

    int on_notify_settings_handler(HWND hwnd, int idFrom, LPNMHDR nmhdr)
    {
        switch(nmhdr->idFrom) {

        case IDC_SETTINGS_TAB_CONTROL:

            if(nmhdr->code == static_cast<UINT>(TCN_SELCHANGE)) {

                HWND tab_ctrl = GetDlgItem(hwnd, IDC_SETTINGS_TAB_CONTROL);
                show_settings_page(TabCtrl_GetCurSel(tab_ctrl));
            }
            break;
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void on_command_settings_handler(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

        case app::LRESULT_LAUNCH_AS_ADMIN:
            EndDialog(hwnd, app::LRESULT_LAUNCH_AS_ADMIN);
            break;

        case IDOK:
            EndDialog(hwnd, 0);
            break;

        case IDCANCEL:
            EndDialog(hwnd, 0);
            break;
        }
    }

//#pragma warning(disable : 4100)
#pragma warning(pop)

    //////////////////////////////////////////////////////////////////////

    INT_PTR main_settings_handler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {
            HANDLE_MSG(hWnd, WM_CTLCOLORDLG, on_ctl_color);
            HANDLE_MSG(hWnd, WM_CTLCOLORSTATIC, on_ctl_color);
            HANDLE_MSG(hWnd, WM_CTLCOLORBTN, on_ctl_color);
            HANDLE_MSG(hWnd, WM_CTLCOLOREDIT, on_ctl_color);
            HANDLE_MSG(hWnd, WM_HSCROLL, on_hscroll);
            HANDLE_MSG(hWnd, WM_VSCROLL, on_vscroll);
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    INT_PTR relaunch_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {
            HANDLE_MSG(dlg, WM_CTLCOLORDLG, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLORSTATIC, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLORBTN, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLOREDIT, on_ctl_color);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_relaunch_handler);
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    INT_PTR explorer_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {
            HANDLE_MSG(dlg, WM_CTLCOLORDLG, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLORSTATIC, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLORBTN, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLOREDIT, on_ctl_color);
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    INT_PTR about_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_CTLCOLORDLG, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLORSTATIC, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLORBTN, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLOREDIT, on_ctl_color);
            HANDLE_MSG(dlg, WM_INITDIALOG, on_init_about_handler);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_about_handler);
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    INT_PTR hotkeys_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_CTLCOLORDLG, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLORSTATIC, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLORBTN, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLOREDIT, on_ctl_color);
            HANDLE_MSG(dlg, WM_INITDIALOG, on_init_hotkeys_handler);
            HANDLE_MSG(dlg, WM_SHOWWINDOW, on_showwindow_hotkeys_handler);
            HANDLE_MSG(dlg, WM_NOTIFY, on_notify_hotkeys_handler);
        }

        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    INT_PTR settings_dialog_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_INITDIALOG, on_init_settings_handler);
            HANDLE_MSG(dlg, WM_NOTIFY, on_notify_settings_handler);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_settings_handler);
        }
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////

namespace imageview
{
    LRESULT show_settings_dialog(HWND parent, uint tab_id)
    {
        current_page = -1;
        return DialogBoxParamA(
            GetModuleHandle(null), MAKEINTRESOURCEA(IDD_DIALOG_SETTINGS), parent, settings_dialog_handler, tab_id);
    }
}