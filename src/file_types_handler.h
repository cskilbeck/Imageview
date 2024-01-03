#pragma once

namespace imageview
{
    HRESULT check_filetype_handler(bool &is_installed);
    HRESULT install_filetype_handler();
    HRESULT remove_filetype_handler();
}
