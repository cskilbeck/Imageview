//////////////////////////////////////////////////////////////////////
// about page

#include "pch.h"

namespace
{
    using namespace imageview;
    using namespace imageview::settings_ui;

    //////////////////////////////////////////////////////////////////////

    HRESULT copy_window_text_to_clipboard(HWND hwnd)
    {
        SetLastError(0);

        int len;
        CHK_ZERO(len = GetWindowTextLengthW(hwnd));

        HANDLE handle;
        CHK_NULL(handle = GlobalAlloc(GHND | GMEM_SHARE, static_cast<size_t>(len * sizeof(wchar)) + 1));

        wchar *buffer;
        CHK_NULL(buffer = reinterpret_cast<wchar *>(GlobalLock(handle)));
        DEFER(GlobalUnlock(handle));

        CHK_ZERO(GetWindowTextW(hwnd, buffer, len + 1));

        CHK_BOOL(OpenClipboard(null));
        DEFER(CloseClipboard());

        CHK_BOOL(EmptyClipboard());
        CHK_BOOL(SetClipboardData(CF_UNICODETEXT, handle));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // brutally suppress the text caret in the 'about' text box

    LRESULT CALLBACK suppress_caret_subclass(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR)
    {
        HideCaret(dlg);
        return DefSubclassProc(dlg, msg, wparam, lparam);
    }

    // ABOUT page \ WM_INITDIALOG

    BOOL on_initdialog_about(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        // get rid of the text caret in the edit box
        HWND about = GetDlgItem(hwnd, IDC_SETTINGS_EDIT_ABOUT);
        SetWindowSubclass(about, suppress_caret_subclass, 0, 0);

        Edit_SetReadOnly(about, true);

        // populate the about box text

        std::vector<std::wstring> lines;

        std::wstring ver{ L"Version?" };
        get_app_version(ver);

        lines.push_back(localize(IDS_AppName));
        lines.push_back(std::format(L"v{}", ver));
        lines.push_back(std::format(L"Built {}", unicode(__TIMESTAMP__)));
        lines.push_back(std::format(L"Running as admin: {}", app::is_elevated));
        lines.push_back(std::format(L"System Memory {} GB", app::system_memory_gb));

        std::wstring about_text;
        wchar const *sep = L"";

        for(auto const &s : lines) {
            about_text.append(sep);
            about_text.append(s);
            sep = L"\r\n";
        }

        SetWindowTextW(about, about_text.c_str());
        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // ABOUT page \ WM_COMMAND

    void on_command_about(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

            // copy 'about' text to clipboard

        case IDC_BUTTON_ABOUT_COPY: {

            if(SUCCEEDED(copy_window_text_to_clipboard(GetDlgItem(hwnd, IDC_SETTINGS_EDIT_ABOUT)))) {

                SetWindowTextW(GetDlgItem(hwnd, IDC_BUTTON_ABOUT_COPY), localize(IDS_COPIED).c_str());
            }

        } break;
        }
    }
}

namespace imageview::settings_ui
{
    //////////////////////////////////////////////////////////////////////
    // ABOUT page \ DLGPROC

    INT_PTR about_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_INITDIALOG, on_initdialog_about);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_about);
        }
        return ctlcolor_base(dlg, msg, wParam, lParam);
    }
}