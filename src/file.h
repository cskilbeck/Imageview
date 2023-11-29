#pragma once

//////////////////////////////////////////////////////////////////////

namespace file
{
    HRESULT load(std::string const &filename, std::vector<byte> &buffer, HANDLE cancel_event = null);

    HRESULT get_full_path(std::string const &filename, std::string &fullpath);
    HRESULT get_path(std::string const &filename, std::string &path);
    HRESULT get_filename(std::string const &filename, std::string &name);
    HRESULT get_extension(std::string const &filename, std::string &extension);
    HRESULT get_size(std::string const &filename, uint64_t &size);
    BOOL exists(std::string const &name);

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
        info(std::string const &n, uint64 d) throw() : name(n), date(d)
        {
        }

        std::string name;
        uint64 date;
    };

    struct folder_scan_result
    {
        std::string path;
        std::vector<info> files;
    };

    HRESULT scan_folder2(std::string const &path,
                         std::vector<std::string> const &extensions,
                         scan_folder_sort_field sort_field,
                         scan_folder_sort_order order,
                         folder_scan_result **result,
                         HANDLE cancel_event);
}
