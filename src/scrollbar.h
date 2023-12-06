#pragma once

//////////////////////////////////////////////////////////////////////

namespace imageview
{
    struct scroll_info
    {
        HWND hwnd;
        RECT page_rect;
        int prev_scroll_pos[SB_CTL];
    };

    void update_scrollbars(scroll_info &info, HWND window, RECT const &rc);
    void scroll_window(scroll_info &info, int bar, int lines);
    void on_scroll(scroll_info &info, int bar, UINT code);
}
