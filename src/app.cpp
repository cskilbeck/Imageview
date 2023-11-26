//////////////////////////////////////////////////////////////////////
// TO DO
// settings dialog
// customise keyboard shortcuts / help screen
// localization
// installation/file associations
// show message if file load is slow
//////////////////////////////////////////////////////////////////////
// TO FIX
// clean up namespaces
// switch to format {}
// logging with levels
// std::string everywhere
// utf8 everywhere
// the cache
// all the leaks

#include "pch.h"
#include <dcomp.h>

#include "shader_inc/vs_rectangle.h"
#include "shader_inc/ps_drawimage.h"
#include "shader_inc/ps_drawrect.h"
#include "shader_inc/ps_solid.h"
#include "shader_inc/ps_spinner.h"
#include "shader_inc/ps_drawgrid.h"

//////////////////////////////////////////////////////////////////////

namespace
{
    using namespace DirectX;

    wchar const *small_font_family_name{ L"Noto Sans" };
    wchar const *mono_font_family_name{ L"Roboto Mono" };

#if defined(_DEBUG)
    void set_d3d_debug_name(ID3D11DeviceChild *resource, char const *name)
    {
        resource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
    }
#else
    void set_d3d_debug_name(...)
    {
    }
#endif

    template <typename T> void set_d3d_debug_name(ComPtr<T> &resource, char const *name)
    {
        set_d3d_debug_name(resource.Get(), name);
    }

    bool is_key_down(uint key)
    {
        return (GetAsyncKeyState(key) & 0x8000) != 0;
    }
}

#define d3d_name(x) set_d3d_debug_name(x, #x)

LPCWSTR App::window_class = L"ImageViewWindowClass_2DAE134A-7E46-4E75-9DFA-207695F48699";

ComPtr<ID3D11Debug> App::d3d_debug;

// file/image cache admin

std::unordered_map<std::wstring, App::image_file *> App::loaded_files;
std::unordered_map<std::wstring, App::image_file *> App::loading_files;

//////////////////////////////////////////////////////////////////////
// cursors for hovering over rectangle interior/corners/edges
// see selection_hover_t

App::cursor_def App::sel_hover_cursors[16] = {
    { App::cursor_def::src::user, IDC_CURSOR_HAND },    //  0 - inside
    { App::cursor_def::src::system, IDC_SIZEWE },       //  1 - left
    { App::cursor_def::src::system, IDC_SIZEWE },       //  2 - right
    { App::cursor_def::src::system, IDC_ARROW },        //  3 - xx left and right shouldn't be possible
    { App::cursor_def::src::system, IDC_SIZENS },       //  4 - top
    { App::cursor_def::src::system, IDC_SIZENWSE },     //  5 - left and top
    { App::cursor_def::src::system, IDC_SIZENESW },     //  6 - right and top
    { App::cursor_def::src::system, IDC_ARROW },        //  7 - xx top left and right
    { App::cursor_def::src::system, IDC_SIZENS },       //  8 - bottom
    { App::cursor_def::src::system, IDC_SIZENESW },     //  9 - bottom left
    { App::cursor_def::src::system, IDC_SIZENWSE },     // 10 - bottom right
    { App::cursor_def::src::system, IDC_ARROW },        // 11 - xx bottom left and right
    { App::cursor_def::src::system, IDC_ARROW },        // 12 - xx bottom and top
    { App::cursor_def::src::system, IDC_ARROW },        // 13 - xx bottom top and left
    { App::cursor_def::src::system, IDC_ARROW },        // 14 - xx bottom top and right
    { App::cursor_def::src::system, IDC_ARROW }         // 15 - xx bottom top left and right
};

//////////////////////////////////////////////////////////////////////
// DragDropHelper stuff

