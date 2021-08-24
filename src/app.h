#pragma once

struct App : public CDragDropHelper
{
    // mouse buttons
    enum
    {
        btn_left = 0,
        btn_middle = 1,
        btn_right = 2,
        btn_count = 3
    };

    // mouse button states
    enum
    {
        btn_down = 0,
        btn_up = 1
    };

    // types of WM_COPYDATA messages that can be sent
    enum class copydata_t : DWORD
    {
        commandline = 1
    };

    // a file that has maybe been loaded, successfully or not
    struct file_loader
    {
        std::wstring filename;                                                 // file path, relative to scanned folder - use this as key for map
        std::vector<byte> bytes;                                               // raw pixels, once it has been loaded - duplicate of texture, so...?
        HRESULT hresult{ HRESULT_FROM_WIN32(ERROR_OPERATION_IN_PROGRESS) };    // error code or S_OK from load_file()
        int index{ -1 };                                                       // position in the list of files
        int view_count{ 0 };                                                   // how many times this has been viewed since being loaded
    };

    App() = default;

    ~App();

    // WM_USER messages for main window
    enum
    {
        WM_FILE_LOAD_COMPLETE = WM_USER,
        WM_FOLDER_SCAN_COMPLETE = WM_USER + 1,
        WM_FILE_ALREADY_LOADED = WM_USER + 2
    };

    // WM_USER messages for scanner thread
    enum
    {
        WM_SCAN_FOLDER = WM_USER,
        WM_EXIT_THREAD = WM_USER + 1
    };

    // singletonish, but not checked
    App(App &&) = delete;
    App &operator=(App &&) = delete;
    App(App const &) = delete;
    App &operator=(App const &) = delete;

    // reset to blank state
    HRESULT init();

    // per-frame update
    HRESULT update();

    // request an image file to be loaded
    HRESULT load_image(wchar const *filepath);

    HRESULT App::show_image(std::vector<byte> const &data, wchar const *filename);

    void load_file(file_loader *f);

    // if file was loaded, try to decode it
    HRESULT decode_image(file_loader *f);

    // new file loaded
    void on_file_load_complete(LPARAM lparam);

    // file found in cache, display it
    void on_file_already_loaded(LPARAM lparam);

    // actual image decoder uses WIC
    HRESULT initialize_image_from_buffer(std::vector<byte> const &buffer);

    // get some defaults for window creation
    HRESULT App::get_startup_rect_and_style(rect *r, DWORD *style, DWORD *ex_style);

    // setup after window has been created
    HRESULT set_window(HWND window);

    void set_windowplacement();

    // window handlers

    HRESULT on_window_size_changed(int width, int height);
    HRESULT on_window_pos_changing(WINDOWPOS *new_pos);
    HRESULT on_dpi_changed(UINT new_dpi, rect *new_rect);

    void on_activated();
    void on_deactivated();
    void on_suspending();
    void on_resuming();
    void on_process_exit();
    void on_mouse_move(point_s pos);
    void on_raw_mouse_move(point_s pos);
    void on_mouse_button(point_s pos, int button, int state);
    void on_mouse_wheel(point_s pos, int delta);
    void on_key_down(int vk_key, LPARAM flags);
    void on_key_up(int vk_key);
    bool on_setcursor();

    void on_folder_scanned(folder_scan_result *scan_result);

    HRESULT on_closing();

    // copy current selection to CF_DIBV5 clipboard
    HRESULT on_copy();

    // paste current clipboard (CF_DIBV5 or CFSTR_FILENAME) into texture
    HRESULT on_paste();

    // toggle windowed or fake fullscreen on current monitor
    void toggle_fullscreen();

    // handle this command line either because it's the command line or another instance
    // of the application was run and it's in single window mode (settings.reuse_window == true)
    HRESULT on_command_line(wchar *cmd_line);

    // if settings.reuse_window and an existing window is found, send it the command line
    // with WM_COPYDATA and return HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)
    // else return S_OK
    HRESULT reuse_window(wchar *cmd_line);

    // some thing(s) was/were dropped onto the window, try to load the first one
    HRESULT on_drop_shell_item(IShellItemArray *psia, DWORD grfKeyState) override;
    HRESULT on_drop_string(wchar const *str) override;

    // IUnknown for the DragDropHelper stuff
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();

    // public so DEFINE_ENUM_OPERATORS can see it
    enum selection_hover_t : uint
    {
        sel_hover_inside = 0,
        sel_hover_left = 1,
        sel_hover_right = 2,
        sel_hover_top = 4,
        sel_hover_bottom = 8,
        sel_hover_outside = 0x80000000
    };

    static LPCWSTR window_class;

private:
    //////////////////////////////////////////////////////////////////////
    //
    // ENUMS which need localized names
    //
    // Each enum needs a header name (the name of the enum)
    // and a localized string for each one
    // They must be contiguous, ascending, zero-based
    //
    // But... how to know the enum from the settings field? not clear...
    //////////////////////////////////////////////////////////////////////

