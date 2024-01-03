//////////////////////////////////////////////////////////////////////

#include "pch.h"

LOG_CONTEXT("filetypes");

//////////////////////////////////////////////////////////////////////

namespace
{
    //////////////////////////////////////////////////////////////////////
    // check if a registry value contains the expected value
    // only set the flag if it does, otherwise leave it

    HRESULT check_registry_value(HKEY key,
                                 std::wstring const &path,
                                 std::wstring const &name,
                                 std::wstring const &expected_value,
                                 bool &is_correct)
    {
        DWORD value_size;
        LSTATUS l = RegGetValueW(key, path.c_str(), name.c_str(), RRF_RT_REG_SZ, null, null, &value_size);

        if(l == ERROR_PATH_NOT_FOUND || l == ERROR_FILE_NOT_FOUND) {
            return S_OK;
        }
        CHK_LSTATUS(l);

        std::wstring value;
        value.resize(value_size);
        CHK_LSTATUS(RegGetValueW(key, path.c_str(), name.c_str(), RRF_RT_REG_SZ, null, value.data(), &value_size));

        while(!value.empty() && value.back() == '\0') {
            value.pop_back();
        }

        is_correct |= _wcsicmp(value.c_str(), expected_value.c_str()) == 0;

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT set_registry_value(HKEY parent_key,
                               std::wstring const &path,
                               std::wstring const &name,
                               std::wstring const &value)
    {
        HKEY key;

        CHK_LSTATUS(RegCreateKeyExW(parent_key, path.c_str(), 0, null, 0, KEY_WRITE, null, &key, null));

        DEFER(RegCloseKey(key));

        CHK_LSTATUS(RegSetValueExW(key,
                                   name.c_str(),
                                   0,
                                   REG_SZ,
                                   reinterpret_cast<byte const *>(value.data()),
                                   static_cast<DWORD>(value.size() * sizeof(wchar))));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT delete_registry_value(HKEY parent_key, std::wstring const &path, std::wstring const &name)
    {
        LSTATUS l = RegDeleteKeyValueW(parent_key, path.c_str(), name.c_str());
        if(l == ERROR_PATH_NOT_FOUND || l == ERROR_FILE_NOT_FOUND) {
            return S_OK;
        }
        return HRESULT_FROM_WIN32(l);
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT delete_registry_key(HKEY parent_key, std::wstring const &path)
    {
        CHK_LSTATUS(RegDeleteTreeW(parent_key, path.c_str()));
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    enum modify_registry_type
    {
        mrt_set,
        mrt_delete,
        mrt_check
    };

    HRESULT update_registry_value(modify_registry_type mrt,
                                  HKEY parent_key,
                                  std::wstring const &path,
                                  std::wstring const &name,
                                  std::wstring const &value,
                                  bool *check)
    {
        switch(mrt) {

        case mrt_set:
            return set_registry_value(parent_key, path, name, value);

        case mrt_delete:
            return delete_registry_value(parent_key, path, name);

        case mrt_check: {
            return check_registry_value(parent_key, path, name, value, *check);
        }
        }

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT update_handler(modify_registry_type mrt, bool *check)
    {
        // HKCU\Software\Classes\ImageView.file\DefaultIcon = REG_SZ exe_path,1
        // HKCU\Software\Classes\ImageView.file\shell\open\command = REG_SZ "exe_path" "%1"
        // HKCU\Software\Classes\{.ext ...}\ImageView.file = REG_SZ empty

        // HKCU\Software\Classes\Applications\ImageView.exe\FriendlyAppName = REG_SZ ImageView
        // HKCU\Software\Classes\Applications\ImageView.exe\DefaultIcon\@Default = REG_SZ REG_SZ exe_path,1
        // HKCU\Software\Classes\Applications\ImageView.exe\shell\open\command\@Default = REG_SZ "exe_path" "%1"
        // HKCU\Software\Classes\Applications\ImageView.exe\SupportedTypes\{.ext} = REG_SZ ""

        // maybe delete HKCU\Software\Classes\ImageView.file
        // maybe delete HKCU\Software\Classes\Applications\ImageView.exe

        auto HK = HKEY_CURRENT_USER;

        std::wstring app_filename;
        CHK_HR(imageview::get_app_filename(app_filename));

        std::wstring app_friendly_name = imageview::localize(IDS_AppName);

        std::wstring handler_name = std::format(L"ImageView.file");
        std::wstring exe_name = std::format(L"ImageView.exe");

        std::wstring handler_path = std::format(L"SOFTWARE\\Classes\\{}", handler_name);
        std::wstring handler_command_path = std::format(L"{}\\shell\\open\\command", handler_path);
        std::wstring handler_icon_path = std::format(L"{}\\DefaultIcon", handler_path);

        std::wstring exe_path = std::format(L"SOFTWARE\\Classes\\Applications\\{}", exe_name);
        std::wstring exe_command_path = std::format(L"{}\\shell\\open\\command", exe_path);
        std::wstring exe_icon_path = std::format(L"{}\\DefaultIcon", exe_path);

        std::wstring command_value = std::format(L"\"{}\" \"%1\"", app_filename);
        std::wstring icon_value = std::format(L"{},1", app_filename);

        CHK_HR(update_registry_value(mrt, HK, handler_icon_path, L"", icon_value, check));
        CHK_HR(update_registry_value(mrt, HK, handler_command_path, L"", command_value, check));
        CHK_HR(update_registry_value(mrt, HK, exe_path, L"FriendlyAppName", app_friendly_name, check));
        CHK_HR(update_registry_value(mrt, HK, exe_icon_path, L"", icon_value, check));
        CHK_HR(update_registry_value(mrt, HK, exe_command_path, L"", command_value, check));

        for(auto const &filetype : imageview::image::load_filetypes.container_formats) {

            std::wstring const &ext = filetype.first;
            std::wstring ext_path1 = std::format(L"SOFTWARE\\Classes\\{}\\OpenWithProgids", ext);
            std::wstring ext_path2 = std::format(L"{}\\SupportedTypes", exe_path);
            std::wstring ext_path3 = std::format(L"SOFTWARE\\Classes\\{}\\OpenWithList\\{}", ext, exe_name);

            CHK_HR(update_registry_value(mrt, HK, ext_path1, handler_name, L"", check));
            CHK_HR(update_registry_value(mrt, HK, ext_path2, ext, L"", check));

            CHK_HR(update_registry_value(mrt, HK, ext_path3, L"", L"", check));

            if(mrt == mrt_delete) {
                CHK_HR(delete_registry_key(HK, ext_path3));
            }

            std::wstring open_with_path = std::format(
                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\{}\\OpenWithProgIds", ext);

            // NOTE: only HKCU for explorer file extension OpenWithProdIds

            CHK_HR(update_registry_value(mrt, HKEY_CURRENT_USER, open_with_path, handler_name, L"", check));
        }

        if(mrt == mrt_delete) {
            CHK_HR(delete_registry_key(HK, handler_path));
            CHK_HR(delete_registry_key(HK, exe_path));
        }

        return S_OK;
    }
}

namespace imageview
{
    //////////////////////////////////////////////////////////////////////
    // returns true if any fragments of the hander are still there

    HRESULT check_filetype_handler(bool &is_installed)
    {
        is_installed = false;
        CHK_HR(update_handler(mrt_check, &is_installed));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT install_filetype_handler()
    {
        CHK_HR(update_handler(mrt_set, null));
        SHChangeNotify(SHCNE_ASSOCCHANGED, 0, null, null);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT remove_filetype_handler()
    {
        CHK_HR(update_handler(mrt_delete, null));
        SHChangeNotify(SHCNE_ASSOCCHANGED, 0, null, null);
        return S_OK;
    }
}
