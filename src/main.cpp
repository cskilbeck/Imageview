//////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "app.h"

#if !defined(UNICODE)
#error Unicode only
#endif

//////////////////////////////////////////////////////////////////////

namespace
{
    using namespace DirectX;

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

    App application;

    std::vector<byte> file_load_buffer;
    std::atomic_bool file_load_complete{ false };
    HRESULT file_load_hresult = S_OK;

    LPCWSTR app_name = L"ImageView";
    LPCWSTR app_class = L"ImageViewWindowClass_2DAE134A-7E46-4E75-9DFA-207695F48699";

    WINDOWPLACEMENT window_placement{ sizeof(WINDOWPLACEMENT) };

    int get_x(LPARAM lp)
    {
        return (int)(short)LOWORD(lp);
    }

    int get_y(LPARAM lp)
    {
        return (int)(short)HIWORD(lp);
    }
}

//////////////////////////////////////////////////////////////////////

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

//////////////////////////////////////////////////////////////////////
// Indicates to hybrid graphics systems to prefer the discrete part by default

extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

//////////////////////////////////////////////////////////////////////

HRESULT check_heif_support(bool &heif_is_supported)
{
    CHK_HR(MFStartup(MF_VERSION));

    IMFActivate **activate{};
    uint32_t count{};

    MFT_REGISTER_TYPE_INFO input;
    input.guidMajorType = MFMediaType_Video;
    input.guidSubtype = MFVideoFormat_HEVC;

    CHK_HR(MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, MFT_ENUM_FLAG_SYNCMFT, &input, null, &activate, &count));

    for(uint32_t i = 0; i < count; i++) {
        activate[i]->Release();
    }

    CoTaskMemFree(activate);
    CHK_HR(MFShutdown());

    heif_is_supported = count > 0;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
    if(!XMVerifyCPUSupport()) {
        MessageBox(null, L"Your computer is too old and crappy to run ImageView", L"ImageView", MB_ICONEXCLAMATION);
        return 1;
    }

    CHECK(application.init());

    HICON icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON2));
    HCURSOR cursor = LoadCursor(null, IDC_ARROW);

    wchar_t *cmd_line = GetCommandLineW();

    if(application.settings.reuse_window) {
        HWND existing_window = FindWindow(app_class, null);
        if(existing_window != null) {
            COPYDATASTRUCT c;
            c.cbData = (DWORD)(wcslen(cmd_line) + 1) * sizeof(wchar_t);
            c.lpData = reinterpret_cast<void *>(cmd_line);
            c.dwData = (DWORD)App::copydata_t::commandline;
            SendMessage(existing_window, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&c));
            return 0;
        }
    }

    CHECK(application.on_command_line(cmd_line));

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = icon;
    wcex.hCursor = cursor;
    wcex.lpszClassName = app_class;
    wcex.hIconSm = icon;

    if(!RegisterClassExW(&wcex)) {
        return 1;
    }

    // styles will also be different depending on fullscreen setting
    DWORD window_style;
    DWORD window_ex_style;
    rect rc;
    CHECK(application.get_startup_rect_and_style(&rc, &window_style, &window_ex_style));

    HWND hwnd = CreateWindowExW(window_ex_style, app_class, app_name, window_style, rc.x(), rc.y(), rc.w(), rc.h(), null, null, hInstance, &application);

    if(hwnd == null) {
        return 1;
    }

    application.check_image_loader();

    if(!application.settings.first_run && !application.settings.fullscreen) {
        WINDOWPLACEMENT w{ application.settings.window_placement };
        w.showCmd = 0;
        SetWindowPlacement(hwnd, &w);
    }

    RAWINPUTDEVICE Rid[1];
    Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
    Rid[0].dwFlags = RIDEV_INPUTSINK;
    Rid[0].hwndTarget = hwnd;
    RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));

    HACCEL haccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));
    if(haccel == null) {
        CHECK(HRESULT_FROM_WIN32(GetLastError()));
    }

    MSG msg = {};
    while(WM_QUIT != msg.message) {
        if(PeekMessage(&msg, null, 0, 0, PM_REMOVE)) {
            if(!TranslateAccelerator(hwnd, haccel, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            application.update();
        }
    }

    CoUninitialize();

    application.on_process_exit();

    return (int)msg.wParam;
}

//////////////////////////////////////////////////////////////////////

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool s_in_sizemove = false;
    static bool s_in_suspend = false;
    static bool s_minimized = false;

    App *app = reinterpret_cast<App *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    if(app == null && !(message == WM_GETMINMAXINFO || message == WM_NCCREATE)) {
        MessageBox(null, L"Fatal error due to a bug!", L"ImageView", MB_ICONEXCLAMATION);
        ExitProcess(1);
    }

    // Log(L"(%04x) %s %08x %08x, app = %p", message, get_wm_name(message), wParam, lParam, app);

    switch(message) {

    // 1st message is always WM_GETMINMAXINFO - you can't use GWLP_USERDATA yet
    case WM_GETMINMAXINFO:
        if(lParam != 0) {
            reinterpret_cast<MINMAXINFO *>(lParam)->ptMinTrackSize = { 320, 200 };
        }
        break;

    // 2nd message is always WM_NCCREATE - setup app pointer for GWLP_USERDATA here
    case WM_NCCREATE: {
        LPCREATESTRUCT c = reinterpret_cast<LPCREATESTRUCT>(lParam);
        if(c != null && c->lpCreateParams != null) {
            auto app_ptr = reinterpret_cast<App *>(c->lpCreateParams);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app_ptr));
            app_ptr->set_window(hWnd);
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    } break;

    // app pointer is valid for all other windows messages
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_PAINT:
        if(s_in_sizemove) {
            app->update();
        } else {
            PAINTSTRUCT ps;
            (void)BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_COPYDATA: {
        COPYDATASTRUCT *c = reinterpret_cast<COPYDATASTRUCT *>(lParam);
        if(c != null) {
            switch((App::copydata_t)c->dwData) {
            case App::copydata_t::commandline:
                app->on_command_line(reinterpret_cast<wchar_t const *>(c->lpData));
                break;
            default:
                break;
            }
        }
    } break;

    case WM_DPICHANGED:
        app->on_dpi_changed((UINT)wParam & 0xffff, reinterpret_cast<rect *>(lParam));
        break;

    case WM_SIZE:
        if(wParam == SIZE_MINIMIZED) {
            if(!s_minimized) {
                s_minimized = true;
                if(!s_in_suspend) {
                    app->on_suspending();
                }
                s_in_suspend = true;
            }
        } else {
            if(s_minimized) {
                s_minimized = false;
                if(s_in_suspend) {
                    app->on_resuming();
                }
                s_in_suspend = false;
            }
            app->on_window_size_changed(LOWORD(lParam), HIWORD(lParam));
        }
        break;

    case WM_WINDOWPOSCHANGING:
        app->on_window_pos_changing(reinterpret_cast<WINDOWPOS *>(lParam));
        break;

    case WM_LBUTTONDOWN:
        app->on_mouse_button(MAKEPOINTS(lParam), App::btn_left, App::btn_down);
        break;

    case WM_RBUTTONDOWN:
        app->on_mouse_button(MAKEPOINTS(lParam), App::btn_right, App::btn_down);
        break;

    case WM_MBUTTONDOWN:
        app->on_mouse_button(MAKEPOINTS(lParam), App::btn_middle, App::btn_down);
        break;

    case WM_LBUTTONUP:
        app->on_mouse_button(MAKEPOINTS(lParam), App::btn_left, App::btn_up);
        break;

    case WM_RBUTTONUP:
        app->on_mouse_button(MAKEPOINTS(lParam), App::btn_right, App::btn_up);
        break;

    case WM_MBUTTONUP:
        app->on_mouse_button(MAKEPOINTS(lParam), App::btn_middle, App::btn_up);
        break;

    case WM_MOUSEMOVE:
        app->on_mouse_move(MAKEPOINTS(lParam));
        break;

    case WM_MOUSEWHEEL: {
        POINT pos{ get_x(lParam), get_y(lParam) };
        ScreenToClient(hWnd, &pos);
        app->on_mouse_wheel(pos, get_y(wParam) / WHEEL_DELTA);
    } break;

    case WM_INPUT: {
        UINT dwSize = sizeof(RAWINPUT);
        static BYTE lpb[sizeof(RAWINPUT)];
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
        RAWINPUT *raw = (RAWINPUT *)lpb;
        if(raw->header.dwType == RIM_TYPEMOUSE) {
            app->on_raw_mouse_move({ (short)raw->data.mouse.lLastX, (short)raw->data.mouse.lLastY });
        }
    } break;

    // suppress alt key behaviour
    case WM_SYSCOMMAND:
        if(wParam == SC_KEYMENU && (lParam >> 16) <= 0) {
            return 0;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_COMMAND:
        switch(LOWORD(wParam)) {
        case ID_ACCELERATOR_PASTE:
            app->on_paste();
            break;
        case ID_ACCELERATOR_COPY:
            app->on_copy();
            break;
        }
        break;

    case WM_KEYDOWN:
        app->on_key_down((int)wParam, lParam);
        break;

    case WM_KEYUP:
        app->on_key_up((int)wParam);
        break;

    case WM_ENTERSIZEMOVE:
        s_in_sizemove = true;
        break;

    case WM_EXITSIZEMOVE:
        s_in_sizemove = false;
        RECT rc;
        GetClientRect(hWnd, &rc);
        app->on_window_size_changed(rc.right - rc.left, rc.bottom - rc.top);
        break;

    case WM_ACTIVATEAPP:
        if(wParam) {
            app->on_activated();
        } else {
            app->on_deactivated();
        }
        break;

    case WM_POWERBROADCAST:
        switch(wParam) {
        case PBT_APMQUERYSUSPEND:
            if(!s_in_suspend) {
                app->on_suspending();
            }
            s_in_suspend = true;
            return TRUE;

        case PBT_APMRESUMESUSPEND:
            if(!s_minimized) {
                if(s_in_suspend) {
                    app->on_resuming();
                }
                s_in_suspend = false;
            }
            return TRUE;
        }
        break;

    case WM_DESTROY:
        app->on_closing();
        PostQuitMessage(0);
        break;

    case WM_SYSKEYDOWN:
        if(wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000) {
            app->toggle_fullscreen();
        }
        break;

    case WM_MENUCHAR:
        return MAKELRESULT(0, MNC_CLOSE);

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
