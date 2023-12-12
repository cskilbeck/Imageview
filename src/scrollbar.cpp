//////////////////////////////////////////////////////////////////////
// Handle scrolling for a window

#include "pch.h"

LOG_CONTEXT("scrollbar");

namespace
{
    //////////////////////////////////////////////////////////////////////

    void update_window_pos(imageview::scroll_info &info, int bar, int pos)
    {
        int move[SB_CTL] = { 0, 0 };
        move[bar] = info.prev_scroll_pos[bar] - pos;
        info.prev_scroll_pos[bar] = pos;
        if(move[bar] != 0) {
            ScrollWindow(info.hwnd, move[SB_HORZ], move[SB_VERT], NULL, NULL);
        }
    }
}

namespace imageview
{
    //////////////////////////////////////////////////////////////////////

    void update_scrollbars(scroll_info &info, HWND window, RECT const &rc, SIZE const &sz)
    {
        info.hwnd = window;

        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;

        // horizontal
        si.nPos = info.prev_scroll_pos[SB_HORZ];
        si.nPage = rect_width(rc) + 1;
        si.nMin = 0;
        si.nMax = sz.cx;
        SetScrollInfo(info.hwnd, SB_HORZ, &si, true);
        GetScrollInfo(info.hwnd, SB_HORZ, &si);
        int xpos = si.nPos;

        // vertical
        si.nPos = info.prev_scroll_pos[SB_VERT];
        si.nPage = rect_height(rc) + 1;
        si.nMin = 0;
        si.nMax = sz.cy;
        SetScrollInfo(info.hwnd, SB_VERT, &si, true);
        GetScrollInfo(info.hwnd, SB_VERT, &si);
        int ypos = si.nPos;

        ScrollWindow(info.hwnd, -xpos, -ypos, null, null);
        info.prev_scroll_pos[SB_HORZ] = xpos;
        info.prev_scroll_pos[SB_VERT] = ypos;
    }

    //////////////////////////////////////////////////////////////////////

    void scroll_window(scroll_info &info, int bar, int lines)
    {
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_TRACKPOS;
        GetScrollInfo(info.hwnd, bar, &si);
        int max_pos = si.nMax - (si.nPage - 1);
        int page = static_cast<int>(si.nPage);
        int line = page * 10 / 100;
        int new_pos = std::clamp(si.nPos + line * lines, si.nMin, max_pos);
        SetScrollPos(info.hwnd, bar, new_pos, true);
        update_window_pos(info, bar, new_pos);
    }

    //////////////////////////////////////////////////////////////////////

    void on_scroll(scroll_info &info, int bar, UINT code)
    {
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_TRACKPOS;
        GetScrollInfo(info.hwnd, bar, &si);

        int const max_pos = si.nMax - (si.nPage - 1);
        int const page = static_cast<int>(si.nPage);
        int const line = page * 10 / 100;

        int new_pos;
        switch(code) {
        case SB_LINEUP:
            new_pos = std::max(si.nPos - line, si.nMin);
            break;

        case SB_LINEDOWN:
            new_pos = std::min(si.nPos + line, max_pos);
            break;

        case SB_PAGEUP:
            new_pos = std::max(si.nPos - page, si.nMin);
            break;

        case SB_PAGEDOWN:
            new_pos = std::min(si.nPos + page, max_pos);
            break;

        case SB_THUMBTRACK:
            new_pos = si.nTrackPos;
            break;

        case SB_TOP:
            new_pos = si.nMin;
            break;

        case SB_BOTTOM:
            new_pos = max_pos;
            break;

        default:
            new_pos = si.nPos;
            break;
        }
        SetScrollPos(info.hwnd, bar, new_pos, true);
        update_window_pos(info, bar, new_pos);
    }
}
