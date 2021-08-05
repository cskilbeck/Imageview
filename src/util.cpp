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
// set cancel_event to cancel the load, it will return ERROR_OPERATION_ABORTED in that case. Can be null
// complete_event is set iff file is loaded without any error. Can be null

HRESULT load_file(std::wstring const &filename, std::vector<byte> &buffer, HANDLE cancel_event, HANDLE complete_event)
{
    // if we error out for any reason, free the buffer
    auto cleanup_buffer = deferred([&] { buffer.clear(); });

    // check args
    if(filename.empty()) {
        return ERROR_BAD_ARGUMENTS;
    }

    defer(if(complete_event != null) { SetEvent(complete_event); });

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

    // error
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

    PIDLIST_ABSOLUTE pidl;
    HRESULT hr = SHGetIDListFromObject(punk, &pidl);
    if(SUCCEEDED(hr)) {
        hr = SHCreateItemFromIDList(pidl, riid, ppv);
        ILFree(pidl);
    } else {
        // perhaps the input is from IE and if so we can construct an item from the URL
        IDataObject *pdo;
        hr = punk->QueryInterface(IID_PPV_ARGS(&pdo));
        if(SUCCEEDED(hr)) {
            static CLIPFORMAT g_cfURL = 0;

            FORMATETC fmte = { GetClipboardFormat(&g_cfURL, CFSTR_SHELLURL), NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
            STGMEDIUM medium;
            hr = pdo->GetData(&fmte, &medium);
            if(SUCCEEDED(hr)) {
                PCSTR pszURL = (PCSTR)GlobalLock(medium.hGlobal);
                if(pszURL) {
                    WCHAR szURL[2048];
                    SHAnsiToUnicode(pszURL, szURL, ARRAYSIZE(szURL));
                    hr = SHCreateItemFromParsingName(szURL, NULL, riid, ppv);
                    GlobalUnlock(medium.hGlobal);
                }
                ReleaseStgMedium(&medium);
            }
            pdo->Release();
        }
    }
    return hr;
}

//////////////////////////////////////////////////////////////////////
// this doesn't do the thing Windows Explorer does where it examines photo metadata and uses the date the picture was taken as the 'date'.
// In this case 'date' means the earliest of created/modified (which, come to think of it, will always be created, so....)

HRESULT scan_folder(wchar_t const *path, std::vector<wchar_t const *> extensions, scan_folder_sort_field sort_field, scan_folder_sort_order order,
                    std::vector<std::wstring> &results)
{
    WIN32_FIND_DATA ffd;

    std::vector<WIN32_FIND_DATA> files;
    std::vector<WIN32_FIND_DATA const *> file_ptrs;

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

                        files.push_back(ffd);
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

    for(auto const &f : files) {
        file_ptrs.push_back(&f);
    }

    std::sort(file_ptrs.begin(), file_ptrs.end(), [=](WIN32_FIND_DATA const *a, WIN32_FIND_DATA const *b) {
        uint64_t fa = 0;
        uint64_t fb = 0;

        WIN32_FIND_DATA const *pa = a;
        WIN32_FIND_DATA const *pb = b;

        if(order == scan_folder_sort_order::descending) {
            pa = b;
            pb = a;
        }

        if(sort_field == scan_folder_sort_field::date) {
            fa = filetime_to_uint64(pa->ftLastWriteTime);
            fb = filetime_to_uint64(pb->ftLastWriteTime);
        }

        int64_t diff = fa - fb;

        if(diff == 0) {
            diff = StrCmpLogicalW(pa->cFileName, pb->cFileName);
        }
        return diff <= 0;
    });

    for(auto f : file_ptrs) {
        results.push_back(f->cFileName);
    }

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
