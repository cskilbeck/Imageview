//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

HRESULT get_image_size(wchar const *filename, uint32 &width, uint32 &height, uint64 &total_size);
HRESULT decode_image(byte const *bytes, size_t file_size, std::vector<byte> &pixels, uint &texture_width, uint &texture_height, uint &row_pitch);
