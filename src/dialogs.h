//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

namespace imageview::dialog
{
    HRESULT open_file(HWND window, std::wstring &path);
    HRESULT save_file(HWND window, std::wstring const &filename, std::wstring &path);
    HRESULT select_color(HWND window, uint32 &color, wchar const *dialog_title);
}
