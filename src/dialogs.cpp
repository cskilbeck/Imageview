//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

namespace
{
    // get file type filter list from save_formats in image_decoder.cpp

    struct filterspec
    {
        std::wstring description;
        std::wstring filter;
    };

    struct GUIDComparer
    {
        bool operator()(const GUID &Left, const GUID &Right) const
        {
            return memcmp(&Left, &Right, sizeof(Right)) < 0;
        }
    };

    std::map<GUID, filterspec, GUIDComparer> slots;
    std::vector<COMDLG_FILTERSPEC> filter_specs;
    uint num_filter_specs;
    uint default_filter_spec;

    //////////////////////////////////////////////////////////////////////

    HRESULT get_filter_specs()
    {
        if(filter_specs.empty()) {

            uint current_filter_spec = 1;

            // file type guid can be referenced by more than one extension (eg jpg, jpeg both point at same guid)

            for(auto const &fmt : image_file_formats) {

                std::wstring const &extension = fmt.first;
                output_image_format const &image_format = fmt.second;

                auto found = slots.find(image_format.file_format);

                if(image_format.is_default()) {
                    default_filter_spec = current_filter_spec;
                }

                current_filter_spec += 1;

                if(found == slots.end()) {

                    slots[image_format.file_format] = { std::format(L"{} files", extension),
                                                        std::format(L"*.{}", extension) };

                } else {

                    filterspec &spec = found->second;
                    spec.filter = std::format(L"{};*.{}", spec.filter, extension);

                    if(image_format.use_name()) {
                        spec.description = std::format(L"{} files", extension);
                    }
                }
            }

            filter_specs.clear();

            for(auto &spec : slots) {
                make_lowercase(spec.second.filter);
                filter_specs.push_back({ spec.second.description.c_str(), spec.second.filter.c_str() });
            }
        }
        num_filter_specs = static_cast<uint>(filter_specs.size());
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    UINT_PTR CALLBACK select_color_dialog_hook_proc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
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
}

//////////////////////////////////////////////////////////////////////

HRESULT select_file_dialog(HWND window, std::wstring &path)
{
    CHK_HR(get_filter_specs());

    ComPtr<IFileDialog> pfd;
    DWORD dwFlags;
    ComPtr<IShellItem> psiResult;
    PWSTR pszFilePath{ null };

    CHK_HR(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)));
    CHK_HR(pfd->GetOptions(&dwFlags));
    CHK_HR(pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_OKBUTTONNEEDSINTERACTION));
    CHK_HR(pfd->SetFileTypes(num_filter_specs, filter_specs.data()));
    CHK_HR(pfd->SetFileTypeIndex(default_filter_spec));
    CHK_HR(pfd->SetOkButtonLabel(L"View"));
    CHK_HR(pfd->SetTitle(std::format(L"{}{}{}", localize(IDS_AppName), L" : ", localize(IDS_SelectFile)).c_str()));
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
    CHK_HR(get_filter_specs());

    ComPtr<IFileDialog> pfd;
    DWORD dwFlags;
    ComPtr<IShellItem> psiResult;
    PWSTR pszFilePath{ null };

    auto options = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OKBUTTONNEEDSINTERACTION | FOS_OVERWRITEPROMPT |
                   FOS_STRICTFILETYPES;

    CHK_HR(CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)));
    CHK_HR(pfd->GetOptions(&dwFlags));
    CHK_HR(pfd->SetOptions(dwFlags | options));
    CHK_HR(pfd->SetFileTypes(num_filter_specs, filter_specs.data()));
    CHK_HR(pfd->SetFileTypeIndex(default_filter_spec));
    CHK_HR(pfd->SetTitle(std::format(L"{}{}{}", localize(IDS_AppName), L" : ", localize(IDS_SelectFile)).c_str()));
    CHK_HR(pfd->Show(window));
    CHK_HR(pfd->GetResult(&psiResult));
    CHK_HR(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));

    path = pszFilePath;
    CoTaskMemFree(pszFilePath);

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

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
