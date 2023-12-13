#pragma once

//////////////////////////////////////////////////////////////////////

namespace imageview
{
    struct scroll_info
    {
        int pos[SB_CTL];
    };

    void update_scrollbars(scroll_info &info, HWND window, RECT const &rc, SIZE const &sz);
    void scroll_window(scroll_info &info, HWND window, int bar, int lines);
    void on_scroll(scroll_info &info, HWND window, int bar, UINT code);
}
