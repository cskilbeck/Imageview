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

    void update_scrollbars(scroll_info &info, HWND window, RECT const &rc)
    {
        info.page_rect = rc;
        info.hwnd = window;
        info.prev_scroll_pos[SB_HORZ] = 0;
        info.prev_scroll_pos[SB_VERT] = 0;

        RECT client_rect;
        GetClientRect(info.hwnd, &client_rect);

        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
        si.nPos = 0;
        si.nTrackPos = 0;
        si.nMin = 0;

        int page_width = rect_width(info.page_rect);
        int client_width = rect_width(client_rect);

        if(page_width < client_width) {
            si.nMax = client_width;
            si.nPage = page_width;
            SetScrollInfo(info.hwnd, SB_HORZ, &si, FALSE);
        }

        int page_height = rect_height(info.page_rect);
        int client_height = rect_height(client_rect);

        if(page_height < client_height) {
            si.nMax = client_height;
            si.nPage = page_height;
            SetScrollInfo(info.hwnd, SB_VERT, &si, FALSE);
        }
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
        SetScrollPos(info.hwnd, bar, new_pos, TRUE);
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
        SetScrollPos(info.hwnd, bar, new_pos, TRUE);
        update_window_pos(info, bar, new_pos);
    }
}
