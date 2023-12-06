//////////////////////////////////////////////////////////////////////

#include "pch.h"

LOG_CONTEXT("settings_dlg");

//////////////////////////////////////////////////////////////////////

namespace
{
    // big sigh - these are for mapping enums to strings for the combo boxes

    using enum_id_map = std::map<uint, uint>;

    // fullscreen_startup_option

    enum_id_map enum_fullscreen_startup_map = {
        { fullscreen_startup_option::start_fullscreen, IDS_ENUM_FULLSCREEN_STARTUP_FULLSCREEN },
        { fullscreen_startup_option::start_windowed, IDS_ENUM_FULLSCREEN_STARTUP_WINDOWED },
        { fullscreen_startup_option::start_remember, IDS_ENUM_FULLSCREEN_STARTUP_REMEMBER },
    };

    // mouse_button

    enum_id_map enum_mouse_buttons_map = {
        { mouse_button_t::btn_left, IDS_ENUM_MOUSE_BUTTON_LEFT },
        { mouse_button_t::btn_right, IDS_ENUM_MOUSE_BUTTON_RIGHT },
        { mouse_button_t::btn_middle, IDS_ENUM_MOUSE_BUTTON_MIDDLE },
    };

    // show_filename_option

    enum_id_map enum_show_filename_map = {
        { show_filename_option::show_filename_always, IDS_ENUM_SHOW_FILENAME_ALWAYS },
        { show_filename_option::show_filename_briefly, IDS_ENUM_SHOW_FILENAME_BRIEFLY },
        { show_filename_option::show_filename_never, IDS_ENUM_SHOW_FILENAME_NEVER },
    };

    // exif_option

    enum_id_map enum_exif_map = {
        { exif_option::exif_option_apply, IDS_ENUM_EXIF_APPLY },
        { exif_option::exif_option_ignore, IDS_ENUM_EXIF_IGNORE },
        { exif_option::exif_option_prompt, IDS_ENUM_EXIF_PROMPT },
    };

    // zoom_mode

    enum_id_map enum_zoom_mode_map = {
        { zoom_mode_t::shrink_to_fit, IDS_ENUM_ZOOM_MODE_SHRINK_TO_FIT },
        { zoom_mode_t::fit_to_window, IDS_ENUM_ZOOM_MODE_FIT_TO_WINDOW },
        { zoom_mode_t::one_to_one, IDS_ENUM_ZOOM_MODE_ONE_TO_ONE },
    };

    // startup_zoom_mode

    enum_id_map enum_startup_zoom_mode_map = {
        { startup_zoom_mode_option::startup_zoom_shrink_to_fit, IDS_ENUM_ZOOM_MODE_SHRINK_TO_FIT },
        { startup_zoom_mode_option::startup_zoom_fit_to_window, IDS_ENUM_ZOOM_MODE_FIT_TO_WINDOW },
        { startup_zoom_mode_option::startup_zoom_one_to_one, IDS_ENUM_ZOOM_MODE_ONE_TO_ONE },
        { startup_zoom_mode_option::startup_zoom_remember, IDS_ENUM_ZOOM_MODE_REMEMBER },
    };

    using namespace imageview;

    //////////////////////////////////////////////////////////////////////

    HWND main_dialog = null;

    int current_page = -1;

    //////////////////////////////////////////////////////////////////////

    enum tab_flags_t
    {
        dont_care = 0,
        hide_if_elevated = 1,
        hide_if_not_elevated = 2,
    };

    struct settings_tab_t
    {
        uint resource_id;
        DLGPROC handler;
        int flags;
        int index;
        HWND hwnd;
    };

    //////////////////////////////////////////////////////////////////////

