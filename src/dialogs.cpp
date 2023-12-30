//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

namespace
{
    //////////////////////////////////////////////////////////////////////

    UINT_PTR CALLBACK select_color_dialog_hook_proc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER(wParam);

        if(uiMsg == WM_INITDIALOG) {
            CHOOSECOLORW *cc = reinterpret_cast<CHOOSECOLORW *>(lParam);
            if(cc != null && cc->lCustData != 0) {
                SetWindowTextW(hdlg, reinterpret_cast<wchar const *>(cc->lCustData));
            }
        }
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////

namespace imageview::dialog
{
    HRESULT open_file(HWND window, std::wstring &path)
    {
        COMDLG_FILTERSPEC *filter_specs;
        uint num_filter_specs;

        CHK_HR(image::get_load_filter_specs(filter_specs, num_filter_specs));

        ComPtr<IFileDialog> pfd;
        DWORD dwFlags;
        ComPtr<IShellItem> psiResult;
        PWSTR pszFilePath{ null };

        std::wstring title = std::format(L"{}{}{}", localize(IDS_AppName), L" : ", localize(IDS_SelectFile));

        CHK_HR(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)));
        CHK_HR(pfd->GetOptions(&dwFlags));
        CHK_HR(pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_OKBUTTONNEEDSINTERACTION));
        CHK_HR(pfd->SetFileTypes(num_filter_specs, filter_specs));
        CHK_HR(pfd->SetFileTypeIndex(1));
        CHK_HR(pfd->SetOkButtonLabel(localize(IDS_OPEN_FILE_OK_BUTTON).c_str()));
        CHK_HR(pfd->SetTitle(title.c_str()));
        CHK_HR(pfd->Show(window));
        CHK_HR(pfd->GetResult(&psiResult));
        CHK_HR(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));

        path = pszFilePath;
        CoTaskMemFree(pszFilePath);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT save_file(HWND window, std::wstring &path)
    {
        COMDLG_FILTERSPEC *filter_specs;
        uint num_filter_specs;

        CHK_HR(image::get_save_filter_specs(filter_specs, num_filter_specs));

        ComPtr<IFileDialog> pfd;
        DWORD dwFlags;
        ComPtr<IShellItem> psiResult;
        PWSTR pszFilePath{ null };

        auto options = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OKBUTTONNEEDSINTERACTION | FOS_OVERWRITEPROMPT |
                       FOS_STRICTFILETYPES;

        std::wstring title = std::format(L"{}{}{}", localize(IDS_AppName), L" : ", localize(IDS_SelectFile));

        CHK_HR(CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)));
        CHK_HR(pfd->GetOptions(&dwFlags));
        CHK_HR(pfd->SetOptions(dwFlags | options));
        CHK_HR(pfd->SetFileTypes(num_filter_specs, filter_specs));
        CHK_HR(pfd->SetFileTypeIndex(1));
        CHK_HR(pfd->SetTitle(title.c_str()));
        CHK_HR(pfd->Show(window));
        CHK_HR(pfd->GetResult(&psiResult));
        CHK_HR(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));

        path = pszFilePath;
        CoTaskMemFree(pszFilePath);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT select_color(HWND window, uint32 &color, wchar const *title)
    {
        static COLORREF custom_colors[16];
        uint32 alpha = color & 0xff000000;

        CHOOSECOLORW cc;
        mem_clear(&cc);
        cc.lStructSize = sizeof(cc);
        cc.hwndOwner = window;
        cc.lpCustColors = (LPDWORD)custom_colors;
        cc.lCustData = reinterpret_cast<LPARAM>(title);
        cc.rgbResult = color;
        cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR | CC_ENABLEHOOK;
        cc.lpfnHook = select_color_dialog_hook_proc;
        if(!ChooseColorW(&cc)) {
            return E_ABORT;
        }
        color = cc.rgbResult | alpha;
        return S_OK;
    }
}