IFACEMETHODIMP App::QueryInterface(REFIID riid, void **ppv)
{
    static QITAB const qit[] = {
        QITABENT(App, IDropTarget),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

//////////////////////////////////////////////////////////////////////

IFACEMETHODIMP_(ULONG) App::AddRef()
{
    return InterlockedIncrement(&refcount);
}

//////////////////////////////////////////////////////////////////////

IFACEMETHODIMP_(ULONG) App::Release()
{
    long cRef = InterlockedDecrement(&refcount);
    if(cRef == 0) {
        delete this;
    }
    return cRef;
}

//////////////////////////////////////////////////////////////////////
// restore window position if not fullscreen and not first time running

void App::setup_initial_windowplacement()
{
    if(!settings.first_run && !settings.fullscreen) {
        WINDOWPLACEMENT w{ settings.window_placement };
        w.showCmd = 0;
        SetWindowPlacement(window, &w);
    }
}

//////////////////////////////////////////////////////////////////////
// save or load a setting

HRESULT App::settings_t::serialize_setting(
    settings_t::serialize_action action, wchar const *key_name, wchar const *name, byte *var, DWORD size)
{
    switch(action) {

    case settings_t::serialize_action::save: {
        HKEY key;
        CHK_HR(RegCreateKeyEx(HKEY_CURRENT_USER, key_name, 0, null, 0, KEY_WRITE, null, &key, null));
        defer(RegCloseKey(key));
        CHK_HR(RegSetValueEx(key, name, 0, REG_BINARY, var, size));
    } break;

    case settings_t::serialize_action::load: {
        HKEY key;
        CHK_HR(RegCreateKeyEx(HKEY_CURRENT_USER, key_name, 0, null, 0, KEY_READ | KEY_QUERY_VALUE, null, &key, null));
        defer(RegCloseKey(key));
        DWORD cbsize = 0;
        if(FAILED(RegQueryValueEx(key, name, null, null, null, &cbsize)) || cbsize != size) {
            return S_FALSE;
        }
        CHK_HR(RegGetValue(
            HKEY_CURRENT_USER, key_name, name, RRF_RT_REG_BINARY, null, reinterpret_cast<DWORD *>(var), &cbsize));
    } break;
    }

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// save or load all the settings

HRESULT App::settings_t::serialize(serialize_action action, wchar const *save_key_name)
{
    if(save_key_name == null) {
        return E_INVALIDARG;
    }

#undef DECL_SETTING
#define DECL_SETTING(type, name, ...) \
    CHK_HR(serialize_setting(action, save_key_name, L#name, reinterpret_cast<byte *>(&name), (DWORD)sizeof(name)))
#include "settings.h"

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// load the settings

HRESULT App::settings_t::load()
{
    return serialize(serialize_action::load, settings_key_name);
}

//////////////////////////////////////////////////////////////////////
// save the settings

HRESULT App::settings_t::save()
{
    return serialize(serialize_action::save, settings_key_name);
}

//////////////////////////////////////////////////////////////////////
// copy selection and record timestamp for flash notification

HRESULT App::on_copy()
{
    copy_timestamp = m_timer.current_time;
    return copy_selection();
}

//////////////////////////////////////////////////////////////////////
// if it's already running, send it the command line and return S_FALSE

HRESULT App::reuse_window(wchar *cmd_line)
{
    if(settings.reuse_window) {
        HWND existing_window = FindWindow(window_class, null);
        if(existing_window != null) {
            COPYDATASTRUCT c;
            c.cbData = (DWORD)(wcslen(cmd_line) + 1) * sizeof(wchar);
            c.lpData = reinterpret_cast<void *>(cmd_line);
            c.dwData = (DWORD)App::copydata_t::commandline;
            SendMessageW(existing_window, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&c));

            // some confusion about whether this is legit but
            // BringWindowToFront doesn't work for top level windows
            SwitchToThisWindow(existing_window, TRUE);
            return S_FALSE;
        }
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// process a command line, could be from another instance

HRESULT App::on_command_line(wchar *cmd_line)
{
    // parse args
    int argc;
    wchar **argv = CommandLineToArgvW(cmd_line, &argc);

    wchar const *filepath{ null };

    if(argc > 1 && argv[1] != null) {
        filepath = argv[1];
    }

    if(filepath != null) {
        return load_image(filepath);
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// ask the file_loader_thread to load a file

void App::request_image(wchar const *filename)
{
    PostThreadMessage(file_loader_thread_id, WM_LOAD_FILE, 0, reinterpret_cast<LPARAM>(filename));
}

//////////////////////////////////////////////////////////////////////
// paste the clipboard - if it's a bitmap, paste that else if it's
// a string, try to load that file

HRESULT App::on_paste()
{
    if(window == null) {
        return E_UNEXPECTED;
    }

    // if clipboard contains a PNG or DIB, make it look like a file we just loaded

    UINT cf_png = RegisterClipboardFormat(L"PNG");
    UINT cf_filename = RegisterClipboardFormat(CFSTR_FILENAMEW);

    bool got_clipboard = false;

    image_file &f = clipboard_image_file;
    f.is_clipboard = true;
    f.bytes.clear();
    f.pixels.clear();
    f.hresult = E_FAIL;

    if(IsClipboardFormatAvailable(cf_png)) {

        CHK_HR(append_clipboard_to_buffer(f.bytes, cf_png));
        got_clipboard = true;

    } else if(IsClipboardFormatAvailable(CF_DIB)) {

        f.bytes.resize(sizeof(BITMAPFILEHEADER));
        CHK_HR(append_clipboard_to_buffer(f.bytes, CF_DIB));
        got_clipboard = true;

        BITMAPFILEHEADER *b = reinterpret_cast<BITMAPFILEHEADER *>(f.bytes.data());
        BITMAPINFOHEADER *i = reinterpret_cast<BITMAPINFOHEADER *>(b + 1);
        memset(b, 0, sizeof(*b));
        b->bfType = 'MB';
        b->bfSize = (DWORD)f.bytes.size();
        b->bfOffBits = sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER);
        if(i->biCompression == BI_BITFIELDS) {
            b->bfOffBits += 12;
        }
    }

    if(got_clipboard) {
        selection_active = false;
        f.filename = L"Clipboard";
        f.hresult = S_OK;
        f.index = -1;
        f.is_cache_load = true;
        f.view_count = 0;
        image &img = f.img;
        CHK_HR(decode_image(f.bytes.data(), f.bytes.size(), f.pixels, img.width, img.height, img.row_pitch));
        f.img.pixels = f.pixels.data();
        return show_image(&f);
    }

    UINT fmt = 0;
    if(IsClipboardFormatAvailable(cf_filename)) {
        fmt = cf_filename;
    } else if(IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        fmt = CF_UNICODETEXT;
    }
    if(fmt != 0) {
        std::vector<byte> buffer;
        CHK_HR(append_clipboard_to_buffer(buffer, fmt));
        return on_drop_string(reinterpret_cast<wchar const *>(buffer.data()));
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// make an image the current one

HRESULT App::display_image(image_file *f)
{
    if(f == null) {
        return E_INVALIDARG;
    }
    selection_active = false;

    HRESULT load_hr = f->hresult;

    if(FAILED(load_hr)) {

        // if this is the first file being loaded and there was a file loading error
        if(load_hr != HRESULT_FROM_WIN32(ERROR_OPERATION_ABORTED) && files_loaded == 0) {

            // show a messagebox (because they probably ran it just now)
            std::wstring load_err = windows_error_message(load_hr);
            std::wstring err_msg = format(L"Error loading %s\n\n%s", f->filename.c_str(), load_err.c_str());
            MessageBoxW(null, err_msg.c_str(), L"ImageView", MB_ICONEXCLAMATION);

            // don't wait for window creation to complete, window might be null, that's ok
            PostMessage(window, WM_CLOSE, 0, 0);
        }

        // and in any case, set window message to error text
        std::wstring err_str = windows_error_message(load_hr);
        std::wstring name;
        CHK_HR(file_get_filename(f->filename.c_str(), name));
        set_message(format(L"Can't load %s - %s", name.c_str(), err_str.c_str()).c_str(), 3);
        return load_hr;
    }
    files_loaded += 1;
    f->view_count += 1;
    return show_image(f);
}

//////////////////////////////////////////////////////////////////////
// actually show an image

HRESULT App::show_image(image_file *f)
{
    current_image_file = f;

    std::wstring name;

    // hresult from load_file
    HRESULT hr = f->hresult;

    ComPtr<ID3D11Texture2D> new_texture;
    ComPtr<ID3D11ShaderResourceView> new_srv;

    // or hresult from create_texture
    if(SUCCEEDED(hr)) {
        hr = f->img.create_texture(d3d_device.Get(), d3d_context.Get(), &new_texture, &new_srv);
    }

    // set texture as current
    if(SUCCEEDED(hr)) {

        image_texture.Attach(new_texture.Detach());
        image_texture_view.Attach(new_srv.Detach());

        d3d_name(image_texture);
        d3d_name(image_texture_view);

        D3D11_TEXTURE2D_DESC image_texture_desc;
        image_texture->GetDesc(&image_texture_desc);

        texture_width = image_texture_desc.Width;
        texture_height = image_texture_desc.Height;

        reset_zoom(settings.zoom_mode);

        current_rect = target_rect;

        m_timer.reset();

        if(settings.show_full_filename_in_titlebar) {
            name = f->filename;
        } else {
            CHK_HR(file_get_filename(f->filename.c_str(), name));
        }
        std::wstring msg = format(L"%s %dx%d", name.c_str(), texture_width, texture_height);
        SetWindowText(window, msg.c_str());
        set_message(msg.c_str(), 2.0f);

    } else {

        std::wstring err_str;

        // "Component not found" isn't meaningful for unknown file type, override it
        if(hr == WINCODEC_ERR_COMPONENTNOTFOUND) {
            err_str = L"Unknown file type";
        } else {
            err_str = windows_error_message(hr);
        }

        CHK_HR(file_get_filename(f->filename.c_str(), name));
        std::wstring msg = format(L"Can't load %s - %s", name.c_str(), err_str.c_str());
        set_message(msg.c_str(), 3.0f);
    }
    return hr;
}

//////////////////////////////////////////////////////////////////////
// scan a folder - this runs in the folder_scanner_thread
// results are sent to the main thread with SendMessage

HRESULT App::do_folder_scan(wchar const *folder_path)
{
    std::wstring path;

    CHK_HR(file_get_path(folder_path, path));

    delete[] folder_path;

    Log(L"Scan folder %s", path.c_str());

    std::vector<wchar const *> extensions{ L"jpg", L"png", L"bmp", L"tiff", L"jpeg" };

    scan_folder_sort_field sort_field = scan_folder_sort_field::name;
    scan_folder_sort_order order = scan_folder_sort_order::ascending;

    folder_scan_result *results;

    CHK_HR(scan_folder2(path.c_str(), extensions, sort_field, order, &results, quit_event));

    // send the results to the window, it will forward them to the app
    WaitForSingleObject(window_created_event, INFINITE);
    SendMessage(window, WM_FOLDER_SCAN_COMPLETE, 0, reinterpret_cast<LPARAM>(results));

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// file_loader_thread waits for file load requests and kicks off
// threads for each one

void App::file_loader_function()
{
    MSG msg;

    // respond to requests from main thread, quit if quit_event gets set
    while(MsgWaitForMultipleObjects(1, &quit_event, false, INFINITE, QS_POSTMESSAGE) != WAIT_OBJECT_0) {
        if(PeekMessage(&msg, null, 0, 0, PM_REMOVE) != 0) {
            switch(msg.message) {
            case WM_LOAD_FILE:
                start_file_loader(reinterpret_cast<image_file *>(msg.lParam));
                break;
            }
        }
    }
    Log(L"File loader thread exit");
}

//////////////////////////////////////////////////////////////////////
// file_scanner_thread waits for scan requests and processes them
// in this thread

void App::scanner_function()
{
    bool quit = false;
    MSG msg;

    // respond to requests from main thread
    while(!quit) {
        switch(MsgWaitForMultipleObjects(1, &quit_event, false, INFINITE, QS_POSTMESSAGE | QS_ALLPOSTMESSAGE)) {
        case WAIT_OBJECT_0:
            quit = true;
            break;
        case WAIT_OBJECT_0 + 1:
            if(GetMessage(&msg, null, 0, 0) != 0) {
                switch(msg.message) {
                case WM_SCAN_FOLDER:
                    do_folder_scan(reinterpret_cast<wchar const *>(msg.lParam));
                    break;
                }
                break;
            }
        }
    }
    Log(L"Scanner thread exit");
}

//////////////////////////////////////////////////////////////////////
// warm the cache by loading N files either side of the current one

HRESULT App::warm_cache()
{
    // if it's a normal file in the current folder
    if(current_image_file != null && !current_image_file->is_clipboard && current_image_file->index != -1) {

        int const num_images_to_cache = 12;

        for(int i = 0; i < num_images_to_cache; ++i) {

            int y = current_image_file->index - (i + 2) / 2 * ((i & 1) * 2 - 1);

            if(y >= 0 && y < (int)current_folder_scan->files.size()) {

                std::wstring this_file = current_folder_scan->path + L"\\" + current_folder_scan->files[y].name;

                if(loading_files.find(this_file) == loading_files.end() &&
                   loaded_files.find(this_file) == loaded_files.end()) {

                    // remove things from cache until it's <= cache_size + required size

                    uint64 img_size;
                    if(SUCCEEDED(get_image_file_size(this_file.c_str(), &img_size))) {

                        if(img_size < settings.cache_size) {

                            while(cache_in_use + img_size > settings.cache_size) {

                                image_file *loser = null;
                                uint loser_diff = 0;
                                for(auto const &fl : loaded_files) {
                                    uint diff = std::abs(fl.second->index - current_file_cursor);
                                    if(diff > loser_diff) {
                                        loser_diff = diff;
                                        loser = fl.second;
                                    }
                                }

                                if(loser != null) {

                                    Log(L"Removing %s (%d) from cache (now %d MB in use)",
                                        loser->filename.c_str(),
                                        loser->index,
                                        cache_in_use / 1048576);

                                    cache_in_use -= loser->total_size();
                                    loaded_files.erase(loser->filename);
                                } else {
                                    break;
                                }
                            }
                        }

                        if((cache_in_use + img_size) <= settings.cache_size) {

                            Log(L"Caching %s at %d", this_file.c_str(), y);
                            image_file *cache_file = new image_file(this_file);
                            cache_file->is_cache_load = true;
                            loading_files[this_file] = cache_file;
                            start_file_loader(cache_file);
                        }
                    }
                }
            }
        }
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// folder scan complete

void App::on_folder_scanned(folder_scan_result *scan_result)
{
    Log(L"%zu images found in %s", scan_result->files.size(), scan_result->path.c_str());

    current_folder_scan.reset(scan_result);

    if(current_image_file != null && current_image_file->index == -1) {
        update_file_index(current_image_file);
    }

    warm_cache();
}

//////////////////////////////////////////////////////////////////////
// move to another file in the folder

void App::move_file_cursor(int movement)
{
    // can't key left/right while folder is still being scanned
    // TODO(chs): remember the moves and apply them when the scan is complete?
    if(current_folder_scan == null || current_file_cursor == -1) {
        return;
    }

    int new_file_cursor = clamp(0, current_file_cursor + movement, (int)current_folder_scan->files.size() - 1);

    if(new_file_cursor != current_file_cursor) {
        current_file_cursor = new_file_cursor;
        std::wstring pic_filename = format(
            L"%s\\%s", current_folder_scan->path.c_str(), current_folder_scan->files[current_file_cursor].name.c_str());
        load_image(pic_filename.c_str());
    }
}

//////////////////////////////////////////////////////////////////////
// kick off a file loader thread

void App::start_file_loader(image_file *loader)
{
    thread_pool.create_thread(
        [this](image_file *fl) {

            // load the file synchronously to this thread
            fl->hresult = load_file(fl->filename.c_str(), fl->bytes, quit_event);

            if(SUCCEEDED(fl->hresult)) {

                // Need to call this in any thread which uses Windows Imaging Component
                CoInitializeEx(null, COINIT_APARTMENTTHREADED);

                // decode the image
                fl->hresult = decode_image(
                    fl->bytes.data(), fl->bytes.size(), fl->pixels, fl->img.width, fl->img.height, fl->img.row_pitch);

                fl->img.pixels = fl->pixels.data();

                CoUninitialize();
            }

            // let the window know, either way, that the file load attempt is complete, failed or
            // otherwise
            WaitForSingleObject(window_created_event, INFINITE);
            PostMessage(window, WM_FILE_LOAD_COMPLETE, 0, reinterpret_cast<LPARAM>(fl));
        },
        loader);
}

//////////////////////////////////////////////////////////////////////
// load an image file or get it from the cache (or notice that it's
// already being loaded and just let it arrive later)

HRESULT App::load_image(wchar const *filepath)
{
    if(filepath == null || filepath[0] == 0) {
        return E_INVALIDARG;
    }

    std::wstring filename = std::wstring(filepath);

    // get somewhat canonical filepath and parts thereof

    std::wstring folder;
    std::wstring name;
    std::wstring fullpath;

    CHK_HR(file_get_full_path(filename.c_str(), fullpath));
    CHK_HR(file_get_filename(fullpath.c_str(), name));
    CHK_HR(file_get_path(fullpath.c_str(), folder));

    if(folder.empty()) {
        folder = L".";
    } else if(folder.back() == '\\') {
        folder.pop_back();
    }

    // if it's in the cache already, just show it

    auto found = loaded_files.find(fullpath);
    if(found != loaded_files.end()) {
        Log(L"Already got %s", name.c_str());
        CHK_HR(display_image(found->second));
        CHK_HR(warm_cache());
        return S_OK;
    }

    // if it's currently being loaded, mark it for viewing when it arrives

    found = loading_files.find(fullpath);
    if(found != loading_files.end()) {
        Log(L"In progress %s", name.c_str());
        requested_file = found->second;
        return S_OK;
    }

    // if it's not a cache_load, remove things from the cache until there's room for it
    // remove files what are furthest away from it by index

    // file_loader object is later transferred from loading_files to loaded_files

    Log(L"Loading %s", name.c_str());

    image_file *fl = new image_file();
    fl->filename = fullpath;
    loading_files[fullpath] = fl;

    // when this file arrives, please display it

    requested_file = fl;

    PostThreadMessage(file_loader_thread_id, WM_LOAD_FILE, 0, reinterpret_cast<LPARAM>(fl));

    // loading a file from a new folder or the current scanned folder?

    if(_wcsicmp(current_folder.c_str(), folder.c_str()) != 0) {

        // tell the scanner thread to scan this new folder
        // when it's done it will notify main thread with a windows message

        // TODO(chs): CLEAR THE CACHE HERE....

        current_folder = folder;

        // sigh, manually marshall the filename for the message, the receiver is responsible for
        // freeing it
        wchar *fullpath_buffer = new wchar[fullpath.size() + 1];
        memcpy(fullpath_buffer, fullpath.c_str(), (fullpath.size() + 1) * sizeof(wchar));

        PostThreadMessage(scanner_thread_id, WM_SCAN_FOLDER, 0, reinterpret_cast<LPARAM>(fullpath_buffer));
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// if loaded file is in the current folder, find index with list of files

HRESULT App::update_file_index(image_file *f)
{
    if(current_folder_scan == null || f == null) {
        return E_INVALIDARG;
    }
    std::wstring folder;
    CHK_HR(file_get_path(f->filename.c_str(), folder));

    if(_wcsicmp(folder.c_str(), current_folder_scan->path.c_str()) != 0) {
        return E_CHANGED_STATE;
    }

    std::wstring name;
    CHK_HR(file_get_filename(f->filename.c_str(), name));
    int id = 0;
    for(auto &ff : current_folder_scan->files) {
        if(_wcsicmp(ff.name.c_str(), name.c_str()) == 0) {
            f->index = id;
            Log(L"%s is at index %d", name.c_str(), id);
            if(current_file_cursor == -1) {
                current_file_cursor = f->index;
            }
            return S_OK;
        }
        id += 1;
    }
    return E_NOT_SET;
}

//////////////////////////////////////////////////////////////////////
// a file got loaded
// move it from loading into loaded pile
// show it if it was the most recently requested image
// maintain cache

void App::on_file_load_complete(LPARAM lparam)
{
    image_file *f = reinterpret_cast<image_file *>(lparam);

    // if(FAILED(f->hresult)) {
    //    delete f;
    //    return;
    //}

    // transfer from loading to loaded
    loading_files.erase(f->filename);
    loaded_files[f->filename] = f;

    std::lock_guard lock(cache_mutex);

    // update cache total size
    cache_in_use += f->total_size();

    // fill in the index so we know where it is in the list of files

    update_file_index(f);

    // if it's most recently requested, show it
    if(f == requested_file) {
        requested_file = null;
        display_image(f);
        current_file_cursor = f->index;
        f->is_cache_load = false;    // warm the cache for this file please
    }

    bool is_cache_load = f->is_cache_load;

    // if this image was displayed, cache some file around it

    if(!is_cache_load) {
        warm_cache();
    }
}

//////////////////////////////////////////////////////////////////////
// get current mouse buttons

bool App::get_mouse_buttons(int button) const
{
    return (mouse_grab & (1 << button)) != 0;
}

//////////////////////////////////////////////////////////////////////

void App::set_mouse_button(int button)
{
    int mask = 1 << (int)button;
    if(mouse_grab == 0) {
        SetCapture(window);
    }
    mouse_grab |= mask;
}

//////////////////////////////////////////////////////////////////////

void App::clear_mouse_buttons(int button)
{
    int mask = 1 << (int)button;
    mouse_grab &= ~mask;
    if(mouse_grab == 0) {
        ReleaseCapture();
    }
}

//////////////////////////////////////////////////////////////////////
// get texture size as a vec2

vec2 App::texture_size() const
{
    return { (float)texture_width, (float)texture_height };
}

//////////////////////////////////////////////////////////////////////
// get window size as a vec2

vec2 App::window_size() const
{
    return { (float)window_width, (float)window_height };
}

//////////////////////////////////////////////////////////////////////
// get size of a texel in window pixels

vec2 App::texel_size() const
{
    return div_point(current_rect.size(), texture_size());
}

//////////////////////////////////////////////////////////////////////
// convert a texel pos to a window pixel pos

vec2 App::texels_to_pixels(vec2 pos) const
{
    return mul_point(pos, texel_size());
}

//////////////////////////////////////////////////////////////////////
// clamp a texel pos to the texture dimensions

vec2 App::clamp_to_texture(vec2 pos) const
{
    vec2 t = texture_size();
    return vec2{ clamp(0.0f, pos.x, t.x - 1), clamp(0.0f, pos.y, t.y - 1) };
}

//////////////////////////////////////////////////////////////////////
// convert a window pixel pos to a texel pos

vec2 App::screen_to_texture_pos(vec2 pos) const
{
    vec2 relative_pos = sub_point(pos, current_rect.top_left());
    return div_point(relative_pos, texel_size());
}

//////////////////////////////////////////////////////////////////////
// convert a window pos (point_s) to texel pos

vec2 App::screen_to_texture_pos(point_s pos) const
{
    return screen_to_texture_pos(vec2(pos));
}

//////////////////////////////////////////////////////////////////////
// convert texel pos to window pixel pos with clamp to texture

vec2 App::texture_to_screen_pos(vec2 pos) const
{
    return add_point(current_rect.top_left(), texels_to_pixels(vec2::floor(clamp_to_texture(pos))));
}

//////////////////////////////////////////////////////////////////////
// convert texel pos to window pixel pos without clamp

vec2 App::texture_to_screen_pos_unclamped(vec2 pos) const
{
    return add_point(current_rect.top_left(), texels_to_pixels(vec2::floor(pos)));
}

//////////////////////////////////////////////////////////////////////
// call this before doing anything else with App, e.g. top of main
//   checks cpu is supported, displays error dialog and returns S_FALSE if not
//   check if reuse_window == true and another instance is running, if so, sends command line there and returns S_FALSE
//   creates some events
//   sets up default mouse cursor
//   starts some threads
//   loads settings
//   checks command line and maybe initiates loading an image file

HRESULT App::init(wchar *cmd_line)
{
    if(!XMVerifyCPUSupport()) {
        std::wstring message = format(localize(IDS_OldCpu), localize(IDS_AppName));
        MessageBox(null, message.c_str(), localize(IDS_AppName), MB_ICONEXCLAMATION);
        return S_FALSE;
    }

    HRESULT hr = reuse_window(cmd_line);

    if(hr == S_FALSE) {
        return S_FALSE;
    }

    if(FAILED(hr)) {
        return hr;
    }

    CHK_BOOL(GetPhysicallyInstalledSystemMemory(&system_memory_size_kb));

    Log(L"System has %lluGB of memory", system_memory_size_kb / 1048576);

    window_created_event = CreateEvent(null, true, false, null);

    current_mouse_cursor = LoadCursor(null, IDC_ARROW);

    // remember default settings for 'reset settings to default' feature in settings dialog
    default_settings = settings;

    // in debug builds, hold middle mouse button at startup to reset settings to defaults
#if defined(_DEBUG)
    if(!is_key_down(VK_MBUTTON))
#endif
        settings.load();

    CHK_NULL(quit_event = CreateEvent(null, true, false, null));

    CHK_HR(thread_pool.init());

    CHK_HR(thread_pool.create_thread_with_message_pump(&scanner_thread_id, [this]() { scanner_function(); }));

    CHK_HR(thread_pool.create_thread_with_message_pump(&file_loader_thread_id, [this]() { file_loader_function(); }));

    CHK_HR(on_command_line(cmd_line));

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// this is a mess, but kinda necessarily so

HRESULT App::get_startup_rect_and_style(rect *r, DWORD *style, DWORD *ex_style)
{
    if(r == null || style == null || ex_style == null) {
        return E_INVALIDARG;
    }

    *ex_style = WS_EX_NOREDIRECTIONBITMAP;

    // if settings.fullscreen, use settings.fullscreen_rect (which will be on a monitor (which may
    // or may not still exist...))

    int default_monitor_width = GetSystemMetrics(SM_CXSCREEN);
    int default_monitor_height = GetSystemMetrics(SM_CYSCREEN);

    // default startup is windowed, 2/3rd size of default monitor
    if(settings.first_run) {
        settings.fullscreen_rect = { 0, 0, default_monitor_width, default_monitor_height };
        *style = WS_OVERLAPPEDWINDOW;
        *r = { 0, 0, default_monitor_width * 2 / 3, default_monitor_height * 2 / 3 };
        window_width = r->w();
        window_height = r->h();
        AdjustWindowRect(r, *style, FALSE);
        *r = center_rect_on_default_monitor(*r);
        return S_OK;
    }

    switch(settings.fullscreen_mode) {
    case fullscreen_startup_option::start_fullscreen:
        settings.fullscreen = true;
        break;
    case fullscreen_startup_option::start_windowed:
        settings.fullscreen = false;
        break;
    default:
        break;
    }

    if(!settings.fullscreen) {
        *style = WS_OVERLAPPEDWINDOW;
        *r = settings.window_placement.rcNormalPosition;
    } else {
        *style = WS_POPUP;

        // check the monitor is still there and the same size
        MONITORINFO i;
        i.cbSize = sizeof(MONITORINFO);
        HMONITOR m = MonitorFromPoint(settings.fullscreen_rect.top_left(), MONITOR_DEFAULTTONEAREST);
        if(m != null && GetMonitorInfo(m, &i) && memcmp(&settings.fullscreen_rect, &i.rcMonitor, sizeof(rect)) == 0) {
            *r = settings.fullscreen_rect;
        } else {
            *r = { 0, 0, default_monitor_width, default_monitor_height };
        }
    }
    window_width = r->w();
    window_height = r->h();
    Log("Startup window is %dx%d", window_width, window_height);
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// toggle between normal window and fake fullscreen

void App::toggle_fullscreen()
{
    settings.fullscreen = !settings.fullscreen;

    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD ex_style = 0;

    if(settings.fullscreen) {
        GetWindowPlacement(window, &settings.window_placement);
        style = WS_POPUP;
    }

    SetWindowLongPtr(window, GWL_STYLE, style);
    SetWindowLongPtr(window, GWL_EXSTYLE, ex_style);

    if(settings.fullscreen) {

        MONITORINFO monitor_info{ 0 };
        monitor_info.cbSize = sizeof(MONITORINFO);

        HMONITOR h = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
        rect window_rect;
        if(h != null && GetMonitorInfo(h, &monitor_info)) {
            window_rect = rect::as(monitor_info.rcMonitor);
        } else {
            window_rect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        }

        settings.fullscreen_rect = window_rect;

        int sx = window_rect.left;
        int sy = window_rect.top;
        int sw = window_rect.w();
        int sh = window_rect.h();

        SetWindowPos(window, HWND_TOP, sx, sy, sw, sh, SWP_FRAMECHANGED | SWP_HIDEWINDOW);
        ShowWindow(window, SW_SHOW);

    } else {

        ignore_dpi_for_a_moment = true;
        SetWindowPlacement(window, &settings.window_placement);
        ignore_dpi_for_a_moment = false;
    }
}

//////////////////////////////////////////////////////////////////////
// set the mouse cursor and track it

void App::set_mouse_cursor(HCURSOR c)
{
    if(c == null) {
        c = LoadCursor(null, IDC_ARROW);
    }
    current_mouse_cursor = c;
    SetCursor(c);
}

//////////////////////////////////////////////////////////////////////
// find where on the selection the mouse is hovering
//    sets selection_hover
//    sets mouse cursor appropriately

void App::check_selection_hover(vec2 pos)
{
    if(!selection_active) {
        set_mouse_cursor(null);
        return;
    }

    rect_f select_rect(select_current, select_anchor);

    // get screen coords of selection rectangle
    vec2 tl = texture_to_screen_pos(select_rect.top_left());
    vec2 br = texture_to_screen_pos_unclamped(add_point({ 1, 1 }, select_rect.bottom_right()));

    // selection grab border is +/- N pixels (setting: 4 to 32 pixels)
    float border = dpi_scale(settings.select_border_grab_size) / 2;
    vec2 b{ border, border };

    // expand outer rect
    vec2 expanded_topleft = vec2::max({ 0, 0 }, sub_point(tl, b));
    vec2 expanded_bottomright = vec2::min(window_size(), add_point(br, b));

    selection_hover = selection_hover_t::sel_hover_outside;

    // mouse is in the expanded box?
    if(rect_f(expanded_topleft, expanded_bottomright).contains(pos)) {

        selection_hover = selection_hover_t::sel_hover_inside;    // which is to say zero

        // distances from the edges
        float ld = fabsf(pos.x - tl.x);
        float rd = fabsf(pos.x - br.x);
        float td = fabsf(pos.y - tl.y);
        float bd = fabsf(pos.y - br.y);

        // closest horizontal, vertical edge distances
        float hd = std::min(ld, rd);
        float vd = std::min(td, bd);

        if(vd < border) {
            selection_hover |= (td < bd) ? sel_hover_top : sel_hover_bottom;
        }
        if(hd < border) {
            selection_hover |= (ld < rd) ? sel_hover_left : sel_hover_right;
        }
        set_mouse_cursor(sel_hover_cursors[selection_hover].get_hcursor());

    } else {
        set_mouse_cursor(null);
    }
}

//////////////////////////////////////////////////////////////////////

HRESULT App::load_accelerators()
{
    CHK_NULL(accelerators = LoadAccelerators(GetModuleHandle(null), MAKEINTRESOURCE(IDR_ACCELERATORS_EN_UK)));
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// Setup the shortcut key labels in a menu based in the accelerators

HRESULT App::setup_menu_accelerators(HMENU menu)
{
    if(accelerators == null) {
        return ERROR_INVALID_DATA;
    }

    HKL keyboard_layout = GetKeyboardLayout(GetCurrentThreadId());

    CHK_NULL(keyboard_layout);

    UINT num_accelerators = CopyAcceleratorTable(accelerators, null, 0);

    if(num_accelerators == 0) {
        return ERROR_NOT_FOUND;
    }

    std::vector<ACCEL> accel_table;
    accel_table.resize(num_accelerators);

    UINT num_accelerators_got = CopyAcceleratorTable(accelerators, accel_table.data(), num_accelerators);

    if(num_accelerators_got != num_accelerators) {
        return ERROR_NOT_ALL_ASSIGNED;
    }

    byte keys[256];
    memset(keys, 0, sizeof(keys));

    // for each accelerator

    for(auto const &a : accel_table) {

        MENUITEMINFO mii;
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID;
        mii.dwItemData = 0;
        mii.dwTypeData = null;

        // find the menu item which that accelerator is the hotkey for

        if(GetMenuItemInfo(menu, a.cmd, MF_BYCOMMAND, &mii)) {

            // setup the menu item string by appending a tab and then the hotkey text

            if(mii.fType == MFT_STRING) {

                std::wstring text;
                text.resize(mii.cbSize + 1);
                mii.dwTypeData = text.data();
                mii.cch = static_cast<uint>(text.size());
                CHK_BOOL(GetMenuItemInfo(menu, a.cmd, MF_BYCOMMAND, &mii));

                // truncate if there's already a tab

                text = text.substr(0, text.find(L'\t'));

                // handle some keys differently because GetKeyNameText is not always useful

                wchar key_name[256];
                switch(a.key) {
                case VK_LEFT:
                    wcsncpy_s(key_name, L"Left", 256);
                    break;
                case VK_RIGHT:
                    wcsncpy_s(key_name, L"Right", 256);
                    break;
                case VK_UP:
                    wcsncpy_s(key_name, L"Up", 256);
                    break;
                case VK_DOWN:
                    wcsncpy_s(key_name, L"Down", 256);
                    break;
                case VK_PRIOR:
                    wcsncpy_s(key_name, L"Page Up", 256);
                    break;
                case VK_NEXT:
                    wcsncpy_s(key_name, L"Page Down", 256);
                    break;
                default:
                    uint scan_code = MapVirtualKeyEx(a.key, MAPVK_VK_TO_VSC, keyboard_layout);
                    GetKeyNameText((scan_code & 0x7f) << 16, key_name, 256);
                    break;
                }

                // build the label with modifier keys

                std::wstring key_label;

                auto append = [](std::wstring &a, wchar const *b) {
                    if(!a.empty()) {
                        a.append(L"-");
                    }
                    a.append(b);
                };

                if(a.fVirt & FCONTROL) {
                    append(key_label, L"Ctrl");
                }
                if(a.fVirt & FSHIFT) {
                    append(key_label, L"Shift");
                }
                if(a.fVirt & FALT) {
                    append(key_label, L"Alt");
                }
                append(key_label, key_name);

                // setup the menu item text

                text = format(L"%s\t%s", text.c_str(), key_label.c_str());
                mii.dwTypeData = text.data();
                mii.cch = static_cast<uint>(text.size());
                mii.fMask = MIIM_STRING;

                CHK_BOOL(SetMenuItemInfo(menu, a.cmd, MF_BYCOMMAND, &mii));
            }
        }
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// call this from WM_NCCREATE when window is first created

HRESULT App::set_window(HWND hwnd)
{
    window = hwnd;

    SetEvent(window_created_event);

    CHK_HR(create_device());

    CHK_HR(create_resources());

    InitializeDragDropHelper(window);

    // if there was no file load requested on the command line, try to paste an image from the clipboard
    if(requested_file == null && IsClipboardFormatAvailable(CF_DIBV5)) {
        return on_paste();
    }

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// WM_SETCURSOR

bool App::on_setcursor()
{
    set_mouse_cursor(current_mouse_cursor);
    return true;
}

//////////////////////////////////////////////////////////////////////
// WM_[L/M/R]BUTTON[UP/DOWN]

void App::on_mouse_button(point_s pos, uint button, int state)
{
    assert(button < btn_count);

    if(state == btn_down) {

        mouse_click_timestamp[button] = GetTickCount64();
        mouse_click[button] = pos;
        mouse_pos[button] = pos;
        memset(mouse_offset + button, 0, sizeof(point_s));

        set_mouse_button(button);

        if(button == settings.select_button) {

            if(selection_active && selection_hover != selection_hover_t::sel_hover_outside) {

                // texel they were on when they grabbed the selection
                drag_select_pos = vec2::floor(screen_to_texture_pos(pos));
                drag_selection = true;
            }

            if(!drag_selection) {

                select_anchor = clamp_to_texture(screen_to_texture_pos(pos));
                select_current = select_anchor;
                selection_active = false;
            }

        } else if(button == settings.zoom_button && !popup_menu_active) {

            ShowCursor(FALSE);
        }

    } else {    // state == btn_up

        clear_mouse_buttons(button);

        // if RMB released within double-click time and haven't
        // moved it much since they pressed it, show popup menu

        if(button == settings.drag_button) {

            uint64 since = GetTickCount64() - mouse_click_timestamp[settings.drag_button];

            if(since < static_cast<uint64>(GetDoubleClickTime())) {

                uint hover_high;
                uint hover_wide;

                SystemParametersInfo(SPI_GETMOUSEHOVERHEIGHT, 0, &hover_high, 0);
                SystemParametersInfo(SPI_GETMOUSEHOVERWIDTH, 0, &hover_wide, 0);

                point_s const &click_pos = mouse_click[settings.drag_button];

                uint x_distance = std::abs(click_pos.x - pos.x);
                uint y_distance = std::abs(click_pos.y - pos.y);

                if(x_distance < hover_wide && y_distance < hover_high) {

                    HMENU menu = LoadMenu(GetModuleHandle(null), MAKEINTRESOURCE(IDR_MENU_POPUP));
                    HMENU popup_menu = GetSubMenu(menu, 0);
                    POINT screen_pos{ click_pos.x, click_pos.y };
                    ClientToScreen(window, &screen_pos);

                    setup_menu_accelerators(popup_menu);

                    popup_menu_active = true;
                    TrackPopupMenu(popup_menu, TPM_RIGHTBUTTON, screen_pos.x, screen_pos.y, 0, window, null);
                    m_timer.reset();
                    popup_menu_active = false;
                }
            }
        }

        else if(button == settings.zoom_button) {

            ShowCursor(TRUE);
            clear_mouse_buttons(settings.drag_button);    // in case they pressed the drag button while zooming

        } else if(button == settings.select_button) {

            drag_selection = false;

            // when selection is finalized, select_anchor is top left, select_current is bottom
            // right
            vec2 tl = vec2::floor(vec2::min(select_anchor, select_current));
            vec2 br = vec2::floor(vec2::max(select_anchor, select_current));
            select_anchor = tl;
            select_current = br;
            selection_size = sub_point(br, tl);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// Raw mouse input move (for zooming while MMB held down)

void App::on_raw_mouse_move(point_s delta)
{
    if(get_mouse_buttons(settings.zoom_button) && !popup_menu_active) {
        do_zoom(mouse_click[settings.zoom_button], std::max(-4, std::min(-delta.y, 4)));
    }
}

//////////////////////////////////////////////////////////////////////
// WM_MOUSEMOVE

void App::on_mouse_move(point_s pos)
{
    if(!get_mouse_buttons(settings.zoom_button)) {
        cur_mouse_pos = pos;
    }

    if(shift_snap) {

        switch(shift_snap_axis) {

        case shift_snap_axis_t::none: {
            int xd = cur_mouse_pos.x - shift_mouse_pos.x;
            int yd = cur_mouse_pos.y - shift_mouse_pos.y;
            float distance = dpi_scale(sqrtf((float)(xd * xd + yd * yd)));
            if(distance > shift_snap_radius) {
                shift_snap_axis = (std::abs(xd) > std::abs(yd)) ? shift_snap_axis_t::y : shift_snap_axis_t::x;
            }
        } break;

        case shift_snap_axis_t::x:
            cur_mouse_pos.x = shift_mouse_pos.x;
            break;
        case shift_snap_axis_t::y:
            cur_mouse_pos.y = shift_mouse_pos.y;
            break;
        }
    }

    for(int i = 0; i < btn_count; ++i) {
        if(get_mouse_buttons(i)) {
            mouse_offset[i] = add_point(mouse_offset[i], sub_point(cur_mouse_pos, mouse_pos[i]));
            mouse_pos[i] = cur_mouse_pos;
        }
    }

    if(selecting) {
        vec2 diff = vec2(sub_point(mouse_click[settings.select_button], pos));
        float len = vec2::length(diff);
        if(len > settings.select_start_distance) {
            selection_active = true;
        }
    }

    if(!get_mouse_buttons(settings.select_button)) {
        check_selection_hover(vec2(pos));
    } else if(selection_hover == selection_hover_t::sel_hover_outside) {
        set_mouse_cursor(null);
    }
}

//////////////////////////////////////////////////////////////////////
// WM_MOUSEWHEEL

void App::on_mouse_wheel(point_s pos, int delta)
{
    do_zoom(pos, delta * 10);
}

//////////////////////////////////////////////////////////////////////
// set the banner message and how long before it fades out

void App::set_message(wchar const *message, double fade_time)
{
    current_message = std::wstring(message);
    message_timestamp = m_timer.wall_time();
    message_fade_time = fade_time;
}

//////////////////////////////////////////////////////////////////////
// zoom in or out, focusing on a point

void App::do_zoom(point_s pos, int delta)
{
    // get normalized position
    float px = (pos.x - current_rect.x) / current_rect.w;
    float py = (pos.y - current_rect.y) / current_rect.h;

    // largest axis dimension
    float max_dim = (float)std::max(texture_width, texture_height);

    // clamp to max zoom
    float max_w = max_zoom * texture_width;
    float max_h = max_zoom * texture_height;

    // clamp so it can't go smaller than 'min_zoom' pixels in largest axis
    float min_w = min_zoom * texture_width / max_dim;
    float min_h = min_zoom * texture_height / max_dim;

    // delta to add to width, height to get new zoom
    float dx = current_rect.w * 0.01f * delta;
    float dy = current_rect.h * 0.01f * delta;

    // new width, height
    float nw = current_rect.w + dx;
    float nh = current_rect.h + dy;

    // clamp to max
    if(nw > max_w || nh > max_h) {
        dx = max_w - current_rect.w;
        dy = max_h - current_rect.h;
    }

    // clamp to min
    if(nw < min_w || nh < min_h) {
        dx = min_w - current_rect.w;
        dy = min_h - current_rect.h;
    }

    current_rect.w += dx;
    current_rect.h += dy;
    current_rect.x -= px * dx;
    current_rect.y -= py * dy;

    target_rect = current_rect;

    has_been_zoomed_or_dragged = true;
}

//////////////////////////////////////////////////////////////////////
// zoom to the selection allowing for labels

void App::zoom_to_selection()
{
    if(!selection_active) {
        reset_zoom(reset_zoom_mode::fit_to_window);
        return;
    }

    // this is bogus
    // first calculate a zoom for just the selection
    // then adjust to count for the labels which gives
    // a close but slightly off result.
    // the right way would be to work out the
    // size including the labels first time
    // but I can't be arsed

    auto calc_target = [&](vec2 const &sa, vec2 const &sc) -> rect_f {
        float w = fabsf(sc.x - sa.x) + 1;
        float h = fabsf(sc.y - sa.y) + 1;
        float mx = (sa.x + sc.x) / 2 + 0.5f;
        float my = (sa.y + sc.y) / 2 + 0.5f;
        float ws = window_width / w;
        float hs = window_height / h;
        float s = std::min(max_zoom, std::min(ws, hs));
        return rect_f{ window_width / 2 - mx * s, window_height / 2 - my * s, texture_width * s, texture_height * s };
    };

    vec2 tl = vec2::min(select_anchor, select_current);
    vec2 br = vec2::max(select_anchor, select_current);

    rect_f new_target = calc_target(tl, br);

    float scale = texture_width / new_target.w;

    vec2 extra{ ((small_label_size.x * 2) + dpi_scale(12)) * scale, ((small_label_size.y * 2) + dpi_scale(6)) * scale };

    // still wrong after 2nd calc_target but less wrong enough
    target_rect = calc_target(sub_point(tl, extra), add_point(extra, br));

    has_been_zoomed_or_dragged = true;
}

//////////////////////////////////////////////////////////////////////
// center the image in the window

void App::center_in_window()
{
    target_rect.x = (window_width - current_rect.w) / 2.0f;
    target_rect.y = (window_height - current_rect.h) / 2.0f;
}

//////////////////////////////////////////////////////////////////////
// reset the zoom mode to one of `reset_zoom_mode`

void App::reset_zoom(reset_zoom_mode mode)
{
    float width_factor = (float)window_width / texture_width;
    float height_factor = (float)window_height / texture_height;
    float scale_factor{ 1.0f };

    using m = reset_zoom_mode;

    switch(mode) {

    case m::one_to_one:
        break;

    case m::shrink_to_fit:
        scale_factor = std::min(1.0f, std::min(width_factor, height_factor));
        break;

    case m::fit_to_window:
        scale_factor = std::min(width_factor, height_factor);
        break;
    }

    target_rect.w = texture_width * scale_factor;
    target_rect.h = texture_height * scale_factor;
    target_rect.x = (window_width - target_rect.w) / 2.0f;
    target_rect.y = (window_height - target_rect.h) / 2.0f;

    last_zoom_mode = mode;
    has_been_zoomed_or_dragged = false;
}

//////////////////////////////////////////////////////////////////////
// WM_KEYUP

void App::on_key_up(int vk_key)
{
    switch(vk_key) {
    case VK_SHIFT: {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(window, &p);
        shift_snap = false;
        on_mouse_move(p);
    } break;
    case VK_CONTROL:
        ctrl_snap = false;
        break;
    }
}

//////////////////////////////////////////////////////////////////////

void App::on_command(uint command)
{
    switch(command) {

    case ID_VIEW_FULLSCREEN:
        toggle_fullscreen();
        break;

    case ID_PASTE:
        on_paste();
        break;

    case ID_COPY:
        on_copy();
        break;

    case ID_ZOOM_RESET:
        reset_zoom(settings.zoom_mode);
        break;

    case ID_ZOOM_CENTER:
        center_in_window();
        break;

    case ID_SELECT_CROP:
        crop_to_selection();
        break;

    case ID_SELECT_ALL:
        select_all();
        break;

    case ID_SELECT_NONE:
        selection_active = false;
        break;

    case ID_VIEW_ALPHA:
        settings.grid_enabled = !settings.grid_enabled;
        break;

    case ID_VIEW_FIXEDGRID:
        settings.fixed_grid = !settings.fixed_grid;
        break;

    case ID_VIEW_GRIDSIZE:
        settings.grid_multiplier = (settings.grid_multiplier + 1) & 7;
        break;

    case ID_VIEW_SETBACKGROUNDCOLOR: {
        uint32 bg_color = color_to_uint32(settings.background_color);
        if(SUCCEEDED(select_color_dialog(window, bg_color, L"Choose background color"))) {
            settings.background_color = uint32_to_color(bg_color);
        }
    }

    case ID_ZOOM_1:
        settings.zoom_mode = reset_zoom_mode::one_to_one;
        reset_zoom(settings.zoom_mode);
        break;

    case ID_ZOOM_FIT:
        settings.zoom_mode = reset_zoom_mode::fit_to_window;
        reset_zoom(settings.zoom_mode);
        break;

    case ID_ZOOM_SHRINKTOFIT:
        settings.zoom_mode = reset_zoom_mode::shrink_to_fit;
        reset_zoom(settings.zoom_mode);
        break;

    case ID_ZOOM_SELECTION:
        zoom_to_selection();
        break;

    case ID_FILE_PREV:
        move_file_cursor(-1);
        break;

    case ID_FILE_NEXT:
        move_file_cursor(1);
        break;

    case ID_FILE_OPEN: {
        std::wstring selected_filename;
        if(SUCCEEDED(select_file_dialog(window, selected_filename))) {
            load_image(selected_filename.c_str());
        }
    } break;

    case ID_FILE_SAVE: {

        std::wstring filename;
        if(SUCCEEDED(save_file_dialog(window, filename))) {

            image const &img = current_image_file->img;
            HRESULT hr = save_image_file(filename.c_str(), img.pixels, img.width, img.height, img.row_pitch);
            if(FAILED(hr)) {
                MessageBox(window, windows_error_message(hr).c_str(), L"Can't save file", MB_ICONEXCLAMATION);
            } else {
                set_message(format(L"Saved %s", filename.c_str()).c_str(), 5);
            }
        }
    } break;

    case ID_EXIT:
        DestroyWindow(window);
        break;
    }
}

//////////////////////////////////////////////////////////////////////

void App::reset_settings()
{
    if(MessageBox(window, L"Reset settings to factory defaults!?", localize(IDS_AppName), MB_YESNO) == IDYES) {

        bool old_fullscreen = settings.fullscreen;
        WINDOWPLACEMENT old_windowplacement = settings.window_placement;
        settings = default_settings;
        if(old_fullscreen != settings.fullscreen) {
            toggle_fullscreen();
        }
    }
}

//////////////////////////////////////////////////////////////////////
// WM_KEYDOWN

void App::on_key_down(int vk_key, LPARAM flags)
{
    UNREFERENCED_PARAMETER(flags);

    uint f = HIWORD(flags);
    bool repeat = (f & KF_REPEAT) == KF_REPEAT;    // previous key-state flag, 1 on autorepeat

    switch(vk_key) {

    case VK_SHIFT:
        if(!repeat) {
            shift_mouse_pos = cur_mouse_pos;
            shift_snap = true;
            shift_snap_axis = shift_snap_axis_t::none;
        }
        break;

    case VK_CONTROL:
        ctrl_snap = true;
        break;
    }
}

//////////////////////////////////////////////////////////////////////
// call this repeatedly when there are no windows messages available

HRESULT App::update()
{
    m_timer.update();

    float delta_t = static_cast<float>(std::min(m_timer.delta(), 0.25));

    auto lerp = [=](float &a, float &b) {
        float d = b - a;
        if(fabsf(d) <= 1.0f) {
            a = b;
        } else {
            a += d * 20 * delta_t;
        }
    };

    lerp(current_rect.x, target_rect.x);
    lerp(current_rect.y, target_rect.y);
    lerp(current_rect.w, target_rect.w);
    lerp(current_rect.h, target_rect.h);

    if(get_mouse_buttons(settings.zoom_button)) {
        POINT old_pos{ mouse_click[settings.zoom_button].x, mouse_click[settings.zoom_button].y };
        ClientToScreen(window, &old_pos);
        SetCursorPos(old_pos.x, old_pos.y);
    }

    if(get_mouse_buttons(settings.drag_button) && !get_mouse_buttons(settings.zoom_button)) {
        current_rect.x += mouse_offset[settings.drag_button].x;
        current_rect.y += mouse_offset[settings.drag_button].y;
        target_rect = current_rect;
        has_been_zoomed_or_dragged = true;
    }

    selecting = get_mouse_buttons(settings.select_button);

    if(selecting && !get_mouse_buttons(settings.zoom_button)) {

        if(drag_selection) {

            vec2 mouse{ cur_mouse_pos };

            // texel mouse is over right now
            vec2 cur_texel_pos = vec2::floor(screen_to_texture_pos(mouse));

            // distance moved in texels since last frame
            vec2 diff = sub_point(cur_texel_pos, drag_select_pos);

            // so, we have a delta, but what to drag?
            // could be a corner, could be an edge, could be the whole selection

            float *x = &select_anchor.x;
            float *y = &select_anchor.y;

            vec2 max{ 0, 0 };
            vec2 min = select_anchor;

            if(selection_hover & sel_hover_left) {
                x = &select_anchor.x;
                max.x = select_current.x;
                min.x = 0.0f;
            } else if(selection_hover & sel_hover_right) {
                x = &select_current.x;
                max.x = (float)texture_width - 1;
                min.x = select_anchor.x;
            }

            if(selection_hover & sel_hover_top) {
                y = &select_anchor.y;
                max.y = select_current.y;
                min.y = 0.0f;
            } else if(selection_hover & sel_hover_bottom) {
                y = &select_current.y;
                max.y = (float)texture_height - 1;
                min.y = select_anchor.y;
            }

            if(selection_hover == sel_hover_inside) {
                min = { 0, 0 };
                vec2 ts = sub_point(texture_size(), { 1, 1 });
                max = sub_point(ts, selection_size);
            }

            // get actual movement
            vec2 old{ *x, *y };

            vec2 np = vec2::clamp(min, add_point(old, diff), max);

            // actual distance moved after clamp
            vec2 delta = sub_point(np, old);

            if(x != null) {
                *x = np.x;
            }
            if(y != null) {
                *y = np.y;
            }

            if(selection_hover == sel_hover_inside) {
                select_current = add_point(select_anchor, selection_size);
            }

            // remember texel coordinate for next frame
            drag_select_pos = add_point(drag_select_pos, delta);

        } else {
            select_current = clamp_to_texture(screen_to_texture_pos(cur_mouse_pos));

            // force the selection to be square if ctrl is held
            if(ctrl_snap) {

                // selection dimensions
                vec2 d = sub_point(select_current, select_anchor);

                // biggest axis
                float m = std::max(fabsf(d.x), fabsf(d.y));

                // in the right quadrant
                vec2 s{ m, m };
                if(d.x < 0) {
                    s.x = -s.x;
                }
                if(d.y < 0) {
                    s.y = -s.y;
                }

                // now clamp to texture
                vec2 n = sub_point(select_anchor, clamp_to_texture(add_point(s, select_anchor)));

                // and get new clamped dimensions
                d = sub_point(select_current, select_anchor);

                // this time snap to the smallest axis
                m = std::min(fabsf(n.x), fabsf(n.y));

                // in the right quadrant
                s = vec2{ m, m };
                if(d.x < 0) {
                    s.x = -s.x;
                }
                if(d.y < 0) {
                    s.y = -s.y;
                }

                select_current = add_point(s, select_anchor);
            }
        }
    }
    memset(mouse_offset, 0, sizeof(mouse_offset));

    CHK_HR(render());

    if(m_frame > 1 && (m_timer.wall_time() > 0.25 || image_texture.Get() != null)) {
        ShowWindow(window, SW_SHOW);
    }

    m_frame += 1;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// select the whole image

void App::select_all()
{
    if(image_texture.Get() != 0) {
        drag_select_pos = { 0, 0 };
        select_anchor = { 0, 0 };
        select_current = sub_point(texture_size(), { 1, 1 });
        selection_size = sub_point(select_current, select_anchor);
        selection_active = true;
        selecting = false;
        drag_selection = false;
    }
}

//////////////////////////////////////////////////////////////////////
// crop the image to the selection

HRESULT App::crop_to_selection()
{
    if(!selection_active) {
        return E_ABORT;
    }

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// copy the current selection into clipboard as a DIBV5

HRESULT App::copy_selection()
{
    if(!selection_active) {
        return E_ABORT;
    }
    vec2 tl = vec2::min(select_anchor, select_current);
    vec2 br = vec2::max(select_anchor, select_current);

    // 1. Copy region into a texture

    D3D11_BOX copy_box;
    copy_box.left = (int)tl.x;
    copy_box.right = std::min(texture_width, (int)br.x + 1);
    copy_box.top = (int)tl.y;
    copy_box.bottom = std::min(texture_height, (int)br.y + 1);
    copy_box.back = 1;
    copy_box.front = 0;

    int w = copy_box.right - copy_box.left;
    int h = copy_box.bottom - copy_box.top;

    if(w < 1 || h < 1) {
        return E_BOUNDS;
    }

    D3D11_TEXTURE2D_DESC desc;
    image_texture->GetDesc(&desc);

    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> tex;

    CHK_HR(d3d_device->CreateTexture2D(&desc, null, &tex));
    d3d_name(tex);

    d3d_context->CopySubresourceRegion(tex.Get(), 0, 0, 0, 0, image_texture.Get(), 0, &copy_box);

    d3d_context->Flush();

    // 2. Map the texture so we can access the pixels

    D3D11_MAPPED_SUBRESOURCE mapped_resource{};
    CHK_HR(d3d_context->Map(tex.Get(), 0, D3D11_MAP_READ, 0, &mapped_resource));
    defer(d3d_context->Unmap(tex.Get(), 0));

    uint32 pitch = mapped_resource.RowPitch;

    size_t pixel_size = 4llu;
    size_t pixel_buffer_size = pixel_size * (size_t)w * h;

    // 3. Create CF_DIBV5 clipformat

    size_t dibv5_buffer_size = sizeof(BITMAPV5HEADER) + pixel_buffer_size;
    HANDLE dibv5_data = GlobalAlloc(GHND | GMEM_SHARE, dibv5_buffer_size);
    if(dibv5_data == null) {
        return HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);
    }
    byte *dibv5_buffer = reinterpret_cast<byte *>(GlobalLock(dibv5_data));
    if(dibv5_buffer == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    BITMAPV5HEADER *bmiv5 = reinterpret_cast<BITMAPV5HEADER *>(dibv5_buffer);

    memset(bmiv5, 0, sizeof(BITMAPV5HEADER));
    bmiv5->bV5Size = sizeof(BITMAPV5HEADER);
    bmiv5->bV5Width = w;
    bmiv5->bV5Height = -h;
    bmiv5->bV5Planes = 1;
    bmiv5->bV5BitCount = 32;
    bmiv5->bV5Compression = BI_RGB;
    bmiv5->bV5SizeImage = 4 * w * h;
    bmiv5->bV5RedMask = 0x00ff0000;
    bmiv5->bV5GreenMask = 0x0000ff00;
    bmiv5->bV5BlueMask = 0x000000ff;
    bmiv5->bV5AlphaMask = 0xff000000;
    bmiv5->bV5CSType = LCS_WINDOWS_COLOR_SPACE;
    bmiv5->bV5Intent = LCS_GM_GRAPHICS;

    byte *pixels = dibv5_buffer + sizeof(BITMAPV5HEADER);

    // got the texels, copy into the DIB for the clipboard, swapping R, B channels
    byte *row = pixels;
    byte *src = reinterpret_cast<byte *>(mapped_resource.pData);
    for(int y = 0; y < h; ++y) {
        uint32 *s = reinterpret_cast<uint32 *>(src);
        uint32 *d = reinterpret_cast<uint32 *>(row);
        for(int x = 0; x < w; ++x) {
            uint32 p = *s++;
            uint32 r = (p >> 0) & 0xff;
            uint32 g = (p >> 8) & 0xff;
            uint32 b = (p >> 16) & 0xff;
            uint32 a = (p >> 24) & 0xff;
            r <<= 0;
            g <<= 8;
            b <<= 16;
            a <<= 24;
            *d++ = a | b | g | r;
        }
        src += pitch;
        row += w * 4llu;
    }

    GlobalUnlock(dibv5_data);

    // 4. Create CF_DIB clipformat

    int stride = (((w * 3) + 3) & -4);
    int dib_img_size = stride * h;

    size_t dib_buffer_size = sizeof(BITMAPINFOHEADER) + dib_img_size;
    HANDLE dib_data = GlobalAlloc(GHND | GMEM_SHARE, dib_buffer_size);
    if(dib_data == null) {
        return HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);
    }
    byte *dib_buffer = reinterpret_cast<byte *>(GlobalLock(dib_data));
    if(dib_buffer == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    BITMAPINFOHEADER *bmi = reinterpret_cast<BITMAPINFOHEADER *>(dib_buffer);

    memset(bmi, 0, sizeof(BITMAPINFOHEADER));
    bmi->biSize = sizeof(BITMAPINFOHEADER);
    bmi->biBitCount = 24;
    bmi->biCompression = BI_RGB;
    bmi->biWidth = w;
    bmi->biHeight = -h;
    bmi->biPlanes = 1;
    bmi->biSizeImage = dib_img_size;

    byte *dib_pixels = dib_buffer + sizeof(BITMAPINFOHEADER);

    byte *dib_row = dib_pixels;
    byte *dib_src = reinterpret_cast<byte *>(mapped_resource.pData);
    for(int y = 0; y < h; ++y) {
        uint32 *s = reinterpret_cast<uint32 *>(dib_src);
        byte *d = dib_row;
        for(int x = 0; x < w; ++x) {
            uint32 p = *s++;
            byte r = (p >> 0) & 0xff;
            byte g = (p >> 8) & 0xff;
            byte b = (p >> 16) & 0xff;
            *d++ = r;
            *d++ = g;
            *d++ = b;
        }
        dib_src += mapped_resource.RowPitch;
        dib_row += stride;
    }

    GlobalUnlock(dib_data);

    // 5. stang them into the clipboard

    CHK_BOOL(OpenClipboard(null));
    defer(CloseClipboard());

    CHK_BOOL(EmptyClipboard());

    CHK_BOOL(SetClipboardData(CF_DIBV5, dibv5_data));
    CHK_BOOL(SetClipboardData(CF_DIB, dib_data));

    return copy_pixels_as_png(pixels, w, h);
}

//////////////////////////////////////////////////////////////////////

HRESULT App::save_image(image_file *f, std::wstring const &filename)
{
    return save_image_file(filename.c_str(), f->img.pixels, f->img.width, f->img.height, f->img.row_pitch);
}

//////////////////////////////////////////////////////////////////////
// get dimensions of a string including padding

HRESULT App::measure_string(std::wstring const &text, IDWriteTextFormat *format, float padding, vec2 &size) const
{
    ComPtr<IDWriteTextLayout> text_layout;

    CHK_HR(dwrite_factory->CreateTextLayout(
        text.c_str(), (UINT32)text.size(), format, (float)window_width * 2, (float)window_height * 2, &text_layout));

    DWRITE_TEXT_METRICS m;
    CHK_HR(text_layout->GetMetrics(&m));

    size.x = m.width + padding * 4;
    size.y = m.height + padding;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// draw some text with a box round it

HRESULT App::draw_string(std::wstring const &text,
                         IDWriteTextFormat *format,
                         vec2 pos,
                         vec2 pivot,
                         float opacity,
                         float corner_radius,
                         float padding)
{
    corner_radius = dpi_scale(corner_radius);
    padding = dpi_scale(padding);
    pos.x = dpi_unscale(pos.x);
    pos.y = dpi_unscale(pos.y);

    ComPtr<IDWriteTextLayout> text_layout;

    // This sucks that you have to create and destroy a com object to draw a text string

    CHK_HR(dwrite_factory->CreateTextLayout(
        text.c_str(), (UINT32)text.size(), format, (float)window_width * 2, (float)window_height * 2, &text_layout));

    // work out baseline as a fraction of the height
    // scale the outline box a bit

    DWRITE_TEXT_METRICS m;
    CHK_HR(text_layout->GetMetrics(&m));

    float w = m.width;
    float h = m.height;
    float r = std::min(h / 3, corner_radius);
    float text_outline_width = 1.0f;
    D2D1_POINT_2F text_pos{ pos.x - (w * pivot.x), pos.y - (h * pivot.y) };

    D2D1_ROUNDED_RECT rr;
    rr.rect = D2D1_RECT_F{ text_pos.x - padding * 2,
                           text_pos.y - padding * 0.5f,
                           text_pos.x + w + padding * 2,
                           text_pos.y + h + padding * 0.5f };
    rr.radiusX = r;
    rr.radiusY = r;

    text_fg_brush->SetOpacity(opacity);
    text_bg_brush->SetOpacity(opacity);
    text_outline_brush->SetOpacity(opacity);

    d2d_render_target->FillRoundedRectangle(rr, text_bg_brush.Get());

    d2d_render_target->DrawRoundedRectangle(rr, text_outline_brush.Get(), text_outline_width);

    d2d_render_target->DrawTextLayout(text_pos, text_layout.Get(), text_fg_brush.Get());

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// push shader_constants to GPU

HRESULT App::update_constants()
{
    D3D11_MAPPED_SUBRESOURCE mapped_subresource;
    CHK_HR(d3d_context->Map(constant_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource));
    memcpy(mapped_subresource.pData, &shader_constants, sizeof(shader_const_t));
    d3d_context->Unmap(constant_buffer.Get(), 0);
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// render a frame

HRESULT App::render()
{
    clear();

    if(image_texture.Get() != null) {

        vec2 scale = div_point(vec2{ 2, 2 }, vec2{ (float)window_width, (float)window_height });

        auto get_scale = [&scale](vec2 f) { return mul_point({ f.x, -f.y }, scale); };
        auto get_offset = [&scale](vec2 f) { return vec2{ f.x * scale.x - 1, 1 - (f.y * scale.y) }; };

        vec2 grid_pos{ 0, 0 };

        float gm = (1 << settings.grid_multiplier) / 4.0f;
        float gs = settings.grid_size * current_rect.w / texture_width * gm;

        if(gs < 4) {
            gs = 0;
        }

        if(settings.fixed_grid) {
            vec2 g2{ 2.0f * gs, 2.0f * gs };
            grid_pos = sub_point(g2, vec2::mod(current_rect.top_left(), g2));
        }

        if(settings.grid_enabled) {
            shader_constants.grid_color[0] = settings.grid_color_1;
            shader_constants.grid_color[1] = settings.grid_color_2;
            shader_constants.grid_color[2] = settings.grid_color_2;
            shader_constants.grid_color[3] = settings.grid_color_1;
        } else {
            shader_constants.grid_color[0] = settings.background_color;
            shader_constants.grid_color[1] = settings.background_color;
            shader_constants.grid_color[2] = settings.background_color;
            shader_constants.grid_color[3] = settings.background_color;
        }

        shader_constants.select_color[0] = settings.select_fill_color;
        shader_constants.select_color[1] = settings.select_outline_color1;
        shader_constants.select_color[2] = settings.select_outline_color2;

        vec2 top_left = vec2::floor(current_rect.top_left());
        vec2 rect_size = vec2::floor(current_rect.size());

        shader_constants.scale = get_scale(rect_size);
        shader_constants.offset = get_offset(top_left);

        shader_constants.grid_size = gs;
        shader_constants.grid_offset = grid_pos;

        CHK_HR(update_constants());

        ///// common state

        d3d_context->RSSetState(rasterizer_state.Get());
        d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        d3d_context->VSSetShader(vertex_shader.Get(), null, 0);
        d3d_context->VSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
        d3d_context->PSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());

        ///// draw grid

        d3d_context->OMSetBlendState(null, null, 0xffffffff);
        d3d_context->PSSetShader(grid_shader.Get(), null, 0);
        d3d_context->Draw(4, 0);

        ///// draw image

        d3d_context->OMSetBlendState(blend_state.Get(), null, 0xffffffff);
        d3d_context->PSSetShader(pixel_shader.Get(), null, 0);
        d3d_context->PSSetShaderResources(0, 1, image_texture_view.GetAddressOf());
        d3d_context->PSSetSamplers(0, 1, sampler_state.GetAddressOf());
        d3d_context->Draw(4, 0);

        ///// drag selection rectangle

        vec2 sa = select_anchor;
        vec2 sc = select_current;

        if(selection_active) {

            // convert selection to screen coords

            vec2 s_tl = vec2::min(sa, sc);
            vec2 s_br = vec2::max(sa, sc);

            // using doubles here because large images
            // at some zoom levels cause rounding errors
            // which this doesn't completely fix but
            // it makes it better (less shimmering on
            // select rectangle position when zooming)

            double xs = (double)rect_size.x / (double)texture_width;
            double ys = (double)rect_size.y / (double)texture_height;

            double tx = floorf(s_tl.x + 0) * xs + top_left.x;
            double ty = floorf(s_tl.y + 0) * ys + top_left.y;
            double bx = floorf(s_br.x + 1) * xs + top_left.x;
            double by = floorf(s_br.y + 1) * ys + top_left.y;

            s_tl = { (float)tx, (float)ty };
            s_br = { (float)bx, (float)by };

            float sbw = (float)settings.select_border_width;

            float select_l = floorf(std::max(top_left.x, s_tl.x)) - sbw;
            float select_t = floorf(std::max(top_left.y, s_tl.y)) - sbw;

            float select_r = floorf(std::min(top_left.x + rect_size.x - 1, s_br.x)) + sbw;
            float select_b = floorf(std::min(top_left.y + rect_size.y - 1, s_br.y)) + sbw;

            // always draw at least something
            float select_w = std::max(1.0f, select_r - select_l + 0.5f);
            float select_h = std::max(1.0f, select_b - select_t + 0.5f);

            // border width clamp if rectangle is too small
            int min_s = (int)std::min(select_w, select_h) - settings.select_border_width * 2;
            int min_t = std::min(settings.select_border_width, min_s);

            int select_border_width = std::max(1, min_t);

            shader_constants.select_border_width = select_border_width;    // set viewport coords for the vertex shader

            shader_constants.scale = get_scale({ select_w, select_h });
            shader_constants.offset = get_offset({ select_l, select_t });

            // set rect coords for the pixel shader
            shader_constants.rect_f[0] = (int)select_l;
            shader_constants.rect_f[1] = (int)select_t;
            shader_constants.rect_f[2] = (int)select_r - 1;
            shader_constants.rect_f[3] = (int)select_b - 1;

            shader_constants.frame = m_frame;

            shader_constants.dash_length = settings.dash_length - 1;
            shader_constants.half_dash_length = settings.dash_length / 2;

            // flash the selection color when they copy
            float f = (float)std::min(1.0, (m_timer.current_time - copy_timestamp) / 0.12);
            shader_constants.select_color[0] = XMVectorLerp(vec4{ 1, 1, 1, 0.5f }, settings.select_fill_color, f);

            CHK_HR(update_constants());

            d3d_context->PSSetShader(rect_shader.Get(), null, 0);
            d3d_context->Draw(4, 0);
        }

        ///// draw crosshairs when alt key is held down

        bool crosshairs_active = false;

        if(is_key_down(VK_LMENU) || is_key_down(VK_RMENU)) {

            crosshairs_active = true;

            d3d_context->PSSetShader(grid_shader.Get(), null, 0);

            shader_constants.grid_color[0] = settings.crosshair_color1;
            shader_constants.grid_color[3] = settings.crosshair_color1;
            shader_constants.grid_color[1] = settings.crosshair_color2;
            shader_constants.grid_color[2] = settings.crosshair_color2;

            float crosshair_grid_size = (float)settings.dash_length;
            shader_constants.grid_size = crosshair_grid_size / 2;

            vec2 g2{ crosshair_grid_size, crosshair_grid_size };

            // get top left and bottom right screen pos of texel under mouse curser
            vec2 p = clamp_to_texture(screen_to_texture_pos(cur_mouse_pos));
            vec2 sp1 = texture_to_screen_pos(p);
            vec2 sp2 = sub_point(add_point(sp1, texel_size()), { 1, 1 });

            if(fabsf(sp1.x - sp2.x) < 1.0f) {
                sp2.x = sp1.x + 1;
            }
            if(fabsf(sp1.y - sp2.y) < 1.0f) {
                sp2.y = sp1.y + 1;
            }

            // draw a vertical crosshair line

            float blip = (float)((m_frame >> 0) & 31);
            grid_pos = sub_point(g2, vec2::mod({ sp1.x + blip, sp1.y + blip }, g2));

            shader_constants.grid_offset = { 0, grid_pos.y };
            shader_constants.offset = get_offset({ sp1.x, 0 });
            shader_constants.scale = get_scale({ sp2.x - sp1.x, (float)window_height });
            CHK_HR(update_constants());
            d3d_context->Draw(4, 0);

            // draw a horizontal crosshair line

            shader_constants.grid_offset = { grid_pos.x, 0 };
            shader_constants.offset = get_offset({ 0, sp1.y });
            shader_constants.scale = get_scale({ (float)window_width, sp2.y - sp1.y });
            CHK_HR(update_constants());
            d3d_context->Draw(4, 0);
        }

        // spinner if file load is slow
        if(false) {
            float w = (float)m_timer.wall_time();
            float t1 = w * 9 + sinf(w * 7);
            float t2 = w * 13;
            float r1 = 23;
            float r2 = 19;
            vec2 sm = mul_point(window_size(), { 0.5f, 0.5f });
            vec2 mid = add_point(sm, { sinf(t1) * r1, cosf(t1) * r1 });
            vec2 off = add_point(sm, { sinf(t2) * r2, cosf(t2) * r2 });
            vec2 mn = sub_point(vec2::min(mid, off), { 12.0f, 12.0f });
            vec2 mx = sub_point(add_point(vec2::max(mid, off), { 12.0f, 12.0f }), mn);

            shader_constants.offset = get_offset(mn);
            shader_constants.scale = get_scale(mx);

            shader_constants.glowing_line_s = mid;
            shader_constants.glowing_line_e = off;
            CHK_HR(update_constants());
            d3d_context->PSSetShader(spinner_shader.Get(), null, 0);
            d3d_context->Draw(4, 0);
        }

        ///// draw text

        d2d_render_target->BeginDraw();

        if(crosshairs_active) {
            vec2 p = clamp_to_texture(screen_to_texture_pos(cur_mouse_pos));
            std::wstring text{ format(L"X %d Y %d", (int)p.x, (int)p.y) };
            vec2 screen_pos = texture_to_screen_pos(p);
            screen_pos.x -= dpi_scale(12);
            screen_pos.y += dpi_scale(8);
            draw_string(
                text, small_text_format.Get(), screen_pos, { 1, 0 }, 1.0f, small_label_padding, small_label_padding);
        }

        if(selection_active) {
            vec2 tl = vec2::min(sa, sc);
            vec2 br = vec2::max(sa, sc);
            vec2 s_tl = texture_to_screen_pos_unclamped(tl);
            vec2 s_br = texture_to_screen_pos_unclamped({ br.x + 1, br.y + 1 });
            s_tl.x -= dpi_scale(12);
            s_tl.y -= dpi_scale(8);
            s_br.x += dpi_scale(12);
            s_br.y += dpi_scale(8);
            POINT s_dim{ (int)(floorf(br.x) - floorf(tl.x)) + 1, (int)(floorf(br.y) - floorf(tl.y)) + 1 };
            draw_string(
                format(L"X %d Y %d", (int)tl.x, (int)tl.y), small_text_format.Get(), s_tl, { 1, 1 }, 1.0f, 2, 2);
            draw_string(format(L"W %d H %d", s_dim.x, s_dim.y), small_text_format.Get(), s_br, { 0, 0 }, 1.0f, 2, 2);
        }

        if(!current_message.empty()) {
            float message_alpha = 0.0f;
            if(message_fade_time != 0.0f) {
                message_alpha = (float)((m_timer.wall_time() - message_timestamp) / message_fade_time);
            }
            if(message_alpha <= 1) {
                message_alpha = 1 - powf(message_alpha, 16);
                vec2 pos{ window_width / 2.0f, window_height - 12.0f };
                draw_string(format(L"%s", current_message.c_str()),
                            large_text_format.Get(),
                            pos,
                            { 0.5f, 1.0f },
                            message_alpha);
            }
        }

        CHK_HR(d2d_render_target->EndDraw());
    }

    CHK_HR(present());

    return S_OK;

    // if we copied into the copy texture, put it into the clipboard now
}

//////////////////////////////////////////////////////////////////////
// clear the backbuffer

void App::clear()
{
    d3d_context->ClearRenderTargetView(rendertarget_view.Get(),
                                       reinterpret_cast<float const *>(&settings.border_color));

    d3d_context->OMSetRenderTargets(1, rendertarget_view.GetAddressOf(), null);

    vec2 ws = window_size();
    CD3D11_VIEWPORT viewport(0.0f, 0.0f, ws.x, ws.y);
    d3d_context->RSSetViewports(1, &viewport);
}

//////////////////////////////////////////////////////////////////////
// swap back/front buffers

HRESULT App::present()
{
    HRESULT hr = swap_chain->Present(1, 0);

    if(hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        hr = on_device_lost();
    }

    return hr;
}

//////////////////////////////////////////////////////////////////////
// WM_CLOSING

HRESULT App::on_closing()
{
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// WM_ACTIVATEAPP

void App::on_activated()
{
    shift_snap = false;
}

//////////////////////////////////////////////////////////////////////
// WM_ACTIVATEAPP

void App::on_deactivated()
{
    mouse_grab = 0;
}

//////////////////////////////////////////////////////////////////////
// App is being power-suspended (or minimized).

void App::on_suspending()
{
}

//////////////////////////////////////////////////////////////////////
// App is being power-resumed (or returning from minimize).

void App::on_resuming()
{
    m_timer.reset();
}

//////////////////////////////////////////////////////////////////////
// WM_WINDOWPOSCHANGING

HRESULT App::on_window_pos_changing(WINDOWPOS *new_pos)
{
    UNREFERENCED_PARAMETER(new_pos);
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// WM_EXITSIZEMOVE

HRESULT App::on_window_size_changed(int width, int height)
{
    window_width = std::max(width, 1);
    window_height = std::max(height, 1);

    CHK_HR(create_resources());

    if(!settings.fullscreen) {
        GetWindowPlacement(window, &settings.window_placement);
        settings.first_run = false;
    }

    // recenter the 'middle' texel on the new size
    // i.e. the texel in the middle of the window
    // this is ok if [0 <= texel < texture_size]
    // because there will actually be a texel there
    // but not so great otherwise... hmmm
    current_rect.x = window_width / 2.0f - (old_window_width / 2.0f - current_rect.x);
    current_rect.y = window_height / 2.0f - (old_window_height / 2.0f - current_rect.y);

    target_rect = current_rect;

    old_window_width = window_width;
    old_window_height = window_height;

    if(!has_been_zoomed_or_dragged) {
        reset_zoom(last_zoom_mode);
        current_rect = target_rect;
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// create the d3d device

HRESULT App::create_device()
{
    UINT create_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
    create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    static D3D_FEATURE_LEVEL const feature_levels[] = { D3D_FEATURE_LEVEL_11_1 };

    D3D_FEATURE_LEVEL feature_level{ D3D_FEATURE_LEVEL_11_1 };

    uint32 num_feature_levels = (UINT)std::size(feature_levels);

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    CHK_HR(D3D11CreateDevice(null,
                             D3D_DRIVER_TYPE_HARDWARE,
                             null,
                             create_flags,
                             feature_levels,
                             num_feature_levels,
                             D3D11_SDK_VERSION,
                             &device,
                             &feature_level,
                             &context));

#if defined(_DEBUG)
    if(SUCCEEDED(device.As(&d3d_debug))) {
        ComPtr<ID3D11InfoQueue> d3d_info_queue;
        if(SUCCEEDED(d3d_debug.As(&d3d_info_queue))) {
            d3d_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            d3d_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
            D3D11_MESSAGE_ID hide[] = {
                D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
                // TODO: Add more message IDs here as needed.
            };
            D3D11_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
            filter.DenyList.pIDList = hide;
            d3d_info_queue->AddStorageFilterEntries(&filter);
        }
    }
#endif

    CHK_HR(device.As(&d3d_device));
    CHK_HR(context.As(&d3d_context));

    CHK_HR(
        d3d_device->CreateVertexShader(vs_rectangle_shaderbin, sizeof(vs_rectangle_shaderbin), null, &vertex_shader));
    d3d_name(vertex_shader);

    CHK_HR(d3d_device->CreatePixelShader(ps_drawimage_shaderbin, sizeof(ps_drawimage_shaderbin), null, &pixel_shader));
    CHK_HR(d3d_device->CreatePixelShader(ps_drawrect_shaderbin, sizeof(ps_drawrect_shaderbin), null, &rect_shader));
    CHK_HR(d3d_device->CreatePixelShader(ps_drawgrid_shaderbin, sizeof(ps_drawgrid_shaderbin), null, &grid_shader));
    CHK_HR(d3d_device->CreatePixelShader(ps_solid_shaderbin, sizeof(ps_solid_shaderbin), null, &solid_shader));
    CHK_HR(d3d_device->CreatePixelShader(ps_spinner_shaderbin, sizeof(ps_spinner_shaderbin), null, &spinner_shader));

    d3d_name(pixel_shader);
    d3d_name(rect_shader);
    d3d_name(grid_shader);
    d3d_name(solid_shader);

    D3D11_SAMPLER_DESC sampler_desc{ CD3D11_SAMPLER_DESC(D3D11_DEFAULT) };

    sampler_desc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

    CHK_HR(d3d_device->CreateSamplerState(&sampler_desc, &sampler_state));
    d3d_name(sampler_state);

    D3D11_BUFFER_DESC constant_buffer_desc{};

    constant_buffer_desc.ByteWidth = (sizeof(shader_const_t) + 0xf) & 0xfffffff0;
    constant_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constant_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    CHK_HR(d3d_device->CreateBuffer(&constant_buffer_desc, null, &constant_buffer));
    d3d_name(constant_buffer);

    D3D11_RASTERIZER_DESC rasterizer_desc{};

    rasterizer_desc.FillMode = D3D11_FILL_SOLID;
    rasterizer_desc.CullMode = D3D11_CULL_NONE;

    CHK_HR(d3d_device->CreateRasterizerState(&rasterizer_desc, &rasterizer_state));
    d3d_name(rasterizer_state);

    // DirectWrite / Direct2D init

    CHK_HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&d2d_factory)));

    auto dwf = reinterpret_cast<IUnknown **>(dwrite_factory.ReleaseAndGetAddressOf());
    CHK_HR(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), dwf));

    CHK_HR(font_context.Initialize(dwrite_factory.Get()));

    UINT const fontResourceIDs[] = { IDR_FONT_NOTO, IDR_FONT_ROBOTO };

    CHK_HR(font_context.CreateFontCollection(fontResourceIDs, sizeof(fontResourceIDs), &font_collection));

    dpi = GetWindowDPI(window);

    CHK_HR(create_text_formats());

    CHK_HR(measure_string(L"X 9999 Y 9999", small_text_format.Get(), small_label_padding, small_label_size));

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// create window size dependent resources

HRESULT App::create_resources()
{
    if(d3d_context.Get() == null) {
        create_device();
    }

    ID3D11RenderTargetView *nullViews[] = { null };
    d3d_context->OMSetRenderTargets(static_cast<UINT>(std::size(nullViews)), nullViews, null);
    d3d_context->Flush();

    rendertarget_view.Reset();
    d2d_render_target.Reset();

    const UINT backBufferWidth = static_cast<UINT>(window_width);
    const UINT backBufferHeight = static_cast<UINT>(window_height);
    const DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;    // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    constexpr UINT backBufferCount = 2;
    const DXGI_SCALING scaling_mode = DXGI_SCALING_STRETCH;
    const DXGI_SWAP_EFFECT swap_effect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    const DXGI_SWAP_CHAIN_FLAG swap_flags = (DXGI_SWAP_CHAIN_FLAG)0;

    // If the swap chain already exists, resize it, otherwise create one.
    if(swap_chain.Get() != null) {
        HRESULT hr = swap_chain->ResizeBuffers(backBufferCount, backBufferWidth, backBufferHeight, backBufferFormat, 0);

        if(hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {

            CHK_HR(on_device_lost());

            // OnDeviceLost will set up the new device.
            return S_OK;
        }
    } else {

        // First, retrieve the underlying DXGI Device from the D3D Device.
        ComPtr<IDXGIDevice1> dxgiDevice;
        CHK_HR(d3d_device.As(&dxgiDevice));

        // Identify the physical adapter (GPU or card) this device is running on.
        ComPtr<IDXGIAdapter> dxgiAdapter;
        CHK_HR(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));

        // And obtain the factory object that created it.
        ComPtr<IDXGIFactory2> dxgiFactory;
        CHK_HR(dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.GetAddressOf())));

        // Create a descriptor for the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = backBufferFormat;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = backBufferCount;
        swapChainDesc.SwapEffect = swap_effect;
        swapChainDesc.Scaling = scaling_mode;
        swapChainDesc.Flags = swap_flags;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
        fsSwapChainDesc.Windowed = TRUE;

        CHK_HR(dxgiFactory->CreateSwapChainForComposition(d3d_device.Get(), &swapChainDesc, NULL, &swap_chain));

        CHK_HR(DCompositionCreateDevice(NULL, IID_PPV_ARGS(&dcomp)));
        CHK_HR(dcomp->CreateTargetForHwnd(window, FALSE, &target));
        CHK_HR(dcomp->CreateVisual(&visual));
        CHK_HR(target->SetRoot(visual.Get()));
        CHK_HR(visual->SetContent(swap_chain.Get()));
        CHK_HR(dcomp->Commit());
    }

    ComPtr<ID3D11Texture2D> backBuffer;
    CHK_HR(swap_chain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())));
    d3d_name(backBuffer);

    CD3D11_RENDER_TARGET_VIEW_DESC desc(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_B8G8R8A8_UNORM);

    CHK_HR(d3d_device->CreateRenderTargetView(backBuffer.Get(), &desc, &rendertarget_view));
    d3d_name(rendertarget_view);

    D3D11_BLEND_DESC blend_desc{ 0 };

    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    CHK_HR(d3d_device->CreateBlendState(&blend_desc, &blend_state));
    d3d_name(blend_state);

    // Direct 2D init

    ComPtr<IDXGISurface> render_surface;
    CHK_HR(swap_chain->GetBuffer(0, IID_PPV_ARGS(render_surface.GetAddressOf())));

    D2D1_RENDER_TARGET_PROPERTIES props;
    props.dpiX = dpi;
    props.dpiY = dpi;
    props.pixelFormat.format = DXGI_FORMAT_UNKNOWN;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
    props.type = D2D1_RENDER_TARGET_TYPE_HARDWARE;
    props.usage = D2D1_RENDER_TARGET_USAGE_NONE;

    CHK_HR(d2d_factory->CreateDxgiSurfaceRenderTarget(render_surface.Get(), &props, d2d_render_target.GetAddressOf()));

    CHK_HR(d2d_render_target->CreateSolidColorBrush(D2D1_COLOR_F{ 1, 1, 1, 0.9f }, &text_fg_brush));
    CHK_HR(d2d_render_target->CreateSolidColorBrush(D2D1_COLOR_F{ 1, 1, 1, 0.25f }, &text_outline_brush));
    CHK_HR(d2d_render_target->CreateSolidColorBrush(D2D1_COLOR_F{ 0, 0, 0, 0.4f }, &text_bg_brush));

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// create the d2d text formats

HRESULT App::create_text_formats()
{
    float large_font_size = dpi_scale(16.0f);
    float small_font_size = dpi_scale(12.0f);

    auto weight = DWRITE_FONT_WEIGHT_REGULAR;
    auto style = DWRITE_FONT_STYLE_NORMAL;
    auto stretch = DWRITE_FONT_STRETCH_NORMAL;

    CHK_HR(dwrite_factory->CreateTextFormat(small_font_family_name,
                                            font_collection.Get(),
                                            weight,
                                            style,
                                            stretch,
                                            large_font_size,
                                            L"en-us",
                                            &large_text_format));
    CHK_HR(dwrite_factory->CreateTextFormat(mono_font_family_name,
                                            font_collection.Get(),
                                            weight,
                                            style,
                                            stretch,
                                            small_font_size,
                                            L"en-us",
                                            &small_text_format));

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// deal with DPI changed

HRESULT App::on_dpi_changed(UINT new_dpi, rect *new_rect)
{
    if(!ignore_dpi_for_a_moment) {

        current_rect.w = (current_rect.w * new_dpi) / dpi;
        current_rect.h = (current_rect.h * new_dpi) / dpi;

        dpi = (float)new_dpi;

        create_text_formats();

        MoveWindow(window, new_rect->x(), new_rect->y(), new_rect->w(), new_rect->h(), true);
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// recreate device and recreate current image texture if necessary

HRESULT App::on_device_lost()
{
    CHK_HR(create_device());

    CHK_HR(create_resources());

    if(current_image_file != null) {
        CHK_HR(display_image(current_image_file));
    }

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// user dropped something on the window, try to load it as a file

HRESULT App::on_drop_shell_item(IShellItemArray *psia, DWORD grfKeyState)
{
    UNREFERENCED_PARAMETER(grfKeyState);
    ComPtr<IShellItem> shell_item;
    CHK_HR(psia->GetItemAt(0, &shell_item));
    PWSTR path{};
    CHK_HR(shell_item->GetDisplayName(SIGDN_FILESYSPATH, &path));
    defer(CoTaskMemFree(path));
    return load_image(path);
}

//////////////////////////////////////////////////////////////////////
// they dropped something that cam be interpreted as text
// if it exists as a file, try to load it
// otherwise.... no dice I guess

HRESULT App::on_drop_string(wchar const *str)
{
    std::wstring bare_name = strip_quotes(str);
    if(file_exists(bare_name.c_str())) {
        return load_image(bare_name.c_str());
    }
    set_message(format(L"Can't load %s", bare_name.c_str()).c_str(), 2.0f);
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}

//////////////////////////////////////////////////////////////////////
// call this last thing before the end of main

void App::on_process_exit()
{
    settings.save();

    // set global quit event
    SetEvent(quit_event);

    // wait for thread pool to drain
    thread_pool.cleanup();

    // then cleanup
    CloseHandle(quit_event);
    CloseHandle(window_created_event);
}
