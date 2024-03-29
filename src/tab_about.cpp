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
        std::wstring text;

        CHK_HR(get_window_text(hwnd, text));

        CHK_HR(copy_string_to_clipboard(text));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // brutally suppress the text caret in the 'about' text box

    LRESULT CALLBACK suppress_caret_subclass(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR)
    {
        HideCaret(dlg);
        return DefSubclassProc(dlg, msg, wparam, lparam);
    }

    //////////////////////////////////////////////////////////////////////
    // ABOUT page \ WM_INITDIALOG

    BOOL on_initdialog_about(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        // get rid of the text caret in the edit box
        HWND about = GetDlgItem(hwnd, IDC_SETTINGS_EDIT_ABOUT);
        SetWindowSubclass(about, suppress_caret_subclass, 0, 0);

        Edit_SetReadOnly(about, true);

        // populate the about box text - TODO (chs): localize the about text?

        std::vector<std::wstring> lines;

        std::wstring ver{ L"Version?" };
        get_app_version(ver);

        lines.push_back(localize(IDS_AppName));
        lines.push_back(std::format(L"Version: {}", ver));
        lines.push_back(std::format(L"Built: {}", unicode(__TIMESTAMP__)));
        lines.push_back(std::format(L"Running as admin: {}", app::is_elevated));
        lines.push_back(std::format(L"System Memory: {} GB", app::system_memory_gb));
        lines.push_back(std::format(L"Command line: {}", GetCommandLineW()));

        std::wstring about_text;
        wchar const *sep = L"";

        for(auto const &s : lines) {
            about_text.append(sep);
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