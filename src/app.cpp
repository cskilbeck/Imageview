//////////////////////////////////////////////////////////////////////
//
// monitor DPI / orientation awareness
// show message if file load is slow
// settings dialog
// customise keyboard shortcuts / help screen
// localization
// Windows 7 support
// installation/file associations
// image decode in the loader thread?
//
// slippery slope:
// 'R' to rotate it and then, save?
// heif support (https://nokiatech.github.io/heif) ? or link to store app
// load images from URLs which are dragged/pasted?
//
//////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "app.h"

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

#define d3d_name(x) set_d3d_debug_name(x, #x)
};    // namespace

LPCWSTR App::window_class = L"ImageViewWindowClass_2DAE134A-7E46-4E75-9DFA-207695F48699";

ComPtr<ID3D11Debug> App::d3d_debug;

std::unordered_map<std::wstring, App::file_loader *> App::loaded_files;
std::unordered_map<std::wstring, App::file_loader *> App::loading_files;

//////////////////////////////////////////////////////////////////////
// cursors for hovering over rectangle interior/corners/edges
// see selection_hover_t

App::cursor_def App::sel_hover_cursors[16] = {
    { App::cursor_type::user, MAKEINTRESOURCE(IDC_CURSOR_HAND) },    // 0 - inside
    { App::cursor_type::system, IDC_SIZEWE },                        // 1 - left
    { App::cursor_type::system, IDC_SIZEWE },                        // 2 - right
    { App::cursor_type::system, IDC_ARROW },                         // 3 - left and right xx shouldn't be possible
    { App::cursor_type::system, IDC_SIZENS },                        // 4 - top
    { App::cursor_type::system, IDC_SIZENWSE },                      // 5 - left and top
    { App::cursor_type::system, IDC_SIZENESW },                      // 6 - right and top
    { App::cursor_type::system, IDC_ARROW },                         // 7 - top left and right xx
    { App::cursor_type::system, IDC_SIZENS },                        // 8 - bottom
    { App::cursor_type::system, IDC_SIZENESW },                      // 9 - bottom left
    { App::cursor_type::system, IDC_SIZENWSE },                      // 10 - bottom right
    { App::cursor_type::system, IDC_ARROW },                         // 11 - bottom left and right xx
    { App::cursor_type::system, IDC_ARROW },                         // 12 - bottom and top xx
    { App::cursor_type::system, IDC_ARROW },                         // 13 - bottom top and left xx
    { App::cursor_type::system, IDC_ARROW },                         // 14 - bottom top and right xx
    { App::cursor_type::system, IDC_ARROW }                          // 15 - bottom top left and right xx
};

//////////////////////////////////////////////////////////////////////

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

void App::set_windowplacement()
{
    if(!settings.first_run && !settings.fullscreen) {
        WINDOWPLACEMENT w{ settings.window_placement };
        w.showCmd = 0;
        SetWindowPlacement(window, &w);
    }
}

//////////////////////////////////////////////////////////////////////

