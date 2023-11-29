//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

namespace image
{
    struct image_t
    {
        byte const
            *pixels;       // could be pointing into a image_file::pixels vector or a GlobalAlloc'ed buffer (in case of
                           // copy or crop), just use it
        uint width;        // width in pixels
        uint height;       // height in pixels
        uint row_pitch;    // row_pitch of buffer, almost certainly redundant

        size_t size() const
        {
            return row_pitch * height;
        }

        HRESULT create_texture(ID3D11Device *d3d_device,
                               ID3D11DeviceContext *d3d_context,
                               ID3D11Texture2D **texture,
                               ID3D11ShaderResourceView **srv);
    };

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

    extern std::map<std::string, output_image_format> file_formats;

    //////////////////////////////////////////////////////////////////////

    HRESULT check_heif_support();

    HRESULT get_size(std::string const &filename, uint32 &width, uint32 &height, uint64 &total_size);

    HRESULT decode(byte const *bytes,
                   size_t file_size,
                   std::vector<byte> &pixels,
                   uint &texture_width,
                   uint &texture_height,
                   uint &row_pitch);

    HRESULT copy_pixels_as_png(byte const *pixels, uint w, uint h);

    HRESULT save(std::string const &filename, byte const *bytes, uint width, uint height, uint pitch);
}
