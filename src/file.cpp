#include "pch.h"

//////////////////////////////////////////////////////////////////////
// loads a file, size is limited to 4GB or less
// buffer will be cleared in case of any error, on success contains file contents
// set cancel_event to cancel the load, it will return E_ABORT in that case
// cancel_event can be null, in which case the load can't be cancelled

//////////////////////////////////////////////////////////////////////

namespace
{
    struct path_parts
    {
        char drive[MAX_PATH];
        char dir[MAX_PATH];
        char fname[MAX_PATH];
        char ext[MAX_PATH];

        HRESULT get(std::string const &filename)
        {
            if(_splitpath_s(filename.c_str(), drive, dir, fname, ext) != 0) {
                return HRESULT_FROM_WIN32(GetLastError());
            }
            return S_OK;
        }
    };
}

namespace imageview::file
{
    HRESULT load(std::string const &filename, std::vector<byte> &buffer, HANDLE cancel_event)
    {
        // if we error out for any reason, free the buffer
        auto cleanup_buffer = defer::deferred([&] { buffer.clear(); });

        // check args
        if(filename.empty()) {
            return HRESULT_FROM_WIN32(ERROR_BAD_ARGUMENTS);
        }

        // create an async file handle
        HANDLE file_handle = CreateFileA(
            filename.c_str(), GENERIC_READ, FILE_SHARE_READ, null, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, null);
        if(file_handle == INVALID_HANDLE_VALUE) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        DEFER(CloseHandle(file_handle));

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
        overlapped.hEvent = CreateEvent(null, true, false, null);

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

    HRESULT scan_folder(std::string const &path,
                        std::vector<std::string> const &extensions,
                        scan_folder_sort_field sort_field,
                        scan_folder_sort_order order,
                        folder_scan_result **result,
                        HANDLE cancel_event)
    {
        if(result == null) {
            return HRESULT_FROM_WIN32(ERROR_BAD_ARGUMENTS);
        }

        HANDLE dir_handle = CreateFileA(
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

#if SLOW_THINGS_DOWN    // artifically slow down file loading
                DWORD x = WaitForSingleObject(cancel_event, (std::rand() % 200) + 200);
                if(x == WAIT_OBJECT_0) {
                    return E_ABORT;
                }
#endif
                // scan folder can be cancelled
                if(WaitForSingleObject(cancel_event, 0) == WAIT_OBJECT_0) {
                    return E_ABORT;
                }

                // if it's a file
                uint32 ignore = FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_VIRTUAL;
                if((f->FileAttributes & ignore) == 0) {

                    std::string filename = utf8(f->FileName, f->FileNameLength / sizeof(wchar));

                    size_t dot = filename.find_last_of('.');

                    if(dot != std::string::npos && dot < filename.size()) {

                        // check if extension is in the list
                        for(auto const &ext : extensions) {

                            size_t ext_len = ext.size();
                            char const *find_ext = ext.c_str();
                            size_t ext_dot = ext.find('.');
                            if(ext_dot != std::string::npos) {
                                find_ext += 1;
                                ext_len -= 1;
                            }

                            size_t max_len = std::min(filename.size() - dot, ext_len);

                            if(_strnicmp(&filename[dot] + 1, find_ext, max_len) == 0) {

                                // it's in the list, add it to the vector of files
                                files.emplace_back(filename, f->LastWriteTime.QuadPart);
                                break;
                            }
                        }
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
                diff = StrCmpLogicalW(unicode(pa->name).c_str(), unicode(pb->name).c_str());
            }
            return diff <= 0;
        });

        *result = r;

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    BOOL exists(std::string const &name)
    {
        DWORD x = GetFileAttributesA(name.c_str());
        DWORD const not_file = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_OFFLINE;
        return x != INVALID_FILE_ATTRIBUTES && ((x & not_file) == 0);
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_path(std::string const &filename, std::string &path)
    {
        path_parts p;
        CHK_HR(p.get(filename));
        path = std::string(p.drive) + p.dir;
        if(path.empty()) {
            path = ".";
        } else if(path.back() == '\\') {
            path.pop_back();
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_filename(std::string const &filename, std::string &name)
    {
        path_parts p;
        CHK_HR(p.get(filename));
        name = std::string(p.fname) + p.ext;
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_extension(std::string const &filename, std::string &extension)
    {
        path_parts p;
        CHK_HR(p.get(filename));
        extension = std::string(p.ext);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_full_path(std::string const &filename, std::string &fullpath)
    {
        char dummy;
        uint size = GetFullPathNameA(filename.c_str(), 1, &dummy, null);
        if(size == 0) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        fullpath.resize(size);
        size = GetFullPathNameA(filename.c_str(), size, &fullpath[0], null);
        if(size == 0) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        fullpath.pop_back();    // remove null-termination
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_size(std::string const &filename, uint64_t &size)
    {
        HANDLE f = CreateFileA(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, null, OPEN_EXISTING, 0, null);
        if(f == INVALID_HANDLE_VALUE) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        DEFER(CloseHandle(f));
        LARGE_INTEGER file_size;
        CHK_BOOL(GetFileSizeEx(f, &file_size));
        size = file_size.QuadPart;
        return S_OK;
    }
}