//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

namespace imageview::image
{
    //////////////////////////////////////////////////////////////////////

    struct image_t
    {
        byte const *pixels;    // image_file::pixels vector or GlobalAlloc'ed buffer (copy/crop), just use it
        uint width;            // width in pixels
        uint height;           // height in pixels
        uint row_pitch;        // row_pitch of buffer, almost certainly redundant

        size_t size() const
        {
            return static_cast<size_t>(row_pitch) * height;
        }
    };

    //////////////////////////////////////////////////////////////////////
    // an image file that has maybe been loaded, successfully or not

    struct image_file
    {
        std::wstring filename;           // file path, use this as key for map
        std::vector<byte> bytes;         // file contents, once it has been loaded
        HRESULT hresult{ E_PENDING };    // error code or S_OK from load_file()
        int index{ -1 };                 // position in the list of files
        int view_count{ 0 };             // how many times this has been viewed since being loaded
        bool is_cache_load{ false };     // true if being loaded just for cache (don't call warm_cache when it arrives)
        std::vector<byte> pixels;        // decoded pixels from the file, format is always BGRA32
        bool is_clipboard{ false };      // is it the dummy clipboard image_file?

        image_t img{};

        bool is_decoded() const
        {
            return img.pixels != null;
        }

        size_t total_size() const
        {
            if(!is_decoded()) {
                return 0;
            }
            return bytes.size() + img.size();
        }
    };

    //////////////////////////////////////////////////////////////////////

    HRESULT create_texture(ID3D11Device *d3d_device,
                           ID3D11DeviceContext *d3d_context,
                           ID3D11Texture2D **texture,
                           ID3D11ShaderResourceView **srv,
                           image_t &image);

    //////////////////////////////////////////////////////////////////////

    enum format_flags : uint
    {
        without_alpha = 0,
        with_alpha = 1,
        is_default = 2,
        use_name = 4,
    };

    //////////////////////////////////////////////////////////////////////

    struct image_format
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

    extern std::map<std::wstring, image_format> formats;
    extern std::mutex formats_mutex;

    //////////////////////////////////////////////////////////////////////

    HRESULT is_file_extension_supported(std::wstring const &extension, bool &is_supported);

    HRESULT check_heif_support();

    HRESULT get_size(std::wstring const &filename, uint32 &width, uint32 &height, uint64 &total_size);

    HRESULT decode(image_file *file);

    HRESULT copy_pixels_as_png(byte const *pixels, uint w, uint h);

    HRESULT save(std::wstring const &filename, byte const *bytes, uint width, uint height, uint pitch);
}
