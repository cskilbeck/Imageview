//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

namespace imageview::dialog
{
    HRESULT open_file(HWND window, std::string &path);
    HRESULT save_file(HWND window, std::string &path);
    HRESULT select_color(HWND window, uint32 &color, char const *dialog_title);
}
