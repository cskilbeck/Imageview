#include "pch.h"

LOG_CONTEXT("recent_files");

//////////////////////////////////////////////////////////////////////

namespace
{
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

    HANDLE recent_files_event;
    std::mutex recent_files_mutex;
    std::set<recent_file_t> all_files;
}

namespace imageview::recent_files
{
    //////////////////////////////////////////////////////////////////////

    void init()
    {
        recent_files_event = CreateEventW(null, true, false, null);

        if(recent_files_event == null) {
            fatal_error(std::format(L"Can't create event: {}", windows_error_message(GetLastError())));
        }

        std::thread([]() {

            (void)CoInitialize(null);

            PWSTR recent_file_path;

            SHGetKnownFolderPath(FOLDERID_Recent, KF_FLAG_DEFAULT, null, &recent_file_path);
            DEFER(CoTaskMemFree(recent_file_path));

            wchar *wild_path;
            HRESULT hr = PathAllocCombine(recent_file_path, L"*.lnk", PATHCCH_NONE, &wild_path);
            if(FAILED(hr)) {
                LOG_ERROR(L"Can't PathAllocCombine? {}", windows_error_message(hr));
                return;
            }
            DEFER(LocalFree(wild_path));

            auto rflock = std::lock_guard{ recent_files_mutex };

            WIN32_FIND_DATAW find_data;
            HANDLE f = FindFirstFileExW(
                wild_path, FindExInfoBasic, &find_data, FindExSearchNameMatch, null, FIND_FIRST_EX_LARGE_FETCH);

            if(f != INVALID_HANDLE_VALUE) {

                do {

                    // this is a bit hacky
                    // assume that the .lnk filename is {original_filename.ext}.lnk
                    // check the file extension before resolving the shortcut (which is slow)
                    // seems to be ok...

                    std::wstring name = make_lowercase(std::wstring(find_data.cFileName));

                    if(!name.ends_with(L".lnk")) {
                        continue;
                    }

                    name = name.substr(0, name.size() - 4);

                    std::wstring extension;
                    if(FAILED(file::get_extension(name, extension))) {
                        continue;
                    }

                    bool supported;
                    if(FAILED(image::can_load_file_extension(extension, supported)) || !supported) {
                        continue;
                    }

                    // we can _probably_ load the file that this shortcut points at, but check for sure

                    std::wstring path = std::format(L"{}\\{}", recent_file_path, find_data.cFileName);

                    std::wstring full_path;
                    if(FAILED(file::path_from_shortcut(path, full_path)) && file::exists(full_path)) {
                        continue;
                    }

                    if(FAILED(file::get_extension(full_path, extension))) {
                        continue;
                    }

                    if(FAILED(image::can_load_file_extension(extension, supported)) && supported) {
                        continue;
                    }

                    // can definitely load it, add it to the list

                    FILETIME create_time;
                    FILETIME access_time;
                    FILETIME write_time;

                    if(FAILED(file::get_time(full_path, create_time, access_time, write_time))) {
                        continue;
                    }

                    all_files.emplace(full_path, access_time);

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
            SetEvent(recent_files_event);
        })
            .detach();
    }

    //////////////////////////////////////////////////////////////////////

    void wait_for_recent_files()
    {
        WaitForSingleObject(recent_files_event, INFINITE);
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_files(std::vector<std::wstring> &filenames)
    {
        if(WaitForSingleObject(recent_files_event, 1) != WAIT_OBJECT_0) {
            return E_NOT_SET;
        }

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