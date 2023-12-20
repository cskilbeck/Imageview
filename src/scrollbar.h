#pragma once

//////////////////////////////////////////////////////////////////////

namespace imageview
{
    // SB_HORZ = 0
    // SB_VERT = 1
    using scroll_pos = int[2];

    void update_scrollbars(scroll_pos &pos, HWND window, RECT const &rc, SIZE const &sz);
    void scroll_window(scroll_pos &pos, HWND window, int bar, int lines);
    void on_scroll(scroll_pos &pos, HWND window, int bar, UINT code);
}
