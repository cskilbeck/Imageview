#pragma once

//////////////////////////////////////////////////////////////////////

namespace hotkeys
{
    extern HACCEL accelerators;
    extern std::vector<ACCEL> accelerator_table;
    extern std::map<uint, std::string> hotkey_text;

    HRESULT load();
    HRESULT setup_menu(HMENU menu, bool file_loaded, bool select_active);
    HRESULT get_hotkey_text(uint id, std::string &text);
}