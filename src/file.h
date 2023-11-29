#pragma once

//////////////////////////////////////////////////////////////////////

namespace file
{
    HRESULT load(std::string const &filename, std::vector<byte> &buffer, HANDLE cancel_event = null);

    HRESULT get_full_path(char const *filename, std::string &fullpath);
    HRESULT get_path(char const *filename, std::string &path);
    HRESULT get_filename(char const *filename, std::string &name);
    HRESULT get_extension(char const *filename, std::string &extension);
    HRESULT get_size(char const *filename, uint64_t &size);
    BOOL exists(char const *name);

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

    HRESULT scan_folder2(char const *path,
                         std::vector<char const *> extensions,
                         scan_folder_sort_field sort_field,
                         scan_folder_sort_order order,
                         folder_scan_result **result,
                         HANDLE cancel_event);
}