HRESULT App::serialize_setting(settings_t::serialize_action action, wchar const *key_name, wchar const *name, byte *var, DWORD size)
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
        CHK_HR(RegGetValue(HKEY_CURRENT_USER, key_name, name, RRF_RT_REG_BINARY, null, reinterpret_cast<DWORD *>(var), &cbsize));
    } break;
    }

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT App::settings_t::serialize(serialize_action action, wchar const *save_key_name)
{
    if(save_key_name == null) {
        return E_INVALIDARG;
    }

#undef DECL_SETTING
#define DECL_SETTING(type, name, ...) CHK_HR(serialize_setting(action, save_key_name, L#name, reinterpret_cast<byte *>(&name), (DWORD)sizeof(name)))
#include "settings.h"

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT App::settings_t::load()
{
    return serialize(serialize_action::load, key_name);
}

//////////////////////////////////////////////////////////////////////

HRESULT App::settings_t::save()
{
    return serialize(serialize_action::save, key_name);
}

//////////////////////////////////////////////////////////////////////

HRESULT App::on_copy()
{
    copy_timestamp = m_timer.current_time;
    return copy_selection();
}

//////////////////////////////////////////////////////////////////////

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

HRESULT App::on_command_line(wchar *cmd_line)
{
    // parse args
    int argc;
    LPWSTR *argv = CommandLineToArgvW(cmd_line, &argc);

    wchar const *filepath{ L"?open" };

    if(argc > 1 && argv[1] != null) {
        filepath = argv[1];
    }
    CHK_HR(load_image(filepath));
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT App::on_paste()
{
    if(IsClipboardFormatAvailable(CF_DIBV5)) {

        // if clipboard contains a DIB, make it look like a file we just loaded
        std::vector<byte> bytes;
        bytes.resize(sizeof(BITMAPFILEHEADER));
        CHK_HR(append_clipboard_to_buffer(bytes, CF_DIBV5));

        BITMAPFILEHEADER *b = reinterpret_cast<BITMAPFILEHEADER *>(bytes.data());

        BITMAPV5HEADER *i = reinterpret_cast<BITMAPV5HEADER *>(b + 1);

        memset(b, 0, sizeof(*b));
        b->bfType = 'MB';
        b->bfSize = (DWORD)bytes.size();
        b->bfOffBits = sizeof(BITMAPV5HEADER) + sizeof(BITMAPFILEHEADER) + i->bV5ProfileSize;

        if(i->bV5Compression == BI_BITFIELDS) {
            b->bfOffBits += 12;
        }

        selection_active = false;
        return show_image(bytes, L"Clipboard");
    }

    UINT filename_fmt = RegisterClipboardFormat(CFSTR_FILENAMEW);
    UINT fmt = 0;
    if(IsClipboardFormatAvailable(filename_fmt)) {
        fmt = filename_fmt;
    } else if(IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        fmt = CF_UNICODETEXT;
    }

    std::vector<byte> buffer;
    CHK_HR(append_clipboard_to_buffer(buffer, fmt));
    wchar const *name = reinterpret_cast<wchar const *>(buffer.data());
    std::wstring bare_name = strip_quotes(name);
    if(file_exists(bare_name.c_str())) {
        return load_image(bare_name.c_str());
    }
    set_message(format(L"Can't load %s - not a file", bare_name.c_str()).c_str(), 2.0f);
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}

//////////////////////////////////////////////////////////////////////

void App::cancel_loader()
{
    Log(L"Cancelling loader threads");
    SetEvent(cancel_loader_event);
    while(true) {
        long threads = InterlockedCompareExchange(&thread_count, 0, 0);
        Log(L"%d loaders are active", threads);
        if(threads == 0) {
            break;
        }
        Sleep(10);
    }
    ResetEvent(cancel_loader_event);
}

//////////////////////////////////////////////////////////////////////

HRESULT App::decode_image(file_loader *f)
{
    selection_active = false;

    HRESULT load_hr = f->hresult;

    if(FAILED(load_hr)) {

        if(load_hr != HRESULT_FROM_WIN32(ERROR_OPERATION_ABORTED) && files_loaded == 0) {
            std::wstring load_err = windows_error_message(load_hr);
            std::wstring err_msg = format(L"Error loading %s\n\n%s", filename.c_str(), load_err.c_str());
            MessageBoxW(null, err_msg.c_str(), L"ImageView", MB_ICONEXCLAMATION);

            // window might be null, that's ok
            PostMessage(window, WM_CLOSE, 0, 0);
        }
        std::wstring err_str = windows_error_message(load_hr);
        wchar *name = PathFindFileName(filename.c_str());
        set_message(format(L"Can't load %s - %s", name, err_str.c_str()).c_str(), 3);
        return load_hr;
    }
    files_loaded += 1;

    f->view_count += 1;
    return show_image(f->bytes, filename.c_str());
}

//////////////////////////////////////////////////////////////////////

HRESULT App::show_image(std::vector<byte> const &data, wchar const *file)
{
    HRESULT hr = initialize_image_from_buffer(data);
    if(SUCCEEDED(hr)) {
        m_timer.reset();
        set_message(file, 2.0f);
    } else {

        std::wstring err_str;

        // "Component not found" isn't meaningful for unknown file type, override it
        if(hr == WINCODEC_ERR_COMPONENTNOTFOUND) {
            err_str = L"Unknown file type";
        } else {
            err_str = windows_error_message(hr);
        }
        wchar *name = PathFindFileName(file);
        set_message(format(L"Can't load %s - %s", name, err_str.c_str()).c_str(), 3);
    }
    return hr;
}

//////////////////////////////////////////////////////////////////////

App::~App()
{
}

//////////////////////////////////////////////////////////////////////

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

void App::scanner_function()
{
    MSG msg;
    bool quit = false;

    // is this the right way to force message queue creation?
    PeekMessage(&msg, (HWND)-1, 0, 0, PM_NOREMOVE);

    // hope so, now notify main thread that scanner_thread is alive
    scanner_thread_running = true;

    // respond to requests from main thread
    while(!quit && GetMessage(&msg, null, 0, 0) != 0) {
        switch(msg.message) {
        case WM_SCAN_FOLDER:
            do_folder_scan(reinterpret_cast<wchar const *>(msg.lParam));
            break;
        case WM_EXIT_THREAD:
            quit = true;
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////

void App::on_folder_scanned(folder_scan_result *scan_result)
{
    Log(L"%llu images found in %s", scan_result->files.size(), scan_result->path.c_str());

    if(current_folder_scan != null) {
        delete current_folder_scan;
        current_folder_scan = null;
    }

    current_folder_scan = scan_result;

    if(current_image_file != null && current_image_file->index == -1) {
        update_file_index(current_image_file);
    }
}

//////////////////////////////////////////////////////////////////////

void App::move_file_cursor(int movement)
{
    // can't key left/right while folder is still being scanned
    // TODO(chs): remember the moves and apply them when the scan is complete?
    if(current_folder_scan == null) {
        return;
    }

    int new_file_cursor = clamp(0, current_file_cursor + movement, (int)current_folder_scan->files.size() - 1);

    if(new_file_cursor != current_file_cursor) {
        current_file_cursor = new_file_cursor;
        std::wstring pic_filename = format(L"%s\\%s", current_folder_scan->path.c_str(), current_folder_scan->files[current_file_cursor].name.c_str());
        load_image(pic_filename.c_str());
    }
}

//////////////////////////////////////////////////////////////////////
// kick off a file loader thread

void App::load_file(file_loader *loader)
{
    // +1 threadcount

    InterlockedIncrement(&thread_count);

    std::thread(

        [this](file_loader *fl) {

            fl->hresult = ::load_file(fl->filename.c_str(), fl->bytes, cancel_loader_event);
            WaitForSingleObject(window_created_event, INFINITE);
            PostMessage(window, WM_FILE_LOAD_COMPLETE, 0, reinterpret_cast<LPARAM>(fl));

            InterlockedDecrement(&thread_count);
        },
        loader)
        .detach();
}

//////////////////////////////////////////////////////////////////////

HRESULT App::load_image(wchar const *filepath)
{
    if(filepath == null || filepath[0] == 0) {
        return E_INVALIDARG;
    }

    filename = std::wstring(filepath);

    // if filename is '?clipboard', attempt to load the contents of the clipboard as a bitmap (synchronously in the UI thread, whevs)

    if(filename.compare(L"?clipboard") == 0) {
        return on_paste();
    }

    // if filename is '?open', show OpenFileDialog

    if(filename.compare(L"?open") == 0) {

        std::wstring selected_filename;
        CHK_HR(select_file_dialog(selected_filename));
        filename = selected_filename.c_str();
    }

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

    // kick folder scanner thread off if we haven't already

    if(!scanner_thread_running) {
        scanner_thread = std::thread([this]() {
            scanner_function();
            Log(L"Scanner thread exit");
        });

        while(!scanner_thread_running) {
            Sleep(0);
        }
        scanner_thread_handle = scanner_thread.native_handle();
        scanner_thread_id = GetThreadId(scanner_thread_handle);
    }

    // if it's in the cache already, just decode and show it

    auto found = loaded_files.find(fullpath);
    if(found != loaded_files.end()) {
        Log(L"Already got %s", name.c_str());
        current_image_file = found->second;
        decode_image(current_image_file);
        return S_OK;
    }

    // if it's currently being loaded, mark it for viewing when it arrives

    found = loading_files.find(fullpath);
    if(found != loading_files.end()) {
        Log(L"In progress %s", name.c_str());
        requested_file = found->second;
        return S_OK;
    }

    // file_loader object is later transferred from loading_files to loaded_files

    Log(L"Loading %s", name.c_str());

    file_loader *fl = new file_loader();
    fl->filename = fullpath;
    loading_files[fullpath] = fl;

    // when this file arrives, please display it

    requested_file = fl;

    // kick off loader thread for this file

    load_file(fl);

    // loading a file from a new folder or the current scanned folder?

    if(_wcsicmp(current_folder.c_str(), folder.c_str()) != 0) {

        // tell the scanner thread to scan this new folder
        // when it's done it will notify main thread with a windows message

        current_folder = folder;

        // sigh, manually marshall the filename for the message, the receiver is responsible for freeing it
        wchar *fullpath_buffer = new wchar[fullpath.size() + 1];
        memcpy(fullpath_buffer, fullpath.c_str(), (fullpath.size() + 1) * sizeof(wchar));

        PostThreadMessage(scanner_thread_id, WM_SCAN_FOLDER, 0, reinterpret_cast<LPARAM>(fullpath_buffer));
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// if loaded file is in the current folder, find index with list of files

HRESULT App::update_file_index(file_loader *f)
{
    if(current_folder_scan == null) {
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
            return S_OK;
        }
        id += 1;
    }
    return E_NOT_SET;
}

//////////////////////////////////////////////////////////////////////

void App::on_file_load_complete(LPARAM lparam)
{
    file_loader *f = reinterpret_cast<file_loader *>(lparam);

    if(FAILED(f->hresult)) {
        delete f;
    } else {

        // transfer from loading to loaded
        loading_files.erase(f->filename);
        loaded_files[f->filename] = f;

        // update cache total size
        cache_in_use += f->bytes.size();

        // if it's most recently requested, show it
        if(f == requested_file) {
            requested_file = null;
            current_image_file = f;
            decode_image(f);
        }

        // fill in the index so we know where it is in the list of files

        update_file_index(f);

        if(f->index != -1) {
            current_file_cursor = f->index;
        }

        // remove things from cache until it's <= cache_size

        while(cache_in_use > settings.cache_size) {

            // find the file in the cache which:
            //      is furthest away from the current file cursor
            //      has been viewed most times (yes, most...)
            //      is the biggest

            std::vector<file_loader *> files;
            files.reserve(loaded_files.size());
            for(auto const e : loaded_files) {
                files.push_back(e.second);
            }

            std::sort(files.begin(), files.end(), [&](file_loader const *a, file_loader const *b) {
                int adiff = std::abs(a->index - current_file_cursor);
                int bdiff = std::abs(b->index - current_file_cursor);
                if(bdiff != adiff) {
                    return adiff > bdiff;
                }
                if(b->view_count != a->view_count) {
                    return a->view_count > b->view_count;
                }
                if(a->bytes.size() != b->bytes.size()) {
                    return a->bytes.size() > b->bytes.size();
                }
                return _wcsicmp(a->filename.c_str(), b->filename.c_str()) < 0;
            });

            file_loader *loser = files.front();

            cache_in_use -= loser->bytes.size();

            Log(L"Removing %s from cache (now %d MB in use)", loser->filename.c_str(), cache_in_use / 1048576);

            loaded_files.erase(loser->filename);
        }
    }
}

//////////////////////////////////////////////////////////////////////

void App::on_file_already_loaded(LPARAM lparam)
{
    file_loader *f = reinterpret_cast<file_loader *>(lparam);
    current_image_file = f;
    decode_image(f);
}

//////////////////////////////////////////////////////////////////////

HRESULT App::initialize_image_from_buffer(std::vector<byte> const &buffer)
{
    ComPtr<ID3D11Resource> new_resource;
    ComPtr<ID3D11ShaderResourceView> new_srv;

    CHK_HR(CreateWICTextureFromMemoryEx(d3d_device.Get(), d3d_context.Get(), buffer.data(), buffer.size(), 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
                                        WIC_LOADER_IGNORE_SRGB | WIC_LOADER_FORCE_RGBA32, &new_resource, &new_srv));

    image_texture.Reset();
    image_texture_view.Reset();

    d3d_name(new_srv);

    CHK_HR(new_resource.As(&image_texture));

    d3d_name(image_texture);

    new_srv.As(&image_texture_view);
    new_srv.Reset();
    d3d_name(image_texture_view);

    D3D11_TEXTURE2D_DESC image_texture_desc;
    image_texture->GetDesc(&image_texture_desc);

    texture_width = image_texture_desc.Width;
    texture_height = image_texture_desc.Height;

    reset_zoom(settings.zoom_mode);

    current_rect = target_rect;

    wchar const *filename_text = filename.c_str();

    if(!settings.show_full_filename_in_titlebar) {
        filename_text = PathFindFileName(filename_text);
    }

    SetWindowText(window, format(L"%s %dx%d", filename_text, texture_width, texture_height).c_str());

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

bool App::get_grab(int button)
{
    return (mouse_grab & (1 << button)) != 0;
}

//////////////////////////////////////////////////////////////////////

void App::set_grab(int button)
{
    int mask = 1 << (int)button;
    if(mouse_grab == 0) {
        SetCapture(window);
    }
    mouse_grab |= mask;
}

//////////////////////////////////////////////////////////////////////

void App::clear_grab(int button)
{
    int mask = 1 << (int)button;
    mouse_grab &= ~mask;
    if(mouse_grab == 0) {
        ReleaseCapture();
    }
}

//////////////////////////////////////////////////////////////////////

vec2 App::texture_size() const
{
    return { (float)texture_width, (float)texture_height };
}

//////////////////////////////////////////////////////////////////////

vec2 App::window_size() const
{
    return { (float)window_width, (float)window_height };
}

//////////////////////////////////////////////////////////////////////

vec2 App::texel_size() const
{
    return div_point(current_rect.size(), texture_size());
}

//////////////////////////////////////////////////////////////////////

vec2 App::texels_to_pixels(vec2 pos)
{
    return mul_point(pos, texel_size());
}

//////////////////////////////////////////////////////////////////////

vec2 App::clamp_to_texture(vec2 pos)
{
    vec2 t = texture_size();
    return vec2{ clamp(0.0f, pos.x, t.x), clamp(0.0f, pos.y, t.y) };
}

//////////////////////////////////////////////////////////////////////

vec2 App::pixels_to_texels(vec2 pos)
{
    return div_point(pos, texel_size());
}

//////////////////////////////////////////////////////////////////////

vec2 App::screen_to_texture_pos(vec2 pos)
{
    return pixels_to_texels(sub_point(pos, current_rect.top_left()));
}

//////////////////////////////////////////////////////////////////////

vec2 App::screen_to_texture_pos(point_s pos)
{
    return screen_to_texture_pos(vec2(pos));
}

//////////////////////////////////////////////////////////////////////

vec2 App::texture_to_screen_pos(vec2 pos)
{
    return add_point(current_rect.top_left(), texels_to_pixels(vec2::floor(clamp_to_texture(pos))));
}

//////////////////////////////////////////////////////////////////////

HRESULT App::init()
{
    CHK_BOOL(GetPhysicallyInstalledSystemMemory(&system_memory_size_kb));

    Log(L"System has %lluGB of memory", system_memory_size_kb / 1048576);

    settings.cache_size = 1048576 * 16;    // 16MB cache

    window_created_event = CreateEvent(null, true, false, null);

    current_mouse_cursor = LoadCursor(null, IDC_ARROW);

    // remember default settings for 'reset settings to default' feature in settings dialog
    default_settings = settings;

    // in debug builds, hold middle mouse button at startup to reset settings to defaults
#if defined(_DEBUG)
    if(!is_key_down(VK_MBUTTON))
#endif
        settings.load();

    CHK_NULL(cancel_loader_event = CreateEvent(null, true, false, null));
    CHK_NULL(quit_event = CreateEvent(null, true, false, null));

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// this is a mess, but kinda necessarily so

HRESULT App::get_startup_rect_and_style(rect *r, DWORD *style, DWORD *ex_style)
{
    if(r == null || style == null || ex_style == null) {
        return E_INVALIDARG;
    }

    *ex_style = 0;    // WS_EX_NOREDIRECTIONBITMAP

    // if settings.fullscreen, use settings.fullscreen_rect (which will be on a monitor (which may or may not still exist...))

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
        HMONITOR m = MonitorFromPoint({ settings.fullscreen_rect.left, settings.fullscreen_rect.top }, MONITOR_DEFAULTTONEAREST);
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

void App::set_cursor(HCURSOR c)
{
    if(c == null) {
        c = LoadCursor(null, IDC_ARROW);
    }
    current_mouse_cursor = c;
    SetCursor(c);
}

//////////////////////////////////////////////////////////////////////

void App::check_selection_hover(vec2 pos)
{
    if(!selection_active) {
        set_cursor(null);
        return;
    }

#if defined(_DEBUG)
    if(is_key_down(VK_TAB)) {
        DebugBreak();
    }
#endif

    // get screen coords of selection rectangle
    vec2 tl = texture_to_screen_pos(vec2::min(select_current, select_anchor));
    vec2 br = texture_to_screen_pos(add_point({ 1, 1 }, vec2::max(select_current, select_anchor)));

    // selection grab border is +/- N pixels (setting: 4 to 32 pixels)
    float border = dpi_scale(settings.select_border_grab_size);
    vec2 b{ border / 2, border / 2 };

    // expand outer rect
    vec2 expanded_topleft = vec2::max({ 0, 0 }, sub_point(tl, b));
    vec2 expanded_bottomright = vec2::min(window_size(), add_point(br, b));

    selection_hover = selection_hover_t::sel_hover_outside;

    // mouse is in the expanded box?
    rect_f expanded_box(expanded_topleft, size(sub_point(expanded_bottomright, expanded_topleft)));
    if(expanded_box.contains(pos)) {

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

        cursor_def const &ct = sel_hover_cursors[selection_hover];
        short id = ct.id;
        HMODULE h = null;

        // if it's a resource of our own, we have to load it from current module
        if(ct.type == App::cursor_type::user) {
            h = GetModuleHandle(null);
        }

        set_cursor(LoadCursor(h, MAKEINTRESOURCE(id)));
    } else {
        set_cursor(null);
    }
}

//////////////////////////////////////////////////////////////////////

HRESULT App::set_window(HWND hwnd)
{
    window = hwnd;

    SetEvent(window_created_event);

    CHK_HR(create_device());

    CHK_HR(create_resources());

    InitializeDragDropHelper(window);

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

bool App::on_setcursor()
{
    set_cursor(current_mouse_cursor);
    return true;
}

//////////////////////////////////////////////////////////////////////

void App::on_mouse_button(point_s pos, int button, int state)
{
    if(state == btn_down) {

        mouse_click[button] = pos;
        mouse_pos[button] = pos;
        memset(mouse_offset + button, 0, sizeof(point_s));

        set_grab(button);

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

        } else if(button == settings.zoom_button) {

            ShowCursor(FALSE);
        }

    } else {

        clear_grab(button);

        if(button == settings.zoom_button) {
            ShowCursor(TRUE);
            clear_grab(settings.drag_button);    // in case they pressed the drag button while zooming
        } else if(button == settings.select_button) {
            drag_selection = false;

            // when selection is finalized, select_anchor is top left, select_current is bottom right
            vec2 tl = vec2::floor(vec2::min(select_anchor, select_current));
            vec2 br = vec2::floor(vec2::max(select_anchor, select_current));
            select_anchor = tl;
            select_current = br;
            selection_size = sub_point(br, tl);
        }
    }
}

//////////////////////////////////////////////////////////////////////

void App::on_raw_mouse_move(point_s delta)
{
    if(get_grab(settings.zoom_button)) {
        do_zoom(mouse_click[settings.zoom_button], std::max(-4, std::min(-delta.y, 4)));
    }
}

//////////////////////////////////////////////////////////////////////

void App::on_mouse_move(point_s pos)
{
    if(!get_grab(settings.zoom_button)) {
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
        if(get_grab(i)) {
            mouse_offset[i] = add_point(mouse_offset[i], sub_point(cur_mouse_pos, mouse_pos[i]));
            mouse_pos[i] = cur_mouse_pos;
        }
    }

    if(selecting) {
        selection_active = true;
    }

    if(!get_grab(settings.select_button)) {
        check_selection_hover(vec2(pos));
    } else if(selection_hover == selection_hover_t::sel_hover_outside) {
        set_cursor(null);
    }
}

//////////////////////////////////////////////////////////////////////

void App::on_mouse_wheel(point_s pos, int delta)
{
    do_zoom(pos, delta * 10);
}

//////////////////////////////////////////////////////////////////////

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

void App::zoom_to_selection()
{
    if(!selection_active) {
        reset_zoom(reset_zoom_mode::fit_to_window);
        return;
    }

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

void App::center_in_window()
{
    target_rect.x = (window_width - current_rect.w) / 2.0f;
    target_rect.y = (window_height - current_rect.h) / 2.0f;
}

//////////////////////////////////////////////////////////////////////

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

void App::on_key_down(int vk_key, LPARAM flags)
{
    uint f = HIWORD(flags);
    bool repeat = (f & KF_REPEAT) == KF_REPEAT;    // previous key-state flag, 1 on autorepeat

    if(repeat) {
        return;
    }

    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    switch(vk_key) {

    case ' ':
        toggle_fullscreen();
        break;

    case 'Z':
        reset_zoom(settings.zoom_mode);
        break;

    case 'C':
        center_in_window();
        break;

    case 'A':
        if(ctrl) {
            select_all();
        } else {
            settings.grid_enabled = !settings.grid_enabled;
        }
        break;

    case 'G':
        if(shift) {
            settings.fixed_grid = !settings.fixed_grid;
        } else {
            settings.grid_multiplier = (settings.grid_multiplier + 1) & 7;
        }
        break;

    case '1':
        settings.zoom_mode = reset_zoom_mode::one_to_one;
        reset_zoom(settings.zoom_mode);
        break;

    case 'F':
        settings.zoom_mode = reset_zoom_mode::fit_to_window;
        reset_zoom(settings.zoom_mode);
        break;

    case 'S':
        settings.zoom_mode = reset_zoom_mode::shrink_to_fit;
        reset_zoom(settings.zoom_mode);
        break;

    case 'X':
        zoom_to_selection();
        break;

    case VK_LEFT:
        move_file_cursor(-1);
        break;

    case VK_RIGHT:
        move_file_cursor(1);
        break;

    case 'O': {
        std::wstring selected_filename;
        if(SUCCEEDED(select_file_dialog(selected_filename))) {
            load_image(selected_filename.c_str());
        }
    } break;

    case 'B': {
        uint32 bg_color = color_to_uint32(settings.background_color);
        if(SUCCEEDED(select_color_dialog(window, bg_color, L"Choose background color"))) {
            settings.background_color = uint32_to_color(bg_color);
        }
    } break;

    case 'N':
        selection_active = false;
        break;

    case VK_ESCAPE:
        DestroyWindow(window);
        break;

    case VK_SHIFT:
        shift_mouse_pos = cur_mouse_pos;
        shift_snap = true;
        shift_snap_axis = shift_snap_axis_t::none;
        break;

    case VK_CONTROL:
        ctrl_snap = true;
        break;
    }
}

//////////////////////////////////////////////////////////////////////

HRESULT App::update()
{
    m_timer.update();

    auto lerp = [&](float &a, float &b) {
        float d = b - a;
        if(fabsf(d) <= 1.0f) {
            a = b;
        } else {
            a += d * 20 * (float)m_timer.delta();
        }
    };

    lerp(current_rect.x, target_rect.x);
    lerp(current_rect.y, target_rect.y);
    lerp(current_rect.w, target_rect.w);
    lerp(current_rect.h, target_rect.h);

    if(get_grab(settings.zoom_button)) {
        POINT old_pos{ mouse_click[settings.zoom_button].x, mouse_click[settings.zoom_button].y };
        ClientToScreen(window, &old_pos);
        SetCursorPos(old_pos.x, old_pos.y);
    }

    if(get_grab(settings.drag_button) && !get_grab(settings.zoom_button)) {
        current_rect.x += mouse_offset[settings.drag_button].x;
        current_rect.y += mouse_offset[settings.drag_button].y;
        target_rect = current_rect;
        has_been_zoomed_or_dragged = true;
    }

    selecting = get_grab(settings.select_button);

    if(selecting && !get_grab(settings.zoom_button)) {

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
                max.x = (float)texture_width;
                min.x = select_anchor.x;
            }

            if(selection_hover & sel_hover_top) {
                y = &select_anchor.y;
                max.y = select_current.y;
                min.y = 0.0f;
            } else if(selection_hover & sel_hover_bottom) {
                y = &select_current.y;
                max.y = (float)texture_height;
                min.y = select_anchor.y;
            }

            if(selection_hover == sel_hover_inside) {
                min = { 0, 0 };
                max = sub_point(texture_size(), selection_size);
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

    if(m_frame > 1 && (m_timer.wall_time() > 0.25 || image_texture.Get() != null || filename.empty())) {
        ShowWindow(window, SW_SHOW);
    }

    m_frame += 1;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

void App::select_all()
{
    if(image_texture.Get() != 0) {
        select_anchor = { 0, 0 };
        select_current = { (float)texture_width - 1, (float)texture_height - 1 };
        selection_active = true;
    }
}

//////////////////////////////////////////////////////////////////////

HRESULT App::copy_selection()
{
    if(!selection_active) {
        return E_ABORT;
    }
    vec2 tl = vec2::min(select_anchor, select_current);
    vec2 br = vec2::max(select_anchor, select_current);

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

    size_t pixel_size = 4llu;

    size_t pixel_buffer_size = pixel_size * (size_t)w * h;

    // BI_BITFIELDS requires 3 DWORDS after the BITMAPV5HEADER
    // See https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapv5header

    size_t buffer_size = sizeof(BITMAPV5HEADER) + pixel_buffer_size + 4llu * 3;

    HANDLE hData = GlobalAlloc(GHND | GMEM_SHARE, buffer_size);

    if(hData == null) {
        return HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);
    }

    byte *pData = reinterpret_cast<byte *>(GlobalLock(hData));

    if(pData == null) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    BITMAPV5HEADER *bmi = reinterpret_cast<BITMAPV5HEADER *>(pData);

    memset(bmi, 0, sizeof(BITMAPV5HEADER));
    bmi->bV5Size = sizeof(BITMAPV5HEADER);
    bmi->bV5Width = w;
    bmi->bV5Height = -h;
    bmi->bV5Planes = 1;
    bmi->bV5BitCount = 32;
    bmi->bV5Compression = BI_BITFIELDS;
    bmi->bV5SizeImage = 4 * w * h;
    bmi->bV5RedMask = 0x00ff0000;
    bmi->bV5GreenMask = 0x0000ff00;
    bmi->bV5BlueMask = 0x000000ff;
    bmi->bV5AlphaMask = 0xff000000;
    bmi->bV5CSType = LCS_WINDOWS_COLOR_SPACE;
    bmi->bV5Intent = LCS_GM_GRAPHICS;

    DWORD *rgb_bitmasks = reinterpret_cast<DWORD *>(pData + sizeof(BITMAPV5HEADER));

    rgb_bitmasks[0] = 0xff0000;
    rgb_bitmasks[1] = 0x00ff00;
    rgb_bitmasks[2] = 0x0000ff;

    byte *pixels = pData + sizeof(BITMAPV5HEADER) + 4llu * 3;

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

    struct file_loader
    {
        std::wstring filename;      // file path, relative to scanned folder - use this as key for map
        std::vector<byte> bytes;    // data, once it has been loaded
        HRESULT hresult{ S_OK };    // error code or S_OK from load_file()
    };

    d3d_context->Flush();

    D3D11_MAPPED_SUBRESOURCE mapped_resource{};
    CHK_HR(d3d_context->Map(tex.Get(), 0, D3D11_MAP_READ, 0, &mapped_resource));

    // got the texels, copy into the DIB for the clipboard, swapping R, B channels
    byte *row = pixels;
    byte *src = reinterpret_cast<byte *>(mapped_resource.pData);
    for(int y = 0; y < h; ++y) {
        uint32 *s = reinterpret_cast<uint32 *>(src);
        uint32 *d = reinterpret_cast<uint32 *>(row);
        for(int x = 0; x < w; ++x) {
            uint32 p = *s++;
            *d++ = (p & 0xff00ff00) | ((p & 0xff0000) >> 16) | ((p & 0xff) << 16);
        }
        src += mapped_resource.RowPitch;
        row += w * 4llu;
    }

    d3d_context->Unmap(tex.Get(), 0);

    GlobalUnlock(hData);

    CHK_BOOL(OpenClipboard(null));
    CHK_BOOL(EmptyClipboard());
    CHK_BOOL(SetClipboardData(CF_DIBV5, hData));
    CHK_BOOL(CloseClipboard());

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT App::measure_string(std::wstring const &text, IDWriteTextFormat *format, float padding, vec2 &size)
{
    ComPtr<IDWriteTextLayout> text_layout;

    CHK_HR(dwrite_factory->CreateTextLayout(text.c_str(), (UINT32)text.size(), format, (float)window_width * 2, (float)window_height * 2, &text_layout));

    DWRITE_TEXT_METRICS m;
    CHK_HR(text_layout->GetMetrics(&m));

    size.x = m.width + padding * 4;
    size.y = m.height + padding;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT App::draw_string(std::wstring const &text, IDWriteTextFormat *format, vec2 pos, vec2 pivot, float opacity, float corner_radius, float padding)
{
    corner_radius = dpi_scale(corner_radius);
    padding = dpi_scale(padding);
    pos.x = dpi_unscale(pos.x);
    pos.y = dpi_unscale(pos.y);

    ComPtr<IDWriteTextLayout> text_layout;

    // This sucks that you have to create and destroy a com object to draw a text string

    CHK_HR(dwrite_factory->CreateTextLayout(text.c_str(), (UINT32)text.size(), format, (float)window_width * 2, (float)window_height * 2, &text_layout));

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
    rr.rect = D2D1_RECT_F{ text_pos.x - padding * 2, text_pos.y - padding * 0.5f, text_pos.x + w + padding * 2, text_pos.y + h + padding * 0.5f };
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

HRESULT App::render()
{
    clear();

    if(image_texture.Get() != null) {

        vec2 scale = div_point(vec2{ 2, 2 }, vec2{ (float)window_width, (float)window_height });

        float x_scale = 2.0f / window_width;
        float y_scale = 2.0f / window_height;

        vec2 grid_pos{ 0, 0 };

        float gm = (1 << settings.grid_multiplier) / 4.0f;
        float gs = settings.grid_size * current_rect.w / texture_width * gm;

        if(gs < 4) {
            gs = 0;
        }

        if(settings.fixed_grid) {
            vec2 g2{ 2.0f * gs, 2.0f * gs };
            grid_pos = sub_point(g2, vec2::mod(current_rect.top_left(), g2));
            // gx = g2 - fmodf(current_rect.x, g2);
            // gy = g2 - fmodf(current_rect.y, g2);
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

        float cx = floorf(current_rect.x);
        float cy = floorf(current_rect.y);
        float cw = floorf(current_rect.w);
        float ch = floorf(current_rect.h);

        shader_constants.scale = mul_point({ cw, -ch }, scale);
        shader_constants.offset = { cx * x_scale - 1, 1 - cy * y_scale };

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

            double tw = (double)texture_width;
            double th = (double)texture_height;

            double xs = cw / tw;
            double ys = ch / th;

            double tx = floorf(s_tl.x + 0) * xs + cx;
            double ty = floorf(s_tl.y + 0) * ys + cy;
            double bx = floorf(s_br.x + 1) * xs + cx;
            double by = floorf(s_br.y + 1) * ys + cy;

            s_tl = { (float)tx, (float)ty };
            s_br = { (float)bx, (float)by };

            float sw = (float)settings.select_border_width;

            // clamp to the image because zooming/rounding etc
            float select_l = floorf(std::max(cx, s_tl.x)) - sw;
            float select_t = floorf(std::max(cy, s_tl.y)) - sw;
            float select_r = floorf(std::min(cx + cw - 1, s_br.x)) + sw;
            float select_b = floorf(std::min(cy + ch - 1, s_br.y)) + sw;

            // always draw at least something
            float select_w = std::max(1.0f, select_r - select_l + 0.5f);
            float select_h = std::max(1.0f, select_b - select_t + 0.5f);

            // border width clamp if rectangle is too small
            int min_s = (int)std::min(select_w, select_h) - settings.select_border_width * 2;
            int min_t = std::min(settings.select_border_width, min_s);

            int select_border_width = std::max(1, min_t);

            shader_constants.select_border_width = select_border_width;    // set viewport coords for the vertex shader

            shader_constants.scale = mul_point({ select_w, -select_h }, scale);
            shader_constants.offset = { select_l * x_scale - 1, 1 - (select_t * y_scale) };

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

            // scale from texels to pixels
            vec2 p(cur_mouse_pos);
            vec2 sp1 = clamp_to_texture(screen_to_texture_pos(p));
            sp1 = texture_to_screen_pos(sp1);

            // draw a vertical crosshair line

            auto draw_vert = [&](float mx, float my) {

                float blip = (float)((m_frame >> 0) & 31);
                grid_pos = sub_point(g2, vec2::mod({ mx + blip, my + blip }, g2));
                shader_constants.grid_offset = { 0, grid_pos.y };
                shader_constants.offset = { mx * x_scale - 1, 1 };
                shader_constants.scale = { 1 * x_scale, -2 };
                CHK_HR(update_constants());
                d3d_context->Draw(4, 0);
                return S_OK;
            };

            // draw a horizontal crosshair line

            auto draw_horiz = [&](float mx, float my) {

                float blip = (float)((m_frame >> 0) & 31);
                grid_pos = sub_point(g2, vec2::mod({ mx + blip, my + blip }, g2));
                shader_constants.grid_offset = { grid_pos.x, 0 };
                shader_constants.offset = { -1, 1 - my * y_scale };
                shader_constants.scale = { 2, -1 * y_scale };
                CHK_HR(update_constants());
                d3d_context->Draw(4, 0);
                return S_OK;
            };

            // draw the ones which are clamped to the topleft of the texel under the cursor

            CHK_HR(draw_vert(sp1.x, sp1.y));
            CHK_HR(draw_horiz(sp1.x, sp1.y));

            // if zoomed texels are bigger than one pixel
            // draw the ones which are clamped to the bottomright of the texel

            vec2 sp2 = clamp_to_texture(screen_to_texture_pos(p));
            sp2.x += 1;
            sp2.y += 1;
            sp2 = texture_to_screen_pos(sp2);
            sp2.x -= 1;
            sp2.y -= 1;

            // only check x distance because zoom is always the same in x and y
            if((sp2.x - sp1.x) > 2) {
                CHK_HR(draw_vert(sp2.x, sp1.y));
                CHK_HR(draw_horiz(sp1.x, sp2.y));
            }
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

            shader_constants.offset = { mn.x * x_scale - 1, 1 - (mn.y * y_scale) };
            shader_constants.scale = mul_point({ mx.x, -mx.y }, scale);

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
            draw_string(text, small_text_format.Get(), screen_pos, { 1, 0 }, 1.0f, small_label_padding, small_label_padding);
        }

        if(selection_active) {
            vec2 tl = vec2::min(sa, sc);
            vec2 br = vec2::max(sa, sc);
            vec2 s_tl = texture_to_screen_pos(tl);
            vec2 s_br = texture_to_screen_pos({ br.x + 1, br.y + 1 });
            s_tl.x -= dpi_scale(12);
            s_tl.y -= dpi_scale(8);
            s_br.x += dpi_scale(12);
            s_br.y += dpi_scale(8);
            POINT s_dim{ (int)(floorf(br.x) - floorf(tl.x)) + 1, (int)(floorf(br.y) - floorf(tl.y)) + 1 };
            draw_string(format(L"X %d Y %d", (int)tl.x, (int)tl.y), small_text_format.Get(), s_tl, { 1, 1 }, 1.0f, 2, 2);
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
                draw_string(format(L"%s", current_message.c_str()), large_text_format.Get(), pos, { 0.5f, 1.0f }, message_alpha);
            }
        }

        CHK_HR(d2d_render_target->EndDraw());
    }

    CHK_HR(present());

    return S_OK;

    // if we copied into the copy texture, put it into the clipboard now
}

//////////////////////////////////////////////////////////////////////

void App::clear()
{
    d3d_context->ClearRenderTargetView(rendertarget_view.Get(), reinterpret_cast<float const *>(&settings.border_color));

    d3d_context->OMSetRenderTargets(1, rendertarget_view.GetAddressOf(), null);

    CD3D11_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(window_width), static_cast<float>(window_height));
    d3d_context->RSSetViewports(1, &viewport);
}

//////////////////////////////////////////////////////////////////////

HRESULT App::present()
{
    HRESULT hr = swap_chain->Present(1, 0);

    if(hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        hr = on_device_lost();
    }

    return hr;
}

//////////////////////////////////////////////////////////////////////

HRESULT App::on_closing()
{
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

void App::on_activated()
{
    shift_snap = false;
}

//////////////////////////////////////////////////////////////////////

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

HRESULT App::on_window_pos_changing(WINDOWPOS *new_pos)
{
    UNREFERENCED_PARAMETER(new_pos);
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

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
    CHK_HR(D3D11CreateDevice(null, D3D_DRIVER_TYPE_HARDWARE, null, create_flags, feature_levels, num_feature_levels, D3D11_SDK_VERSION, &device, &feature_level, &context));

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

    CHK_HR(d3d_device->CreateVertexShader(vs_rectangle_shaderbin, sizeof(vs_rectangle_shaderbin), null, &vertex_shader));
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

HRESULT App::create_resources()
{
    if(d3d_context.Get() == null) {
        create_device();
    }

    ID3D11RenderTargetView *nullViews[] = { nullptr };
    d3d_context->OMSetRenderTargets(static_cast<UINT>(std::size(nullViews)), nullViews, nullptr);
    d3d_context->Flush();

    rendertarget_view.Reset();
    d2d_render_target.Reset();

    const UINT backBufferWidth = static_cast<UINT>(window_width);
    const UINT backBufferHeight = static_cast<UINT>(window_height);
    const DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;    // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    constexpr UINT backBufferCount = 1;
    const DXGI_SCALING scaling_mode = DXGI_SCALING_STRETCH;
    const DXGI_SWAP_EFFECT swap_effect = DXGI_SWAP_EFFECT_DISCARD;
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

        CHK_HR(dxgiFactory->CreateSwapChainForHwnd(d3d_device.Get(), window, &swapChainDesc, &fsSwapChainDesc, nullptr, &swap_chain));

        CHK_HR(dxgiFactory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));
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

HRESULT App::create_text_formats()
{
    float large_font_size = dpi_scale(16.0f);
    float small_font_size = dpi_scale(12.0f);

    auto weight = DWRITE_FONT_WEIGHT_REGULAR;
    auto style = DWRITE_FONT_STYLE_NORMAL;
    auto stretch = DWRITE_FONT_STRETCH_NORMAL;

    CHK_HR(dwrite_factory->CreateTextFormat(small_font_family_name, font_collection.Get(), weight, style, stretch, large_font_size, L"en-us", &large_text_format));
    CHK_HR(dwrite_factory->CreateTextFormat(mono_font_family_name, font_collection.Get(), weight, style, stretch, small_font_size, L"en-us", &small_text_format));

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

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

HRESULT App::on_device_lost()
{
    CHK_HR(create_device());

    CHK_HR(create_resources());

    if(current_image_file != null) {
        CHK_HR(decode_image(current_image_file));
    }

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

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
    std::wstring s(str);
    if(s[0] == '"') {
        s = s.substr(1, s.size() - 2);
    }
    return load_image(s.c_str());
}

//////////////////////////////////////////////////////////////////////

void App::on_process_exit()
{
    settings.save();

    // this kills all loader threads
    cancel_loader();

    // murder the scanner thread

    // tell scan_folder2 to quit
    SetEvent(quit_event);

    // tell the thread itself to quit
    PostThreadMessage(scanner_thread_id, WM_EXIT_THREAD, 0, 0);

    // wait for it to die
    while(WaitForSingleObject(scanner_thread_handle, 100) != WAIT_OBJECT_0) {
        Log(L"Waiting for scanner thread to exit");
    }

    // should really join() here but... whevs
    scanner_thread.detach();

    // then cleanup

    if(current_folder_scan != null) {
        delete current_folder_scan;
        current_folder_scan = null;
    }

    CloseHandle(cancel_loader_event);
    CloseHandle(quit_event);
    CloseHandle(window_created_event);
}