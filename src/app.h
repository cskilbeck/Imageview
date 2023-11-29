//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

namespace app
{
    //////////////////////////////////////////////////////////////////////
    // mouse buttons

    enum
    {
        btn_left = 0,
        btn_middle = 1,
        btn_right = 2,
        btn_count = 3
    };

    //////////////////////////////////////////////////////////////////////
    // mouse button states

    enum
    {
        btn_down = 0,
        btn_up = 1
    };

    //////////////////////////////////////////////////////////////////////
    // types of WM_COPYDATA messages that can be sent

    enum class copydata_t : DWORD
    {
        commandline = 1
    };

    //////////////////////////////////////////////////////////////////////
    // WM_USER messages for main window

    enum
    {
        WM_FILE_LOAD_COMPLETE = WM_USER,         // a file load completed (lparam -> file_loader *)
        WM_FOLDER_SCAN_COMPLETE = WM_USER + 1    // a folder scan completed (lparam -> folder_scan_results *)
    };

    //////////////////////////////////////////////////////////////////////
    // WM_USER messages for scanner thread

    enum
    {
        WM_SCAN_FOLDER = WM_USER    // please scan a folder (lparam -> path)
    };

    //////////////////////////////////////////////////////////////////////
    // WM_USER messages for file_loader thread

    enum
    {
        WM_LOAD_FILE = WM_USER    // please load this file (lparam -> filepath)
    };

    //////////////////////////////////////////////////////////////////////
    // reset to blank state before anything happens, load settings
    // maybe return S_FALSE if existing instance was reused

    HRESULT init(std::string const &cmd_line);

    //////////////////////////////////////////////////////////////////////
    // get some defaults for window creation

    HRESULT get_startup_rect_and_style(rect *r, DWORD *style, DWORD *ex_style);

    //////////////////////////////////////////////////////////////////////
    // load the accelerator (aka hotkey) table

    HRESULT load_accelerators();

    //////////////////////////////////////////////////////////////////////
    // Call at end of WM_NCCREATE (after DefWindowProc)

    HRESULT on_post_create(HWND hwnd);

    //////////////////////////////////////////////////////////////////////
    // call this after CreateWindow

    void setup_initial_windowplacement();

    //////////////////////////////////////////////////////////////////////
    // per-frame update

    HRESULT update();

    //////////////////////////////////////////////////////////////////////
    // send WM_FILE_LOAD_COMPLETE to call this

    void on_file_load_complete(LPARAM lparam);

    //////////////////////////////////////////////////////////////////////
    // send WM_FOLDER_SCAN_COMPLETE to call this

    void on_folder_scanned(file::folder_scan_result *scan_result);

    //////////////////////////////////////////////////////////////////////
    // window handlers

    HRESULT on_window_size_changing(int width, int height);
    HRESULT on_window_size_changed(int width, int height);

    HRESULT on_window_pos_changed(WINDOWPOS *new_pos);
    HRESULT on_window_pos_changing(WINDOWPOS *new_pos);

    HRESULT on_dpi_changed(UINT new_dpi, rect *new_rect);

    void on_activated();
    void on_deactivated();
    void on_suspending();
    void on_resuming();
    void on_process_exit();
    void on_mouse_move(point_s pos);
    void on_raw_mouse_move(point_s pos);
    void on_mouse_button_down(point_s pos, uint button);
    void on_mouse_button_up(point_s pos, uint button);
    void on_mouse_wheel(point_s pos, int delta);
    void on_command(uint command);
    void on_key_down(int vk_key, LPARAM flags);
    void on_key_up(int vk_key);
    bool on_setcursor();

    //////////////////////////////////////////////////////////////////////
    // WM_DESTROY

    HRESULT on_closing();

    //////////////////////////////////////////////////////////////////////
    // copy current selection to clipboard

    HRESULT on_copy();

    //////////////////////////////////////////////////////////////////////
    // paste current clipboard into texture

    HRESULT on_paste();

    //////////////////////////////////////////////////////////////////////
    // toggle windowed or fake fullscreen on current monitor

    void toggle_fullscreen();

    //////////////////////////////////////////////////////////////////////
    // handle this command line either because it's the command line or another instance
    // of the application was run and it's in single window mode (settings.reuse_window == true)

    HRESULT on_command_line(std::string const &cmd_line);

    //////////////////////////////////////////////////////////////////////

    extern LPCSTR window_class;
    extern bool is_elevated;

    static constexpr LRESULT LRESULT_LAUNCH_AS_ADMIN = 0x29034893;

    //////////////////////////////////////////////////////////////////////
    // the current accelerator (hotkey) table

    extern HACCEL accelerators;
};
