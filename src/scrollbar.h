#pragma once

//////////////////////////////////////////////////////////////////////

namespace imageview
{
    struct scroll_info
    {
        HWND hwnd{ null };
        int prev_scroll_pos[SB_CTL];
    };

    void update_scrollbars(scroll_info &info, HWND window, RECT const &rc, SIZE const &sz);
    void scroll_window(scroll_info &info, int bar, int lines);
    void on_scroll(scroll_info &info, int bar, UINT code);
}
