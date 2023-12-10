#include "pch.h"

LOG_CONTEXT("recent_files");

//////////////////////////////////////////////////////////////////////

namespace
{
    HRESULT path_from_shortcut(std::wstring const &shortcut_filename, std::wstring &path)
    {
        ComPtr<IShellLinkW> shellLink;
        CHK_HR(CoCreateInstance(CLSID_ShellLink,
                                NULL,
                                CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW,
                                reinterpret_cast<LPVOID *>(shellLink.GetAddressOf())));
        DEFER(shellLink->Release());

        ComPtr<IPersistFile> persistFile;
        CHK_HR(shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<LPVOID *>(persistFile.GetAddressOf())));
        DEFER(persistFile->Release());

        CHK_HR(persistFile->Load(shortcut_filename.c_str(), STGM_READ));

        WCHAR target_path[MAX_PATH];
        CHK_HR(shellLink->GetPath(target_path, MAX_PATH, 0, SLGP_UNCPRIORITY));

        path = std::wstring(target_path);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    struct recent_file_t
    {
        std::wstring file_path;
        FILETIME file_time;

        bool operator<(recent_file_t const &o) const
        {
            return CompareFileTime(&file_time, &o.file_time) > 0;    // reverse so newest first
        };
    };

    std::mutex recent_files_mutex;
    std::set<recent_file_t> all_files;
}

namespace imageview::recent_files
{
    //////////////////////////////////////////////////////////////////////

    void init()
    {
        std::thread([]() {

            CoInitialize(null);

            PWSTR recent_file_path;

            SHGetKnownFolderPath(FOLDERID_Recent, KF_FLAG_DEFAULT, null, &recent_file_path);
            DEFER(CoTaskMemFree(recent_file_path));

            // TODO (chs): use PathCchCombineEx
            std::wstring wildcard = std::format(L"{}\\*", recent_file_path);

            auto rflock = std::lock_guard{ recent_files_mutex };

            WIN32_FIND_DATAW find_data;
            HANDLE f = FindFirstFileExW(
                wildcard.c_str(), FindExInfoBasic, &find_data, FindExSearchNameMatch, null, FIND_FIRST_EX_LARGE_FETCH);
            if(f != INVALID_HANDLE_VALUE) {
                do {

                    std::wstring full_path;
                    path_from_shortcut(std::format(L"{}\\{}", recent_file_path, find_data.cFileName), full_path);

                    // ho hum, all files have .lnk extension
                    // strip that first

                    std::wstring stripped = make_lowercase(full_path);

                    if(stripped.ends_with(L".lnk")) {
                        stripped = stripped.substr(0, stripped.size() - 4);
                    }

                    std::string extension;
                    file::get_extension(utf8(stripped), extension);

                    bool supported;
                    image::is_file_extension_supported(extension, supported);

                    if(supported) {
                        LOG_DEBUG(L"POINTS AT {}", full_path);
                        all_files.emplace(full_path, find_data.ftLastAccessTime);
                    }

                } while(FindNextFileW(f, &find_data));
            }
            CoUninitialize();
        })
            .detach();
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_files(std::vector<std::wstring> &filenames)
    {
        auto rflock = std::lock_guard{ recent_files_mutex };

        filenames.clear();

        for(auto const &file : all_files) {
            filenames.push_back(file.file_path);
            if(filenames.size() >= 10) {
                break;
            }
        }
        return S_OK;
    }
}