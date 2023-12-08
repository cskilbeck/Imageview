#pragma once

namespace imageview::settings_dialog
{
    //////////////////////////////////////////////////////////////////////
    // all the settings on the settings page derive from this

    struct setting_controller
    {
        setting_controller(char const *n, uint s, uint dlg_id, DLGPROC dlgproc)
            : internal_name(n), string_resource_id(s), dialog_resource_id(dlg_id), dlg_proc(dlgproc), window(null)
        {
        }

        // internal name of the setting
        char const *internal_name;

        // user friendly descriptive name
        uint string_resource_id;

        // create dialog from this resource id
        uint dialog_resource_id;

        DLGPROC dlg_proc;

        HWND window;

        virtual void setup_controls(HWND hwnd);

        // update the dialog controls with current value of this setting
        virtual void update_controls() = 0;

        // get a pointer to the setting object associated with a setting window
        template <typename T> static T &get(HWND w)
        {
            T *p = reinterpret_cast<T *>(GetWindowLongPtrA(w, GWLP_USERDATA));
            return *p;
        }
    };

    // this for everybody to make all controls have the correct background color
    INT_PTR ctlcolor_base(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

    // sets up controls
    BOOL on_initdialog_setting(HWND hwnd, HWND hwndFocus, LPARAM lParam);

    INT_PTR setting_base_dlgproc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);
}
