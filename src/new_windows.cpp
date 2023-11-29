#include "pch.h"

// wrapper / fallbacks for Win7 vs newer functions

float GetWindowDPI(HWND w)
{
    UINT dpi = 0;

    HMODULE h = LoadLibraryA("user32.dll");
    if(h != null) {

        typedef UINT (*GetDpiForWindowFN)(HWND w);
        GetDpiForWindowFN get_dpi_fn = reinterpret_cast<GetDpiForWindowFN>(GetProcAddress(h, "GetDpiForWindow"));
        if(get_dpi_fn != null) {
            dpi = get_dpi_fn(w);
        }
        FreeLibrary(h);
    }

    if(dpi == 0) {
        HDC dc = GetDC(null);
        dpi = (UINT)GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(null, dc);
    }

    return (float)dpi;
}