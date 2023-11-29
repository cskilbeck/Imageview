//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

HRESULT select_file_dialog(HWND window, std::string &path);
HRESULT save_file_dialog(HWND window, std::string &path);
HRESULT select_color_dialog(HWND window, uint32 &color, char const *dialog_title);
