#pragma once

// when should it refresh the recent files list?

namespace imageview::recent_files
{
    void init();

    void wait_for_recent_files();

    void add(std::wstring const &filename, FILETIME const &access_time);

    HRESULT get_files(std::vector<std::wstring> &filenames);
};
