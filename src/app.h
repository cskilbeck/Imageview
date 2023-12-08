//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

namespace imageview::app
{
    //////////////////////////////////////////////////////////////////////
    // is it running with elevated privileges

    extern bool is_elevated;

    //////////////////////////////////////////////////////////////////////
    // how much ram

    extern uint64 system_memory_gb;

    //////////////////////////////////////////////////////////////////////
    // the current accelerator (hotkey) table

    extern HACCEL accelerators;

    //////////////////////////////////////////////////////////////////////
    // the current application instance

    extern HMODULE instance;

    //////////////////////////////////////////////////////////////////////
    // the application window - use with care

    extern HWND window;

    //////////////////////////////////////////////////////////////////////
    // WM_USER messages for main window

    enum user_message_t : uint
    {
        WM_FILE_LOAD_COMPLETE = WM_USER,          // a file load completed (lparam -> file_loader *)
        WM_FOLDER_SCAN_COMPLETE = WM_USER + 1,    // a folder scan completed (lparam -> folder_scan_results *)
        WM_NEW_SETTINGS = WM_USER + 2,            // here (lparam is a copy of dialog settings) are some new settings
        WM_RELAUNCH_AS_ADMIN,                     // please relaunch the application with admin privileges
    };

    //////////////////////////////////////////////////////////////////////
    // return this from settings dialog to relaunch as admin

    static constexpr LRESULT LRESULT_LAUNCH_AS_ADMIN = 0x29034893;
};