    // what should reset_zoom do
    enum class reset_zoom_mode : uint
    {
        one_to_one,
        fit_to_window,
        shrink_to_fit
    };

    enum class fullscreen_startup_option : uint
    {
        start_windowed,      // start up windowed
        start_fullscreen,    // start up fullscreen
        start_remember       // start up in whatever mode (fullscreen or windowed) it was in last time the app was exited
    };

    // whether to remember the window position or not
    enum class window_position_option : uint
    {
        window_pos_remember,    // restore last window position
        window_pos_default      // reset window position to default each time
    };

    // how to show the filename overlay
    enum class show_filename_option : uint
    {
        always,
        briefly,
        never
    };

    // what to do about exif rotation/flip data
    enum class exif_option : uint
    {
        ignore,    // always ignore it
        apply,     // always apply it
        prompt     // prompt if it's anything other than default 0 rotation
    };

    // these are things that can be activated with a key
    enum class action : uint
    {
        zoom_to_selection = 0,
        shrink_to_fit,
        fit_to_window,
        toggle_fullscreen,
        quit,
        copy,
        paste,
        open_file,
        options
    };

    //////////////////////////////////////////////////////////////////////
    //
    // END of enums which need localized names
    //
    //////////////////////////////////////////////////////////////////////

    // settings get serialized/deserialized to/from the registry
    struct settings_t
    {
        // use a header so we can implement the serializer more easily
#define DECL_SETTING(type, name, ...) type name{ __VA_ARGS__ };
#include "settings.h"

        HRESULT save();
        HRESULT load();

        // where in the registry to put the settings. this does not need to be localized... right?
        static wchar constexpr *key_name{ L"Software\\ImageView" };

        enum class serialize_action
        {
            save,
            load
        };

        HRESULT serialize(serialize_action action, wchar const *save_key_name);
    };

    static HRESULT serialize_setting(settings_t::serialize_action action, wchar const *key_name, wchar const *name, byte *var, DWORD size);

    settings_t settings;
    settings_t default_settings;

    // most recent filename requested to view
    std::wstring filename;

    // folder containing most recently loaded file (so we know if a folder scan is in the same folder as current file)
    std::wstring current_folder;

    folder_scan_result *current_folder_scan{ null };
    int current_file_cursor{ 0 };

    void move_file_cursor(int movement);

    // how many files loaded in this run
    // which is used to decide whether to MessageBox and DestroyWindow when load_file() fails
    // if files_loaded == 0 and load fails, show messagebox and bomb
    // else just show error as text in the window
    // increment files_loaded when a file load succeeds
    // note that this does not account for failing to decode the image....
    int files_loaded{ 0 };

    // thread admin
    file_loader *requested_file{ null };

    // persistent folder scanner thread
    std::thread scanner_thread;
    HANDLE scanner_thread_handle{ null };
    DWORD scanner_thread_id{ 0 };
    std::atomic_bool scanner_thread_running{ false };

    void scanner_function();
    HRESULT do_folder_scan(wchar const *path);

    HRESULT update_file_index(file_loader *f);

    // set this to cancel all async file loading activity
    HANDLE cancel_loader_event;

    // set this to signal that the application is exiting
    HANDLE quit_event;

    // # of file loader threads in flight, set cancel event then wait for this to fall to 0
    LONG thread_count;

    // files which have been loaded
    static std::unordered_map<std::wstring, file_loader *> loaded_files;

    // files which have been requested to load
    static std::unordered_map<std::wstring, file_loader *> loading_files;

    file_loader *current_image_file{ null };

    void cancel_loader();

    // render one frame
    HRESULT render();

    // clear the backbufer
    void clear();

    // present the backbuffer
    HRESULT present();

    // create all d3d, d2d, dwrite factories and devices
    HRESULT create_device();

    // create resources (eg after window resize)
    HRESULT create_resources();

    HRESULT create_text_formats();

    // device lost (eg sleep/resume, another app goes fullscreen)
    HRESULT on_device_lost();

    // copy the current selection into a CF_DIBV5
    HRESULT copy_selection();

    // send the shader constants to the GPU
    HRESULT update_constants();

    void set_cursor(HCURSOR c);

    // mouse button admin
    void set_grab(int button);
    void clear_grab(int button);
    bool get_grab(int button);

    // zoom admin
    void reset_zoom(reset_zoom_mode mode);
    void do_zoom(point_s pos, int delta);
    void zoom_to_selection();
    void center_in_window();

    // set selection to whole image
    void select_all();

    // converting to/from screen/texel coords
    vec2 screen_to_texture_pos(vec2 pos);
    vec2 screen_to_texture_pos(point_s pos);
    vec2 texture_to_screen_pos(vec2 pos);

    vec2 texels_to_pixels(vec2 pos);
    vec2 pixels_to_texels(vec2 pos);

    vec2 texel_size() const;
    vec2 texture_size() const;
    vec2 window_size() const;

    vec2 clamp_to_texture(vec2 pos);

    HANDLE window_created_event{ null };

