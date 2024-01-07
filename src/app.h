//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

namespace imageview
{
    enum rotation_angle_t : uint
    {
        rotate_0 = 0,
        rotate_90 = 1,
        rotate_180 = 2,
        rotate_270 = 3,
        rotate_max = 4
    };

    enum flip_type_t : uint
    {
        flip_horizontal = 0,
        flip_vertical = 1
    };
}

namespace imageview::app
{
    //////////////////////////////////////////////////////////////////////
    // the application window - use with care

    extern HWND window;

    //////////////////////////////////////////////////////////////////////
    // is it running with elevated privileges

    extern bool is_elevated;

    //////////////////////////////////////////////////////////////////////
    // how much ram

    extern uint64 system_memory_gb;

    //////////////////////////////////////////////////////////////////////
    // the current application instance

    extern HMODULE instance;

    //////////////////////////////////////////////////////////////////////
    // WM_USER messages for main window

    enum user_message_t : uint
    {
        WM_FILE_LOAD_COMPLETE = WM_USER,          // a file load completed (lparam -> file_loader *)
        WM_FOLDER_SCAN_COMPLETE = WM_USER + 1,    // a folder scan completed (lparam -> folder_scan_results *)
        WM_NEW_SETTINGS = WM_USER + 2,            // here (lparam is a copy of dialog settings) are some new settings
        WM_RELAUNCH_AS_ADMIN = WM_USER + 3,       // please relaunch the application with admin privileges
    };

    //////////////////////////////////////////////////////////////////////
    // return this from settings dialog to relaunch as admin

    static constexpr LRESULT LRESULT_LAUNCH_AS_ADMIN = 0x29034893;
};
