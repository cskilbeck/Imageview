//////////////////////////////////////////////////////////////////////
// ImageView

// Goals

// Minimize application size and system resource usage
// Minimalist, responsive UI
// Flexible user input settings
// Handle most common image file formats
// Run standalone with no installer
// Rotate/flip images
// Can be localized without too much hassle

// Limitations

// Windows only (Windows 7 and newer)
// Cannot edit or mark up images

// Implementation

// Native Win32 APIs
// C++ std library
// WIC for all image load/save operations
// UTF16 exclusively
// D3D11 for drawing
// DirectWrite for text output
// Registry to store settings

//////////////////////////////////////////////////////////////////////
// TO DO

// file type association / handler thing ('open with' menu)
// shortcut editor
// mutual exclude mouse buttons
// get all the mouse handling stuff out of update() and into mouse move handler
// settings dialog : cache size not reet
// pause render()/update() when app loses focus
// make render() pixel correct for select rect etc

// flip/rotate
// colorspace / SRGB / HDR

// fix the cache
// fix all the leaks
// proper error handling/reporting

// ?folder scanning broken after on_command_line from external?

// get these into another file:
//      d3d
//      d2d
//      cache
//      scanner

#include "pch.h"

LOG_CONTEXT("app");

//////////////////////////////////////////////////////////////////////

namespace imageview::app
{
    // the window handle
    HWND window{ null };

    // is it running as admin
    bool is_elevated{ false };

    // detected system memory size at startup
    uint64 system_memory_gb;

    // GetModuleHandle(null)
    HMODULE instance;
}

//////////////////////////////////////////////////////////////////////

namespace
{
    using namespace imageview;
    using namespace DirectX;

    using app::window;

#include "shader_inc/vs_rectangle.h"
#include "shader_inc/ps_texture.h"
#include "shader_inc/ps_copy_texture.h"
#include "shader_inc/ps_solid.h"
#include "shader_inc/ps_stripe.h"

    //////////////////////////////////////////////////////////////////////
    // WM_USER messages for scanner thread

    enum scanner_thread_user_message_t : uint
    {
        WM_SCAN_FOLDER = WM_USER,    // please scan a folder (lparam -> path)
    };

    //////////////////////////////////////////////////////////////////////
    // WM_USER messages for file_loader thread

    enum loader_thread_user_message_t : uint
    {
        WM_LOAD_FILE = WM_USER    // please load this file (lparam -> filepath)
    };

    //////////////////////////////////////////////////////////////////////
    // types of WM_COPYDATA messages that can be sent

    enum class copydata_t : DWORD
    {
        commandline = 1
    };

    //////////////////////////////////////////////////////////////////////
    // selection admin

    // how, if at all, is the selection being modified
    enum select_mode_t
    {
        // no selection defined
        // on mouse click followed by mouse move more than threshold, set select_anchor.xy = mouse.xy
        select_mode_none = 0,

        // dragging a corner
        // set select_current.xy = mouse.xy
        select_mode_dragging_corner = 1,

        // dragging top or bottom edge
        // set select_current.y = mouse.y
        select_mode_dragging_vertical = 2,

        // dragging left or right edge
        // set select_current.x = mouse.x
        select_mode_dragging_horizontal = 3,

        // dragging whole selection (grabbed inside the rectangle)
        // set select_current and select_anchor
        select_mode_dragging_all = 2,
    };

    // where on selection is mouse hovering
    enum selection_hover_t : uint
    {
        sel_hover_inside = 0,
        sel_hover_left = 1,
        sel_hover_right = 2,
        sel_hover_top = 4,
        sel_hover_bottom = 8,
        sel_hover_topleft = sel_hover_left | sel_hover_top,
        sel_hover_topright = sel_hover_right | sel_hover_top,
        sel_hover_bottomleft = sel_hover_left | sel_hover_bottom,
        sel_hover_bottomright = sel_hover_right | sel_hover_bottom,

        sel_hover_outside = 0x80000000
    };

    DEFINE_ENUM_FLAG_OPERATORS(selection_hover_t);

    enum class shift_snap_axis_t
    {
        none,    // snap mode might be active but no axis chosen yet
        x,
        y
    };

    enum class snap_mode_t
    {
        none,
        axis,
        square
    };

    //////////////////////////////////////////////////////////////////////
    // valid rotation angles are 0, 90, 180, 270

    matrix rotate_matrix[4] = {
        XMMatrixSet(+1, +0, 0, 0, +0, +1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1),
        XMMatrixSet(+0, -1, 0, 0, +1, +0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1),
        XMMatrixSet(-1, +0, 0, 0, +0, -1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1),
        XMMatrixSet(+0, +1, 0, 0, -1, +0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1),
    };

    // flip [neither, x, y, x & y]

    matrix flip_matrix[4] = {
        XMMatrixSet(+1, 0, 0, 0, 0, +1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1),
        XMMatrixSet(-1, 0, 0, 0, 0, +1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1),
        XMMatrixSet(+1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1),
        XMMatrixSet(-1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1),
    };

    rotation_angle_t rotation = rotate_0;

    bool flip_horiz{ false };
    bool flip_vert{ false };

    //////////////////////////////////////////////////////////////////////
    // fonts are hard coded for now, embedded as resources

    wchar const *label_font_family_name{ L"Noto Sans" };
    wchar const *banner_font_family_name{ L"Roboto Mono" };

    // folder containing most recently loaded file (so we know if a folder scan is in the same folder as current file)
    std::wstring current_folder;

    // index in folder scan of currently viewed file
    int current_file_cursor{ -1 };

    //////////////////////////////////////////////////////////////////////
    // files which have been loaded

    std::unordered_map<std::wstring, image::image_file *> loaded_files;

    //////////////////////////////////////////////////////////////////////
    // files which have been requested to load

    std::unordered_map<std::wstring, image::image_file *> loading_files;

    //////////////////////////////////////////////////////////////////////

    LPCWSTR window_class = L"ImageViewWindowClass_2DAE134A-7E46-4E75-9DFA-207695F48699";

    // most recently scanned folder results
    std::unique_ptr<file::folder_scan_result> current_folder_scan;

    // how many files loaded so far in this run
    int files_loaded{ 0 };

    // the most recently requested file to show - when a file_load succeeds, if it's this one, show it
    image::image_file *requested_file{ null };

    // dummy image file for showing the clipboard
    image::image_file clipboard_image_file;

    // folder scanner thread id so we can PostMessage to it
    uint scanner_thread_id{ (uint)-1 };

    // file loading happens in this thread
    uint file_loader_thread_id{ (uint)-1 };

    // set this to signal that the application is exiting
    // all threads should quit asap when this is set
    HANDLE quit_event{ null };

    // which image currently being displayed (which might be that dodgy clipboard one)
    image::image_file *current_file{ null };

    // wait on this before sending a message to the window which must arrive safely
    HANDLE window_created_event{ null };

    // admin for showing a message
    std::wstring current_message;
    double message_timestamp{ 0 };
    int message_fade_time{ 0 };

    vec2 small_label_size{ 0, 0 };
    float label_pad{ 2.0f };

    // hex text of current pixel being hovered over (if alt key held down)
    std::wstring rgb_pixel_text;

    // cache admin
    uint64 cache_in_use{ 0 };
    std::mutex cache_mutex;

    // how many times WM_SHOWWINDOW (because startup hassle)
    int window_show_count{ 0 };

    // window size admin
    int window_width{ 1280 };
    int window_height{ 720 };
    int old_window_width{ 1280 };
    int old_window_height{ 720 };

    // if popup_menu_active, disable some mouse zoom stuff
    bool popup_menu_active{ false };

    // when we're calling SetWindowPos, suppress DPI change handling because it causes a weird problem
    // this isn't a great solution but it does get by. Nobody seems to know the right way to handle that
    bool ignore_dpi_for_a_moment{ false };

    // current dpi for the window, updated by WM_DPICHANGED
    float dpi{ 96.0f };

    // on exit, relaunch as admin
    bool relaunch_as_admin{ false };

    // some window admin
    bool s_in_sizemove = false;
    bool s_in_suspend = false;
    bool s_minimized = false;

    thread_pool_t thread_pool;

    //////////////////////////////////////////////////////////////////////
    // shader constants header is shared with the HLSL files

#pragma pack(push, 4)

    struct shader_const_t
    {

#include "shader_constants.h"

    } shader;

#pragma pack(pop)

    // all the com pointers

    ComPtr<ID3D11Device1> d3d_device;
    ComPtr<ID3D11DeviceContext1> d3d_context;

    ComPtr<IDXGISwapChain1> swap_chain;
    ComPtr<ID3D11RenderTargetView> rendertarget_view;

#if USE_DIRECTCOMPOSITION
    ComPtr<IDCompositionDevice> directcomposition_device;
    ComPtr<IDCompositionTarget> directcomposition_target;
    ComPtr<IDCompositionVisual> directcomposition_visual;
#endif

    ComPtr<ID3D11PixelShader> main_shader;

    ComPtr<ID3D11PixelShader> texture_shader;
    ComPtr<ID3D11PixelShader> solid_shader;
    ComPtr<ID3D11PixelShader> stripe_shader;

    ComPtr<ID3D11PixelShader> copy_texture_shader;

    ComPtr<ID3D11VertexShader> vertex_shader;

    ComPtr<ID3D11ShaderResourceView> image_texture_view;
    ComPtr<ID3D11Texture2D> image_texture;

    ComPtr<ID3D11SamplerState> sampler_state;
    ComPtr<ID3D11Buffer> constant_buffer;
    ComPtr<ID3D11RasterizerState> rasterizer_state;

    ComPtr<ID3D11BlendState> blend_state;

    ComPtr<IDWriteFactory1> dwrite_factory;
    ComPtr<ID2D1Factory> d2d_factory;
    ComPtr<ID2D1RenderTarget> d2d_render_target;

    ComPtr<ID2D1SolidColorBrush> text_fg_brush;
    ComPtr<ID2D1SolidColorBrush> text_outline_brush;
    ComPtr<ID2D1SolidColorBrush> text_bg_brush;

    ComPtr<IDWriteTextFormat> banner_format;
    ComPtr<IDWriteTextFormat> label_format;

    ComPtr<IDWriteFontCollection> font_collection;

    // see font_loader.h
    ResourceFontContext font_context;

    // mouse admin
    int mouse_grab{ 0 };
    POINT mouse_pos[btn_count] = {};
    POINT mouse_offset[btn_count] = {};
    POINT mouse_click[btn_count] = {};
    POINT cur_mouse_pos{};
    POINT shift_mouse_pos{};
    POINT ctrl_mouse_pos{};
    uint64 mouse_click_timestamp[btn_count];
    bool crosshairs_active{ false };

    // hold modifier key to snap selection square or fix on an axis
    shift_snap_axis_t snap_axis{ shift_snap_axis_t::none };
    snap_mode_t snap_mode{ snap_mode_t::none };
    float axis_snap_radius{ 8 };    // TODO (chs): make this a setting?

    bool selecting{ false };         // dragging new selection rectangle
    bool drag_selection{ false };    // dragging the existing selection rectangle
    bool grabbing_selection{ false };

    bool select_active{ false };          // a defined selection rectangle exists
    selection_hover_t selection_hover;    // where on the selection rectangle being hovered
    vec2 drag_select_pos{ 0, 0 };         // where they originally grabbed the selection rectangle in texels
    vec2 selection_size{ 0, 0 };          // size of selection rectangle in texels

    // the fixed point of the selection rectangle
    vec2 select_anchor;

    // the floating point of the selection rectangle
    vec2 select_current;

    // sticky zoom mode on window resize
    bool has_been_zoomed_or_dragged{ false };
    zoom_mode_t last_zoom_mode{ zoom_mode_t::shrink_to_fit };

    // texture drawn in this rectangle which is in pixels
    rect_f current_rect;

    // texture rectangle target for animated zoom etc
    rect_f target_rect;

    // true texture dimensions (with no transform)
    int actual_texture_width{ 0 };
    int actual_texture_height{ 0 };

    // texture dimensions after transform (rotate 90 or 270)
    int texture_width{ 0 };
    int texture_height{ 0 };

    // zoom limits in pixels
    float min_zoom{ 8 };      // min size of entire image
    float max_zoom{ 192 };    // max size of 1 pixel

    // frame/wall timer for animation
    timer_t m_timer;

    // when was the selection copied for animating flash
    double copy_timestamp{ 0 };

    HCURSOR current_mouse_cursor;

    // which frame rendering
    int frame_count{ 0 };

    // 10 recent files
    std::vector<std::wstring> recent_files_list;

    //////////////////////////////////////////////////////////////////////
    // cursors for hovering over rectangle interior/corners/edges
    // see selection_hover_t

    struct cursor_def
    {
        enum class src
        {
            system,
            user
        };

        src source;
        short id;

        cursor_def(src s, wchar const *i) : source(s), id((short)((intptr_t)i & 0xffff))
        {
        }

        cursor_def(src s, short i) : source(s), id(i)
        {
        }

        HCURSOR get_hcursor() const
        {
            HMODULE h = null;
            if(source == src::user) {
                h = app::instance;
            }
            return LoadCursor(h, MAKEINTRESOURCEW(id));
        }
    };

    cursor_def sel_hover_cursors[16] = {
        { cursor_def::src::user, IDC_CURSOR_HAND },    //  0 - inside
        { cursor_def::src::system, IDC_SIZEWE },       //  1 - left
        { cursor_def::src::system, IDC_SIZEWE },       //  2 - right
        { cursor_def::src::system, IDC_ARROW },        //  3 - xx left and right shouldn't be possible
        { cursor_def::src::system, IDC_SIZENS },       //  4 - top
        { cursor_def::src::system, IDC_SIZENWSE },     //  5 - left and top
        { cursor_def::src::system, IDC_SIZENESW },     //  6 - right and top
        { cursor_def::src::system, IDC_ARROW },        //  7 - xx top left and right
        { cursor_def::src::system, IDC_SIZENS },       //  8 - bottom
        { cursor_def::src::system, IDC_SIZENESW },     //  9 - bottom left
        { cursor_def::src::system, IDC_SIZENWSE },     // 10 - bottom right
        { cursor_def::src::system, IDC_ARROW },        // 11 - xx bottom left and right
        { cursor_def::src::system, IDC_ARROW },        // 12 - xx bottom and top
        { cursor_def::src::system, IDC_ARROW },        // 13 - xx bottom top and left
        { cursor_def::src::system, IDC_ARROW },        // 14 - xx bottom top and right
        { cursor_def::src::system, IDC_ARROW }         // 15 - xx bottom top left and right
    };

    HRESULT show_image(image::image_file *f);

    //////////////////////////////////////////////////////////////////////
    // set the banner message and how long before it fades out

    void set_message(std::wstring const &message, int fade_time)
    {
        current_message = message;
        message_timestamp = m_timer.wall_time();
        message_fade_time = fade_time;
    }

    //////////////////////////////////////////////////////////////////////
    // message box for catastrophic errors