    // draw a string with a border and background fill
    HRESULT draw_string(std::wstring const &text, IDWriteTextFormat *format, vec2 pos, vec2 pivot, float opacity = 1.0f, float corner_radius = 4.0f, float padding = 4.0f);
    HRESULT measure_string(std::wstring const &text, IDWriteTextFormat *format, float padding, vec2 &size);

    // admin for showing a message
    std::wstring current_message;
    double message_timestamp{ 0 };
    double message_fade_time{ 0 };

    void set_message(wchar const *message, double fade_time);

    vec2 small_label_size{ 0, 0 };
    float small_label_padding{ 2.0f };

    uint64 system_memory_size_kb{ 0 };

    uint64 cache_in_use{ 0 };

    // for IUnknown
    long refcount;

    // the window handle
    HWND window{ null };

    // cached window size
    int window_width{ 1280 };
    int window_height{ 720 };
    int old_window_width{ 1280 };
    int old_window_height{ 720 };

    // when we're calling SetWindowPos, suppress DPI change handling because it causes a weird problem
    // this isn't a great solution but it does get by. Nobody seems to know the right way to handle that
    bool ignore_dpi_for_a_moment{ false };

    // current dpi for the window, updated by WM_DPICHANGED
    float dpi;

    // scale a number by the current dpi
    template <typename T> T dpi_scale(T x)
    {
        return (T)((x * dpi) / 96.0f);
    }

    // and vice versa
    template <typename T> T dpi_unscale(T x)
    {
        return (T)((x * 96.0f) / dpi);
    }

    // shader constants header is shared with the HLSL files
#pragma pack(push, 4)
    struct shader_const_t
#include "shader_constants.h"
#pragma pack(pop)
        ;

    shader_const_t shader_constants;

    // all the com pointers

    static ComPtr<ID3D11Debug> d3d_debug;

    ComPtr<ID3D11Device1> d3d_device;
    ComPtr<ID3D11DeviceContext1> d3d_context;

    ComPtr<IDXGISwapChain1> swap_chain;
    ComPtr<ID3D11RenderTargetView> rendertarget_view;

    ComPtr<ID3D11PixelShader> pixel_shader;
    ComPtr<ID3D11PixelShader> grid_shader;
    ComPtr<ID3D11PixelShader> rect_shader;
    ComPtr<ID3D11PixelShader> solid_shader;
    ComPtr<ID3D11PixelShader> spinner_shader;
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

    ComPtr<IDWriteTextFormat> large_text_format;
    ComPtr<IDWriteTextFormat> small_text_format;

    ComPtr<IDWriteFontCollection> font_collection;

    // see font_loader.h
    ResourceFontContext font_context;

    // mouse admin
    int mouse_grab{ 0 };
    point_s mouse_pos[btn_count] = {};
    point_s mouse_offset[btn_count] = {};
    point_s mouse_click[btn_count] = {};
    point_s cur_mouse_pos;
    point_s shift_mouse_pos;
    point_s ctrl_mouse_pos;

    enum class shift_snap_axis_t
    {
        none,
        x,
        y
    };

    shift_snap_axis_t shift_snap_axis{ shift_snap_axis_t::none };
    bool shift_snap{ false };
    bool ctrl_snap{ false };

    float shift_snap_radius{ 8 };

    void check_selection_hover(vec2 pos);

    enum class cursor_type
    {
        system,
        user
    };

    struct cursor_def
    {
        cursor_type type;
        short id;

        cursor_def(cursor_type _type, wchar const *_id) : type(_type), id((short)((intptr_t)_id & 0xffff))
        {
        }
    };

    // selection_hover_t bitmask maps into these cursors
    static cursor_def sel_hover_cursors[16];

    // selection admin
    bool selecting{ false };              // dragging new selection rectangle
    bool selection_active{ false };       // a selection rectangle exists
    bool drag_selection{ false };         // dragging the selection rectangle
    selection_hover_t selection_hover;    // which part of the selection rectangle being dragged (all, corner, edge)
    vec2 drag_select_pos{ 0, 0 };         // where they originally grabbed the selection rectangle in texels
    vec2 selection_size{ 0, 0 };          // size of selection rectangle in texels

    HCURSOR current_mouse_cursor;

    // the point they first clicked
    vec2 select_anchor;

    // the point where the mouse is now (could be above, below, left, right of anchor)
    vec2 select_current;

    // sticky zoom mode on window resize
    bool has_been_zoomed_or_dragged{ false };
    reset_zoom_mode last_zoom_mode{ reset_zoom_mode::shrink_to_fit };

    // texture drawn in this rectangle
    rect_f current_rect;

    // texture rectangle target for animated zoom
    rect_f target_rect;

    // texture dimensions
    int texture_width{ 0 };
    int texture_height{ 0 };

    // zoom limits in pixels
    float min_zoom{ 8 };
    float max_zoom{ 512 };

    // frame/wall timer for animation
    timer_t m_timer;

    // when was the selection copied for animating flash
    double copy_timestamp{ 0 };

    // which frame rendering
    int m_frame{ 0 };
};

DEFINE_ENUM_FLAG_OPERATORS(App::selection_hover_t);
