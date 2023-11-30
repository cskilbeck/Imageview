//////////////////////////////////////////////////////////////////////

#include "pch.h"

LOG_CONTEXT("main");

//////////////////////////////////////////////////////////////////////

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

////////////////////////////////////////////////////////////////////////
// Indicates to hybrid graphics systems to prefer the discrete part by default
// disabled because we don't need to use the fancy GPU to draw an image

// extern "C" {
//__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
//__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
//}

//////////////////////////////////////////////////////////////////////

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
    std::string cmd_line{ GetCommandLineA() };

    HRESULT hr = imageview::app::init(cmd_line);

    if(FAILED(hr)) {
        imageview::display_error(std::format("Command line {}", cmd_line), hr);
        return 0;
    }

    HICON icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_DEFAULT));
    HCURSOR cursor = LoadCursor(null, IDC_ARROW);

    WNDCLASSEXA wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = icon;
    wcex.hCursor = cursor;
    wcex.lpszClassName = imageview::app::window_class;
    wcex.hIconSm = icon;

    CHK_BOOL(RegisterClassExA(&wcex));

    DWORD window_style;
    DWORD window_ex_style;
    imageview::rect rc;
    CHECK(imageview::app::get_startup_rect_and_style(&rc, &window_style, &window_ex_style));

    HWND hwnd;
    CHK_NULL(hwnd = CreateWindowExA(window_ex_style,
                                    imageview::app::window_class,
                                    imageview::localize(IDS_AppName).c_str(),
                                    window_style,
                                    rc.x(),
                                    rc.y(),
                                    rc.w(),
                                    rc.h(),
                                    null,
                                    null,
                                    hInstance,
                                    null));

    CHK_HR(imageview::app::load_accelerators());

    MSG msg{ 0 };

    do {
        if(PeekMessage(&msg, null, 0, 0, PM_REMOVE)) {
            if(!TranslateAccelerator(hwnd, imageview::app::accelerators, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            imageview::app::update();
        }
    } while(msg.message != WM_QUIT);

    imageview::app::on_process_exit();

    CoUninitialize();

    return 0;
}

//////////////////////////////////////////////////////////////////////

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool s_in_sizemove = false;
    static bool s_in_suspend = false;
    static bool s_minimized = false;

#if defined(_DEBUG)
    switch(message) {
    case WM_INPUT:
    case WM_SETCURSOR:
    case WM_NCMOUSEMOVE:
    case WM_MOUSEMOVE:
    case WM_NCHITTEST:
    case WM_ENTERIDLE:
        break;
    default:
        LOG_DEBUG("({:04x}) {} {:08x} {:08x}", message, imageview::get_wm_name(message), wParam, lParam);
        break;
    }
#endif

    switch(message) {

        //////////////////////////////////////////////////////////////////////
        // 1st message is always WM_GETMINMAXINFO

    case WM_GETMINMAXINFO:
        if(lParam != 0) {
            reinterpret_cast<MINMAXINFO *>(lParam)->ptMinTrackSize = { 320, 200 };
        }
        break;

        //////////////////////////////////////////////////////////////////////
        // 2nd message is always WM_NCCREATE

    case WM_NCCREATE: {
        LRESULT r = DefWindowProc(hWnd, message, wParam, lParam);
        imageview::app::on_post_create(hWnd);
        return r;
    } break;

        //////////////////////////////////////////////////////////////////////

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

        //////////////////////////////////////////////////////////////////////
        // resize backbuffer before window size actually changes to avoid
        // flickering at the borders when resizing

        // BUT minimize/maximize...

    case WM_NCCALCSIZE: {
        DefWindowProc(hWnd, message, wParam, lParam);
        if(IsWindowVisible(hWnd)) {
            NCCALCSIZE_PARAMS *params = reinterpret_cast<LPNCCALCSIZE_PARAMS>(lParam);
            imageview::rect const &new_client_rect = params->rgrc[0];
            imageview::app::on_window_size_changing(new_client_rect.w(), new_client_rect.h());
        }
        return 0;
    }

        //////////////////////////////////////////////////////////////////////

    case WM_ERASEBKGND:
        return 1;

        //////////////////////////////////////////////////////////////////////

    case WM_PAINT:
        PAINTSTRUCT ps;
        (void)BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        if(s_in_sizemove) {
            imageview::app::update();
        }
        break;

        //////////////////////////////////////////////////////////////////////
        // in single instance mode, we can get sent a new command line

    case WM_COPYDATA: {
        COPYDATASTRUCT *c = reinterpret_cast<COPYDATASTRUCT *>(lParam);
        if(c != null) {
            switch((imageview::app::copydata_t)c->dwData) {
            case imageview::app::copydata_t::commandline:
                if(s_minimized) {
                    ShowWindow(hWnd, SW_RESTORE);
                }
                SetForegroundWindow(hWnd);
                SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                imageview::app::on_command_line(reinterpret_cast<char const *>(c->lpData));
                break;
            default:
                break;
            }
        }
    } break;

        //////////////////////////////////////////////////////////////////////

    case WM_SHOWWINDOW:
        if(wParam) {
            SetCursor(LoadCursor(null, IDC_ARROW));
        }
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_SETCURSOR:
        if(LOWORD(lParam) != HTCLIENT || !imageview::app::on_setcursor()) {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_DPICHANGED:
        imageview::app::on_dpi_changed((UINT)wParam & 0xffff, reinterpret_cast<imageview::rect *>(lParam));
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_SIZE:
        if(wParam == SIZE_MINIMIZED) {
            if(!s_minimized) {
                s_minimized = true;
                if(!s_in_suspend) {
                    imageview::app::on_suspending();
                }
                s_in_suspend = true;
            }
        } else {
            if(s_minimized) {
                s_minimized = false;
                if(s_in_suspend) {
                    imageview::app::on_resuming();
                }
                s_in_suspend = false;
            }
            imageview::rect rc;
            GetClientRect(hWnd, &rc);
            imageview::app::on_window_size_changed(rc.w(), rc.h());
        }
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_WINDOWPOSCHANGING:
        imageview::app::on_window_pos_changing(reinterpret_cast<WINDOWPOS *>(lParam));
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_LBUTTONDOWN:
        imageview::app::on_mouse_button_down(MAKEPOINTS(lParam), imageview::app::btn_left);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_RBUTTONDOWN:
        imageview::app::on_mouse_button_down(MAKEPOINTS(lParam), imageview::app::btn_right);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_MBUTTONDOWN:
        imageview::app::on_mouse_button_down(MAKEPOINTS(lParam), imageview::app::btn_middle);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_LBUTTONUP:
        imageview::app::on_mouse_button_up(MAKEPOINTS(lParam), imageview::app::btn_left);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_RBUTTONUP:
        imageview::app::on_mouse_button_up(MAKEPOINTS(lParam), imageview::app::btn_right);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_MBUTTONUP:
        imageview::app::on_mouse_button_up(MAKEPOINTS(lParam), imageview::app::btn_middle);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_MOUSEMOVE:
        imageview::app::on_mouse_move(MAKEPOINTS(lParam));
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_MOUSEWHEEL: {
        POINT pos{ imageview::get_x(lParam), imageview::get_y(lParam) };
        ScreenToClient(hWnd, &pos);
        imageview::app::on_mouse_wheel(pos, imageview::get_y(wParam) / WHEEL_DELTA);
    } break;

        //////////////////////////////////////////////////////////////////////

    case WM_INPUT: {
        UINT dwSize = sizeof(RAWINPUT);
        static BYTE lpb[sizeof(RAWINPUT)];
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
        RAWINPUT *raw = (RAWINPUT *)lpb;
        if(raw->header.dwType == RIM_TYPEMOUSE) {
            imageview::app::on_raw_mouse_move({ (short)raw->data.mouse.lLastX, (short)raw->data.mouse.lLastY });
        }
    } break;

        //////////////////////////////////////////////////////////////////////

    case WM_COMMAND:
        imageview::app::on_command(LOWORD(wParam));
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_KEYDOWN:
        imageview::app::on_key_down((int)wParam, lParam);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_KEYUP:
        imageview::app::on_key_up((int)wParam);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_ENTERSIZEMOVE:
        s_in_sizemove = true;
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_EXITSIZEMOVE:
        s_in_sizemove = false;
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_ACTIVATEAPP:
        if(wParam) {
            imageview::app::on_activated();
        } else {
            imageview::app::on_deactivated();
        }
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_POWERBROADCAST:

        switch(wParam) {

        case PBT_APMQUERYSUSPEND:
            if(!s_in_suspend) {
                imageview::app::on_suspending();
            }
            s_in_suspend = true;
            return TRUE;

        case PBT_APMRESUMESUSPEND:
            if(!s_minimized) {
                if(s_in_suspend) {
                    imageview::app::on_resuming();
                }
                s_in_suspend = false;
            }
            return TRUE;
        }
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_DESTROY:
        imageview::app::on_closing();
        PostQuitMessage(0);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_SYSKEYDOWN: {
        uint flags = HIWORD(lParam);
        bool key_up = (flags & KF_UP) == KF_UP;                // transition-state flag, 1 on keyup
        bool repeat = (flags & KF_REPEAT) == KF_REPEAT;        // previous key-state flag, 1 on autorepeat
        bool alt_down = (flags & KF_ALTDOWN) == KF_ALTDOWN;    // ALT key was pressed

        if(!key_up && !repeat && alt_down) {
            switch(wParam) {
            case VK_RETURN:
                imageview::app::toggle_fullscreen();
                break;
            case VK_F4:
                DestroyWindow(hWnd);
            }
        }
    } break;

        //////////////////////////////////////////////////////////////////////

    case WM_MENUCHAR:
        return MAKELRESULT(0, MNC_CLOSE);

        //////////////////////////////////////////////////////////////////////

    case imageview::app::WM_FILE_LOAD_COMPLETE:
        imageview::app::on_file_load_complete(lParam);
        break;

        //////////////////////////////////////////////////////////////////////

    case imageview::app::WM_FOLDER_SCAN_COMPLETE:
        imageview::app::on_folder_scanned(reinterpret_cast<imageview::file::folder_scan_result *>(lParam));
        break;

        //////////////////////////////////////////////////////////////////////

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
