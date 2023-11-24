//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

HRESULT select_file_dialog(HWND window, std::wstring &path)
{
    COMDLG_FILTERSPEC file_types[] = { { L"All files", L"*.*" }, { L"Image files", L"*.jpg;*.png;*.bmp" } };
    uint num_file_types = (uint)std::size(file_types);

    ComPtr<IFileDialog> pfd;
    DWORD dwFlags;
    ComPtr<IShellItem> psiResult;
    PWSTR pszFilePath{ null };

    CHK_HR(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)));
    CHK_HR(pfd->GetOptions(&dwFlags));
    CHK_HR(pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_OKBUTTONNEEDSINTERACTION));
    CHK_HR(pfd->SetFileTypes(num_file_types, file_types));
    CHK_HR(pfd->SetFileTypeIndex(2));
    CHK_HR(pfd->SetOkButtonLabel(L"View"));
    CHK_HR(pfd->SetTitle(format(L"%s%s%s", localize(IDS_AppName), L" : ", localize(IDS_SelectFile)).c_str()));
    CHK_HR(pfd->Show(window));
    CHK_HR(pfd->GetResult(&psiResult));
    CHK_HR(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));

    path = pszFilePath;
    CoTaskMemFree(pszFilePath);

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT save_file_dialog(HWND window, std::wstring &path)
{
    // TODO (chs): get this from the list of save_formats in image_decoder.cpp

    COMDLG_FILTERSPEC file_types[] = { { L"JPEG files", L"*.jpg" },
                                       { L"PNG files", L"*.png" },
                                       { L"TIFF files", L"*.tiff" },
                                       { L"BMP Files", L"*.bmp" } };
    uint num_file_types = (uint)std::size(file_types);

    ComPtr<IFileDialog> pfd;
    DWORD dwFlags;
    ComPtr<IShellItem> psiResult;
    PWSTR pszFilePath{ null };

    CHK_HR(CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)));
    CHK_HR(pfd->GetOptions(&dwFlags));
    CHK_HR(pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OKBUTTONNEEDSINTERACTION |
                           FOS_OVERWRITEPROMPT | FOS_STRICTFILETYPES));
    CHK_HR(pfd->SetFileTypes(num_file_types, file_types));
    CHK_HR(pfd->SetFileTypeIndex(2));
    CHK_HR(pfd->SetOkButtonLabel(L"Save"));
    CHK_HR(pfd->SetTitle(format(L"%s%s%s", localize(IDS_AppName), L" : ", localize(IDS_SelectFile)).c_str()));
    CHK_HR(pfd->Show(window));
    CHK_HR(pfd->GetResult(&psiResult));
    CHK_HR(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));

    path = pszFilePath;
    CoTaskMemFree(pszFilePath);

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

static UINT_PTR CALLBACK select_color_dialog_hook_proc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);

    if(uiMsg == WM_INITDIALOG) {
        CHOOSECOLOR *cc = reinterpret_cast<CHOOSECOLOR *>(lParam);
        if(cc != null && cc->lCustData != 0) {
            SetWindowText(hdlg, reinterpret_cast<wchar *>(cc->lCustData));
        }
    }
    return 0;
}

HRESULT select_color_dialog(HWND window, uint32 &color, wchar const *title)
{
    static COLORREF custom_colors[16];

    CHOOSECOLOR cc{ 0 };
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = window;
    cc.lpCustColors = (LPDWORD)custom_colors;
    cc.lCustData = reinterpret_cast<LPARAM>(title);
    cc.rgbResult = color;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR | CC_ENABLEHOOK;
    cc.lpfnHook = select_color_dialog_hook_proc;
    if(!ChooseColor(&cc)) {
        return E_ABORT;
    }
    color = cc.rgbResult;
    return S_OK;
}
