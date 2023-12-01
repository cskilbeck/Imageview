//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

namespace
{
    bool heif_support_checked = false;

    // get file type filter list from save_formats in image_decoder.cpp

    struct filterspec
    {
        std::wstring description;
        std::wstring filter;
        bool is_default;
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

    uint default_filter_spec{ 1 };

    filterspec all_image_files{ L"Image files", {}, {} };

    using namespace imageview;

    //////////////////////////////////////////////////////////////////////
    // make the array of COMDLG_FILTERSPEC from image::file_formats

    HRESULT get_filter_specs()
    {
        if(!heif_support_checked) {
            CHK_HR(image::check_heif_support());
            heif_support_checked = true;
        }

        if(filter_specs.empty()) {

            // file type guid can be referenced by more than one extension (eg jpg, jpeg both point at same guid)

            wchar const *all_sep = L"";

            for(auto const &fmt : image::image_formats) {

                std::wstring extension = unicode(fmt.first);
                image::image_format const &image_format = fmt.second;

                all_image_files.filter = std::format(L"{}{}*.{}", all_image_files.filter, all_sep, unicode(fmt.first));
                all_sep = L";";

                filterspec &spec = slots[image_format.file_format];

                wchar const *sep = L";";

                if(spec.description.empty()) {
                    sep = L"";
                }

                spec.filter = std::format(L"{}{}*.{}", spec.filter, sep, extension);
                spec.is_default = image_format.is_default();

                if(image_format.use_name() || spec.description.empty()) {
                    spec.description = std::format(L"{} files", extension);
                }
            }

            filter_specs.clear();

            for(auto &spec : slots) {
                make_lowercase(spec.second.filter);
                filter_specs.push_back({ spec.second.description.c_str(), spec.second.filter.c_str() });
            }

            make_lowercase(all_image_files.filter);
            filter_specs.push_back({ all_image_files.description.c_str(), all_image_files.filter.c_str() });
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
                SetWindowTextA(hdlg, reinterpret_cast<char const *>(cc->lCustData));
            }
        }
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////

namespace imageview::dialog
{

    HRESULT open_file(HWND window, std::string &path)
    {
        CHK_HR(get_filter_specs());

        ComPtr<IFileDialog> pfd;
        DWORD dwFlags;
        ComPtr<IShellItem> psiResult;
        PWSTR pszFilePath{ null };

        std::string title = std::format("{}{}{}", localize(IDS_AppName), " : ", localize(IDS_SelectFile));

        CHK_HR(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)));
        CHK_HR(pfd->GetOptions(&dwFlags));
        CHK_HR(pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_OKBUTTONNEEDSINTERACTION));
        CHK_HR(pfd->SetFileTypes(num_filter_specs, filter_specs.data()));
        CHK_HR(pfd->SetFileTypeIndex(num_filter_specs));
        CHK_HR(pfd->SetOkButtonLabel(L"View"));
        CHK_HR(pfd->SetTitle(unicode(title).c_str()));
        CHK_HR(pfd->Show(window));
        CHK_HR(pfd->GetResult(&psiResult));
        CHK_HR(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));

        path = utf8(pszFilePath);
        CoTaskMemFree(pszFilePath);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT save_file(HWND window, std::string &path)
    {
        CHK_HR(get_filter_specs());

        ComPtr<IFileDialog> pfd;
        DWORD dwFlags;
        ComPtr<IShellItem> psiResult;
        PWSTR pszFilePath{ null };

        auto options = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OKBUTTONNEEDSINTERACTION | FOS_OVERWRITEPROMPT |
                       FOS_STRICTFILETYPES;

        std::string title = std::format("{}{}{}", localize(IDS_AppName), " : ", localize(IDS_SelectFile));

        CHK_HR(CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)));
        CHK_HR(pfd->GetOptions(&dwFlags));
        CHK_HR(pfd->SetOptions(dwFlags | options));
        CHK_HR(pfd->SetFileTypes(num_filter_specs, filter_specs.data()));
        CHK_HR(pfd->SetFileTypeIndex(default_filter_spec));
        CHK_HR(pfd->SetTitle(unicode(title).c_str()));
        CHK_HR(pfd->Show(window));
        CHK_HR(pfd->GetResult(&psiResult));
        CHK_HR(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));

        path = utf8(pszFilePath);
        CoTaskMemFree(pszFilePath);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT select_color(HWND window, uint32 &color, char const *title)
    {
        static COLORREF custom_colors[16];

        CHOOSECOLORA cc{ 0 };
        cc.lStructSize = sizeof(cc);
        cc.hwndOwner = window;
        cc.lpCustColors = (LPDWORD)custom_colors;
        cc.lCustData = reinterpret_cast<LPARAM>(title);
        cc.rgbResult = color;
        cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR | CC_ENABLEHOOK;
        cc.lpfnHook = select_color_dialog_hook_proc;
        if(!ChooseColorA(&cc)) {
            return E_ABORT;
        }
        color = cc.rgbResult;
        return S_OK;
    }
}