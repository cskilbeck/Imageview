//////////////////////////////////////////////////////////////////////

#include "pch.h"

LOG_CONTEXT("dialogs");

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
        static uint filetype_index = 0;

        COMDLG_FILTERSPEC *filter_specs = image::load_filetypes.comdlg_filterspecs.data();

        uint num_filter_specs = static_cast<uint>(image::load_filetypes.comdlg_filterspecs.size());

        uint default_index = image::load_filetypes.default_index;

        if(filetype_index == 0) {
            filetype_index = default_index;
        }

        ComPtr<IFileDialog> pfd;
        DWORD dwFlags;
        ComPtr<IShellItem> psiResult;
        PWSTR pszFilePath{ null };

        std::wstring title = std::format(L"{}{}{}", localize(IDS_AppName), L" : ", localize(IDS_SelectFile));

        CHK_HR(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)));
        CHK_HR(pfd->GetOptions(&dwFlags));
        CHK_HR(pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_OKBUTTONNEEDSINTERACTION));
        CHK_HR(pfd->SetFileTypes(num_filter_specs, filter_specs));
        CHK_HR(pfd->SetFileTypeIndex(filetype_index));
        CHK_HR(pfd->SetOkButtonLabel(localize(IDS_OPEN_FILE_OK_BUTTON).c_str()));
        CHK_HR(pfd->SetTitle(title.c_str()));
        CHK_HR(pfd->Show(window));
        CHK_HR(pfd->GetResult(&psiResult));
        CHK_HR(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));

        CHK_HR(pfd->GetFileTypeIndex(&filetype_index));

        path = pszFilePath;
        CoTaskMemFree(pszFilePath);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT save_file(HWND window, std::wstring const &filename, std::wstring &path)
    {
        static uint filetype_index = 0;

        COMDLG_FILTERSPEC *filter_specs = image::save_filetypes.comdlg_filterspecs.data();

        uint num_filter_specs = static_cast<uint>(image::save_filetypes.comdlg_filterspecs.size());

        uint default_index = image::save_filetypes.default_index;

        if(filetype_index != 0) {
            default_index = filetype_index;
        }

        std::wstring ext;
        std::wstring name_only;
        std::wstring folder_path;

        // search for the file extension in the encoder filter specs

        if(!filename.empty()) {

            CHK_HR(file::get_filename(filename, name_only));
            CHK_HR(file::get_path(filename, folder_path));
            CHK_HR(file::get_extension(filename, ext));
            ext = make_lowercase(ext);

            bool matched_encoder = false;
            for(uint i = 0; i < num_filter_specs; ++i) {

                if(wcsstr(filter_specs[i].pszSpec, ext.c_str()) != null) {
                    default_index = i + 1;
                    matched_encoder = true;
                    break;
                }
            }
            if(!matched_encoder) {
                ext = L".png";
                CHK_HR(file::get_barename(filename, name_only));
                name_only += ext;
                default_index = image::save_filetypes.default_index;
            }
        }

        if(ext.empty()) {
            ext = L".png";
        }
        std::wstring default_ext = (ext.front() == L'.') ? ext.substr(1) : ext;

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
        if(!folder_path.empty()) {
            ComPtr<IShellItem> folder_item;
            if(SUCCEEDED(SHCreateItemFromParsingName(folder_path.c_str(), null, IID_PPV_ARGS(&folder_item)))) {
                pfd->SetFolder(folder_item.Get());
            }
        }
        CHK_HR(pfd->SetFileName(name_only.c_str()));
        CHK_HR(pfd->SetFileTypes(num_filter_specs, filter_specs));
        CHK_HR(pfd->SetFileTypeIndex(default_index));
        CHK_HR(pfd->SetDefaultExtension(default_ext.c_str()));
        CHK_HR(pfd->SetTitle(title.c_str()));
        CHK_HR(pfd->Show(window));
        CHK_HR(pfd->GetResult(&psiResult));

        CHK_HR(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));

        CHK_HR(pfd->GetFileTypeIndex(&filetype_index));

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
        cc.rgbResult = color & 0x00ffffff;
        cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR | CC_ENABLEHOOK;
        cc.lpfnHook = select_color_dialog_hook_proc;
        if(!ChooseColorW(&cc)) {
            return E_ABORT;
        }
        color = cc.rgbResult | alpha;
        return S_OK;
    }
}