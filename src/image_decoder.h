//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

enum format_flags : uint
{
    without_alpha = 0,
    with_alpha = 1,
    is_default = 2,
    use_name = 4,
};

struct output_image_format
{
    GUID file_format;
    WICPixelFormatGUID pixel_format;
    format_flags flags;

    bool supports_alpha() const
    {
        return (flags & format_flags::with_alpha) == format_flags::with_alpha;
    }

    bool is_default() const
    {
        return (flags & format_flags::is_default) == format_flags::is_default;
    }

    bool use_name() const
    {
        return (flags & format_flags::use_name) == format_flags::use_name;
    }
};

extern std::map<std::string, output_image_format> image_file_formats;

//////////////////////////////////////////////////////////////////////

HRESULT check_heif_support();

HRESULT get_image_size(char const *filename, uint32 &width, uint32 &height, uint64 &total_size);

HRESULT decode_image(byte const *bytes,
                     size_t file_size,
                     std::vector<byte> &pixels,
                     uint &texture_width,
                     uint &texture_height,
                     uint &row_pitch);

HRESULT copy_pixels_as_png(byte const *pixels, uint w, uint h);

HRESULT save_image_file(char const *filename, byte const *bytes, uint width, uint height, uint pitch);
