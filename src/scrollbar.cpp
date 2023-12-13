//////////////////////////////////////////////////////////////////////
// Handle scrolling for a window

#include "pch.h"

LOG_CONTEXT("scrollbar");

namespace imageview
{
    //////////////////////////////////////////////////////////////////////

    void update_scrollbars(scroll_info &info, HWND hwnd, RECT const &rc, SIZE const &sz)
    {
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;

        // horizontal
        si.nPos = info.pos[SB_HORZ];
        si.nPage = rect_width(rc) + 1;
        si.nMin = 0;
        si.nMax = sz.cx;
        SetScrollInfo(hwnd, SB_HORZ, &si, true);
        info.pos[SB_HORZ] = GetScrollPos(hwnd, SB_HORZ);

        // vertical
        si.nPos = info.pos[SB_VERT];
        si.nPage = rect_height(rc) + 1;
        si.nMin = 0;
        si.nMax = sz.cy;
        SetScrollInfo(hwnd, SB_VERT, &si, true);
        info.pos[SB_VERT] = GetScrollPos(hwnd, SB_VERT);
    }

    //////////////////////////////////////////////////////////////////////

    void scroll_window(scroll_info &info, HWND hwnd, int bar, int lines)
    {
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_TRACKPOS;
        GetScrollInfo(hwnd, bar, &si);
        int max_pos = si.nMax - (si.nPage - 1);
        int page = static_cast<int>(si.nPage);
        int line = page * 10 / 100;
        int new_pos = std::clamp(si.nPos + line * lines, si.nMin, max_pos);
        SetScrollPos(hwnd, bar, new_pos, true);
        info.pos[bar] = GetScrollPos(hwnd, bar);
    }

    //////////////////////////////////////////////////////////////////////

    void on_scroll(scroll_info &info, HWND hwnd, int bar, UINT code)
    {
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_TRACKPOS;
        GetScrollInfo(hwnd, bar, &si);

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
        SetScrollPos(hwnd, bar, new_pos, true);
        info.pos[bar] = GetScrollPos(hwnd, bar);
    }
}
