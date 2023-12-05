#include "pch.h"

//////////////////////////////////////////////////////////////////////

namespace
{
    HKL keyboard_layout;
}

//////////////////////////////////////////////////////////////////////

namespace imageview::hotkeys
{
    HACCEL accelerators{ null };
    std::vector<ACCEL> accelerator_table;
    std::map<uint, std::string> hotkey_text;

    // load the accelerator table from the resource
    // load any modifications from the registry

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
            std::string key_name;
            switch(a.key) {
            case VK_LEFT:
                key_name = localize(IDS_KEYNAME_LEFT);
                break;
            case VK_RIGHT:
                key_name = localize(IDS_KEYNAME_RIGHT);
                break;
            case VK_UP:
                key_name = localize(IDS_KEYNAME_UP);
                break;
            case VK_DOWN:
                key_name = localize(IDS_KEYNAME_DOWN);
                break;
            case VK_PRIOR:
                key_name = localize(IDS_KEYNAME_PAGEUP);
                break;
            case VK_NEXT:
                key_name = localize(IDS_KEYNAME_PAGEDOWN);
                break;
            case VK_OEM_COMMA:
            case ',':
                key_name = localize(IDS_KEYNAME_COMMA);
                break;
            case VK_OEM_PERIOD:
            case '.':
                key_name = localize(IDS_KEYNAME_PERIOD);
                break;

            default:
                uint scan_code = MapVirtualKeyEx(a.key, MAPVK_VK_TO_VSC, keyboard_layout);
                GetKeyNameTextA((scan_code & 0x7f) << 16, key_name_buffer, _countof(key_name_buffer));
                key_name = std::string(key_name_buffer);
                break;
            }

            // build the label with modifier keys

            std::string key_label;

            auto append = [&](std::string &a, std::string const &b) {
                if(!a.empty()) {
                    a.append("-");
                }
                a.append(b);
            };

            if(a.fVirt & FCONTROL) {
                append(key_label, localize(IDS_KEYNAME_CTRL));
            }
            if(a.fVirt & FSHIFT) {
                append(key_label, localize(IDS_KEYNAME_SHIFT));
            }
            if(a.fVirt & FALT) {
                append(key_label, localize(IDS_KEYNAME_ALT));
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