    void error_message_box(std::wstring const &msg, HRESULT hr)
    {
        std::wstring err = windows_error_message(hr);
        message_box(null, std::format(L"{}\r\n{}", msg, err), MB_ICONEXCLAMATION);
    }

    //////////////////////////////////////////////////////////////////////
    // make an image the current one

    HRESULT display_image(image::image_file *f)
    {
        if(f == null) {
            return E_INVALIDARG;
        }
        select_active = false;

        HRESULT load_hr = f->hresult;

        if(FAILED(load_hr)) {

            // if this is the first file being loaded and there was a file loading error
            if(load_hr != HRESULT_FROM_WIN32(ERROR_OPERATION_ABORTED) && files_loaded == 0) {

                error_message_box(f->filename, load_hr);

                // bit harsh, quitting here...?
                DestroyWindow(window);
            }

            // and in any case, set banner message to error text
            std::wstring err_str = windows_error_message(load_hr);
            std::wstring name;
            CHK_HR(file::get_filename(f->filename, name));
            set_message(std::format(L"{} {} - {}", localize(IDS_CANT_LOAD_FILE), name, err_str), 3);
            return load_hr;
        }
        files_loaded += 1;
        f->view_count += 1;
        return show_image(f);
    }

    //////////////////////////////////////////////////////////////////////
    // get size of file + decoded image for cache

