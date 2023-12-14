#pragma once

//////////////////////////////////////////////////////////////////////

namespace imageview::file
{
    HRESULT load(std::wstring const &filename, std::vector<byte> &buffer, HANDLE cancel_event = null);
    HRESULT get_full_path(std::wstring const &filename, std::wstring &fullpath);
    HRESULT get_path(std::wstring const &filename, std::wstring &path);
    HRESULT get_filename(std::wstring const &filename, std::wstring &name);
    HRESULT get_extension(std::wstring const &filename, std::wstring &extension);
    HRESULT get_size(std::wstring const &filename, uint64_t &size);
    HRESULT paths_are_different(std::wstring const &a, std::wstring const &b, bool &differ);
    BOOL exists(std::wstring const &name);

    //////////////////////////////////////////////////////////////////////

    enum class scan_folder_sort_field
    {
        name,
        date,
    };

    enum class scan_folder_sort_order
    {
        ascending,
        descending
    };

    struct info
    {
        info(std::wstring const &n, uint64 d) throw() : name(n), date(d)
        {
        }

        std::wstring name;
        uint64 date;
    };

    struct folder_scan_result
    {
        std::wstring path;
        std::vector<info> files;
    };

    HRESULT scan_folder(std::wstring const &path,
                        std::vector<std::wstring> const &extensions,
                        scan_folder_sort_field sort_field,
                        scan_folder_sort_order order,
                        folder_scan_result **result,
                        HANDLE cancel_event);
}
