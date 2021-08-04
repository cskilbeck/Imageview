#pragma once

#include "timer.h"

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

    // what should reset_zoom do
    enum class reset_zoom_mode
    {
        one_to_one,
        fit_to_window,
        shrink_to_fit
    };

    enum class fullscreen_startup_option
    {
        start_windowed,      // start up windowed
        start_fullscreen,    // start up fullscreen
        start_remember       // start up in whatever mode (fullscreen or windowed) it was in last time the app was exited
    };

    // whether to remember the window position or not
    enum class window_position_option
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

    // types of WM_COPYDATA messages that can be sent
    enum class copydata_t : DWORD
    {
        commandline = 1
    };

    // settings get serialized/deserialized to/from the registry
    struct settings_t
    {
        // use a header so we can implement the serializer more easily
#define DECL_SETTING(type, name, ...) type name{ __VA_ARGS__ };
#include "settings.h"

        HRESULT save();
        HRESULT load();

        // where in the registry to put the settings. this does not need to be localized... right?
        static wchar_t constexpr *key_name{ L"Software\\ImageView" };

        enum class serialize_action
        {
            save,
            load
        };

        HRESULT serialize(serialize_action action, wchar_t const *save_key_name);
    };

    App() = default;

    ~App() = default;

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
    HRESULT load_image(wchar_t const *filepath);

    // if file was loaded, try to decode it
    void check_image_loader();

    // actual image decoder uses WIC
    HRESULT initialize_image_from_buffer(std::vector<byte> const &buffer);

    // get some defaults for window creation
    HRESULT App::get_startup_rect_and_style(rect *r, DWORD *style, DWORD *ex_style);

    // setup after window has been created
    HRESULT set_window(HWND window);

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
    
    HRESULT on_closing();

    // copy current selection to CF_DIBV5 clipboard
    HRESULT on_copy();

    // paste current clipboard (CF_DIBV5 or CFSTR_FILENAME) into texture
    HRESULT on_paste();

    // toggle windowed or fake fullscreen on current monitor
    void toggle_fullscreen();

    // handle this command line either because it's the command line or another instance
    // of the application was run and it's in single window mode
    HRESULT on_command_line(wchar_t const *cmd_line);

    // some thing(s) was/were dropped onto the window, try to load the first one
    HRESULT on_drop_shell_item(IShellItemArray *psia, DWORD grfKeyState) override;
    HRESULT on_drop_string(wchar_t const *str) override;

    settings_t settings;
    settings_t default_settings;

    // IUnknown for the DragDropHelper stuff
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();

private:
    // file loading admin - file loader can be cancelled so a new request
    // can interrupt an existing one
    std::thread loader_thread;
    std::thread scanner_thread;
    HANDLE cancel_loader_event;
    HANDLE loader_complete_event;
    HANDLE scanner_complete_event;
    LONG thread_count;

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
    point_f screen_to_texture_pos(point_f pos);
    point_f screen_to_texture_pos(point_s pos);
    point_f texel_to_screen_pos(point_f pos);

    point_f clamp_to_texture(point_f pos);

    // draw a string with a border and background fill
    HRESULT draw_string(std::wstring const &text, IDWriteTextFormat *format, point_f pos, point_f pivot, float opacity = 1.0f, float corner_radius = 4.0f, float padding = 4.0f);
    HRESULT measure_string(std::wstring const &text, IDWriteTextFormat *format, float padding, point_f &size);

    point_f small_label_size{ 0, 0 };
    float small_label_padding{ 2.0f };

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

    // filename requested to load
    std::wstring filename;

    // bytes loaded from the file
    std::vector<byte> file_load_buffer;

    // how the file load went
    HRESULT file_load_hresult{ S_OK };

    // have we attempted to decode the loaded file?
    bool image_decode_complete{ false };

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

    // selection admin
    bool selecting{ false };
    bool selection_active{ false };

    // the point they first clicked
    point_f select_anchor;

    // the point where the mouse is now (could be above, below, left, right of anchor)
    point_f select_current;

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
    double file_load_timestamp{ 0 };

    // which frame rendering
    int m_frame{ 0 };
};