    HRESULT get_image_file_size(std::wstring const &filename, uint64 *size)
    {
        if(size == null || filename.empty()) {
            return E_INVALIDARG;
        }

        uint64 file_size;

        CHK_HR(file::get_size(filename, file_size));

        uint32 w, h;
        uint64 image_size;

        CHK_HR(image::get_size(filename, w, h, image_size));

        *size = (size_t)image_size + file_size;

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // kick off a file loader thread

    void start_file_loader(image::image_file *loader)
    {
        thread_pool.create_thread(
            [](image::image_file *fl) {

                LOG_CONTEXT("file_loader");

                // load the file synchronously to this thread
                fl->hresult = file::load(fl->filename, fl->bytes, quit_event);

                if(SUCCEEDED(fl->hresult)) {

                    // Need to call this in any thread which uses Windows Imaging Component
                    (void)CoInitializeEx(null, COINIT_APARTMENTTHREADED);

                    // decode the image
                    fl->hresult = image::decode(fl);

                    CoUninitialize();
                }

                // let the window know, either way, that the file load attempt is complete, failed or
                // otherwise
                WaitForSingleObject(window_created_event, INFINITE);
                PostMessageW(window, app::WM_FILE_LOAD_COMPLETE, 0, reinterpret_cast<LPARAM>(fl));
            },
            loader);
    }

    //////////////////////////////////////////////////////////////////////
    // warm the cache by loading N files either side of the current one

    HRESULT warm_cache()
    {
        // if it's a normal file in the current folder
        if(current_file != null && !current_file->is_clipboard && current_file->index != -1) {

            int const num_images_to_cache = 12;

            for(int i = 0; i < num_images_to_cache; ++i) {

                int y = current_file->index - (i + 2) / 2 * ((i & 1) * 2 - 1);

                if(y >= 0 && y < (int)current_folder_scan->files.size()) {

                    std::wstring this_file = current_folder_scan->path + L"\\" + current_folder_scan->files[y].name;

                    if(loading_files.find(this_file) == loading_files.end() &&
                       loaded_files.find(this_file) == loaded_files.end()) {

                        // remove things from cache until it's <= cache_size + required size

                        uint64 img_size;
                        if(SUCCEEDED(get_image_file_size(this_file, &img_size))) {

                            size_t cache_size = settings.cache_size_mb * 1048576llu;

                            while((cache_in_use + img_size) > cache_size) {

                                image::image_file *loser = null;
                                uint loser_diff = 0;
                                for(auto const &fl : loaded_files) {
                                    uint diff = std::abs(fl.second->index - current_file_cursor);
                                    if(diff > loser_diff) {
                                        loser_diff = diff;
                                        loser = fl.second;
                                    }
                                }

                                if(loser != null) {

                                    LOG_DEBUG(L"Removing {} ({}) from cache (now {} MB in use)",
                                              loser->filename,
                                              loser->index,
                                              cache_in_use / 1048576);

                                    cache_in_use -= loser->total_size();
                                    loaded_files.erase(loser->filename);
                                } else {
                                    break;
                                }
                            }

                            if((cache_in_use + img_size) <= cache_size) {

                                LOG_DEBUG(L"Caching {} at {}", this_file, y);
                                image::image_file *cache_file = new image::image_file();
                                cache_file->filename = this_file;
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
    // load an image file or get it from the cache (or notice that it's
    // already being loaded and just let it arrive later)

    HRESULT load_image(std::wstring const &filename)
    {
        if(filename.empty()) {
            return E_INVALIDARG;
        }

        // get somewhat canonical filepath and parts thereof

        std::wstring folder;
        std::wstring name;
        std::wstring fullpath;

        CHK_HR(file::get_full_path(filename, fullpath));
        CHK_HR(file::get_filename(fullpath, name));
        CHK_HR(file::get_path(fullpath, folder));

        if(folder.empty()) {
            folder = L".";
        } else if(folder.back() == '\\') {
            folder.pop_back();
        }

        // if it's in the cache already, just show it
        // TODO (chs): make this file path compare canonical

        auto found = loaded_files.find(fullpath);
        if(found != loaded_files.end()) {
            LOG_DEBUG(L"Already got {}", name);
            CHK_HR(display_image(found->second));
            CHK_HR(warm_cache());
            return S_OK;
        }

        // TODO (chs): fix this lame
        if(settings.show_filename != show_filename_never) {
            set_message(std::format(L"{}{}", fullpath, localize(IDS_IS_LOADING)), 5);
        }

        // if it's currently being loaded, mark it for viewing when it arrives

        found = loading_files.find(fullpath);
        if(found != loading_files.end()) {
            LOG_DEBUG(L"In progress {}", name);
            requested_file = found->second;
            return S_OK;
        }

        // if it's not a cache_load, remove things from the cache until there's room for it
        // remove files what are furthest away from it by index

        // file_loader object is later transferred from loading_files to loaded_files

        LOG_INFO(L"Loading {}", name);

        image::image_file *fl = new image::image_file();
        fl->filename = fullpath;
        loading_files[fullpath] = fl;

        // when this file arrives, please display it

        requested_file = fl;

        // ask the loader thread to load it

        PostThreadMessage(file_loader_thread_id, WM_LOAD_FILE, 0, reinterpret_cast<LPARAM>(fl));

        // if it's coming from a new folder

        bool is_new_folder;
        CHK_HR(file::paths_are_different(current_folder, folder, is_new_folder));

        if(is_new_folder) {

            // if(_stricmp(current_folder.c_str(), folder.c_str()) != 0) {

            // tell the scanner thread to scan this new folder
            // when it's done it will notify main thread with a windows message

            // TODO(chs): CLEAR THE CACHE HERE....

            current_folder = folder;

            // sigh, manually marshall the filename for the message, the receiver is responsible for
            // freeing it
            wchar *fullpath_buffer = new wchar[fullpath.size() + 1];
            memcpy(fullpath_buffer, fullpath.c_str(), (fullpath.size() + 1) * sizeof(wchar));

            PostThreadMessageW(scanner_thread_id, WM_SCAN_FOLDER, 0, reinterpret_cast<LPARAM>(fullpath_buffer));
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT load_image_file(std::wstring const &filepath)
    {
        if(!file::exists(filepath)) {
            set_message(std::format(L"{} {}", localize(IDS_CANT_LOAD_FILE), filepath), 2);
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }
        return load_image(filepath);
    }

    //////////////////////////////////////////////////////////////////////
    // DragDrop admin

    struct FileDropper : public CDragDropHelper
    {
        //////////////////////////////////////////////////////////////////////
        // user dropped something on the window, try to load it as a file

        HRESULT on_drop_shell_item(IShellItemArray *psia, DWORD grfKeyState) override
        {
            UNREFERENCED_PARAMETER(grfKeyState);
            ComPtr<IShellItem> shell_item;
            CHK_HR(psia->GetItemAt(0, &shell_item));
            PWSTR path{};
            CHK_HR(shell_item->GetDisplayName(SIGDN_FILESYSPATH, &path));
            DEFER(CoTaskMemFree(path));
            return load_image_file(path);
        }

        //////////////////////////////////////////////////////////////////////
        // dropped a thing that can be interpreted as text, try to load it as a file

        HRESULT on_drop_string(wchar const *str) override
        {
            return load_image_file(strip_quotes(str));
        }
    } file_dropper;

    //////////////////////////////////////////////////////////////////////
    // D3D Debug admin

#if defined(_DEBUG)

    ComPtr<ID3D11Debug> d3d_debug;

    void set_d3d_debug_name(ID3D11DeviceChild *resource, char const *name)
    {
        resource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
    }

    template <typename T> void set_d3d_debug_name(ComPtr<T> &resource, char const *name)
    {
        set_d3d_debug_name(resource.Get(), name);
    }

#define D3D_SET_NAME(x) set_d3d_debug_name(x, #x)

#else

#define D3D_SET_NAME(...) \
    do {                  \
    } while(false)

#endif

    //////////////////////////////////////////////////////////////////////

    void clear_message()
    {
        current_message.clear();
    }

    //////////////////////////////////////////////////////////////////////
    // SetWindowText with ** admin ** prepender and current filename

    HRESULT setup_window_text()
    {
        std::wstring name(localize(IDS_AppName));
        std::wstring details;
        std::wstring admin;

        if(app::is_elevated) {

            admin = localize(IDS_ADMIN);
        }

        if(current_file != null) {

            name = current_file->filename;

            if(!settings.show_full_filename_in_titlebar) {

                CHK_HR(file::get_filename(name, name));
            }
            // TODO (chs): localize 'x'
            details = std::format(L" {}x{}", current_file->img.width, current_file->img.height);
        }
        SetWindowTextW(window, std::format(L"{}{}{}", admin, name, details).c_str());
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // new settings have arrived, update stuff which isn't automatically
    // picked up by the renderer

    void on_new_settings()
    {
        setup_window_text();
    }

    //////////////////////////////////////////////////////////////////////

    RECT center_rect_on_default_monitor(RECT const &r)
    {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        int ww = rect_width(r);
        int wh = rect_height(r);
        RECT rc;
        rc.left = (sw - ww) / 2;
        rc.top = (sh - wh) / 2;
        rc.right = rc.left + ww;
        rc.bottom = rc.top + wh;
        return rc;
    }

    //////////////////////////////////////////////////////////////////////
    // append helps because then we can prepend a BITMAPFILEHEADER
    // when we're loading the clipboard and pretend it's a file

    HRESULT append_clipboard_to_buffer(std::vector<byte> &buffer, UINT format)
    {
        CHK_BOOL(OpenClipboard(null));

        DEFER(CloseClipboard());

        HANDLE c = GetClipboardData(format);
        if(c == null) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        void *data = GlobalLock(c);
        if(data == null) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        DEFER(GlobalUnlock(c));

        size_t size = GlobalSize(c);
        if(size == 0) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        size_t existing = buffer.size();
        buffer.resize(size + existing);
        memcpy(buffer.data() + existing, data, size);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // scale a number by the current dpi

    template <typename T> T dpi_scale(T x)
    {
        return (T)((x * dpi) / 96.0f);
    }

    //////////////////////////////////////////////////////////////////////
    // and vice versa

    template <typename T> T dpi_unscale(T x)
    {
        return (T)((x * 96.0f) / dpi);
    }

    //////////////////////////////////////////////////////////////////////

    matrix get_texture_transform()
    {
        matrix m = rotate_matrix[static_cast<uint>(rotation)];

        if(flip_horiz || flip_vert) {

            bool xflip = flip_horiz;
            bool yflip = flip_vert;

            if(rotation == rotate_90 || rotation == rotate_270) {

                std::swap(xflip, yflip);
            }

            uint flipper = (xflip ? 1 : 0) + (yflip ? 2 : 0);

            m = XMMatrixMultiply(flip_matrix[flipper], m);
        }
        return m;
    }

    //////////////////////////////////////////////////////////////////////
    // draw a rectangle at screen pixel coordinates

    HRESULT draw_rectangle(rect_f const &rc, D3D11_VIEWPORT const &vp)
    {
        // get viewport coordinates from pixel coordinates

        float x = rc.x;
        float y = rc.y;
        float w = rc.w;
        float h = rc.h;

        float left = x * 2.0f / vp.Width - 1.0f;
        float top = y * -2.0f / vp.Height + 1.0f;

        float width = w * 2.0f / vp.Width;
        float height = h * -2.0f / vp.Height;

        // if rotated 90 or 270, swap some stuff around

        shader.rect = { left, top, width, height };

        // update shader constants

        D3D11_MAPPED_SUBRESOURCE mapped_subresource;

        CHK_HR(d3d_context->Map(constant_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource));

        memcpy(mapped_subresource.pData, &shader, sizeof(shader_const_t));

        d3d_context->Unmap(constant_buffer.Get(), 0);

        // draw a quad

        d3d_context->Draw(4, 0);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    //
    // copy_selection_to_texture
    //
    // This is all rather involved due to the idiotic decision to keep
    // the original image untransformed and apply transformations (flip/rotate)
    // when drawing it so we have to 'untransform' the selection rectangle, then
    // copy the resulting piece, then apply the transform back to the copied bit.
    // Most of which could be avoided by just modifying the image when
    // flip/rotate are applied. But... hey ho, who knows what strange problems
    // that might entail...

    HRESULT copy_selection_to_texture(ID3D11Texture2D **texture)
    {
        vec2 t = vec2(sub_point(POINT{ texture_width, texture_height }, { 1, 1 }));

        vec2 tl{ 0, 0 };
        vec2 br{ static_cast<float>(texture_width) - 1, static_cast<float>(texture_height) - 1 };

        if(select_active) {
            tl = select_anchor;
            br = select_current;
        }

        LOG_DEBUG(L"Was {:4.0f},{:4.0f}..{:4.0f},{:4.0f} ({:4.0f},{:4.0f})",
                  tl.x,
                  tl.y,
                  br.x,
                  br.y,
                  br.x - tl.x,
                  br.y - tl.y);

        // unfsck the selection rect

        bool fh = flip_horiz;
        bool fv = flip_vert;

        if(rotation == rotate_90 || rotation == rotate_270) {
            std::swap(fh, fv);
        }

        if(fh) {
            tl.x = t.x - tl.x;
            br.x = t.x - br.x;
        }

        if(fv) {
            tl.y = t.y - tl.y;
            br.y = t.y - br.y;
        }

        switch(rotation) {

        case rotate_90:
            tl = { tl.y, t.x - tl.x };
            br = { br.y, t.x - br.x };
            break;

        case rotate_180:
            tl = { t.x - tl.x, t.y - tl.y };
            br = { t.x - br.x, t.y - br.y };
            break;

        case rotate_270:
            tl = { t.y - tl.y, tl.x };
            br = { t.y - br.y, br.x };
            break;
        }

        vec2 ntl = vec2::min(tl, br);
        vec2 nbr = vec2::max(tl, br);

        tl = ntl;
        br = nbr;

        LOG_DEBUG(L"Now {:4.0f},{:4.0f}..{:4.0f},{:4.0f} ({:4.0f},{:4.0f})",
                  tl.x,
                  tl.y,
                  br.x,
                  br.y,
                  br.x - tl.x,
                  br.y - tl.y);

        // 1. Copy region into a texture

        // TODO (chs): transform the selection by some version of current flip/rotate matrix

        D3D11_BOX copy_box;
        copy_box.left = (int)tl.x;
        copy_box.right = std::min(actual_texture_width, (int)br.x + 1);
        copy_box.top = (int)tl.y;
        copy_box.bottom = std::min(actual_texture_height, (int)br.y + 1);
        copy_box.back = 1;
        copy_box.front = 0;

        int copy_w = copy_box.right - copy_box.left;
        int copy_h = copy_box.bottom - copy_box.top;

        if(copy_w < 1 || copy_h < 1) {
            return E_BOUNDS;
        }

        D3D11_TEXTURE2D_DESC desc;
        image_texture->GetDesc(&desc);

        desc.Width = copy_w;
        desc.Height = copy_h;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;

        ComPtr<ID3D11Texture2D> tex;
        CHK_HR(d3d_device->CreateTexture2D(&desc, null, &tex));
        D3D_SET_NAME(tex);

        d3d_context->CopySubresourceRegion(tex.Get(), 0, 0, 0, 0, image_texture.Get(), 0, &copy_box);

        d3d_context->Flush();

        ComPtr<ID3D11Texture2D> tex_2d;
        ComPtr<ID3D11RenderTargetView> rtv_2d;

        // get dimensions of source texture

        D3D11_TEXTURE2D_DESC source_desc;

        tex->GetDesc(&source_desc);

        // swap w/h if rotated 90/270 degrees

        uint w = source_desc.Width;
        uint h = source_desc.Height;

        if(rotation == rotate_90 || rotation == rotate_270) {
            std::swap(w, h);
        }

        // create staging texture for grabbing it into

        ComPtr<ID3D11Texture2D> staging_texture;

        CD3D11_TEXTURE2D_DESC staging_texture_desc(
            DXGI_FORMAT_B8G8R8A8_UNORM, w, h, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);

        CHK_HR(d3d_device->CreateTexture2D(&staging_texture_desc, null, staging_texture.GetAddressOf()));

        // create dest texture and rtv

        CD3D11_TEXTURE2D_DESC texture_desc(DXGI_FORMAT_B8G8R8A8_UNORM,
                                           w,
                                           h,
                                           1,
                                           1,
                                           D3D11_BIND_RENDER_TARGET,
                                           D3D11_USAGE_DEFAULT,
                                           D3D11_CPU_ACCESS_READ);

        CHK_HR(d3d_device->CreateTexture2D(&texture_desc, null, tex_2d.GetAddressOf()));

        CD3D11_RENDER_TARGET_VIEW_DESC rtv_desc(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_B8G8R8A8_UNORM);

        CHK_HR(d3d_device->CreateRenderTargetView(tex_2d.Get(), &rtv_desc, rtv_2d.GetAddressOf()));

        // create srv for source texture

        CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc(tex.Get(), D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_B8G8R8A8_UNORM);

        ComPtr<ID3D11ShaderResourceView> srv;
        CHK_HR(d3d_device->CreateShaderResourceView(tex.Get(), &srv_desc, srv.GetAddressOf()));

        // setup query so we know when the copy is finished

        CD3D11_QUERY_DESC query_desc(D3D11_QUERY_EVENT);
        ComPtr<ID3D11Query> query;

        CHK_HR(d3d_device->CreateQuery(&query_desc, query.GetAddressOf()));

        // copy it

        CD3D11_VIEWPORT viewport(tex_2d.Get(), rtv_2d.Get());

        d3d_context->OMSetRenderTargets(1, rtv_2d.GetAddressOf(), null);
        d3d_context->RSSetViewports(1, &viewport);
        d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        d3d_context->VSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
        d3d_context->VSSetShader(vertex_shader.Get(), null, 0);
        d3d_context->PSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
        d3d_context->PSSetShader(copy_texture_shader.Get(), null, 0);
        d3d_context->PSSetShaderResources(0, 1, srv.GetAddressOf());
        d3d_context->PSSetSamplers(0, 1, sampler_state.GetAddressOf());
        d3d_context->RSSetState(rasterizer_state.Get());
        d3d_context->OMSetBlendState(null, null, 0xffffffff);

        shader.texture_transform = get_texture_transform();

        draw_rectangle({ 0, 0, static_cast<float>(w), static_cast<float>(h) }, viewport);

        d3d_context->End(query.Get());

        uint32 query_result;

        while(d3d_context->GetData(query.Get(), &query_result, sizeof(uint32), 0) != S_OK) {
            Sleep(0);
        }

        d3d_context->CopyResource(staging_texture.Get(), tex_2d.Get());

        d3d_context->Flush();

        *texture = staging_texture.Detach();

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // copy the current selection into clipboard as a DIBV5

    HRESULT copy_selection()
    {
        ComPtr<ID3D11Texture2D> tex;
        CHK_HR(copy_selection_to_texture(tex.GetAddressOf()));

        D3D11_TEXTURE2D_DESC desc;
        tex->GetDesc(&desc);
        int w = desc.Width;
        int h = desc.Height;

        // copy the pixels

        D3D11_MAPPED_SUBRESOURCE mapped_resource{};
        CHK_HR(d3d_context->Map(tex.Get(), 0, D3D11_MAP_READ, 0, &mapped_resource));
        DEFER(d3d_context->Unmap(tex.Get(), 0));

        uint32 pitch = mapped_resource.RowPitch;

        size_t pixel_size = 4llu;
        size_t pixel_buffer_size = pixel_size * (size_t)w * h;

        // create CF_DIBV5 clipformat

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
        mem_clear(bmiv5);
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

        // copy into the DIB for the clipboard, swapping R, B channels
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

        // create CF_DIB clipformat

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

        mem_clear(bmi);
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

        // stang them into the clipboard

        CHK_BOOL(OpenClipboard(null));
        DEFER(CloseClipboard());

        CHK_BOOL(EmptyClipboard());

        CHK_BOOL(SetClipboardData(CF_DIBV5, dibv5_data));
        CHK_BOOL(SetClipboardData(CF_DIB, dib_data));

        // encode as a png also - required for chrome and many others

        CHK_HR(image::copy_pixels_as_png(pixels, w, h));

        // done

        // TODO (chs): localize 'x'
        set_message(std::format(L"{} {}x{}", localize(IDS_COPIED), w, h), 3);

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // copy selection and record timestamp for flash notification

    HRESULT on_copy()
    {
        copy_timestamp = m_timer.current();
        return copy_selection();
    }

    //////////////////////////////////////////////////////////////////////
    // process a command line, could be from another instance

    HRESULT on_command_line(std::wstring const &cmd_line)
    {
        LOG_INFO(L"COMMAND LINE: {}", cmd_line);

        // parse args
        int argc;
        wchar **argv = CommandLineToArgvW(cmd_line.c_str(), &argc);

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
    // get texture size as a vec2

    vec2 texture_size()
    {
        return { static_cast<float>(texture_width), static_cast<float>(texture_height) };
    }

    //////////////////////////////////////////////////////////////////////
    // get window size as a vec2

    vec2 window_size()
    {
        return { static_cast<float>(window_width), static_cast<float>(window_height) };
    }

    //////////////////////////////////////////////////////////////////////
    // get size of a texel in window pixels

    vec2 texel_size()
    {
        return div_point(current_rect.size(), texture_size());
    }

    //////////////////////////////////////////////////////////////////////
    // convert a texel pos to a window pixel pos

    vec2 texels_to_pixels(vec2 pos)
    {
        return mul_point(pos, texel_size());
    }

    //////////////////////////////////////////////////////////////////////
    // clamp a texel pos to the texture dimensions

    vec2 clamp_to_texture(vec2 pos)
    {
        vec2 t = texture_size();
        return vec2{ std::clamp(pos.x, 0.0f, t.x - 1), std::clamp(pos.y, 0.0f, t.y - 1) };
    }

    //////////////////////////////////////////////////////////////////////
    // convert a window pixel pos to a texel pos

    vec2 screen_to_texture_pos(vec2 pos)
    {
        vec2 relative_pos = sub_point(pos, current_rect.top_left());
        return div_point(relative_pos, texel_size());
    }

    //////////////////////////////////////////////////////////////////////
    // convert a window pos to texel pos

    vec2 screen_to_texture_pos(POINT pos)
    {
        return screen_to_texture_pos(vec2(pos));
    }

    //////////////////////////////////////////////////////////////////////
    // convert texel pos to window pixel pos with clamp to texture

    vec2 texture_to_screen_pos(vec2 pos)
    {
        return add_point(current_rect.top_left(), texels_to_pixels(vec2::floor(clamp_to_texture(pos))));
    }

    //////////////////////////////////////////////////////////////////////
    // convert texel pos to window pixel pos without clamp

    vec2 texture_to_screen_pos_unclamped(vec2 pos)
    {
        return add_point(current_rect.top_left(), texels_to_pixels(vec2::floor(pos)));
    }

    //////////////////////////////////////////////////////////////////////
    // set the current mouse cursor

    void set_mouse_cursor(HCURSOR c)
    {
        if(c == null) {
            c = LoadCursor(null, IDC_ARROW);
        }
        current_mouse_cursor = c;
        SetCursor(c);
    }

    //////////////////////////////////////////////////////////////////////
    // reset the zoom mode to one of `reset_zoom_mode`

    void reset_zoom(zoom_mode_t mode)
    {
        float width_factor = static_cast<float>(window_width) / texture_width;
        float height_factor = static_cast<float>(window_height) / texture_height;

        float scale_factor{ 1.0f };

        switch(mode) {

        case zoom_mode_t::one_to_one:
            break;

        case zoom_mode_t::shrink_to_fit:
            scale_factor = std::min(1.0f, std::min(width_factor, height_factor));
            break;

        case zoom_mode_t::fit_to_window:
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
    // select nothing

    void clear_selection()
    {
        drag_selection = false;
        grabbing_selection = false;
        select_active = false;
        set_mouse_cursor(null);
        SetCursor(current_mouse_cursor);    // update cursor now, don't wait for a mouse move
    }

    //////////////////////////////////////////////////////////////////////
    // select the whole image

    void select_all()
    {
        if(image_texture.Get() != null) {
            drag_select_pos = { 0, 0 };
            select_anchor = { 0, 0 };
            select_current = sub_point(texture_size(), { 1, 1 });
            selection_size = select_current;
            select_active = true;
            selecting = false;
            drag_selection = false;
            grabbing_selection = false;
        }
    }

    //////////////////////////////////////////////////////////////////////

    void reset_transform()
    {
        flip_horiz = false;
        flip_vert = false;
        rotation = rotate_0;
        texture_width = actual_texture_width;
        texture_height = actual_texture_height;
        reset_zoom(last_zoom_mode);
        current_rect = target_rect;
        clear_selection();
    }

    //////////////////////////////////////////////////////////////////////

    void flip(flip_type_t flip_type)
    {
        flip_type_t select_flip = flip_type;

        // if the image is on its side, flip the other axis

        if(rotation == rotate_90 || rotation == rotate_270) {

            flip_type = static_cast<flip_type_t>(1 - flip_type);
        }

        // flip_x / flip_y are used for drawing the texture

        if(flip_type == flip_horizontal) {
            flip_horiz = !flip_horiz;
        } else {
            flip_vert = !flip_vert;
        }

        // flip the selection visually

        switch(select_flip) {

        case flip_horizontal: {
            vec2 t = sub_point(texture_size(), { 1, 1 });
            select_anchor.x = t.x - select_anchor.x;
            select_current.x = t.x - select_current.x;
        } break;

        case flip_vertical: {
            vec2 t = sub_point(texture_size(), { 1, 1 });
            select_anchor.y = t.y - select_anchor.y;
            select_current.y = t.y - select_current.y;
        } break;
        }
    }

    //////////////////////////////////////////////////////////////////////

    void rotate(int direction)
    {
        rotation = static_cast<rotation_angle_t>(static_cast<uint>(rotation + direction) % rotate_max);

        // rotate the selection

        selecting = false;

        if(select_active) {

            vec2 t = sub_point(texture_size(), { 1, 1 });

            if(direction > 0) {

                select_anchor = { t.y - select_anchor.y, select_anchor.x };
                select_current = { t.y - select_current.y, select_current.x };

            } else {

                select_anchor = { select_anchor.y, t.x - select_anchor.x };
                select_current = { select_current.y, t.x - select_current.x };
            }
        }

        // dimensions are swapped for any 90 degree rotation

        std::swap(texture_width, texture_height);

        // rotate the draw rectangle by 90 degrees

        target_rect = target_rect.rotate90();
        current_rect = target_rect;
    }

    //////////////////////////////////////////////////////////////////////
    // get dimensions of a string including padding

    HRESULT measure_string(std::wstring const &text, IDWriteTextFormat *format, float padding, vec2 &size)
    {
        ComPtr<IDWriteTextLayout> text_layout;

        CHK_HR(dwrite_factory->CreateTextLayout(text.c_str(),
                                                static_cast<UINT32>(text.size()),
                                                format,
                                                static_cast<float>(window_width * 2),
                                                static_cast<float>(window_height * 2),
                                                &text_layout));

        DWRITE_TEXT_METRICS m;
        CHK_HR(text_layout->GetMetrics(&m));

        size.x = m.width + padding * 4;
        size.y = m.height + padding;

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // create the d2d text formats

    HRESULT create_text_formats()
    {
        float banner_font_size = dpi_scale(16.0f);
        float label_font_size = dpi_scale(12.0f);

        auto weight = DWRITE_FONT_WEIGHT_REGULAR;
        auto style = DWRITE_FONT_STYLE_NORMAL;
        auto stretch = DWRITE_FONT_STRETCH_NORMAL;

        // TODO (chs): localization

        CHK_HR(dwrite_factory->CreateTextFormat(label_font_family_name,
                                                font_collection.Get(),
                                                weight,
                                                style,
                                                stretch,
                                                banner_font_size,
                                                L"en-us",
                                                banner_format.ReleaseAndGetAddressOf()));
        CHK_HR(dwrite_factory->CreateTextFormat(banner_font_family_name,
                                                font_collection.Get(),
                                                weight,
                                                style,
                                                stretch,
                                                label_font_size,
                                                L"en-us",
                                                label_format.ReleaseAndGetAddressOf()));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // create the d3d device
    // it's ok to do this before we know how big to make the rendertarget_view

    HRESULT create_device()
    {
        LOG_DEBUG(L"CREATE DEVICE: {}x{}", window_width, window_height);

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

        CHK_HR(d3d_device->CreateVertexShader(vs_rectangle_bin, sizeof(vs_rectangle_bin), null, &vertex_shader));
        D3D_SET_NAME(vertex_shader);

        CHK_HR(d3d_device->CreatePixelShader(ps_solid_bin, sizeof(ps_solid_bin), null, &solid_shader));
        D3D_SET_NAME(solid_shader);

        CHK_HR(d3d_device->CreatePixelShader(ps_texture_bin, sizeof(ps_texture_bin), null, &texture_shader));
        D3D_SET_NAME(texture_shader);

        CHK_HR(d3d_device->CreatePixelShader(
            ps_copy_texture_bin, sizeof(ps_copy_texture_bin), null, &copy_texture_shader));

        D3D_SET_NAME(copy_texture_shader);

        CHK_HR(d3d_device->CreatePixelShader(ps_stripe_bin, sizeof(ps_stripe_bin), null, &stripe_shader));
        D3D_SET_NAME(stripe_shader);

        CD3D11_SAMPLER_DESC sampler_desc(D3D11_DEFAULT);
        sampler_desc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        sampler_desc.BorderColor[0] = 0;
        sampler_desc.BorderColor[1] = 0;
        sampler_desc.BorderColor[2] = 0;
        sampler_desc.BorderColor[3] = 0;
        CHK_HR(d3d_device->CreateSamplerState(&sampler_desc, &sampler_state));
        D3D_SET_NAME(sampler_state);

        CD3D11_BUFFER_DESC constant_buffer_desc((sizeof(shader_const_t) + 0xf) & 0xfffffff0,
                                                D3D11_BIND_CONSTANT_BUFFER,
                                                D3D11_USAGE_DYNAMIC,
                                                D3D11_CPU_ACCESS_WRITE);
        CHK_HR(d3d_device->CreateBuffer(&constant_buffer_desc, null, &constant_buffer));
        D3D_SET_NAME(constant_buffer);

        CD3D11_RASTERIZER_DESC rasterizer_desc(D3D11_DEFAULT);
        CHK_HR(d3d_device->CreateRasterizerState(&rasterizer_desc, &rasterizer_state));
        D3D_SET_NAME(rasterizer_state);

        // DirectWrite / Direct2D init

        CHK_HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&d2d_factory)));

        auto dwf = reinterpret_cast<IUnknown **>(dwrite_factory.ReleaseAndGetAddressOf());
        CHK_HR(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), dwf));

        CHK_HR(font_context.Initialize(dwrite_factory.Get()));

        UINT const fontResourceIDs[] = { IDR_FONT_NOTO, IDR_FONT_ROBOTO };

        CHK_HR(font_context.CreateFontCollection(fontResourceIDs, sizeof(fontResourceIDs), &font_collection));

        dpi = get_window_dpi(window);

        CHK_HR(create_text_formats());

        CHK_HR(measure_string(L"X 9999 Y 9999", label_format.Get(), label_pad, small_label_size));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // create window size dependent resources

    HRESULT create_resources()
    {
        LOG_DEBUG(L"CREATE RESOURCES: {}x{}", window_width, window_height);

        if(d3d_device.Get() == null) {
            create_device();
        }

        ID3D11RenderTargetView *nullViews[] = { null };
        d3d_context->OMSetRenderTargets(static_cast<UINT>(std::size(nullViews)), nullViews, null);
        d3d_context->Flush();

        rendertarget_view.Reset();
        d2d_render_target.Reset();

        const UINT client_width = static_cast<UINT>(window_width);
        const UINT client_height = static_cast<UINT>(window_height);
        const DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
        const DXGI_SCALING scaling_mode = DXGI_SCALING_STRETCH;
        const DXGI_SWAP_CHAIN_FLAG swap_flags = (DXGI_SWAP_CHAIN_FLAG)0;

#if USE_DIRECTCOMPOSITION
        constexpr UINT backBufferCount = 2;
        const DXGI_SWAP_EFFECT swap_effect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
#else
        constexpr UINT backBufferCount = 1;
        const DXGI_SWAP_EFFECT swap_effect = DXGI_SWAP_EFFECT_DISCARD;
#endif

        // If the swap chain already exists, resize it, otherwise create one.
        if(swap_chain.Get() != null) {

            HRESULT hr = swap_chain->ResizeBuffers(backBufferCount, client_width, client_height, format, swap_flags);

            if(hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {

                CHK_HR(create_device());
                swap_chain.Reset();
            }
        }

        if(swap_chain.Get() == null) {

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
            swapChainDesc.Width = client_width;
            swapChainDesc.Height = client_height;
            swapChainDesc.Format = format;
            swapChainDesc.SampleDesc.Count = 1;
            swapChainDesc.SampleDesc.Quality = 0;
            swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapChainDesc.BufferCount = backBufferCount;
            swapChainDesc.SwapEffect = swap_effect;
            swapChainDesc.Scaling = scaling_mode;
            swapChainDesc.Flags = swap_flags;

            DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
            fsSwapChainDesc.Windowed = TRUE;

#if USE_DIRECTCOMPOSITION
            CHK_HR(dxgiFactory->CreateSwapChainForComposition(d3d_device.Get(), &swapChainDesc, NULL, &swap_chain));

            CHK_HR(DCompositionCreateDevice(NULL, IID_PPV_ARGS(&directcomposition_device)));
            CHK_HR(directcomposition_device->CreateTargetForHwnd(window, FALSE, &directcomposition_target));
            CHK_HR(directcomposition_device->CreateVisual(&directcomposition_visual));
            CHK_HR(directcomposition_target->SetRoot(directcomposition_visual.Get()));
            CHK_HR(directcomposition_visual->SetContent(swap_chain.Get()));
            CHK_HR(directcomposition_device->Commit());
#else
            CHK_HR(
                dxgiFactory->CreateSwapChainForHwnd(d3d_device.Get(), window, &swapChainDesc, NULL, NULL, &swap_chain));
#endif
        }

        ComPtr<ID3D11Texture2D> back_buffer;
        CHK_HR(swap_chain->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf())));
        D3D_SET_NAME(back_buffer);

        CD3D11_RENDER_TARGET_VIEW_DESC desc(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_B8G8R8A8_UNORM);

        CHK_HR(d3d_device->CreateRenderTargetView(back_buffer.Get(), &desc, &rendertarget_view));
        D3D_SET_NAME(rendertarget_view);

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
        D3D_SET_NAME(blend_state);

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

        CHK_HR(
            d2d_factory->CreateDxgiSurfaceRenderTarget(render_surface.Get(), &props, d2d_render_target.GetAddressOf()));

        // TODO (chs): settings colors for these elements

        CHK_HR(d2d_render_target->CreateSolidColorBrush(D2D1_COLOR_F{ 1, 1, 1, 0.9f }, &text_fg_brush));
        CHK_HR(d2d_render_target->CreateSolidColorBrush(D2D1_COLOR_F{ 1, 1, 1, 0.25f }, &text_outline_brush));
        CHK_HR(d2d_render_target->CreateSolidColorBrush(D2D1_COLOR_F{ 0, 0, 0, 0.4f }, &text_bg_brush));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // paste the clipboard - if it's a bitmap, paste that else if it's
    // a string, try to load that file

    HRESULT on_paste()
    {
        if(window == null) {
            return E_UNEXPECTED;
        }

        // if clipboard contains a PNG or DIB, make it look like a file we just loaded

        UINT cf_png = RegisterClipboardFormatW(L"PNG");
        UINT cf_filename = RegisterClipboardFormatW(CFSTR_FILENAMEW);

        image::image_file &f = clipboard_image_file;
        f.is_clipboard = true;
        f.bytes.clear();
        f.pixels.clear();
        f.hresult = E_FAIL;

        if(IsClipboardFormatAvailable(cf_png)) {

            CHK_HR(append_clipboard_to_buffer(f.bytes, cf_png));

        } else if(IsClipboardFormatAvailable(CF_DIB)) {

            f.bytes.resize(sizeof(BITMAPFILEHEADER));

            CHK_HR(append_clipboard_to_buffer(f.bytes, CF_DIB));

            BITMAPFILEHEADER *b = reinterpret_cast<BITMAPFILEHEADER *>(f.bytes.data());
            BITMAPINFOHEADER *i = reinterpret_cast<BITMAPINFOHEADER *>(b + 1);
            mem_clear(b);
            b->bfType = 'MB';
            b->bfSize = (DWORD)f.bytes.size();
            b->bfOffBits = sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER);
            if(i->biCompression == BI_BITFIELDS) {
                b->bfOffBits += 12;
            }
        }

        if(!f.bytes.empty()) {
            clear_selection();
            f.filename = localize(IDS_CLIPBOARD_FILENAME);
            f.hresult = S_OK;
            f.index = -1;
            f.is_cache_load = true;
            f.view_count = 0;
            CHK_HR(image::decode(&f));
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
            return file_dropper.on_drop_string(reinterpret_cast<wchar const *>(buffer.data()));
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // scan a folder - this runs in the folder_scanner_thread
    // results are sent to the main thread with SendMessage

    HRESULT do_folder_scan(wchar const *folder_path)
    {
        LOG_CONTEXT("folder_scan");

        std::wstring path;

        CHK_HR(file::get_path(folder_path, path));

        delete[] folder_path;

        LOG_INFO(L"Scan folder {}", path);

        file::scan_folder_sort_field sort_field = file::scan_folder_sort_field::name;
        file::scan_folder_sort_order order = file::scan_folder_sort_order::ascending;

        file::folder_scan_result *results;

        CHK_HR(scan_folder(path, sort_field, order, &results, quit_event));

        // send the results to the window, it will forward them to the app
        WaitForSingleObject(window_created_event, INFINITE);
        SendMessage(window, app::WM_FOLDER_SCAN_COMPLETE, 0, reinterpret_cast<LPARAM>(results));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // file_loader_thread waits for file load requests and kicks off
    // threads for each one

    void file_loader_function()
    {
        MSG msg;

        // respond to requests from main thread, quit if quit_event gets set
        while(MsgWaitForMultipleObjects(1, &quit_event, false, INFINITE, QS_POSTMESSAGE) != WAIT_OBJECT_0) {
            if(PeekMessage(&msg, null, 0, 0, PM_REMOVE) != 0) {
                switch(msg.message) {
                case WM_LOAD_FILE:
                    start_file_loader(reinterpret_cast<image::image_file *>(msg.lParam));
                    break;
                }
            }
        }
        LOG_INFO(L"File loader thread exit");
    }

    //////////////////////////////////////////////////////////////////////
    // file_scanner_thread waits for scan requests and processes them
    // in this thread

    void scanner_function()
    {
        LOG_CONTEXT("folder_scan");

        CoInitialize(null);

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
        LOG_INFO(L"Scanner thread exit");
    }

    //////////////////////////////////////////////////////////////////////
    // if loaded file is in the current folder, find index with list of files

    HRESULT update_file_index(image::image_file *f)
    {
        if(current_folder_scan == null || f == null) {
            return E_INVALIDARG;
        }
        std::wstring folder;
        CHK_HR(file::get_path(f->filename, folder));

        if(_wcsicmp(folder.c_str(), current_folder_scan->path.c_str()) != 0) {
            return E_CHANGED_STATE;
        }

        std::wstring name;
        CHK_HR(file::get_filename(f->filename, name));
        int id = 0;
        for(auto &ff : current_folder_scan->files) {
            if(_wcsicmp(ff.name.c_str(), name.c_str()) == 0) {
                f->index = id;
                LOG_DEBUG(L"{} is at index {}", name, id);
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
    // folder scan complete

    void on_folder_scanned(file::folder_scan_result *scan_result)
    {
        LOG_INFO(L"{} images found in {}", scan_result->files.size(), scan_result->path);

        current_folder_scan.reset(scan_result);

        if(current_file != null && current_file->index == -1) {
            update_file_index(current_file);
        }

        warm_cache();
    }

    //////////////////////////////////////////////////////////////////////
    // move to another file in the folder

    void move_file_cursor(int movement)
    {
        // can't key left/right while folder is still being scanned
        // TODO(chs): remember the moves and apply them when the scan is complete?
        if(current_folder_scan == null || current_file_cursor == -1) {
            return;
        }

        int new_file_cursor = std::clamp(current_file_cursor + movement, 0, (int)current_folder_scan->files.size() - 1);

        if(new_file_cursor != current_file_cursor) {
            current_file_cursor = new_file_cursor;
            std::wstring const &name = current_folder_scan->files[current_file_cursor].name;
            load_image(std::format(L"{}\\{}", current_folder_scan->path, name));
        }
    }

    //////////////////////////////////////////////////////////////////////
    // a file got loaded
    // move it from loading into loaded pile
    // show it if it was the most recently requested image
    // maintain cache

    void on_file_load_complete(image::image_file *f)
    {
        LOG_DEBUG(L"LOADED {}", f->filename);

        if(FAILED(f->hresult)) {
            set_message(std::format(L"{} {}", f->filename, windows_error_message(f->hresult)), 5);
            delete f;
            return;
        }

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

    bool get_mouse_buttons(mouse_button_t button)
    {
        return (mouse_grab & (1 << static_cast<int>(button))) != 0;
    }

    //////////////////////////////////////////////////////////////////////

    void set_mouse_button(mouse_button_t button)
    {
        int mask = 1 << static_cast<int>(button);
        if(mouse_grab == 0) {
            SetCapture(window);
        }
        mouse_grab |= mask;
    }

    //////////////////////////////////////////////////////////////////////

    void clear_mouse_button(mouse_button_t button)
    {
        int mask = 1 << static_cast<int>(button);
        mouse_grab &= ~mask;
        if(mouse_grab == 0) {
            ReleaseCapture();
        }
    }

    //////////////////////////////////////////////////////////////////////
    // this is a mess, but kinda necessarily so

    HRESULT get_startup_rect_and_style(RECT *r, DWORD *style, DWORD *ex_style)
    {
        if(r == null || style == null || ex_style == null) {
            return E_INVALIDARG;
        }

#if USE_DIRECTCOMPOSITION
        *ex_style = WS_EX_NOREDIRECTIONBITMAP;
#else
        *ex_style = 0;
#endif

        // if settings.fullscreen, use settings.fullscreen_rect (which will be on a monitor (which may
        // or may not still exist...))

        int default_monitor_width = GetSystemMetrics(SM_CXSCREEN);
        int default_monitor_height = GetSystemMetrics(SM_CYSCREEN);

        // default startup is windowed, 2/3rd size of default monitor
        if(settings.first_run) {
            settings.fullscreen_rect = { 0, 0, default_monitor_width, default_monitor_height };
            *style = WS_OVERLAPPEDWINDOW;
            *r = { 0, 0, default_monitor_width * 2 / 3, default_monitor_height * 2 / 3 };
            window_width = rect_width(*r);
            window_height = rect_height(*r);
            AdjustWindowRectEx(r, *style, FALSE, *ex_style);
            *r = center_rect_on_default_monitor(*r);
            return S_OK;
        }

        switch(settings.fullscreen_startup_mode) {
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

            // get client size of window rect for WS_OVERLAPPEDWINDOW
            RECT z{ 0, 0, 0, 0 };
            AdjustWindowRectEx(&z, *style, false, *ex_style);
            window_width = std::max(100, rect_width(*r) - rect_width(z));
            window_height = std::max(100, rect_height(*r) - rect_height(z));

        } else {

            *style = WS_POPUP;

            // check the monitor is still there and the same size
            MONITORINFO i;
            i.cbSize = sizeof(MONITORINFO);
            HMONITOR m = MonitorFromPoint(rect_top_left(settings.fullscreen_rect), MONITOR_DEFAULTTONEAREST);
            if(m != null && GetMonitorInfo(m, &i) &&
               memcmp(&settings.fullscreen_rect, &i.rcMonitor, sizeof(RECT)) == 0) {
                *r = settings.fullscreen_rect;
            } else {
                *r = { 0, 0, default_monitor_width, default_monitor_height };
            }
            // client size is same as window rect for WS_POPUP (no border/caption etc)
            window_width = rect_width(*r);
            window_height = rect_height(*r);
        }
        LOG_INFO(L"Startup window is {}x{} (at {},{})", window_width, window_height, r->left, r->top);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // Setup the shortcut key labels in the popup menu based on the accelerators

    HRESULT setup_menu_accelerators(HMENU menu)
    {
        // admin for enabling/disabling menu items based on app state

        static auto got_selection = []() -> uint { return current_file != null && select_active ? 0 : MFS_DISABLED; };

        static auto got_image = []() -> uint { return current_file != null ? 0 : MFS_DISABLED; };

        static auto check_alpha = []() -> uint { return settings.checkerboard_enabled ? MFS_CHECKED : 0; };

        static auto check_fullscreen = []() -> uint { return settings.fullscreen ? MFS_CHECKED : 0; };

        static auto check_fixedgrid = []() -> uint { return settings.fixed_checkerboard ? MFS_CHECKED : 0; };

        static std::unordered_map<UINT, std::function<uint()>> menu_process_table = {
            { ID_COPY, got_selection },
            { ID_SELECT_ALL, got_image },
            { ID_SELECT_NONE, got_image },
            { ID_SELECT_CROP, got_selection },
            { ID_FILE_SAVE, got_image },
            { ID_FILE_NEXT, got_image },
            { ID_FILE_PREV, got_image },
            { ID_VIEW_ALPHA, check_alpha },
            { ID_VIEW_FULLSCREEN, check_fullscreen },
            { ID_VIEW_FIXEDGRID, check_fixedgrid },
            { ID_ZOOM_1, got_image },
            { ID_ZOOM_ALL, got_image },
            { ID_ZOOM_CENTER, got_image },
            { ID_ZOOM_FIT, got_image },
            { ID_ZOOM_SHRINKTOFIT, got_image },
            { ID_ZOOM_ORIGINAL, got_image },
            { ID_ZOOM_RESET, got_image },
            { ID_ZOOM_SELECTION, got_image },
        };

        // scan the menu to enable/disable, add/remove checks and add hotkey info

        std::stack<HMENU> menu_stack;

        menu_stack.push(menu);

        while(!menu_stack.empty()) {

            HMENU cur_menu = menu_stack.top();
            menu_stack.pop();

            int item_count = GetMenuItemCount(cur_menu);

            for(int i = 0; i < item_count; ++i) {

                MENUITEMINFOW mii;
                mii.cbSize = sizeof(mii);
                mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_SUBMENU;

                if(GetMenuItemInfoW(cur_menu, i, MF_BYPOSITION, &mii)) {

                    // if it's a sub menu, just push it
                    if(mii.hSubMenu != null) {
                        menu_stack.push(mii.hSubMenu);
                    }

                    // else setup the menu item
                    else if(mii.fType == MFT_STRING) {

                        // get the menu item text
                        mii.fMask = MIIM_STRING;
                        mii.dwItemData = 0;
                        mii.dwTypeData = null;
                        mii.cch = 0;
                        CHK_BOOL(GetMenuItemInfoW(cur_menu, i, MF_BYPOSITION, &mii));
                        mii.cch += 1;
                        std::wstring text;
                        text.resize(mii.cch);
                        mii.fMask = MIIM_STRING;
                        mii.dwItemData = 0;
                        mii.dwTypeData = text.data();
                        CHK_BOOL(GetMenuItemInfoW(cur_menu, i, MF_BYPOSITION, &mii));
                        text.pop_back();
                        text = text.substr(0, text.find('\t'));    // truncate if there's already a tab

                        // append hotkeys to string
                        std::wstring key_label;
                        if(hotkeys::get_hotkey_text(mii.wID, key_label) == S_OK) {
                            text = std::format(L"{}\t{}", text, key_label);
                        }

                        // callback sets enabled/disabled/checked states
                        mii.fState = MF_ENABLED;    // which is 0
                        auto process_fn = menu_process_table.find(mii.wID);
                        if(process_fn != menu_process_table.end()) {
                            mii.fState |= process_fn->second();
                        }

                        // set the text and state
                        mii.fMask = MIIM_STRING | MIIM_STATE;
                        mii.dwTypeData = text.data();
                        mii.cch = static_cast<uint>(text.size());

                        CHK_BOOL(SetMenuItemInfoW(cur_menu, i, MF_BYPOSITION, &mii));
                    }
                }
            }
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // toggle between normal window and fake fullscreen

    void toggle_fullscreen()
    {
        settings.fullscreen = !settings.fullscreen;

        DWORD style = WS_OVERLAPPEDWINDOW;
        DWORD ex_style = 0;

        if(settings.fullscreen) {
            GetWindowPlacement(window, &settings.window_placement);
            LOG_INFO(L"toggle_fullscreen: {}", rect_to_string(settings.window_placement.rcNormalPosition));
            style = WS_POPUP;
        }

        SetWindowLongPtr(window, GWL_STYLE, style);
        SetWindowLongPtr(window, GWL_EXSTYLE, ex_style);

        if(settings.fullscreen) {

            MONITORINFO monitor_info{ 0 };
            monitor_info.cbSize = sizeof(MONITORINFO);

            HMONITOR h = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
            RECT window_rect;
            if(h != null && GetMonitorInfoW(h, &monitor_info)) {
                window_rect = monitor_info.rcMonitor;
            } else {
                window_rect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
            }

            settings.fullscreen_rect = window_rect;

            int sx = window_rect.left;
            int sy = window_rect.top;
            int sw = rect_width(window_rect);
            int sh = rect_height(window_rect);

            // this hide/show nonsense to deal with D3D (and it's still ugly)
            SetWindowPos(window, HWND_TOP, sx, sy, sw, sh, SWP_FRAMECHANGED | SWP_HIDEWINDOW);
            ShowWindow(window, SW_SHOW);

        } else {

            ignore_dpi_for_a_moment = true;
            SetWindowPlacement(window, &settings.window_placement);
            ignore_dpi_for_a_moment = false;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // find where on the selection the mouse is hovering
    //    sets selection_hover
    //    sets mouse cursor appropriately

    void check_selection_hover(vec2 pos)
    {
        if(!select_active) {
            set_mouse_cursor(null);
            return;
        }

        rect_f select_rect(select_current, select_anchor);

        // get screen coords of selection rectangle
        vec2 tl = texture_to_screen_pos(select_rect.top_left());
        vec2 br = texture_to_screen_pos_unclamped(add_point({ 1, 1 }, select_rect.bottom_right()));

        // selection grab border is +/- N pixels (setting: 4 to 32 pixels)
        float border = dpi_scale(settings.select_border_grab_size) / 2.0f;
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
    // WM_[L/M/R]BUTTONDOWN]

    void on_mouse_button_down(POINT pos, mouse_button_t button)
    {
        assert(button < btn_count);

        mouse_click_timestamp[button] = GetTickCount64();
        mouse_click[button] = pos;
        mouse_pos[button] = pos;
        memset(mouse_offset, 0, sizeof(mouse_offset));

        set_mouse_button(button);

        if(button == settings.select_button) {

            bool renew_selection{ false };

            if(select_active && selection_hover != sel_hover_outside) {

                vec2 tl = vec2::min(select_anchor, select_current);
                vec2 br = vec2::max(select_anchor, select_current);

                if(selection_hover == sel_hover_topleft || selection_hover == sel_hover_topright ||
                   selection_hover == sel_hover_bottomleft || selection_hover == sel_hover_bottomright) {

                    select_current = clamp_to_texture(screen_to_texture_pos(pos));
                    grabbing_selection = false;
                    renew_selection = true;

                    if((selection_hover & sel_hover_left) != 0) {
                        select_anchor.x = br.x;
                    } else {
                        select_anchor.x = tl.x;
                    }
                    if((selection_hover & sel_hover_top) != 0) {
                        select_anchor.y = br.y;
                    } else {
                        select_anchor.y = tl.y;
                    }
                } else {
                    grabbing_selection = true;
                    drag_select_pos = vec2::floor(screen_to_texture_pos(pos));
                }
            }

            if(!renew_selection && !grabbing_selection && image_texture.Get() != null) {

                select_anchor = clamp_to_texture(screen_to_texture_pos(pos));
                select_current = select_anchor;
                select_active = false;
            }

        } else if(button == settings.zoom_button && !popup_menu_active) {

            ShowCursor(FALSE);
        }
    }

    //////////////////////////////////////////////////////////////////////
    // WM_[L/M/R]BUTTONUP]

    void on_mouse_button_up(POINT pos, uint button)
    {
        mouse_button_t btn = static_cast<mouse_button_t>(button);
        clear_mouse_button(btn);

        // if RMB released within double-click time and haven't
        // moved it much since they pressed it, show popup menu

        if(btn == settings.menu_button) {

            uint64 since = GetTickCount64() - mouse_click_timestamp[settings.menu_button];

            if(since < static_cast<uint64>(GetDoubleClickTime())) {

                uint hover_high;
                uint hover_wide;

                SystemParametersInfo(SPI_GETMOUSEHOVERHEIGHT, 0, &hover_high, 0);
                SystemParametersInfo(SPI_GETMOUSEHOVERWIDTH, 0, &hover_wide, 0);

                POINT const &click_pos = mouse_click[settings.menu_button];

                uint x_distance = std::abs(click_pos.x - pos.x);
                uint y_distance = std::abs(click_pos.y - pos.y);

                if(x_distance < hover_wide && y_distance < hover_high) {

                    HMENU menu = LoadMenu(app::instance, MAKEINTRESOURCE(IDR_MENU_POPUP));
                    HMENU popup_menu = GetSubMenu(menu, 0);
                    POINT screen_pos{ click_pos.x, click_pos.y };
                    ClientToScreen(window, &screen_pos);

                    setup_menu_accelerators(popup_menu);

                    // insert recent files into the recent files menu entry

                    bool got_recent_files{ true };
                    if(FAILED(recent_files::get_files(recent_files_list))) {
                        recent_files_list.push_back(std::format(L"{}", localize(IDS_SCANNING_RECENT_FILES)));
                        got_recent_files = false;
                    }

                    MENUITEMINFOW dummy_info;
                    mem_clear(&dummy_info);
                    dummy_info.cbSize = sizeof(dummy_info);
                    dummy_info.fMask = MIIM_ID;
                    if(GetMenuItemInfoW(menu, ID_RECENT_DUMMY, MF_BYCOMMAND, &dummy_info) == 0) {
                        log_win32_error(L"GetMenuItemInfoW");
                    } else {
                        uint index = ID_RECENT_FILE_00;
                        MENUITEMINFOW recent_info;
                        mem_clear(&recent_info);
                        recent_info.cbSize = sizeof(recent_info);
                        recent_info.fMask = MIIM_ID | MIIM_STRING | MIIM_DATA;
                        recent_info.fType = MFT_STRING;
                        for(auto &f : recent_files_list) {
                            recent_info.wID = index;
                            recent_info.cch = static_cast<uint>(f.size());
                            recent_info.dwTypeData = f.data();
                            InsertMenuItemW(menu, dummy_info.wID, MF_BYCOMMAND, &recent_info);

                            if(!got_recent_files) {
                                EnableMenuItem(menu, index, MF_BYCOMMAND | MF_GRAYED);
                            }
                            index += 1;
                        }
                        DeleteMenu(menu, ID_RECENT_DUMMY, MF_BYCOMMAND);
                    }

                    popup_menu_active = true;
                    TrackPopupMenu(popup_menu, TPM_RIGHTBUTTON, screen_pos.x, screen_pos.y, 0, window, null);
                    clear_message();
                    m_timer.reset();
                    popup_menu_active = false;
                }
            }
        }

        if(btn == settings.zoom_button) {

            ShowCursor(TRUE);
            clear_mouse_button(settings.drag_button);    // in case they pressed the drag button while zooming

        } else if(btn == settings.select_button) {

            if(grabbing_selection) {
                clear_selection();
            } else {

                drag_selection = false;
                grabbing_selection = false;

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
    // set max zoom to 1/10th of monitor largest dimension

    void set_max_zoom(RECT const *r)
    {
        HMONITOR m = MonitorFromPoint(rect_midpoint(*r), MONITOR_DEFAULTTOPRIMARY);
        if(m != null) {
            MONITORINFO mi;
            mi.cbSize = sizeof(mi);
            if(GetMonitorInfo(m, &mi)) {
                long w = rect_width(mi.rcMonitor);
                long h = rect_height(mi.rcMonitor);
                long dimension = std::max(w, h);
                max_zoom = floor(dimension / 10.0f);
                LOG_DEBUG(L"max_zoom set to {} (monitor is {}x{})", max_zoom, w, h);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////
    // zoom in or out, focusing on a point

    void do_zoom(POINT pos, int delta)
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

    void zoom_to_selection()
    {
        if(!select_active) {
            reset_zoom(zoom_mode_t::fit_to_window);
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
            return rect_f{
                window_width / 2 - mx * s, window_height / 2 - my * s, texture_width * s, texture_height * s
            };
        };

        vec2 tl = vec2::min(select_anchor, select_current);
        vec2 br = vec2::max(select_anchor, select_current);

        rect_f new_target = calc_target(tl, br);

        float scale = texture_width / new_target.w;

        vec2 extra{ ((small_label_size.x * 2) + dpi_scale(12)) * scale,
                    ((small_label_size.y * 2) + dpi_scale(6)) * scale };

        // still wrong after 2nd calc_target but less wrong enough
        target_rect = calc_target(sub_point(tl, extra), add_point(extra, br));

        has_been_zoomed_or_dragged = true;
    }

    //////////////////////////////////////////////////////////////////////
    // center the image in the window

    void center_in_window()
    {
        target_rect.x = (window_width - current_rect.w) / 2.0f;
        target_rect.y = (window_height - current_rect.h) / 2.0f;
    }

    //////////////////////////////////////////////////////////////////////
    // crop the image to the selection

    HRESULT crop_to_selection()
    {
        if(!select_active) {
            return E_NOT_VALID_STATE;
        }

        ComPtr<ID3D11Texture2D> tex;
        CHK_HR(copy_selection_to_texture(tex.GetAddressOf()));

        D3D11_TEXTURE2D_DESC desc;
        tex->GetDesc(&desc);
        int width = desc.Width;
        int height = desc.Height;

        select_active = false;

        image::image_file &f = clipboard_image_file;

        f.is_clipboard = true;
        f.bytes.clear();
        f.pixels.clear();
        f.filename = localize(IDS_CROPPED_FILENAME);
        f.hresult = S_OK;
        f.index = -1;
        f.is_cache_load = true;
        f.view_count = 0;

        D3D11_MAPPED_SUBRESOURCE mapped_resource{};
        CHK_HR(d3d_context->Map(tex.Get(), 0, D3D11_MAP_READ, 0, &mapped_resource));
        DEFER(d3d_context->Unmap(tex.Get(), 0));

        f.pixels.resize(static_cast<size_t>(mapped_resource.RowPitch) * height);

        memcpy(f.pixels.data(), mapped_resource.pData, f.pixels.size());

        f.img.pixels = f.pixels.data();
        f.img.width = width;
        f.img.height = height;
        f.img.row_pitch = mapped_resource.RowPitch;

        clear_selection();

        return show_image(&f);
    }

    //////////////////////////////////////////////////////////////////////
    // draw some text with a box round it

    HRESULT draw_string(std::wstring const &text,
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

        CHK_HR(dwrite_factory->CreateTextLayout(text.c_str(),
                                                (UINT32)text.size(),
                                                format,
                                                (float)window_width * 2,
                                                (float)window_height * 2,
                                                &text_layout));

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
    // draw text overlays after drawing the image

    HRESULT draw_text_overlays()
    {
        d2d_render_target->BeginDraw();

        if(crosshairs_active) {

            vec2 p = clamp_to_texture(vec2::floor(screen_to_texture_pos(cur_mouse_pos)));

            vec2 ts = sub_point(texture_size(), { 1, 1 });

            vec2 texel_pos = vec2::rotate(ts, p, rotation);

            if(flip_horiz) {
                texel_pos.x = ts.x - texel_pos.x;
            }

            if(flip_vert) {
                texel_pos.y = ts.y - texel_pos.y;
            }

            byte const *pixel{ null };
            CHK_HR(current_file->img.get_pixel(static_cast<uint>(texel_pos.x), static_cast<uint>(texel_pos.y), pixel));

            vec2 half_texel = mul_point(texel_size(), { 0.5f, 0.5f });
            vec2 screen_pos = add_point(texture_to_screen_pos(p), half_texel);

            vec2 draw_pos = add_point(screen_pos, { -dpi_scale(label_pad * 4), -dpi_scale(label_pad * 2) });

            std::wstring text{ std::format(L"{},{}", p.x, p.y) };
            // std::wstring text{ std::format(
            //    L"After {},{} from Before {},{} ({})", texel_pos.x, texel_pos.y, p.x, p.y, (uint)rotation) };
            draw_string(text, label_format.Get(), draw_pos, { 1, 1 }, 1.0f, label_pad, label_pad);

            // pixels are BGRA, show as RGBA
            if(pixel != null) {
                rgb_pixel_text = std::format(L"{:02X}{:02X}{:02X}{:02X}", pixel[2], pixel[1], pixel[0], pixel[3]);
            } else {
                rgb_pixel_text = L"????????";
            }
            draw_pos = add_point(screen_pos, { dpi_scale(label_pad * 4), -dpi_scale(label_pad * 2) });

            draw_string(rgb_pixel_text, label_format.Get(), draw_pos, { 0, 1 }, 1.0f, label_pad, label_pad);
        }

        if(select_active) {

            vec2 select_tl = vec2::min(select_anchor, select_current);
            vec2 select_br = vec2::max(select_anchor, select_current);

            vec2 offset{ dpi_scale(18.0f), dpi_scale(12.0f) };
            vec2 s_tl = sub_point(texture_to_screen_pos_unclamped(select_tl), offset);
            vec2 s_br = add_point(texture_to_screen_pos_unclamped({ select_br.x + 1, select_br.y + 1 }), offset);
            draw_string(std::format(L"{},{}", (int)select_tl.x, (int)select_tl.y),
                        label_format.Get(),
                        s_tl,
                        { 1, 1 },
                        1.0f,
                        2,
                        2);
            float sw = floor(select_br.x) - floorf(select_tl.x) + 1;
            float sh = floorf(select_br.y) - floorf(select_tl.y) + 1;
            draw_string(std::format(L"{}x{}", sw, sh), label_format.Get(), s_br, { 0, 0 }, 1.0f, 2, 2);
        }

        if(!current_message.empty()) {

            float elapsed = static_cast<float>(m_timer.wall_time() - message_timestamp);

            float message_alpha{ 0 };

            if(message_fade_time == 0) {

                message_alpha = 1.0f;

            } else if(elapsed < message_fade_time) {

                message_alpha = elapsed / std::max(0.1f, static_cast<float>(message_fade_time));

                message_alpha = 1 - powf(message_alpha, 16);
            }

            if(message_alpha > 0.0f) {

                vec2 pos{ dpi_scale(16.0f), window_height - dpi_scale(12.0f) };

                draw_string(std::format(L"{}", current_message),
                            banner_format.Get(),
                            pos,
                            { 0.0f, 1.0f },
                            message_alpha,
                            dpi_scale(3.0f),
                            dpi_scale(3.0f));
            }
        }

        CHK_HR(d2d_render_target->EndDraw());

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // render a frame

    HRESULT render()
    {
        // create d3d stuff if necessary

        if(d3d_device.Get() == null) {
            CHK_HR(create_device());
        }

        if(rendertarget_view.Get() == null) {
            CHK_HR(create_resources());
        }

        // setup rendertarget and viewport

        d3d_context->OMSetRenderTargets(1, rendertarget_view.GetAddressOf(), null);

        CD3D11_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(window_width), static_cast<float>(window_height));

        d3d_context->RSSetViewports(1, &viewport);

        // clear screen

        vec4 border_color = color_from_uint32(settings.border_color);
        d3d_context->ClearRenderTargetView(rendertarget_view.Get(), reinterpret_cast<float *>(&border_color));

        // all the image related stuff

        if(image_texture.Get() != null) {

            // draw texture with optional checkerboard background

            // setup UV transform based on flip/rotate

            shader.texture_transform = get_texture_transform();

            // setup checkerboard or background color

            vec4 g1;
            vec4 g2;

            if(!settings.checkerboard_enabled) {

                g1 = color_from_uint32(settings.background_color);
                g2 = g1;

            } else {

                g1 = color_from_uint32(settings.grid_color_1);
                g2 = color_from_uint32(settings.grid_color_2);

                // setup checkerboard offset

                shader.checkerboard_offset = { 0, 0 };

                if(!settings.fixed_checkerboard) {

                    shader.checkerboard_offset = { -current_rect.x, -current_rect.y };
                }

                // setup checkerboard size

                uint gs = settings.grid_size * (1 << settings.grid_multiplier);
                shader.check_strip_size = 1.0f / std::max(4u, gs);
            }

            shader.colors[0] = g1;
            shader.colors[1] = g2;
            shader.colors[2] = g2;
            shader.colors[3] = g1;

            // draw the image

            d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

            d3d_context->VSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
            d3d_context->VSSetShader(vertex_shader.Get(), null, 0);

            d3d_context->PSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
            d3d_context->PSSetShader(texture_shader.Get(), null, 0);
            d3d_context->PSSetShaderResources(0, 1, image_texture_view.GetAddressOf());
            d3d_context->PSSetSamplers(0, 1, sampler_state.GetAddressOf());

            d3d_context->RSSetState(rasterizer_state.Get());

            d3d_context->OMSetBlendState(null, null, 0xffffffff);

            draw_rectangle(current_rect, viewport);

            // draw selection overlay and selection outline

            uint32 select_color = settings.select_fill_color;
            bool draw_selection = select_active;

            vec2 tl = vec2::min(select_anchor, select_current);
            vec2 br = vec2::max(select_anchor, select_current);

            if(!select_active) {
                tl = vec2{ 0, 0 };
                br = texture_size();
                select_color = 0x00000000;
            }

            double const select_flash_time = settings.copy_flash_time / 10.0;

            if(copy_timestamp != 0.0) {

                double since = m_timer.current() - copy_timestamp;

                if(since <= select_flash_time) {

                    draw_selection = true;
                    int lerp = static_cast<int>((since / select_flash_time) * 255.0);
                    select_color = color_lerp(settings.copy_flash_color, select_color, lerp);

                } else {

                    copy_timestamp = 0.0;
                }
            }

            if(draw_selection) {

                // first the solid rectangle selection overlay

                d3d_context->PSSetShader(solid_shader.Get(), null, 0);
                d3d_context->OMSetBlendState(blend_state.Get(), null, 0xffffffff);

                br = add_point(br, { 1.0f, 1.0f });

                // TODO (chs): fix the rounding errors which can happen in here

                tl = texture_to_screen_pos_unclamped(tl);
                br = texture_to_screen_pos_unclamped(br);

                shader.colors[0] = color_from_uint32(select_color);

                CHK_HR(draw_rectangle({ tl.x, tl.y, br.x - tl.x, br.y - tl.y }, viewport));

                if(select_active) {

                    // then the stripey outline

                    d3d_context->PSSetShader(stripe_shader.Get(), null, 0);

                    shader.colors[0] = color_from_uint32(settings.select_outline_color1);
                    shader.colors[1] = color_from_uint32(settings.select_outline_color2);

                    shader.check_strip_size = 1.0f / settings.dash_length;

                    // NOTE: dodgy fix for the stripe-near-zero problem is to add stripe * max(window_width,
                    // window_height) to the frame count so it doesn't wrap near the origin

                    shader.frame = fmodf(settings.dash_anim_speed * frame_count / 10.0f, settings.dash_length * -2.0f) +
                                   settings.dash_length * std::max(window_width, window_height);

                    float bw = static_cast<float>(settings.select_border_width);

                    // top
                    rect_f horiz{ tl.x - bw, tl.y - bw, (br.x - tl.x) + bw * 2.0f, bw };
                    CHK_HR(draw_rectangle(horiz, viewport));

                    // bottom
                    horiz.y = br.y;
                    CHK_HR(draw_rectangle(horiz, viewport));

                    // left
                    rect_f vert{ vert.x = tl.x - bw, tl.y, bw, br.y - tl.y };
                    CHK_HR(draw_rectangle(vert, viewport));

                    // right
                    vert.x = br.x;
                    CHK_HR(draw_rectangle(vert, viewport));
                }
            }

            // draw crosshairs

            if(crosshairs_active) {

                d3d_context->PSSetShader(stripe_shader.Get(), null, 0);
                d3d_context->OMSetBlendState(blend_state.Get(), null, 0xffffffff);

                shader.colors[0] = color_from_uint32(settings.crosshair_color1);
                shader.colors[1] = color_from_uint32(settings.crosshair_color2);

                shader.check_strip_size = 1.0f / settings.crosshair_dash_length;

                // NOTE: same dodgy fix for the stripe-near-zero problem

                shader.frame = fmodf(settings.crosshair_dash_anim_speed * frame_count / 10.0f,
                                     settings.crosshair_dash_length * -2.0f) +
                               settings.crosshair_dash_length * std::max(window_width, window_height);

                // get screen position of center of texel under cursor

                vec2 p = clamp_to_texture(screen_to_texture_pos(cur_mouse_pos));
                vec2 c = mul_point(texel_size(), { 0.5f, 0.5f });
                c = add_point(texture_to_screen_pos(p), c);

                float bw = static_cast<float>(settings.crosshair_width);
                float hbw = bw / 2.0f;

                // horizontal across the whole screen
                rect_f horiz{ 0, c.y - hbw, static_cast<float>(window_width), bw };
                CHK_HR(draw_rectangle(horiz, viewport));

                // top vertical bit
                rect_f vert{ c.x - hbw, 0, bw, c.y - hbw };
                CHK_HR(draw_rectangle(vert, viewport));

                // botton vertical bit
                vert.y = c.y + hbw;
                vert.h = window_height - vert.y;
                CHK_HR(draw_rectangle(vert, viewport));
            }
        }

        draw_text_overlays();

        HRESULT hr = swap_chain->Present(1, 0);

        if(hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {

            CHK_HR(create_device());

            CHK_HR(create_resources());

            if(current_file != null) {
                CHK_HR(display_image(current_file));
            }
        }

        return hr;
    }

    //////////////////////////////////////////////////////////////////////
    // call this repeatedly when there are no windows messages available

    HRESULT update()
    {
        // TODO (chs): allow crosshairs on another key
        crosshairs_active = current_file != null && (GetKeyState(VK_MENU) & 0x8000) != 0;

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

        // update image position/zoom towards target

        lerp(current_rect.x, target_rect.x);
        lerp(current_rect.y, target_rect.y);
        lerp(current_rect.w, target_rect.w);
        lerp(current_rect.h, target_rect.h);

        // reset mouse to original click position in zoom mode

        if(get_mouse_buttons(settings.zoom_button)) {

            POINT click_pos = mouse_click[settings.zoom_button];
            ClientToScreen(window, &click_pos);
            SetCursorPos(click_pos.x, click_pos.y);
        }

        memset(mouse_offset, 0, sizeof(mouse_offset));

        // delay showing window until file is loaded (or 1/4 second, whichever comes first)

        if(frame_count > 0 && (m_timer.wall_time() > 0.25 || image_texture.Get() != null) && !IsWindowVisible(window)) {

            // don't use saved window placement if it wasn't loaded from saved settings
            // (or its in fullscreen mode)

            if(settings.first_run || settings.fullscreen) {
                ShowWindow(window, SW_SHOW);
                settings.first_run = false;
            } else if(!settings.fullscreen) {
                SetWindowPlacement(window, &settings.window_placement);
            }
        }

        if(rendertarget_view.Get() != null) {
            CHK_HR(render());
        }

        settings_ui::update();

        frame_count += 1;

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // send new settings to the settings dialog if it's active...

    void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

        case ID_VIEW_FULLSCREEN:
            toggle_fullscreen();
            break;

        case ID_PASTE:
            on_paste();
            break;

        case ID_COPY:
            on_copy();
            break;

        case ID_COPY_RGB:
            if(crosshairs_active && SUCCEEDED(copy_string_to_clipboard(rgb_pixel_text))) {
                set_message(std::format(L"{} {}", localize(IDS_COPIED), rgb_pixel_text), 3);
            }
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
            clear_selection();
            break;

        case ID_VIEW_ALPHA:
            settings.checkerboard_enabled = !settings.checkerboard_enabled;
            settings_ui::new_settings_update();
            break;

        case ID_VIEW_FIXEDGRID:
            settings.fixed_checkerboard = !settings.fixed_checkerboard;
            settings_ui::new_settings_update();
            break;

        case ID_VIEW_GRIDSIZE:
            settings.grid_multiplier = (settings.grid_multiplier + 1) & 7;
            settings_ui::new_settings_update();
            break;

        case ID_VIEW_SETBACKGROUNDCOLOR:
            if(SUCCEEDED(dialog::select_color(
                   window, settings.background_color, localize(IDS_SETTING_NAME_BACKGROUND_COLOR).c_str()))) {
                settings_ui::new_settings_update();
            }
            break;

        case ID_VIEW_SETBORDERCOLOR:
            if(SUCCEEDED(dialog::select_color(
                   window, settings.border_color, localize(IDS_SETTING_NAME_BORDER_COLOR).c_str()))) {
                settings_ui::new_settings_update();
            }
            break;

        case ID_ROTATE_CLOCKWISE:
            rotate(1);
            break;

        case ID_ROTATE_COUNTER_CLOCKWISE:
            rotate(-1);
            break;

        case ID_FLIP_HORIZONTAL:
            flip(flip_horizontal);
            break;

        case ID_FLIP_VERTICAL:
            flip(flip_vertical);
            break;

        case ID_RESET_TRANSFORM:
            reset_transform();
            break;

        case ID_ZOOM_1:
            settings.zoom_mode = zoom_mode_t::one_to_one;
            reset_zoom(settings.zoom_mode);
            settings_ui::new_settings_update();
            break;

        case ID_ZOOM_FIT:
            settings.zoom_mode = zoom_mode_t::fit_to_window;
            reset_zoom(settings.zoom_mode);
            settings_ui::new_settings_update();
            break;

        case ID_ZOOM_SHRINKTOFIT:
            settings.zoom_mode = zoom_mode_t::shrink_to_fit;
            reset_zoom(settings.zoom_mode);
            settings_ui::new_settings_update();
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
            if(SUCCEEDED(dialog::open_file(window, selected_filename))) {
                load_image(selected_filename);
            }
        } break;

        case ID_FILE_SAVE: {

            if(current_file != null) {
                std::wstring filename;
                if(!current_file->is_clipboard) {
                    filename = current_file->filename;
                }
                std::wstring chosen_filename;
                if(SUCCEEDED(dialog::save_file(window, filename, chosen_filename))) {

                    image::image_t const &img = current_file->img;
                    HRESULT hr = image::save(chosen_filename,
                                             img.pixels,
                                             img.width,
                                             img.height,
                                             img.row_pitch,
                                             flip_horiz,
                                             flip_vert,
                                             rotation);
                    if(FAILED(hr)) {
                        std::wstring msg = std::format(L"{}\r\n\r\n{}\r\n\r\n{}",
                                                       localize(IDS_CANT_SAVE_FILE),
                                                       filename,
                                                       windows_error_message(hr));
                        message_box(window, msg, MB_ICONEXCLAMATION);
                    } else {
                        set_message(std::format(L"{} {}", localize(IDS_SAVED_FILE), filename), 5);
                    }
                }
            }
        } break;

        case ID_FILE_SETTINGS_EXPLORER:
            settings_ui::show_settings_dialog(window, IDD_DIALOG_SETTINGS_EXPLORER);
            break;

        case ID_FILE_SETTINGS:
            settings_ui::show_settings_dialog(window, IDD_DIALOG_SETTINGS_MAIN);
            break;

        case ID_EXIT:
            DestroyWindow(window);
            break;
        }

        if(id >= ID_RECENT_FILE_00 && id <= ID_RECENT_FILE_19) {
            uint index = id - ID_RECENT_FILE_00;
            if(index < recent_files_list.size()) {
                load_image_file(recent_files_list[index]);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void OnGetMinMaxInfo(HWND hwnd, LPMINMAXINFO lpMinMaxInfo)
    {
        lpMinMaxInfo->ptMinTrackSize = { 320, 200 };
    }

    //////////////////////////////////////////////////////////////////////

    LRESULT OnNCCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
    {
        window = hwnd;

        RAWINPUTDEVICE Rid[1];
        Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
        Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
        Rid[0].dwFlags = RIDEV_INPUTSINK;
        Rid[0].hwndTarget = hwnd;
        RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));

        // if we have a saved windowplacement and we're not in fullscreen mode

        if(!settings.first_run && !settings.fullscreen) {

            // set the window placement but keep the window hidden

            WINDOWPLACEMENT hidden = settings.window_placement;
            hidden.flags = 0;
            hidden.showCmd = SW_HIDE;
            SetWindowPlacement(window, &hidden);
        }

        // notify that the window is created

        SetEvent(window_created_event);

        file_dropper.InitializeDragDropHelper(window);

        setup_window_text();
        return TRUE;
    }

    //////////////////////////////////////////////////////////////////////

    void OnClose(HWND hwnd)
    {
        DestroyWindow(hwnd);
    }

    //////////////////////////////////////////////////////////////////////
    // WM_NCCALCSIZE happens before any visual update to the window so resize
    // backbuffer before any drawing to avoid flicker
    //
    // BUT minimize/maximize...

    UINT OnNCCalcSize(HWND hwnd, BOOL fCalcValidRects, NCCALCSIZE_PARAMS *lpcsp)
    {
        FORWARD_WM_NCCALCSIZE(hwnd, fCalcValidRects, lpcsp, DefWindowProcA);

        RECT const &new_client_rect = lpcsp->rgrc[0];

        // if starting window maximized, ignore the first wm_nccalcsize, it's got bogus dimensions

        if(!(frame_count == 0 && settings.window_placement.showCmd == SW_SHOWMAXIMIZED)) {

            int new_width = rect_width(new_client_rect);
            int new_height = rect_height(new_client_rect);
            WINDOWPLACEMENT wp;
            wp.length = sizeof(wp);
            GetWindowPlacement(window, &wp);

            if(wp.showCmd == SW_SHOWMINIMIZED || wp.showCmd == SW_MINIMIZE) {
                return S_OK;
            }

            window_width = std::max(new_width, 1);
            window_height = std::max(new_height, 1);

            if(FAILED(create_resources())) {
                // Hmmm
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
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    BOOL OnEraseBkgnd(HWND hwnd, HDC hdc)
    {
        return TRUE;
    }

    //////////////////////////////////////////////////////////////////////

    void OnPaint(HWND hwnd)
    {
        PAINTSTRUCT ps;
        (void)BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        if(s_in_sizemove) {
            update();
        }
    }

    //////////////////////////////////////////////////////////////////////
    // in single instance mode, we can get sent a new command line
    // this is how the other instance sends it to us

    void OnCopyData(HWND hwnd, HWND from, PCOPYDATASTRUCT c)
    {
        if(c != null) {

            switch((copydata_t)c->dwData) {

            case copydata_t::commandline: {

                if(s_minimized) {
                    ShowWindow(hwnd, SW_RESTORE);
                }
                SetForegroundWindow(hwnd);
                SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                SwitchToThisWindow(window, true);

                // must copy the data before returning from this message handler
                std::wstring cmd_line(reinterpret_cast<wchar const *>(c->lpData));
                on_command_line(cmd_line);

            } break;

            default:
                break;
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    BOOL OnSetCursor(HWND hwnd, HWND hwndCursor, UINT codeHitTest, UINT msg)
    {
        if(codeHitTest == HTCLIENT) {
            SetCursor(current_mouse_cursor);
            return TRUE;
        }
        return FORWARD_WM_SETCURSOR(hwnd, hwndCursor, codeHitTest, msg, DefWindowProcA);
    }

    //////////////////////////////////////////////////////////////////////

    BOOL OnWindowPosChanging(HWND hwnd, LPWINDOWPOS lpwpos)
    {
        lpwpos->cx &= -2;
        lpwpos->cy &= -2;
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void OnSize(HWND hwnd, UINT state, int cx, int cy)
    {
        if(state == SIZE_MINIMIZED) {
            if(!s_minimized) {
                s_minimized = true;
                s_in_suspend = true;
            }
        } else {
            if(s_minimized) {
                s_minimized = false;
                if(s_in_suspend) {
                    m_timer.reset();
                }
                s_in_suspend = false;
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
    {
        on_mouse_button_down({ x, y }, btn_left);
    }

    void OnRButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
    {
        on_mouse_button_down({ x, y }, btn_right);
    }

    void OnMButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
    {
        on_mouse_button_down({ x, y }, btn_middle);
    }

    void OnLButtonUp(HWND hwnd, int x, int y, UINT keyFlags)
    {
        on_mouse_button_up({ x, y }, btn_left);
    }

    void OnRButtonUp(HWND hwnd, int x, int y, UINT keyFlags)
    {
        on_mouse_button_up({ x, y }, btn_right);
    }

    void OnMButtonUp(HWND hwnd, int x, int y, UINT keyFlags)
    {
        on_mouse_button_up({ x, y }, btn_middle);
    }

    //////////////////////////////////////////////////////////////////////

    void OnMouseMove(HWND hwnd, int mx, int my, UINT keyFlags)
    {
        POINT pos{ mx, my };

        if(!get_mouse_buttons(settings.zoom_button)) {
            cur_mouse_pos = pos;
        }

        if(snap_mode == snap_mode_t::axis) {

            switch(snap_axis) {

            case shift_snap_axis_t::none: {
                int xd = cur_mouse_pos.x - shift_mouse_pos.x;
                int yd = cur_mouse_pos.y - shift_mouse_pos.y;
                float distance = dpi_scale(sqrtf((float)(xd * xd + yd * yd)));
                if(distance > axis_snap_radius) {
                    snap_axis = (std::abs(xd) > std::abs(yd)) ? shift_snap_axis_t::y : shift_snap_axis_t::x;
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

        for(uint i = btn_min; i < btn_count; ++i) {

            mouse_button_t btn = static_cast<mouse_button_t>(i);

            if(get_mouse_buttons(btn)) {

                mouse_offset[i] = add_point(mouse_offset[i], sub_point(cur_mouse_pos, mouse_pos[i]));
                mouse_pos[i] = cur_mouse_pos;
            }
        }

        if(grabbing_selection) {

            vec2 diff = vec2(sub_point(mouse_click[settings.select_button], pos));
            float len = vec2::length(diff);
            if(len > dpi_scale(settings.select_start_distance)) {
                drag_selection = true;
                grabbing_selection = false;
            }
        }

        if(selecting) {
            vec2 diff = vec2(sub_point(mouse_click[settings.select_button], pos));
            float len = vec2::length(diff);
            if(len > dpi_scale(settings.select_start_distance)) {
                select_active = true;
            }
        }

        if(get_mouse_buttons(settings.drag_button) && !get_mouse_buttons(settings.zoom_button)) {
            current_rect.x += mouse_offset[settings.drag_button].x;
            current_rect.y += mouse_offset[settings.drag_button].y;
            target_rect = current_rect;
            has_been_zoomed_or_dragged = true;
        }

        selecting = get_mouse_buttons(settings.select_button) && image_texture.Get() != null && !grabbing_selection;

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

                vec2 max = sub_point(texture_size(), { 1, 1 });
                vec2 min{ 0, 0 };

                if(selection_hover == sel_hover_inside) {

                    min = { 0, 0 };
                    vec2 ts = sub_point(texture_size(), { 1, 1 });
                    max = sub_point(ts, selection_size);

                } else {

                    if(selection_hover & sel_hover_left) {

                        x = &select_anchor.x;
                        max.y = min.y = select_anchor.y;
                    } else if(selection_hover & sel_hover_right) {

                        x = &select_current.x;
                        max.y = min.y = select_anchor.y;
                    }

                    if(selection_hover & sel_hover_top) {

                        y = &select_anchor.y;
                        max.x = min.x = select_anchor.x;

                    } else if(selection_hover & sel_hover_bottom) {

                        y = &select_current.y;
                        max.x = min.x = select_anchor.x;
                    }
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

            } else if(image_texture.Get() != null) {

                select_current = clamp_to_texture(screen_to_texture_pos(cur_mouse_pos));

                // force the selection to be square if ctrl is held
                if(snap_mode == snap_mode_t::square) {

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

        if(!get_mouse_buttons(settings.select_button) && select_active) {
            check_selection_hover(vec2(POINT{ mx, my }));
        } else if(selection_hover == selection_hover_t::sel_hover_outside) {
            set_mouse_cursor(null);
        }
    }

    //////////////////////////////////////////////////////////////////////

    void OnMouseWheel(HWND hwnd, int xPos, int yPos, int zDelta, UINT fwKeys)
    {
        POINT pos{ xPos, yPos };
        ScreenToClient(hwnd, &pos);
        do_zoom(pos, zDelta * 10 / WHEEL_DELTA);
    }

    //////////////////////////////////////////////////////////////////////

    void OnKeyDown(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
    {
        switch(vk) {

        case VK_SHIFT:
            if((flags & 0x4000) == 0) {
                shift_mouse_pos = cur_mouse_pos;
                snap_mode = snap_mode_t::axis;
                snap_axis = shift_snap_axis_t::none;
            }
            break;

        case VK_CONTROL:
            snap_mode = snap_mode_t::square;
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////

    void OnKeyUp(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
    {
        switch(vk) {
        case VK_SHIFT: {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(window, &p);
            snap_mode = snap_mode_t::none;
            OnMouseMove(hwnd, p.x, p.y, 0);
        } break;
        case VK_CONTROL:
            snap_mode = snap_mode_t::none;
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////

    void OnActivateApp(HWND hwnd, BOOL fActivate, DWORD dwThreadId)
    {
        if(fActivate) {
            snap_mode = snap_mode_t::none;
        } else {
            mouse_grab = 0;
        }
    }

    //////////////////////////////////////////////////////////////////////

    void OnDestroy(HWND hwnd)
    {
        if(!settings.fullscreen) {
            GetWindowPlacement(window, &settings.window_placement);
        }
        PostQuitMessage(0);
    }

    //////////////////////////////////////////////////////////////////////

    DWORD OnMenuChar(HWND hwnd, UINT ch, UINT flags, HMENU hmenu)
    {
        return MAKELRESULT(0, MNC_CLOSE);
    }

    //////////////////////////////////////////////////////////////////////

    void OnDpiChanged(HWND hwnd, UINT xdpi, UINT ydpi, RECT const *new_rect)
    {
        set_max_zoom(new_rect);

        if(!ignore_dpi_for_a_moment) {

            uint new_dpi = xdpi;

            current_rect.w = (current_rect.w * new_dpi) / dpi;
            current_rect.h = (current_rect.h * new_dpi) / dpi;

            dpi = static_cast<float>(new_dpi);

            create_text_formats();

            MoveWindow(window, new_rect->left, new_rect->top, rect_width(*new_rect), rect_height(*new_rect), true);
        }
    }

    //////////////////////////////////////////////////////////////////////

    void OnInput(HWND hwnd, HRAWINPUT input)
    {
        UINT dwSize = sizeof(RAWINPUT);
        static BYTE lpb[sizeof(RAWINPUT)];
        GetRawInputData(input, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
        RAWINPUT *raw = (RAWINPUT *)lpb;
        if(raw->header.dwType == RIM_TYPEMOUSE && get_mouse_buttons(settings.zoom_button) && !popup_menu_active) {
            int delta_y = static_cast<int>(raw->data.mouse.lLastY);
            do_zoom(mouse_click[settings.zoom_button], std::max(-4, std::min(-delta_y, 4)));
        }
    }

    //////////////////////////////////////////////////////////////////////

    void OnEnterSizeMove(HWND hwnd)
    {
        s_in_sizemove = true;
    }

    //////////////////////////////////////////////////////////////////////

    void OnExitSizeMove(HWND hwnd)
    {
        s_in_sizemove = false;
    }

    //////////////////////////////////////////////////////////////////////
    // NOTE: new_setting only valid if setting == PBT_POWERSETTINGCHANGE

    UINT OnPowerBroadcast(HWND hwnd, UINT setting, PPOWERBROADCAST_SETTING new_setting)
    {
        switch(setting) {

        case PBT_APMQUERYSUSPEND:
            s_in_suspend = true;
            return TRUE;

        case PBT_APMRESUMESUSPEND:
            if(!s_minimized) {
                if(s_in_suspend) {
                    m_timer.reset();
                }
                s_in_suspend = false;
            }
            return TRUE;
        }
        return FALSE;
    }

    //////////////////////////////////////////////////////////////////////

    void OnShowWindow(HWND hwnd, BOOL fShow, UINT status)
    {
        // first time being shown?
        if(window_show_count == 0) {

            // init max zoom
            RECT wr;
            GetWindowRect(hwnd, &wr);
            set_max_zoom(&wr);

            // and pop the settings app to the explorer page if running elevated
            if(app::is_elevated) {
                PostMessage(hwnd, WM_COMMAND, ID_FILE_SETTINGS_EXPLORER, (LPARAM)hwnd);
            }
        }
        window_show_count += 1;
    }

    //////////////////////////////////////////////////////////////////////

    void OnSysCommand(HWND hwnd, UINT cmd, int x, int y)
    {
        if(cmd != SC_KEYMENU || y > 0) {
            FORWARD_WM_SYSCOMMAND(hwnd, cmd, x, y, DefWindowProcA);
        }
    }

    //////////////////////////////////////////////////////////////////////

    LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
#if defined(_DEBUG) && 0
        switch(message) {
        case WM_INPUT:
        // case WM_SETCURSOR:
        // case WM_NCHITTEST:
        case WM_NCMOUSEMOVE:
        case WM_MOUSEMOVE:
        case WM_ENTERIDLE:
            break;
        default:
            LOG_DEBUG(L"({:04x}) {} {:08x} {:08x}", message, get_wm_name(message), wParam, lParam);
            break;
        }
#endif

        switch(message) {

            //////////////////////////////////////////////////////////////////////

            HANDLE_MSG(hwnd, WM_GETMINMAXINFO, OnGetMinMaxInfo);
            HANDLE_MSG(hwnd, WM_NCCREATE, OnNCCreate);
            HANDLE_MSG(hwnd, WM_CLOSE, OnClose);
            HANDLE_MSG(hwnd, WM_SHOWWINDOW, OnShowWindow);
            HANDLE_MSG(hwnd, WM_NCCALCSIZE, OnNCCalcSize);
            HANDLE_MSG(hwnd, WM_ERASEBKGND, OnEraseBkgnd);
            HANDLE_MSG(hwnd, WM_PAINT, OnPaint);
            HANDLE_MSG(hwnd, WM_COPYDATA, OnCopyData);
            HANDLE_MSG(hwnd, WM_SETCURSOR, OnSetCursor);
            HANDLE_MSG(hwnd, WM_SIZE, OnSize);
            HANDLE_MSG(hwnd, WM_WINDOWPOSCHANGING, OnWindowPosChanging);
            HANDLE_MSG(hwnd, WM_LBUTTONDOWN, OnLButtonDown);
            HANDLE_MSG(hwnd, WM_RBUTTONDOWN, OnRButtonDown);
            HANDLE_MSG(hwnd, WM_MBUTTONDOWN, OnMButtonDown);
            HANDLE_MSG(hwnd, WM_LBUTTONUP, OnLButtonUp);
            HANDLE_MSG(hwnd, WM_RBUTTONUP, OnRButtonUp);
            HANDLE_MSG(hwnd, WM_MBUTTONUP, OnMButtonUp);
            HANDLE_MSG(hwnd, WM_MOUSEMOVE, OnMouseMove);
            HANDLE_MSG(hwnd, WM_MOUSEWHEEL, OnMouseWheel);
            HANDLE_MSG(hwnd, WM_KEYDOWN, OnKeyDown);
            HANDLE_MSG(hwnd, WM_KEYUP, OnKeyUp);
            HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
            HANDLE_MSG(hwnd, WM_ACTIVATEAPP, OnActivateApp);
            HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
            HANDLE_MSG(hwnd, WM_MENUCHAR, OnMenuChar);
            HANDLE_MSG(hwnd, WM_DPICHANGED, OnDpiChanged);
            HANDLE_MSG(hwnd, WM_INPUT, OnInput);
            HANDLE_MSG(hwnd, WM_ENTERSIZEMOVE, OnEnterSizeMove);
            HANDLE_MSG(hwnd, WM_EXITSIZEMOVE, OnExitSizeMove);
            HANDLE_MSG(hwnd, WM_POWERBROADCAST, OnPowerBroadcast);
            HANDLE_MSG(hwnd, WM_SYSCOMMAND, OnSysCommand);

            //////////////////////////////////////////////////////////////////////

        case app::WM_FILE_LOAD_COMPLETE:
            on_file_load_complete(reinterpret_cast<image::image_file *>(lParam));
            break;

            //////////////////////////////////////////////////////////////////////

        case app::WM_FOLDER_SCAN_COMPLETE:
            on_folder_scanned(reinterpret_cast<file::folder_scan_result *>(lParam));
            break;

            //////////////////////////////////////////////////////////////////////

        case app::WM_NEW_SETTINGS: {
            settings_t *new_settings = reinterpret_cast<settings_t *>(lParam);
            settings = *new_settings;
            delete new_settings;
            on_new_settings();
        } break;

        case app::WM_RELAUNCH_AS_ADMIN: {
            relaunch_as_admin = true;
            DestroyWindow(window);
        } break;

            //////////////////////////////////////////////////////////////////////

        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // actually show an image

    HRESULT show_image(image::image_file *f)
    {
        rotation = rotate_0;

        current_file = f;

        FILETIME now;
        GetSystemTimeAsFileTime(&now);

        std::wstring name;

        // hresult from load_file
        HRESULT hr = f->hresult;

        ComPtr<ID3D11Texture2D> new_texture;
        ComPtr<ID3D11ShaderResourceView> new_srv;

        if(d3d_device.Get() == null) {
            CHK_HR(create_device());
        }

        // or hresult from create_texture
        if(SUCCEEDED(hr)) {
            hr = create_texture(d3d_device.Get(), d3d_context.Get(), &new_texture, &new_srv, f->img);
        }

        // set texture as current
        if(SUCCEEDED(hr)) {

            if(!f->is_clipboard) {
                settings.last_file_loaded = f->filename;

                // this will fail with E_ACCESSDENIED if it's on a read-only volume, that's... ok I think?
                hr = file::set_access_time(f->filename, now);
                if(FAILED(hr) && hr != E_ACCESSDENIED) {
                    LOG_ERROR(L"Error setting file last access time: {}", windows_error_message(hr));
                    return hr;
                }
            }

            image_texture.Attach(new_texture.Detach());
            image_texture_view.Attach(new_srv.Detach());

            D3D_SET_NAME(image_texture);
            D3D_SET_NAME(image_texture_view);

            D3D11_TEXTURE2D_DESC image_texture_desc;
            image_texture->GetDesc(&image_texture_desc);

            actual_texture_width = image_texture_desc.Width;
            actual_texture_height = image_texture_desc.Height;

            texture_width = actual_texture_width;
            texture_height = actual_texture_height;

            reset_transform();

            m_timer.reset();

            setup_window_text();

            int fade_time;
            switch(settings.show_filename) {
            case show_filename_always:
                fade_time = 0;
                break;
            case show_filename_briefly:
                fade_time = 5;
                break;
            default:
            case show_filename_never:
                fade_time = -1;
                break;
            }

            if(fade_time >= 0) {
                std::wstring msg{ std::format(L"{} {}x{}", f->filename, texture_width, texture_height) };
                set_message(msg, fade_time);
            }

        } else {

            std::wstring err_str;

            // "Component not found" isn't meaningful for unknown file type, override it
            if(hr == WINCODEC_ERR_COMPONENTNOTFOUND) {
                err_str = localize(IDS_UNKNOWN_FILE_TYPE);
            } else {
                err_str = windows_error_message(hr);
            }

            CHK_HR(file::get_filename(f->filename, name));
            set_message(std::format(L"{} {} - {}", localize(IDS_CANT_LOAD_FILE), name, err_str), 3);
        }
        return hr;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT main()
    {
        stopwatch main_stopwatch(L"main");

        app::instance = GetModuleHandle(null);

        // check for required CPU support (for DirectXMath SIMD)

        if(!XMVerifyCPUSupport()) {
            message_box(null, localize(IDS_OldCpu).c_str(), MB_ICONEXCLAMATION);
            return 0;
        }

        main_stopwatch.report(L"initial");

        recent_files::init();

        main_stopwatch.report(L"recent_files::init");

        // com

        CHK_HR(CoInitializeEx(null, COINIT_APARTMENTTHREADED));

        CHK_HR(imageview::get_is_process_elevated(app::is_elevated));

        // How much does this impact startup time?
        image::init_filetypes();

        main_stopwatch.report(L"init_filetypes");

        // in debug builds, hold middle mouse button at startup to reset settings to defaults
        {
            HRESULT hr = S_OK;
#if defined(_DEBUG)
            if((GetAsyncKeyState(VK_MBUTTON) & 0x8000) == 0)
#endif
                hr = settings.load();

            if(FAILED(hr)) {
                message_box(null,
                            std::format(L"{}\r\n{}", localize(IDS_FAILED_TO_LOAD_SETTINGS), windows_error_message(hr)),
                            MB_ICONEXCLAMATION);
                settings = default_settings;
            }
        }

        std::wstring cmd_line{ GetCommandLineW() };

        main_stopwatch.report(L"GetCommandLine");

        // if single window mode

        if(settings.reuse_window) {

            // and it's already running

            HWND existing_window = FindWindowW(window_class, null);
            if(existing_window != null) {

                // send the existing instance the command line (which might be
                // a filename to load)

                if(!cmd_line.empty()) {
                    COPYDATASTRUCT c;
                    c.cbData = static_cast<DWORD>(cmd_line.size() * sizeof(wchar));
                    c.lpData = reinterpret_cast<void *>(cmd_line.data());
                    c.dwData = static_cast<DWORD>(copydata_t::commandline);
                    SendMessageW(existing_window, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&c));
                }

                // Some confusion about whether this is legit but
                // BringWindowToFront doesn't work for top level windows.

                // The target instance also calls this, maybe one of them will work?

                SwitchToThisWindow(existing_window, TRUE);

                return S_OK;
            }
        }

        main_stopwatch.report(L"reuse_window");

        // report system memory for log

        uint64 system_memory_size_kb{ 0 };

        CHK_BOOL(GetPhysicallyInstalledSystemMemory(&system_memory_size_kb));

        app::system_memory_gb = system_memory_size_kb / 1048576;

        LOG_INFO(L"System has {}GB of memory", app::system_memory_gb);

        main_stopwatch.report(L"get system memory");

        // load/create/init some things

        window_created_event = CreateEvent(null, true, false, null);

        current_mouse_cursor = LoadCursor(null, IDC_ARROW);

        CHK_NULL(quit_event = CreateEvent(null, true, false, null));

        CHK_HR(thread_pool.init());

        CHK_HR(thread_pool.create_thread_with_message_pump(&scanner_thread_id, []() { scanner_function(); }));

        CHK_HR(thread_pool.create_thread_with_message_pump(&file_loader_thread_id, []() { file_loader_function(); }));

        main_stopwatch.report(L"create some threads");

        // tee up a loadimage if specified on the command line

        CHK_HR(on_command_line(cmd_line));

        main_stopwatch.report(L"on_command_line");

        // right, register window class

        HICON icon = LoadIconW(app::instance, MAKEINTRESOURCEW(IDI_ICON_DEFAULT));
        HCURSOR cursor = LoadCursorW(null, IDC_ARROW);

        WNDCLASSEXW wcex;
        mem_clear(&wcex);
        wcex.cbSize = sizeof(WNDCLASSEXA);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.hInstance = app::instance;
        wcex.hIcon = icon;
        wcex.hCursor = cursor;
        wcex.lpszClassName = window_class;
        wcex.hIconSm = icon;
        wcex.hbrBackground = null;

        CHK_BOOL(RegisterClassExW(&wcex));

        // get window position from settings

        DWORD window_style;
        DWORD window_ex_style;
        RECT rc;
        CHK_HR(get_startup_rect_and_style(&rc, &window_style, &window_ex_style));

        // create the window

        HWND hwnd;
        CHK_NULL(hwnd = CreateWindowExW(window_ex_style,
                                        window_class,
                                        localize(IDS_AppName).c_str(),
                                        window_style,
                                        rc.left,
                                        rc.top,
                                        rect_width(rc),
                                        rect_height(rc),
                                        null,
                                        null,
                                        app::instance,
                                        null));

        CHK_HR(hotkeys::load());

        if(requested_file == null) {

            if(settings.auto_paste && IsClipboardFormatAvailable(CF_DIBV5)) {

                on_paste();

            } else if(settings.reload_last_file && !settings.last_file_loaded.empty()) {

                load_image(settings.last_file_loaded);
            }
        }

        MSG msg;
        msg.message = WM_NULL;

        while(msg.message != WM_QUIT) {

            if(PeekMessageW(&msg, null, 0, 0, PM_REMOVE)) {

                if(msg.hwnd != window || !TranslateAcceleratorW(window, hotkeys::accelerators, &msg)) {

                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }

            } else {

                HRESULT hr = update();
                if(FAILED(hr)) {

                    error_message_box(localize(IDS_FATAL_ERROR), hr);
                    break;
                }
            }
        }

        // window has been destroyed, save settings and clean up

        if(!settings_purged) {

            CHK_HR(settings.save());
        }

        SetEvent(quit_event);

        recent_files::wait_for_recent_files();

        thread_pool.cleanup();

        CloseHandle(quit_event);
        CloseHandle(window_created_event);

        CoUninitialize();

        // if relaunching as admin, do it last thing

        if(relaunch_as_admin) {
            std::wstring args;
            int argc;
            LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
            for(int i = 1; i < argc; ++i) {
                args = args + argv[i];
            }
            ShellExecuteW(null, L"runas", argv[0], args.c_str(), 0, SW_SHOWNORMAL);
        }
        return S_OK;
    }
}

//////////////////////////////////////////////////////////////////////

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    main();
    return 0;
}
