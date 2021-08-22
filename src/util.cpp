//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

HRESULT load_resource(DWORD id, wchar_t const *type, void **buffer, size_t *size)
{
    if(buffer == null || size == null || type == null || id == 0) {
        return ERROR_BAD_ARGUMENTS;
    }

    HINSTANCE instance = GetModuleHandle(null);

    HRSRC rsrc = FindResource(instance, MAKEINTRESOURCE(id), type);
    if(rsrc == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    size_t len = SizeofResource(instance, rsrc);
    if(len == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    HGLOBAL mem = LoadResource(instance, rsrc);
    if(mem == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    void *data = LockResource(mem);
    if(data == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    *buffer = data;
    *size = len;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// append helps because then we can prepend a BITMAPFILEHEADER
// when we're loading the clipboard and pretend it's a file

HRESULT append_clipboard_to_buffer(std::vector<byte> &buffer, UINT format)
{
    CHK_BOOL(OpenClipboard(null));

    defer(CloseClipboard());

    HANDLE c = GetClipboardData(format);
    if(c == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    void *data = GlobalLock(reinterpret_cast<HGLOBAL>(c));
    if(data == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    defer(GlobalUnlock(c));

    size_t size = GlobalSize(reinterpret_cast<HGLOBAL>(c));
    if(size == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    size_t existing = buffer.size();
    buffer.resize(size + existing);
    memcpy(buffer.data() + existing, data, size);

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// loads a file, size is limited to 4GB or less
// buffer will be cleared in case of any error, on success contains file contents
// set cancel_event to cancel the load, it will return ERROR_OPERATION_ABORTED in that case
// cancel_event can be null, in which case the load can't be cancelled

HRESULT load_file(std::wstring const &filename, std::vector<byte> &buffer, HANDLE cancel_event)
{
    // if we error out for any reason, free the buffer
    auto cleanup_buffer = deferred([&] { buffer.clear(); });

    // check args
    if(filename.empty()) {
        return ERROR_BAD_ARGUMENTS;
    }

    // create an async file handle
    HANDLE file_handle = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, null, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, null);
    if(file_handle == INVALID_HANDLE_VALUE) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    defer(CloseHandle(file_handle));

    // get the size of the file
    LARGE_INTEGER file_size;
    CHK_BOOL(GetFileSizeEx(file_handle, &file_size));

    // check 4GB file size limit
    if(file_size.HighPart != 0) {
        return ERROR_FILE_TOO_LARGE;
    }

    // make space in the buffer
    buffer.resize(file_size.LowPart);

    // prepare for readfile
    OVERLAPPED overlapped{ 0 };
    overlapped.hEvent = CreateEvent(null, true, false, null);

    if(overlapped.hEvent == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    defer(CloseHandle(overlapped.hEvent));

    // issue the file read
    if(!ReadFile(file_handle, buffer.data(), file_size.LowPart, null, &overlapped) && GetLastError() != ERROR_IO_PENDING) {

        return HRESULT_FROM_WIN32(GetLastError());
    }

    // wait for the file read to complete or cancel signal
    DWORD bytes_loaded;
    HANDLE handles[2] = { overlapped.hEvent, cancel_event };
    DWORD handle_count = 1;
    if(cancel_event != null) {
        handle_count = 2;
    }

    switch(WaitForMultipleObjects(handle_count, handles, false, INFINITE)) {

    // io completed, check we got it all
    case WAIT_OBJECT_0:

        CHK_BOOL(GetOverlappedResult(file_handle, &overlapped, &bytes_loaded, false));

        if(bytes_loaded != file_size.LowPart) {

            // didn't get it all, not sure how to find out what went wrong? GetLastError isn't it...
            // this shouldn't happen because FILE_SHARE_READ but... who knows?
            return ERROR_IO_INCOMPLETE;
        }
        // don't clean up the buffer, it's full of good stuff now
        cleanup_buffer.cancel();
        return S_OK;

    // cancel requested, cancel the IO and return operation aborted
    case WAIT_OBJECT_0 + 1:
        // don't check for CancelIo error status
        // if it fails, there's nothing we can do about it anyway
        // and we want to return ERROR_OPERATION_ABORTED
        CancelIo(file_handle);
        return ERROR_OPERATION_ABORTED;

    // this should not be possible
    case WAIT_ABANDONED:
        return E_UNEXPECTED;

    // error in WaitForMultipleObjects
    default:
        return HRESULT_FROM_WIN32(GetLastError());
    }
}

//////////////////////////////////////////////////////////////////////

RECT center_rect_on_default_monitor(RECT const &r)
{
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = r.right - r.left;
    int wh = r.bottom - r.top;
    RECT rc;
    rc.left = (sw - ww) / 2;
    rc.top = (sh - wh) / 2;
    rc.right = rc.left + ww;
    rc.bottom = rc.top + wh;
    return rc;
}

//////////////////////////////////////////////////////////////////////

HRESULT load_bitmap(wchar_t const *filename, IWICBitmapFrameDecode **decoder)
{
    if(decoder == null) {
        return ERROR_BAD_ARGUMENTS;
    }

    ComPtr<IWICImagingFactory> wic_factory;
    ComPtr<IWICBitmapDecoder> wic_bmp_decoder;
    ComPtr<IWICFormatConverter> wic_format_converter;
    ComPtr<IWICBitmapFrameDecode> wic_bmp_frame_decoder;

    CHK_HR(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_factory)));
    CHK_HR(wic_factory->CreateDecoderFromFilename(filename, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &wic_bmp_decoder));
    CHK_HR(wic_factory->CreateFormatConverter(&wic_format_converter));
    CHK_HR(wic_bmp_decoder->GetFrame(0, &wic_bmp_frame_decoder));
    CHK_HR(wic_format_converter->Initialize(wic_bmp_frame_decoder.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeMedianCut));
    // HR(dc->CreateBitmapFromWicBitmap(d2dConverter, NULL, &d2dBmp));
    *decoder = wic_bmp_frame_decoder.Detach();
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// helper to convert a data object with HIDA format or folder into a shell item
// note: if the data object contains more than one item this function will fail
// if you want to operate on the full selection use SHCreateShellItemArrayFromDataObject

HRESULT create_shell_item_from_object(IUnknown *punk, REFIID riid, void **ppv)
{
    *ppv = NULL;

    // try the basic default first
    PIDLIST_ABSOLUTE pidl;
    if(SUCCEEDED(SHGetIDListFromObject(punk, &pidl))) {
        defer(ILFree(pidl));
        CHK_HR(SHCreateItemFromIDList(pidl, riid, ppv));
        return S_OK;
    }

    // perhaps the input is from IE and if so we can construct an item from the URL
    IDataObject *pdo;
    CHK_HR(punk->QueryInterface(IID_PPV_ARGS(&pdo)));
    defer(pdo->Release());

    CLIPFORMAT g_cfURL = 0;
    FORMATETC fmte = { GetClipboardFormat(&g_cfURL, CFSTR_SHELLURL), NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium;
    CHK_HR(pdo->GetData(&fmte, &medium));
    scoped([&]() { ReleaseStgMedium(&medium); });

    PCSTR pszURL = (PCSTR)GlobalLock(medium.hGlobal);
    if(pszURL == null) {
        return ERROR_CANTREAD;
    }
    defer(GlobalUnlock(medium.hGlobal));

    WCHAR szURL[2048];
    if(SHAnsiToUnicode(pszURL, szURL, ARRAYSIZE(szURL)) == 0) {
        return ERROR_ILLEGAL_CHARACTER;
    }

    CHK_HR(SHCreateItemFromParsingName(szURL, NULL, riid, ppv));
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT get_file_id(wchar_t const *filename, uint32_t *volume_id, uint64_t *id)
{
    if(volume_id == null || id == null || filename == null || filename[0] == 0) {
        return ERROR_BAD_ARGUMENTS;
    }
    HANDLE file_handle = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, null, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, null);
    if(file_handle == INVALID_HANDLE_VALUE) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    defer(CloseHandle(file_handle));
    BY_HANDLE_FILE_INFORMATION info;
    CHK_BOOL(GetFileInformationByHandle(file_handle, &info));
    *id = ((uint64_t)info.nFileIndexHigh << 32) | info.nFileIndexLow;
    *volume_id = info.dwVolumeSerialNumber;
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT scan_folder2(wchar_t const *path, std::vector<wchar_t const *> extensions, scan_folder_sort_field sort_field, scan_folder_sort_order order, folder_scan_result **result,
                     HANDLE cancel_event)
{
    if(result == null) {
        return ERROR_BAD_ARGUMENTS;
    }

    HANDLE dir_handle = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, null, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, null);

    if(dir_handle == INVALID_HANDLE_VALUE) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    defer(CloseHandle(dir_handle));

    BY_HANDLE_FILE_INFORMATION main_dir_info;
    CHK_NULL(GetFileInformationByHandle(dir_handle, &main_dir_info));

    if((main_dir_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return ERROR_BAD_PATHNAME;
    }

    FILE_INFO_BY_HANDLE_CLASS info_class = FileIdBothDirectoryRestartInfo;

    // 64k for file info buffer should hold at least a few entries
    std::vector<byte> dir_info(65536);

    auto r = new folder_scan_result();

    CHK_HR(file_get_full_path(path, r->path));

    std::vector<file_info> &files = r->files;

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

            // scan folder can be cancelled
            if(WaitForSingleObject(cancel_event, 0) == WAIT_OBJECT_0) {
                return ERROR_OPERATION_ABORTED;
            }

            // if it's a file
            uint32_t ignore = FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_VIRTUAL;
            if((f->FileAttributes & ignore) == 0) {
                // find the extension
                size_t namelen = f->FileNameLength / sizeof(wchar_t);
                for(wchar_t const *i = f->FileName + namelen; i > f->FileName; --i) {
                    if(*i == '.') {
                        i += 1;
                        size_t ext_len = namelen - (i - f->FileName);

                        // check if extension is in the list
                        for(wchar_t const *ext : extensions) {

                            // extensions can be specified with or without a leading dot
                            wchar_t const *find_ext = ext;
                            if(*find_ext == '.') {
                                find_ext += 1;
                            }

                            if(_wcsnicmp(i, find_ext, ext_len) == 0) {

                                // it's in the list, add it to the vector of files
                                files.emplace_back(std::wstring(f->FileName, f->FileName + namelen), f->LastWriteTime.QuadPart);
                                break;
                            }
                        }
                        break;
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
    std::sort(files.begin(), files.end(), [=](file_info const &a, file_info const &b) {

        // either ascending
        file_info const *pa = &a;
        file_info const *pb = &b;

        // or descending
        if(order == scan_folder_sort_order::descending) {
            pa = &b;
            pb = &a;
        }

        // by name
        uint64_t fa = 0;
        uint64_t fb = 0;

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
// this doesn't do the thing Windows Explorer does where it examines photo metadata and uses the date the picture was taken as the 'date'.
// In this case 'date' means the earliest of created/modified (which, come to think of it, will always be created, so....)

HRESULT scan_folder(wchar_t const *path, std::vector<wchar_t const *> extensions, scan_folder_sort_field sort_field, scan_folder_sort_order order, std::vector<file_info> &files)
{
    WIN32_FIND_DATA ffd;

    auto filetime_to_uint64 = [](FILETIME const &f) { return f.dwLowDateTime + ((uint64_t)f.dwHighDateTime << 32); };

    std::wstring str_path(path);
    if(!str_path.empty() && str_path.back() != '\\') {
        str_path.append(L"\\*");
    } else {
        str_path.append(L"*");
    }

    HANDLE hFind = FindFirstFile(str_path.c_str(), &ffd);

    if(hFind == INVALID_HANDLE_VALUE) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    defer(FindClose(hFind));

    do
        if((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {

            for(wchar_t const *ext : extensions) {

                wchar_t const *find_ext = ext;
                if(*find_ext == '.') {
                    find_ext += 1;
                }

                LPWSTR e = PathFindExtension(ffd.cFileName);
                if(e != null && *e == '.') {
                    e += 1;
                    if(_wcsicmp(e, find_ext) == 0) {

                        uint64_t last_write = (uint64_t)ffd.ftLastWriteTime.dwHighDateTime << 32 | ffd.ftLastWriteTime.dwLowDateTime;
                        files.emplace_back(std::wstring(ffd.cFileName), last_write);
                        break;
                    }
                }
            }
        }
    while(FindNextFile(hFind, &ffd) != 0);

    DWORD err = GetLastError();
    if(err != ERROR_NO_MORE_FILES) {
        return HRESULT_FROM_WIN32(err);
    }

    // sort the files according to the order/field parameters
    std::sort(files.begin(), files.end(), [=](file_info const &a, file_info const &b) {

        // either ascending
        file_info const *pa = &a;
        file_info const *pb = &b;

        // or descending
        if(order == scan_folder_sort_order::descending) {
            pa = &b;
            pb = &a;
        }

        // by name
        uint64_t fa = 0;
        uint64_t fb = 0;

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

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT select_file_dialog(std::wstring &path)
{
    COMDLG_FILTERSPEC file_types[] = { { L"All files", L"*.*" }, { L"Image files", L"*.jpg;*.png;*.bmp" } };

    ComPtr<IFileDialog> pfd;
    DWORD dwFlags;
    ComPtr<IShellItem> psiResult;
    PWSTR pszFilePath{ null };

    CHK_HR(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)));
    CHK_HR(pfd->GetOptions(&dwFlags));
    CHK_HR(pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_OKBUTTONNEEDSINTERACTION));
    CHK_HR(pfd->SetFileTypes((uint)std::size(file_types), file_types));
    CHK_HR(pfd->SetFileTypeIndex(2));
    CHK_HR(pfd->SetOkButtonLabel(L"View"));
    CHK_HR(pfd->SetTitle(format(L"%s%s%s", localize(IDS_AppName), L" : ", localize(IDS_SelectFile)).c_str()));
    CHK_HR(pfd->Show(null));
    CHK_HR(pfd->GetResult(&psiResult));
    CHK_HR(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));

    path = pszFilePath;
    CoTaskMemFree(pszFilePath);

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// get a localized string by id

std::wstring const &str_local(uint id)
{
    static std::unordered_map<uint, std::wstring> localized_strings;
    static std::wstring const unknown{ L"?" };

    auto f = localized_strings.find(id);

    if(f != localized_strings.end()) {
        return f->second;
    }

    // SetThreadUILanguage(MAKELCID(LANG_FRENCH, SUBLANG_NEUTRAL));

    wchar_t *str;
    int len = LoadString(GetModuleHandle(null), id, reinterpret_cast<wchar_t *>(&str), 0);

    if(len == 0) {
        return unknown;
    }
    return localized_strings.insert({ id, std::wstring(str, len) }).first->second;
}

//////////////////////////////////////////////////////////////////////
// get a null-terminated wchar_t pointer to the localized string

wchar_t const *localize(uint id)
{
    return str_local(id).c_str();
}

//////////////////////////////////////////////////////////////////////

namespace
{
    struct path_parts
    {
        wchar_t drive[MAX_PATH];
        wchar_t dir[MAX_PATH];
        wchar_t fname[MAX_PATH];
        wchar_t ext[MAX_PATH];

        HRESULT get(wchar_t const *filename)
        {
            if(_wsplitpath_s(filename, drive, dir, fname, ext) != 0) {
                return HRESULT_FROM_WIN32(GetLastError());
            }
            return S_OK;
        }
    };
}

HRESULT file_get_path(wchar_t const *filename, std::wstring &path)
{
    path_parts p;
    CHK_HR(p.get(filename));
    path = std::wstring(p.drive) + p.dir;
    if(path.empty()) {
        path = L".";
    } else if(path.back() == L'\\') {
        path.pop_back();
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT file_get_filename(wchar_t const *filename, std::wstring &name)
{
    path_parts p;
    CHK_HR(p.get(filename));
    name = std::wstring(p.fname) + p.ext;
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT file_get_extension(wchar_t const *filename, std::wstring &extension)
{
    path_parts p;
    CHK_HR(p.get(filename));
    extension = std::wstring(p.ext);
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT file_get_full_path(wchar_t const *filename, std::wstring &fullpath)
{
    wchar_t tiny[1];
    uint size = GetFullPathName(filename, 1, tiny, null);
    if(size == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    fullpath.resize(size);
    size = GetFullPathName(filename, size, &fullpath[0], null);
    if(size == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    return S_OK;
}
