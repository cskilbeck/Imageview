//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

HRESULT select_file_dialog(HWND window, std::wstring &path);
HRESULT save_file_dialog(HWND window, std::wstring &path);
HRESULT select_color_dialog(HWND window, uint32 &color, wchar const *dialog_title);
