#pragma once

// when should it refresh the recent files list?

namespace imageview::recent_files
{
    void init();

    HRESULT get_files(std::vector<std::wstring> &filenames);

    HRESULT get_full_path(uint index);
};