    INT_PTR settings_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR hotkeys_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR explorer_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR relaunch_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);
    INT_PTR about_handler(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam);

    //////////////////////////////////////////////////////////////////////
    // all the tabs that can be created

    settings_tab_t tabs[] = {
        { IDD_DIALOG_SETTINGS_MAIN, settings_handler, dont_care },
        { IDD_DIALOG_SETTINGS_HOTKEYS, hotkeys_handler, dont_care },
        { IDD_DIALOG_SETTINGS_EXPLORER, explorer_handler, hide_if_not_elevated },
        { IDD_DIALOG_SETTINGS_RELAUNCH, relaunch_handler, hide_if_elevated },
        { IDD_DIALOG_SETTINGS_ABOUT, about_handler, dont_care },
    };

    //////////////////////////////////////////////////////////////////////
    // which tabs will be shown (some might be hidden)

    std::vector<settings_tab_t *> active_tabs;

    //////////////////////////////////////////////////////////////////////
    // all the settings derive from this

    struct setting
    {
        setting(char const *n, uint s, uint dlg_id, uint text_id, DLGPROC handler)
            : internal_name(n)
            , string_resource_id(s)
            , dialog_resource_id(dlg_id)
            , text_item_ctl_id(text_id)
            , dlg_handler(handler)
        {
        }

        // internal name of the setting
        char const *internal_name;

        // user friendly descriptive name for the dialog
        uint string_resource_id;

        // create dialog from this resource id
        uint dialog_resource_id;

        // which control contains the descriptive text
        uint text_item_ctl_id;

        DLGPROC dlg_handler;

        // set name text and update controls for editing this setting
        virtual void setup_controls(HWND hwnd)
        {
            SetWindowTextW(GetDlgItem(hwnd, text_item_ctl_id), unicode(localize(string_resource_id)).c_str());
            update_controls(hwnd);
        }

        // update the dialog controls with current value of this setting
        virtual void update_controls(HWND)
        {
        }
    };

    //////////////////////////////////////////////////////////////////////
    // scroll info for the settings page

    scroll_info settings_scroll;

    //////////////////////////////////////////////////////////////////////

    bool should_hide_tab(settings_tab_t const *t)
    {
        return (t->flags & hide_if_elevated) != 0 && app::is_elevated ||
               (t->flags & hide_if_not_elevated) != 0 && !app::is_elevated;
    }

    //////////////////////////////////////////////////////////////////////
    // switch to a tab page

    HRESULT show_settings_page(uint tab)
    {
        if(tab >= active_tabs.size()) {
            LOG_ERROR("!? Tab {} is out of range (there are {} tabs)", tab, active_tabs.size());
            return HRESULT_FROM_WIN32(ERROR_BAD_ARGUMENTS);
        }
        if(current_page != -1) {
            ShowWindow(active_tabs[current_page]->hwnd, SW_HIDE);
        }
        current_page = tab;
        ShowWindow(active_tabs[current_page]->hwnd, SW_SHOW);
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // UNREFERENCED PARAMETER warning suppressed in here (too unwieldy)

#pragma warning(push)
#pragma warning(disable : 4100)

    //////////////////////////////////////////////////////////////////////
    // dialogs which are in tab pages have white backgrounds

    HBRUSH on_ctl_color(HWND hwnd, HDC hdc, HWND hwndChild, int type)
    {
        return GetSysColorBrush(COLOR_WINDOW);
    }

    //////////////////////////////////////////////////////////////////////
    // the separator has a gray background

    HBRUSH on_ctl_color_separator(HWND hwnd, HDC hdc, HWND hwndChild, int type)
    {
        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
        return GetSysColorBrush(COLOR_3DFACE);
    }

    //////////////////////////////////////////////////////////////////////
    // default ctl color for most controls

    INT_PTR setting_ctlcolor_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_CTLCOLORDLG, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLORSTATIC, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLORBTN, on_ctl_color);
            HANDLE_MSG(dlg, WM_CTLCOLOREDIT, on_ctl_color);
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    BOOL on_initdialog_setting_handler(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, lParam);
        setting *s = reinterpret_cast<setting *>(lParam);
        s->setup_controls(hwnd);
        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // base handler for settings

    INT_PTR setting_base_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_INITDIALOG, on_initdialog_setting_handler);
        }
        return setting_ctlcolor_handler(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////

    INT_PTR setting_separator_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_CTLCOLORSTATIC, on_ctl_color_separator);
            HANDLE_MSG(dlg, WM_INITDIALOG, on_initdialog_setting_handler);
        }
        return setting_base_handler(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // a separator 'setting' isn't really a setting, just a... separator

    struct separator_setting : setting
    {
        separator_setting(uint s)
            : setting(
                  "separator", s, IDD_DIALOG_SETTING_SEPARATOR, IDC_STATIC_SETTING_SEPARATOR, setting_separator_handler)
        {
        }
    };

    //////////////////////////////////////////////////////////////////////
    // a bool setting is a checkbox

    struct bool_setting : setting
    {
        bool_setting(char const *n, uint s, uint dlg_id, uint text_id, DLGPROC handler, bool *b)
            : setting(n, s, dlg_id, text_id, handler), value(b)
        {
        }

        void update_controls(HWND hwnd) override
        {
            Button_SetCheck(GetDlgItem(hwnd, IDC_CHECK_SETTING_BOOL), *value ? BST_CHECKED : BST_UNCHECKED);
        }

        bool *value;
    };

    //////////////////////////////////////////////////////////////////////
    // enum setting is a combobox

    struct enum_setting : setting
    {
        enum_setting(
            char const *n, uint s, uint dlg_id, uint text_id, DLGPROC handler, enum_id_map const &names, uint *b)
            : setting(n, s, dlg_id, text_id, handler), enum_names(names), value(b)
        {
        }

        void setup_controls(HWND hwnd) override
        {
            HWND combo_box = GetDlgItem(hwnd, IDC_COMBO_SETTING_ENUM);
            int index = 0;
            for(auto const &name : enum_names) {
                ComboBox_AddString(combo_box, localize(name.second).c_str());
                ComboBox_SetItemData(combo_box, index, name.first);
                index += 1;
            }
            setting::setup_controls(hwnd);
        }

        void update_controls(HWND hwnd) override
        {
            int index = 0;
            for(auto const &name : enum_names) {
                if(name.first == *value) {
                    HWND combo_box = GetDlgItem(hwnd, IDC_COMBO_SETTING_ENUM);
                    ComboBox_SetCurSel(combo_box, index);
                    break;
                }
                index += 1;
            }
        }

        uint *value;
        std::map<uint, uint> const &enum_names;    // map<enum_value, string_id>
    };

    //////////////////////////////////////////////////////////////////////
    // color setting is a button and edit control for the hex

    struct color_setting : setting
    {
        color_setting(char const *n, uint s, uint dlg_id, uint text_id, DLGPROC handler, vec4 *b)
            : setting(n, s, dlg_id, text_id, handler), value(b)
        {
        }

        void update_controls(HWND hwnd) override
        {
            uint32 color = color_swap_red_blue(color_to_uint32(*value));
            std::string hex = std::format("{:06x}", color & 0xffffff);
            make_uppercase(hex);
            SetWindowTextA(GetDlgItem(hwnd, IDC_EDIT_SETTING_COLOR), hex.c_str());
        }

        vec4 *value;
    };

    //////////////////////////////////////////////////////////////////////
    // a ranged setting is... more complicated

    template <typename T> struct ranged_setting : setting
    {
        ranged_setting(char const *n, uint s, uint dlg_id, uint text_id, DLGPROC handler, T *b, T minval, T maxval)
            : setting(n, s, dlg_id, text_id, handler), value(b), min_value(minval), max_value(maxval)
        {
        }

        void update_controls(HWND hwnd) override
        {
        }

        T *value;
        T min_value;
        T max_value;
    };

    //////////////////////////////////////////////////////////////////////
    // all the settings on the settings page

    std::list<setting *> setting_controllers;

    //////////////////////////////////////////////////////////////////////
    // copy of settings currently being edited

    settings_t dialog_settings;

    //////////////////////////////////////////////////////////////////////
    // copy of settings from when the dialog was shown (for 'cancel')

    settings_t revert_settings;

    //////////////////////////////////////////////////////////////////////
    // a colored button for setting colors

    void on_drawitem_setting_color(HWND hwnd, const DRAWITEMSTRUCT *lpDrawItem)
    {
        if(lpDrawItem->CtlID == IDC_BUTTON_SETTING_COLOR) {

            color_setting *setting = reinterpret_cast<color_setting *>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

            uint color = color_to_uint32(*setting->value) & 0xffffff;

            SetDCBrushColor(lpDrawItem->hDC, color);

            SelectObject(lpDrawItem->hDC, GetStockObject(DC_BRUSH));

            Rectangle(lpDrawItem->hDC,
                      lpDrawItem->rcItem.left,
                      lpDrawItem->rcItem.top,
                      lpDrawItem->rcItem.right,
                      lpDrawItem->rcItem.bottom);
        }
    }

    //////////////////////////////////////////////////////////////////////
    // they clicked the colored button or edited the hex text

    void on_command_setting_color(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        color_setting *setting = reinterpret_cast<color_setting *>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

        switch(id) {

        case IDC_BUTTON_SETTINGS_BACKGROUND_COLOR: {

            std::string title = localize(setting->string_resource_id);
            uint new_color = color_to_uint32(*setting->value);
            if(dialog::select_color(GetParent(hwnd), new_color, title.c_str()) == S_OK) {
                *setting->value = color_from_uint32(new_color);
                setting->update_controls(hwnd);
                InvalidateRect(hwnd, null, true);
            }
        } break;

        case IDC_EDIT_SETTING_COLOR: {

            switch(codeNotify) {

            case EN_CHANGE: {

                HWND edit_control = GetDlgItem(hwnd, IDC_EDIT_SETTING_COLOR);
                int len = GetWindowTextLengthA(edit_control);
                if(len > 0) {
                    std::string txt;
                    txt.resize(len + 1llu);
                    GetWindowTextA(edit_control, txt.data(), len + 1);
                    txt.pop_back();
                    uint32 new_color{};
                    if(SUCCEEDED(color_from_string(txt, new_color))) {
                        *setting->value = color_from_uint32(new_color);
                        InvalidateRect(hwnd, null, true);
                    }
                }
            } break;
            }
        } break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // they chose a new enum value from the combobox

    void on_command_setting_enum(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {
        case IDC_COMBO_SETTING_ENUM: {
            HWND combo_box = GetDlgItem(hwnd, IDC_COMBO_SETTING_ENUM);
            int sel = ComboBox_GetCurSel(combo_box);
            uint v = static_cast<uint>(ComboBox_GetItemData(combo_box, sel));
            enum_setting *setting = reinterpret_cast<enum_setting *>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
            *setting->value = v;
        } break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // suppress the text caret in the 'about' text box

    LRESULT CALLBACK suppress_cursor_subclass(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR)
    {
        HideCaret(dlg);
        return DefSubclassProc(dlg, msg, wparam, lparam);
    }

    //////////////////////////////////////////////////////////////////////
    // BOOL setting \ DLGPROC

    INT_PTR setting_bool_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return setting_base_handler(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // ENUM setting \ DLGPROC

    INT_PTR setting_enum_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_COMMAND, on_command_setting_enum);
        }
        return setting_base_handler(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // COLOR setting \ DLGPROC

    INT_PTR setting_color_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_DRAWITEM, on_drawitem_setting_color);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_setting_color);
        }
        return setting_base_handler(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // RANGED setting \ DLGPROC

    INT_PTR setting_ranged_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return setting_base_handler(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // PAGES
    //////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_VSCROLL

    void on_vscroll_settings_handler(HWND hwnd, HWND hwndCtl, UINT code, int pos)
    {
        on_scroll(settings_scroll, SB_VERT, code);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_INITDIALOG

    BOOL on_initdialog_settings_handler(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        if(setting_controllers.empty()) {

            // snapshot current settings
            dialog_settings = settings;

#undef DECL_SETTING_SEPARATOR
#undef DECL_SETTING_BOOL
#undef DECL_SETTING_COLOR
#undef DECL_SETTING_ENUM
#undef DECL_SETTING_RANGED
#undef DECL_SETTING_INTERNAL

#define DECL_SETTING_SEPARATOR(string_id) setting_controllers.push_back(new separator_setting(string_id))

#define DECL_SETTING_BOOL(name, string_id, value)                           \
    setting_controllers.push_back(new bool_setting(#name,                   \
                                                   string_id,               \
                                                   IDD_DIALOG_SETTING_BOOL, \
                                                   IDC_CHECK_SETTING_BOOL,  \
                                                   setting_bool_handler,    \
                                                   &dialog_settings.name));

#define DECL_SETTING_COLOR(name, string_id, r, g, b, a)                       \
    setting_controllers.push_back(new color_setting(#name,                    \
                                                    string_id,                \
                                                    IDD_DIALOG_SETTING_COLOR, \
                                                    IDC_STATIC_SETTING_COLOR, \
                                                    setting_color_handler,    \
                                                    &dialog_settings.name));

#define DECL_SETTING_ENUM(type, name, string_id, enum_names, value)         \
    setting_controllers.push_back(new enum_setting(#name,                   \
                                                   string_id,               \
                                                   IDD_DIALOG_SETTING_ENUM, \
                                                   IDC_STATIC_SETTING_ENUM, \
                                                   setting_enum_handler,    \
                                                   enum_names,              \
                                                   reinterpret_cast<uint *>(&dialog_settings.name)));

#define DECL_SETTING_RANGED(type, name, string_id, value, min, max)                   \
    setting_controllers.push_back(new ranged_setting<type>(#name,                     \
                                                           string_id,                 \
                                                           IDD_DIALOG_SETTING_RANGED, \
                                                           IDC_STATIC_SETTING_RANGED, \
                                                           setting_ranged_handler,    \
                                                           &dialog_settings.name,     \
                                                           min,                       \
                                                           max));

#define DECL_SETTING_INTERNAL(setting_type, name, ...)

#include "settings_fields.h"
        }

        // get parent tab window rect
        HWND tab_ctrl = GetDlgItem(main_dialog, IDC_SETTINGS_TAB_CONTROL);

        RECT tab_rect;
        GetWindowRect(tab_ctrl, &tab_rect);
        MapWindowPoints(null, main_dialog, reinterpret_cast<LPPOINT>(&tab_rect), 2);
        TabCtrl_AdjustRect(tab_ctrl, false, &tab_rect);

        float dpi = get_window_dpi(main_dialog);

        auto dpi_scale = [=](int x) { return static_cast<int>(x * dpi / 96.0f); };

        int const top_margin = dpi_scale(12);
        int const inner_margin = dpi_scale(2);
        int const bottom_margin = dpi_scale(12);

        int cur_height = top_margin;

        for(auto const s : setting_controllers) {
            HWND setting = CreateDialogParamA(app::instance,
                                              MAKEINTRESOURCE(s->dialog_resource_id),
                                              hwnd,
                                              s->dlg_handler,
                                              reinterpret_cast<LPARAM>(s));
            RECT r;
            GetWindowRect(setting, &r);
            SetWindowPos(
                setting, null, 0, cur_height, rect_width(tab_rect), rect_height(r), SWP_NOZORDER | SWP_SHOWWINDOW);
            cur_height += rect_height(r) + inner_margin;
        }

        cur_height += bottom_margin;

        // make this window big enough to contain the settings
        SetWindowPos(hwnd, null, 0, 0, rect_width(tab_rect), cur_height, SWP_NOZORDER | SWP_NOMOVE);
        update_scrollbars(settings_scroll, hwnd, tab_rect);

        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ WM_MOUSEWHEEL

    void on_mousewheel_settings_handler(HWND hwnd, int xPos, int yPos, int zDelta, UINT fwKeys)
    {
        scroll_window(settings_scroll, SB_VERT, -zDelta / WHEEL_DELTA);
    }

    //////////////////////////////////////////////////////////////////////
    // SETTINGS page \ DLGPROC

    INT_PTR settings_handler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(hWnd, WM_VSCROLL, on_vscroll_settings_handler);
            HANDLE_MSG(hWnd, WM_INITDIALOG, on_initdialog_settings_handler);
            HANDLE_MSG(hWnd, WM_MOUSEWHEEL, on_mousewheel_settings_handler);
        }
        return setting_ctlcolor_handler(hWnd, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // RELAUNCH (explorer) page handlers
    //////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    // RELAUNCH (explorer) page \ WM_COMMAND

    void on_command_relaunch_handler(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

        case IDC_BUTTON_SETTINGS_RELAUNCH: {
            EndDialog(main_dialog, app::LRESULT_LAUNCH_AS_ADMIN);
        } break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // RELAUNCH (explorer) page \ DLGPROC

    INT_PTR relaunch_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_COMMAND, on_command_relaunch_handler);
        }
        return setting_ctlcolor_handler(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // EXPLORER page handlers
    //////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    // EXPLORER page \ DLGPROC

    INT_PTR explorer_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return setting_ctlcolor_handler(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // ABOUT page handlers
    //////////////////////////////////////////////////////////////////////

    // ABOUT page \ WM_INITDIALOG

    BOOL on_init_about_handler(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        HWND about = GetDlgItem(hwnd, IDC_SETTINGS_EDIT_ABOUT);
        SetWindowSubclass(about, suppress_cursor_subclass, 0, 0);
        SendMessage(about, EM_SETREADONLY, 1, 0);
        std::string version{ "Version?" };
        get_app_version(version);
        SetWindowTextA(about,
                       std::format("{}\r\nv{}\r\nBuilt {}\r\nRunning as admin: {}\r\nSystem Memory {} GB\r\n",
                                   localize(IDS_AppName),
                                   version,
                                   __TIMESTAMP__,
                                   app::is_elevated,
                                   app::system_memory_gb)
                           .c_str());
        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // ABOUT page \ WM_COMMAND

    void on_command_about_handler(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

            // copy 'about' text to clipboard

        case IDC_BUTTON_ABOUT_COPY: {
            HWND edit_control = GetDlgItem(hwnd, IDC_SETTINGS_EDIT_ABOUT);

            SetLastError(0);
            int len = GetWindowTextLengthA(edit_control);
            if(len == 0) {
                LOG_ERROR("Can't GetWindowTextLength for clipboard: {}", windows_error_message());
                return;
            }
            HANDLE handle = GlobalAlloc(GHND | GMEM_SHARE, static_cast<size_t>(len) + 1);
            if(handle == null) {
                LOG_ERROR("Can't GlobalAlloc {} for clipboard: {}", len, windows_error_message());
                return;
            }
            char *buffer = reinterpret_cast<char *>(GlobalLock(handle));
            if(buffer == null) {
                LOG_ERROR("Can't GlobalLock clipboard buffer: {}", windows_error_message());
                return;
            }

            if(GetWindowTextA(edit_control, buffer, len + 1) == 0) {
                LOG_ERROR("Can't GetWindowText for clipboard: {}", windows_error_message());
                return;
            }

            if(!OpenClipboard(null)) {
                LOG_ERROR("Can't OpenClipboard: {}", windows_error_message());
                return;
            }
            DEFER(CloseClipboard());

            if(!EmptyClipboard()) {
                LOG_ERROR("Can't EmptyClipboard: {}", windows_error_message());
                return;
            }

            if(!SetClipboardData(CF_TEXT, handle)) {
                LOG_ERROR("Can't SetClipboardData clipboard: {}", windows_error_message());
                return;
            }
            LOG_INFO("Copied 'About' text to clipboard");
            SetWindowTextW(GetDlgItem(hwnd, IDC_BUTTON_ABOUT_COPY), unicode(localize(IDS_COPIED)).c_str());

        } break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // ABOUT page \ DLGPROC

    INT_PTR about_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_INITDIALOG, on_init_about_handler);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_about_handler);
        }
        return setting_ctlcolor_handler(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page handlers
    //////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page \ WM_INITDIALOG

    BOOL on_init_hotkeys_handler(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        HWND listview = GetDlgItem(hwnd, IDC_LIST_HOTKEYS);

        RECT listview_rect;
        GetWindowRect(hwnd, &listview_rect);

        int inner_width = rect_width(listview_rect) - GetSystemMetrics(SM_CXVSCROLL);

        LVCOLUMNA column;
        mem_clear(&column);
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.fmt = LVCFMT_LEFT;
        column.cx = inner_width * 70 / 100;
        column.pszText = const_cast<LPSTR>("Action");
        ListView_InsertColumn(listview, 0, &column);
        column.cx = inner_width * 30 / 100;
        column.pszText = const_cast<LPSTR>("Hotkey");
        ListView_InsertColumn(listview, 1, &column);
        ListView_SetExtendedListViewStyle(listview, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_FLATSB);
        ListView_SetView(listview, LV_VIEW_DETAILS);

        // get all the hotkey descriptions in order

        std::map<std::string, uint> descriptions;

        for(auto const &a : hotkeys::hotkey_text) {
            descriptions[localize(a.first)] = a.first;
        }

        // populate the listview

        int index = 0;
        for(auto const &a : descriptions) {

            // there should be a string corresponding to the command id
            std::string action_text = a.first;
            std::string key_text;
            if(hotkeys::get_hotkey_text(a.second, key_text) == S_OK) {
                LVITEMA item;
                mem_clear(&item);
                item.mask = LVIF_TEXT;
                item.iItem = index;
                item.iSubItem = 0;
                item.pszText = action_text.data();
                ListView_InsertItem(listview, &item);
                index += 1;

                item.mask = LVIF_TEXT;
                item.iSubItem = 1;
                item.pszText = key_text.data();
                ListView_SetItem(listview, &item);
            }
        }

        return false;
    }

    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page \ WM_SHOWWINDOW

    void on_showwindow_hotkeys_handler(HWND hwnd, BOOL fShow, UINT status)
    {
        HWND listview = GetDlgItem(hwnd, IDC_LIST_HOTKEYS);

        // if being hidden, deselect listview item and hide change button
        if(!fShow) {
            int selected_item_index = ListView_GetSelectionMark(listview);
            int clear_state = 0;
            ListView_SetItemState(listview, selected_item_index, clear_state, LVIS_SELECTED | LVIS_FOCUSED);
        }
    }

    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page \ WM_NOTIFY

    int on_notify_hotkeys_handler(HWND hwnd, int idFrom, LPNMHDR nmhdr)
    {
        switch(nmhdr->idFrom) {

        case IDC_LIST_HOTKEYS: {

            switch(nmhdr->code) {

            case LVN_ITEMCHANGED: {

                LPNMLISTVIEW nm = reinterpret_cast<LPNMLISTVIEW>(nmhdr);
                if((nm->uNewState & LVIS_FOCUSED) != 0) {
                    //
                }
                break;
            } break;
            }
        }
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // HOTKEYS page \ DLGPROC

    INT_PTR hotkeys_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_INITDIALOG, on_init_hotkeys_handler);
            HANDLE_MSG(dlg, WM_SHOWWINDOW, on_showwindow_hotkeys_handler);
            HANDLE_MSG(dlg, WM_NOTIFY, on_notify_hotkeys_handler);
        }
        return setting_ctlcolor_handler(dlg, msg, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////
    // MAIN DIALOG HANDLERS
    //////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    // MAIN dialog \ WM_INITDIALOG

    BOOL on_init_main_handler(HWND hwnd, HWND hwndFocus, LPARAM lParam)
    {
        main_dialog = hwnd;

        // setup the tab pages

        uint requested_tab_resource_id = static_cast<uint>(lParam);
        int active_tab_index{ 0 };

        HWND tab_ctrl = GetDlgItem(hwnd, IDC_SETTINGS_TAB_CONTROL);

        active_tabs.clear();

        int index = 0;
        for(int i = 0; i < _countof(tabs); ++i) {

            settings_tab_t *tab = tabs + i;

            if(!should_hide_tab(tab)) {

                std::string tab_text = localize((uint64)tab->resource_id);
                TCITEMA tci;
                tci.mask = TCIF_TEXT;
                tci.pszText = const_cast<char *>(tab_text.c_str());
                TabCtrl_InsertItem(tab_ctrl, index, &tci);
                tab->index = index;
                index += 1;
                active_tabs.push_back(tab);
            }
        }

        // now the tabs are added, get the inner size of the tab control for adding child pages
        RECT tab_rect;
        GetWindowRect(tab_ctrl, &tab_rect);
        MapWindowPoints(null, hwnd, reinterpret_cast<LPPOINT>(&tab_rect), 2);
        TabCtrl_AdjustRect(tab_ctrl, false, &tab_rect);

        // create the dialog pages

        for(auto t : active_tabs) {

            if(!should_hide_tab(t)) {

                if(t->resource_id == requested_tab_resource_id) {
                    active_tab_index = t->index;
                }

                CHK_NULL(t->hwnd = CreateDialogA(app::instance, MAKEINTRESOURCE(t->resource_id), hwnd, t->handler));

                SetWindowPos(
                    t->hwnd, HWND_TOP, tab_rect.left, tab_rect.top, rect_width(tab_rect), rect_height(tab_rect), 0);
            }
        }

        TabCtrl_SetCurSel(tab_ctrl, active_tab_index);

        // center dialog in main window rect

        HWND parent;
        if((parent = GetParent(hwnd)) == NULL) {
            parent = GetDesktopWindow();
        }

        RECT parent_rect;
        GetWindowRect(parent, &parent_rect);

        RECT dlg_rect;
        GetWindowRect(hwnd, &dlg_rect);

        int x = parent_rect.left + ((rect_width(parent_rect) - rect_width(dlg_rect)) / 2);
        int y = parent_rect.top + ((rect_height(parent_rect) - rect_height(dlg_rect)) / 2);

        SetWindowPos(hwnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);

        show_settings_page(active_tab_index);

        return false;
    }

    //////////////////////////////////////////////////////////////////////
    // MAIN dialog \ WM_NOTIFY

    int on_notify_main_handler(HWND hwnd, int idFrom, LPNMHDR nmhdr)
    {
        switch(nmhdr->idFrom) {

        case IDC_SETTINGS_TAB_CONTROL:

            if(nmhdr->code == static_cast<UINT>(TCN_SELCHANGE)) {

                HWND tab_ctrl = GetDlgItem(hwnd, IDC_SETTINGS_TAB_CONTROL);
                show_settings_page(TabCtrl_GetCurSel(tab_ctrl));
            }
            break;
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////
    // MAIN dialog \ WM_COMMAND

    void on_command_main_handler(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
    {
        switch(id) {

        case app::LRESULT_LAUNCH_AS_ADMIN:
            EndDialog(hwnd, app::LRESULT_LAUNCH_AS_ADMIN);
            break;

        case IDOK:
            EndDialog(hwnd, 0);
            break;

        case IDCANCEL:
            EndDialog(hwnd, 0);
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // MAIN dialog \ DLGPROC

    INT_PTR main_dialog_handler(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch(msg) {

            HANDLE_MSG(dlg, WM_INITDIALOG, on_init_main_handler);
            HANDLE_MSG(dlg, WM_NOTIFY, on_notify_main_handler);
            HANDLE_MSG(dlg, WM_COMMAND, on_command_main_handler);
        }
        return 0;
    }

    //#pragma warning(disable : 4100)
#pragma warning(pop)
}

//////////////////////////////////////////////////////////////////////

namespace imageview
{
    LRESULT show_settings_dialog(HWND parent, uint tab_id)
    {
        current_page = -1;
        return DialogBoxParamA(
            app::instance, MAKEINTRESOURCEA(IDD_DIALOG_SETTINGS), parent, main_dialog_handler, tab_id);
    }
}