#include "pch.h"

//////////////////////////////////////////////////////////////////////

namespace
{
    struct path_parts
    {
        wchar drive[MAX_PATH];
        wchar directory[MAX_PATH];
        wchar filename[MAX_PATH];
        wchar extension[MAX_PATH];
    };

    HRESULT get_path_parts(path_parts &p, std::wstring const &filename)
    {
        if(_wsplitpath_s(filename.c_str(), p.drive, p.directory, p.filename, p.extension) != 0) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        return S_OK;
    }
}

namespace imageview::file
{
    //////////////////////////////////////////////////////////////////////
    // loads a file, size is limited to 4GB or less
    // buffer will be cleared in case of any error, on success contains file contents
    // set cancel_event to cancel the load, it will return E_ABORT in that case
    // cancel_event can be null, in which case the load can't be cancelled
    // NOTE: this function suppresses the expected update of LastAccessTime

    HRESULT load(std::wstring const &filename, std::vector<byte> &buffer, HANDLE cancel_event)
    {
        // if we error out for any reason, free the buffer
        auto cleanup_buffer = defer::deferred([&] { buffer.clear(); });

        // check args
        if(filename.empty()) {
            return HRESULT_FROM_WIN32(ERROR_BAD_ARGUMENTS);
        }

        // create an async file handle
        HANDLE file_handle = CreateFileW(filename.c_str(),
                                         GENERIC_READ | GENERIC_WRITE,
                                         FILE_SHARE_READ,
                                         null,
                                         OPEN_EXISTING,
                                         FILE_FLAG_OVERLAPPED,
                                         null);
        if(file_handle == INVALID_HANDLE_VALUE) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        DEFER(CloseHandle(file_handle));

        FILETIME dummy;
        dummy.dwLowDateTime = 0xffffffff;
        dummy.dwHighDateTime = 0xffffffff;
        CHK_BOOL(SetFileTime(file_handle, null, &dummy, null));

        // get the size of the file
        LARGE_INTEGER file_size;
        CHK_BOOL(GetFileSizeEx(file_handle, &file_size));

        // check 4GB file size limit
        if(file_size.HighPart != 0) {
            return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        }

        // make space in the buffer
        buffer.resize(file_size.LowPart);

        // prepare for readfile
        OVERLAPPED overlapped{ 0 };
        overlapped.hEvent = CreateEventW(null, true, false, null);

        if(overlapped.hEvent == null) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        DEFER(CloseHandle(overlapped.hEvent));

        // issue the file read
        if(!ReadFile(file_handle, buffer.data(), file_size.LowPart, null, &overlapped) &&
           GetLastError() != ERROR_IO_PENDING) {

            return HRESULT_FROM_WIN32(GetLastError());
        }

        // wait for the file read to complete or cancel signal
        DWORD bytes_loaded;
        HANDLE handles[2] = { overlapped.hEvent, cancel_event };
        DWORD handle_count = 1;
        if(cancel_event != null) {
            handle_count = 2;
        }

#if SLOW_THINGS_DOWN    // artifically slow down file loading
        DWORD x = WaitForSingleObject(cancel_event, (std::rand() % 2000) + 1000);
        if(x == WAIT_OBJECT_0) {
            CancelIo(file_handle);
            return E_ABORT;
        }
#endif

        switch(WaitForMultipleObjects(handle_count, handles, false, INFINITE)) {

        // io completed, check we got it all
        case WAIT_OBJECT_0:

            CHK_BOOL(GetOverlappedResult(file_handle, &overlapped, &bytes_loaded, false));

            if(bytes_loaded != file_size.LowPart) {

                // didn't get it all, not sure how to find out what went wrong? GetLastError isn't it...
                // this shouldn't happen because FILE_SHARE_READ but... who knows?
                return HRESULT_FROM_WIN32(ERROR_IO_INCOMPLETE);
            }
            // don't clean up the buffer, it's full of good stuff now
            cleanup_buffer.cancel();
            return S_OK;

        // cancel requested, cancel the IO and return operation aborted
        case WAIT_OBJECT_0 + 1:
            // don't check for CancelIo error status
            // if it fails, there's nothing we can do about it anyway
            // and we want to return E_ABORT
            CancelIo(file_handle);
            return E_ABORT;

        // this should not be possible
        case WAIT_ABANDONED:
            return E_UNEXPECTED;

        // error in WaitForMultipleObjects
        default:
            return HRESULT_FROM_WIN32(GetLastError());
        }
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT scan_folder(std::wstring const &path,
                        scan_folder_sort_field sort_field,
                        scan_folder_sort_order order,
                        folder_scan_result **result,
                        HANDLE cancel_event)
    {
        if(result == null) {
            return HRESULT_FROM_WIN32(ERROR_BAD_ARGUMENTS);
        }

        HANDLE dir_handle = CreateFileW(
            path.c_str(), GENERIC_READ, FILE_SHARE_READ, null, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, null);

        if(dir_handle == INVALID_HANDLE_VALUE) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        DEFER(CloseHandle(dir_handle));

        BY_HANDLE_FILE_INFORMATION main_dir_info;
        CHK_NULL(GetFileInformationByHandle(dir_handle, &main_dir_info));

        if((main_dir_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            return HRESULT_FROM_WIN32(ERROR_BAD_PATHNAME);
        }

        FILE_INFO_BY_HANDLE_CLASS info_class = FileIdBothDirectoryRestartInfo;

        // 64k for file info buffer should hold at least a few entries
        std::vector<byte> dir_info(65536);

        auto r = new folder_scan_result();

        CHK_HR(file::get_full_path(path, r->path));

        std::vector<file::info> &files = r->files;

        while(true) {

            if(!GetFileInformationByHandleEx(dir_handle, info_class, dir_info.data(), (DWORD)dir_info.size())) {

                DWORD err = GetLastError();

                if(err == ERROR_MORE_DATA) {

                    dir_info.resize(dir_info.size() * 2);
                    continue;
                }

                if(err == ERROR_NO_MORE_FILES) {
                    break;
                }
                return HRESULT_FROM_WIN32(err);
            }

            info_class = FileIdBothDirectoryInfo;

            PFILE_ID_BOTH_DIR_INFO f = reinterpret_cast<PFILE_ID_BOTH_DIR_INFO>(dir_info.data());

            while(true) {

#if SLOW_THINGS_DOWN    // artifically slow down folder scanning
                DWORD x = WaitForSingleObject(cancel_event, (std::rand() % 200) + 200);
                if(x == WAIT_OBJECT_0) {
                    return E_ABORT;
                }
#endif
                // scan folder can be cancelled
                if(WaitForSingleObject(cancel_event, 0) == WAIT_OBJECT_0) {
                    return E_ABORT;
                }

                // if it's a normal file

                if((f->FileAttributes & (FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_VIRTUAL |
                                         FILE_ATTRIBUTE_OFFLINE)) == 0) {

                    std::wstring filename = std::wstring(f->FileName, f->FileNameLength / sizeof(wchar));

                    std::wstring extension;
                    CHK_HR(get_extension(filename, extension));

                    bool supported;
                    CHK_HR(image::can_load_file_extension(extension, supported));
                    if(supported) {

                        // it's something we can load, add it to the vector of files
                        files.emplace_back(filename, f->LastWriteTime.QuadPart);
                    }
                }

                // break out if there are no more in this block
                if(f->NextEntryOffset == 0) {
                    break;
                }

                // skip to next entry in this block
                f = reinterpret_cast<PFILE_ID_BOTH_DIR_INFO>(reinterpret_cast<byte *>(f) + f->NextEntryOffset);
            }
        }

        // sort the files according to the order/field parameters
        std::sort(files.begin(), files.end(), [=](file::info const &a, file::info const &b) {

            // either ascending
            file::info const *pa = &a;
            file::info const *pb = &b;

            // or descending
            if(order == scan_folder_sort_order::descending) {
                pa = &b;
                pb = &a;
            }

            // by name
            uint64 fa = 0;
            uint64 fb = 0;

            // or date
            if(sort_field == scan_folder_sort_field::date) {
                fa = pa->date;
                fb = pb->date;
            }

            int64_t diff = fa - fb;

            // if date the same (or name ordering), compare names
            if(diff == 0) {
                diff = StrCmpLogicalW(pa->name.c_str(), pb->name.c_str());
            }
            return diff <= 0;
        });

        *result = r;

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    BOOL exists(std::wstring const &name)
    {
        DWORD x = GetFileAttributesW(name.c_str());
        DWORD const not_file = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_OFFLINE;
        return x != INVALID_FILE_ATTRIBUTES && ((x & not_file) == 0);
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_path(std::wstring const &filename, std::wstring &path)
    {
        path_parts p;
        CHK_HR(get_path_parts(p, filename));
        path = std::wstring(p.drive) + p.directory;
        if(path.empty()) {
            path = L".";
        } else if(path.back() == '\\') {
            path.pop_back();
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_barename(std::wstring const &path, std::wstring &name)
    {
        path_parts p;
        CHK_HR(get_path_parts(p, path));
        name = std::wstring(p.filename);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_filename(std::wstring const &path, std::wstring &filename)
    {
        path_parts p;
        CHK_HR(get_path_parts(p, path));
        filename = std::wstring(p.filename) + p.extension;
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_extension(std::wstring const &filename, std::wstring &extension)
    {
        path_parts p;
        CHK_HR(get_path_parts(p, filename));
        extension = std::wstring(p.extension);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_full_path(std::wstring const &filename, std::wstring &fullpath)
    {
        wchar dummy;
        uint size = GetFullPathNameW(filename.c_str(), 1, &dummy, null);
        if(size == 0) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        fullpath.resize(size);
        size = GetFullPathNameW(filename.c_str(), size, &fullpath[0], null);
        if(size == 0) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        fullpath.pop_back();    // remove null-termination
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_size(std::wstring const &filename, uint64_t &size)
    {
        WIN32_FILE_ATTRIBUTE_DATA attr;
        CHK_BOOL(GetFileAttributesExW(filename.c_str(), GetFileExInfoStandard, &attr));
        size = (static_cast<uint64>(attr.nFileSizeHigh) << 32) | attr.nFileSizeLow;
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT set_access_time(std::wstring const &filename, FILETIME const &time)
    {
        if(filename.empty()) {
            return HRESULT_FROM_WIN32(ERROR_BAD_ARGUMENTS);
        }

        HANDLE file_handle = CreateFileW(
            filename.c_str(), GENERIC_READ | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, null, OPEN_EXISTING, 0, null);

        if(file_handle == INVALID_HANDLE_VALUE) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        DEFER(CloseHandle(file_handle));

        CHK_BOOL(SetFileTime(file_handle, null, &time, null));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_time(std::wstring const &filename, FILETIME &create, FILETIME &access, FILETIME &write)
    {
        WIN32_FILE_ATTRIBUTE_DATA attr;
        CHK_BOOL(GetFileAttributesExW(filename.c_str(), GetFileExInfoStandard, &attr));

        create = attr.ftCreationTime;
        access = attr.ftLastAccessTime;
        write = attr.ftLastWriteTime;

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT path_from_shortcut(std::wstring const &shortcut_filename, std::wstring &path)
    {
        ComPtr<IShellLinkW> shell_link;
        CHK_HR(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shell_link)));

        ComPtr<IPersistFile> persist_file;
        CHK_HR(shell_link.As(&persist_file));

        CHK_HR(persist_file->Load(shortcut_filename.c_str(), STGM_READ));

        LPITEMIDLIST item_id_list;
        CHK_HR(shell_link->GetIDList(&item_id_list));

        DEFER(CoTaskMemFree(item_id_list));

        wchar target_path[MAX_PATH * 4];

        if(!SHGetPathFromIDList(item_id_list, target_path)) {
            return E_FAIL;
        }

        path = std::wstring(target_path);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT paths_are_different(std::wstring const &a, std::wstring const &b, bool &differ)
    {
        PWSTR pa;
        CHK_HR(PathAllocCanonicalize(a.c_str(), PATHCCH_ALLOW_LONG_PATHS, &pa));
        DEFER(LocalFree(pa));

        PWSTR pb;
        CHK_HR(PathAllocCanonicalize(b.c_str(), PATHCCH_ALLOW_LONG_PATHS, &pb));
        DEFER(LocalFree(pb));

        size_t la = wcslen(pa);
        size_t lb = wcslen(pb);

        differ = (la != lb) || _wcsicmp(pa, pb) != 0;

        return S_OK;
    }
}
