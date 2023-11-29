#pragma once

//////////////////////////////////////////////////////////////////////

HRESULT load_file(std::string const &filename, std::vector<byte> &buffer, HANDLE cancel_event = null);

HRESULT file_get_full_path(char const *filename, std::string &fullpath);
HRESULT file_get_path(char const *filename, std::string &path);
HRESULT file_get_filename(char const *filename, std::string &name);
HRESULT file_get_extension(char const *filename, std::string &extension);
HRESULT file_get_size(char const *filename, uint64_t &size);
BOOL file_exists(char const *name);

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

struct file_info
{
    file_info(std::string const &n, uint64 d) throw() : name(n), date(d)
    {
    }

    std::string name;
    uint64 date;
};

struct folder_scan_result
{
    std::string path;
    std::vector<file_info> files;
};

HRESULT scan_folder2(char const *path,
                     std::vector<char const *> extensions,
                     scan_folder_sort_field sort_field,
                     scan_folder_sort_order order,
                     folder_scan_result **result,
                     HANDLE cancel_event);
