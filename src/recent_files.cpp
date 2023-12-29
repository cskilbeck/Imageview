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

        // silently fail because many permissions failures are not interesting
        if(FAILED(persistFile->Load(shortcut_filename.c_str(), STGM_READ))) {
            return S_FALSE;
        }

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

            (void)CoInitialize(null);

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

                    std::wstring path = std::format(L"{}\\{}", recent_file_path, find_data.cFileName);

                    std::wstring full_path;

                    if(path_from_shortcut(path, full_path) == S_OK) {

                        std::wstring extension;
                        file::get_extension(full_path, extension);

                        bool supported;
                        image::is_file_extension_supported(extension, supported);

                        if(supported) {
                            FILETIME create_time;
                            FILETIME access_time;
                            FILETIME write_time;
                            if(SUCCEEDED(file::get_time(full_path, create_time, access_time, write_time))) {
                                all_files.emplace(full_path, access_time);
                            }
                        }
                    }

                } while(FindNextFileW(f, &find_data));

                for(auto const &ff : all_files) {
                    SYSTEMTIME system_time;
                    FileTimeToSystemTime(&ff.file_time, &system_time);
                    LOG_DEBUG(L"POINTS AT {}, last accessed {}/{}/{}",
                              ff.file_path,
                              system_time.wDay,
                              system_time.wMonth,
                              system_time.wYear);
                }
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
            if(filenames.size() == settings.recent_files_count) {
                break;
            }
        }
        return S_OK;
    }
}