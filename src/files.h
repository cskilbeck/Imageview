#pragma once

//////////////////////////////////////////////////////////////////////

HRESULT load_file(std::wstring filename, std::vector<byte> &buffer, HANDLE cancel_event = null);

HRESULT file_get_full_path(wchar const *filename, std::wstring &fullpath);
HRESULT file_get_path(wchar const *filename, std::wstring &path);
HRESULT file_get_filename(wchar const *filename, std::wstring &name);
HRESULT file_get_extension(wchar const *filename, std::wstring &extension);
HRESULT file_get_size(wchar const *filename, uint64_t &size);
BOOL file_exists(wchar const *name);

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
    file_info(std::wstring const &n, uint64 d) throw() : name(n), date(d)
    {
    }

    std::wstring name;
    uint64 date;
};

struct folder_scan_result
{
    std::wstring path;
    std::vector<file_info> files;
};

HRESULT scan_folder2(wchar const *path,
                     std::vector<wchar const *> extensions,
                     scan_folder_sort_field sort_field,
                     scan_folder_sort_order order,
                     folder_scan_result **result,
                     HANDLE cancel_event);
