#include "pch.h"

//////////////////////////////////////////////////////////////////////

namespace
{
    HKL keyboard_layout;
}

//////////////////////////////////////////////////////////////////////

namespace hotkeys
{
    HACCEL accelerators{ null };
    std::vector<ACCEL> accelerator_table;
    std::map<uint, std::string> hotkey_text;

    HRESULT load()
    {
        // load the accelerator table and get all the ACCEL structures

        CHK_NULL(accelerators = LoadAccelerators(GetModuleHandle(null), MAKEINTRESOURCE(IDR_ACCELERATORS_EN_UK)));

        UINT num_accelerators = CopyAcceleratorTable(accelerators, null, 0);

        if(num_accelerators == 0) {
            return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        }
        accelerator_table.resize(num_accelerators);

        UINT num_accelerators_got = CopyAcceleratorTable(accelerators, accelerator_table.data(), num_accelerators);

        if(num_accelerators_got != num_accelerators) {
            accelerator_table.clear();
            return HRESULT_FROM_WIN32(ERROR_NOT_ALL_ASSIGNED);
        }

        // make a string for each hotkey (e.g. "Ctrl-N")

        keyboard_layout = GetKeyboardLayout(GetCurrentThreadId());

        for(auto const &a : accelerator_table) {

            char const *separator = "";

            std::string &text = hotkey_text[a.cmd];

            if(!text.empty()) {
                separator = ",";
            }

            char key_name_buffer[256];
            char const *key_name;
            switch(a.key) {
            case VK_LEFT:
                key_name = "Left";
                break;
            case VK_RIGHT:
                key_name = "Right";
                break;
            case VK_UP:
                key_name = "Up";
                break;
            case VK_DOWN:
                key_name = "Down";
                break;
            case VK_PRIOR:
                key_name = "Page Up";
                break;
            case VK_NEXT:
                key_name = "Page Down";
                break;
            case VK_OEM_COMMA:
            case ',':
                key_name = "Comma";
                break;
            case VK_OEM_PERIOD:
            case '.':
                key_name = "Period";
                break;

            default:
                uint scan_code = MapVirtualKeyEx(a.key, MAPVK_VK_TO_VSC, keyboard_layout);
                GetKeyNameTextA((scan_code & 0x7f) << 16, key_name_buffer, _countof(key_name_buffer));
                key_name = key_name_buffer;
                break;
            }

            // build the label with modifier keys

            std::string key_label;

            auto append = [&](std::string &a, char const *b) {
                if(!a.empty()) {
                    a.append("-");
                }
                a.append(b);
            };

            if(a.fVirt & FCONTROL) {
                append(key_label, "Ctrl");
            }
            if(a.fVirt & FSHIFT) {
                append(key_label, "Shift");
            }
            if(a.fVirt & FALT) {
                append(key_label, "Alt");
            }
            append(key_label, key_name);

            text = std::format("{}{}{}", text, separator, key_label);
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_hotkey_text(uint id, std::string &text)
    {
        auto found = hotkey_text.find(id);

        if(found != hotkey_text.end()) {

            text = found->second;
            return S_OK;
        }

        return S_FALSE;
    }
}