//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

namespace
{
    // com_initializer goes _before_ the App declaration so CoInitialize is called before App ctor

    struct com_initializer
    {
        com_initializer()
        {
            if(FAILED(CoInitializeEx(null, COINIT_APARTMENTTHREADED))) {
                ExitProcess(1);
            }
        }

        ~com_initializer()
        {
            CoUninitialize();
        }
    } initializer;

}    // namespace

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

HRESULT check_heif_support(bool &heif_is_supported)
{
    CHK_HR(MFStartup(MF_VERSION));
    DEFER(MFShutdown());

    IMFActivate **activate{};
    uint32 count{};

    MFT_REGISTER_TYPE_INFO input;
    input.guidMajorType = MFMediaType_Video;
    input.guidSubtype = MFVideoFormat_HEVC;

    CHK_HR(MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, MFT_ENUM_FLAG_SYNCMFT, &input, null, &activate, &count));
    DEFER(CoTaskMemFree(activate));

    for(uint32 i = 0; i < count; i++) {
        activate[i]->Release();
    }

    heif_is_supported = count > 0;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
    wchar *cmd_line = GetCommandLineW();

    HRESULT hr = App::init(cmd_line);

    // quit if OpenFileDialog was shown and cancelled by the user
    // or existing instance was reused or cpu not supported
    if(hr == HRESULT_FROM_WIN32(ERROR_CANCELLED) || hr == S_FALSE) {
        return 0;
    }

    if(FAILED(hr)) {
        display_error(std::format(L"Command line {}", cmd_line).c_str(), hr);
        return 0;
    }

    HICON icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_DEFAULT));
    HCURSOR cursor = LoadCursor(null, IDC_ARROW);

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = icon;
    wcex.hCursor = cursor;
    wcex.lpszClassName = App::window_class;
    wcex.hIconSm = icon;

    CHK_BOOL(RegisterClassExW(&wcex));

    DWORD window_style;
    DWORD window_ex_style;
    rect rc;
    CHECK(App::get_startup_rect_and_style(&rc, &window_style, &window_ex_style));

    HWND hwnd;
    CHK_NULL(hwnd = CreateWindowExW(window_ex_style,
                                    App::window_class,
                                    localize(IDS_AppName),
                                    window_style,
                                    rc.x(),
                                    rc.y(),
                                    rc.w(),
                                    rc.h(),
                                    null,
                                    null,
                                    hInstance,
                                    null));

    App::setup_initial_windowplacement();

    RAWINPUTDEVICE Rid[1];
    Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
    Rid[0].dwFlags = RIDEV_INPUTSINK;
    Rid[0].hwndTarget = hwnd;
    RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));

    CHK_HR(App::load_accelerators());

    MSG msg;

    do {
        if(PeekMessage(&msg, null, 0, 0, PM_REMOVE)) {
            if(!TranslateAccelerator(hwnd, App::accelerators, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            App::update();
        }
    } while(msg.message != WM_QUIT);

    App::on_process_exit();

    CoUninitialize();

    return 0;
}

//////////////////////////////////////////////////////////////////////

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool s_in_sizemove = false;
    static bool s_in_suspend = false;
    static bool s_minimized = false;

    // Log(L"(%04x) %s %08x %08x, app = %p", message, get_wm_name(message), wParam, lParam, app);

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
        App::set_window(hWnd);
        LRESULT r = DefWindowProc(hWnd, message, wParam, lParam);
        App::on_post_create();
        return r;
    } break;

        //////////////////////////////////////////////////////////////////////
        // app pointer is valid for all other windows messages

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

        //////////////////////////////////////////////////////////////////////
        // resize backbuffer before window size actually changes to avoid
        // flickering at the borders when resizing

    case WM_NCCALCSIZE: {
        DefWindowProc(hWnd, message, wParam, lParam);
        if(IsWindowVisible(hWnd)) {
            NCCALCSIZE_PARAMS *params = reinterpret_cast<LPNCCALCSIZE_PARAMS>(lParam);
            rect const &new_client_rect = params->rgrc[0];
            App::on_window_size_changed(new_client_rect.w(), new_client_rect.h());
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
            App::update();
        }
        break;

        //////////////////////////////////////////////////////////////////////
        // in single instance mode, we can get sent a new command line

    case WM_COPYDATA: {
        COPYDATASTRUCT *c = reinterpret_cast<COPYDATASTRUCT *>(lParam);
        if(c != null) {
            switch((App::copydata_t)c->dwData) {
            case App::copydata_t::commandline:
                if(s_minimized) {
                    ShowWindow(hWnd, SW_RESTORE);
                }
                SetForegroundWindow(hWnd);
                SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                App::on_command_line(reinterpret_cast<wchar *>(c->lpData));
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
        if(LOWORD(lParam) != HTCLIENT || !App::on_setcursor()) {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_DPICHANGED:
        App::on_dpi_changed((UINT)wParam & 0xffff, reinterpret_cast<rect *>(lParam));
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_SIZE:
        if(wParam == SIZE_MINIMIZED) {
            if(!s_minimized) {
                s_minimized = true;
                if(!s_in_suspend) {
                    App::on_suspending();
                }
                s_in_suspend = true;
            }
        } else {
            if(s_minimized) {
                s_minimized = false;
                if(s_in_suspend) {
                    App::on_resuming();
                }
                s_in_suspend = false;
            }
        }
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_WINDOWPOSCHANGING:
        App::on_window_pos_changing(reinterpret_cast<WINDOWPOS *>(lParam));
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_LBUTTONDOWN:
        App::on_mouse_button_down(MAKEPOINTS(lParam), App::btn_left);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_RBUTTONDOWN:
        App::on_mouse_button_down(MAKEPOINTS(lParam), App::btn_right);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_MBUTTONDOWN:
        App::on_mouse_button_down(MAKEPOINTS(lParam), App::btn_middle);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_LBUTTONUP:
        App::on_mouse_button_up(MAKEPOINTS(lParam), App::btn_left);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_RBUTTONUP:
        App::on_mouse_button_up(MAKEPOINTS(lParam), App::btn_right);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_MBUTTONUP:
        App::on_mouse_button_up(MAKEPOINTS(lParam), App::btn_middle);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_MOUSEMOVE:
        App::on_mouse_move(MAKEPOINTS(lParam));
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_MOUSEWHEEL: {
        POINT pos{ get_x(lParam), get_y(lParam) };
        ScreenToClient(hWnd, &pos);
        App::on_mouse_wheel(pos, get_y(wParam) / WHEEL_DELTA);
    } break;

        //////////////////////////////////////////////////////////////////////

    case WM_INPUT: {
        UINT dwSize = sizeof(RAWINPUT);
        static BYTE lpb[sizeof(RAWINPUT)];
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
        RAWINPUT *raw = (RAWINPUT *)lpb;
        if(raw->header.dwType == RIM_TYPEMOUSE) {
            App::on_raw_mouse_move({ (short)raw->data.mouse.lLastX, (short)raw->data.mouse.lLastY });
        }
    } break;

        //////////////////////////////////////////////////////////////////////

    case WM_COMMAND:
        App::on_command(LOWORD(wParam));
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_KEYDOWN:
        App::on_key_down((int)wParam, lParam);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_KEYUP:
        App::on_key_up((int)wParam);
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_ENTERSIZEMOVE:
        s_in_sizemove = true;
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_EXITSIZEMOVE:
        s_in_sizemove = false;
        rect rc;
        GetClientRect(hWnd, &rc);
        App::on_window_size_changed(rc.w(), rc.h());
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_ACTIVATEAPP:
        if(wParam) {
            App::on_activated();
        } else {
            App::on_deactivated();
        }
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_POWERBROADCAST:

        switch(wParam) {

        case PBT_APMQUERYSUSPEND:
            if(!s_in_suspend) {
                App::on_suspending();
            }
            s_in_suspend = true;
            return TRUE;

        case PBT_APMRESUMESUSPEND:
            if(!s_minimized) {
                if(s_in_suspend) {
                    App::on_resuming();
                }
                s_in_suspend = false;
            }
            return TRUE;
        }
        break;

        //////////////////////////////////////////////////////////////////////

    case WM_DESTROY:
        App::on_closing();
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
                App::toggle_fullscreen();
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

    case App::WM_FILE_LOAD_COMPLETE:
        App::on_file_load_complete(lParam);
        break;

        //////////////////////////////////////////////////////////////////////

    case App::WM_FOLDER_SCAN_COMPLETE:
        App::on_folder_scanned(reinterpret_cast<folder_scan_result *>(lParam));
        break;

        //////////////////////////////////////////////////////////////////////

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
