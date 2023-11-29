//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

struct output_image_format
{
    GUID file_format;
    WICPixelFormatGUID pixel_format;
    bool alpha;
};

enum alpha_supported
{
    without_alpha = 0,
    with_alpha = 1
};

extern std::map<std::wstring, output_image_format> save_formats;

//////////////////////////////////////////////////////////////////////

HRESULT get_image_size(wchar const *filename, uint32 &width, uint32 &height, uint64 &total_size);

HRESULT decode_image(byte const *bytes,
                     size_t file_size,
                     std::vector<byte> &pixels,
                     uint &texture_width,
                     uint &texture_height,
                     uint &row_pitch);

HRESULT copy_pixels_as_png(byte const *pixels, uint w, uint h);

HRESULT save_image_file(wchar_t const *filename, byte const *bytes, uint width, uint height, uint pitch);
