//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

namespace imageview::image
{
    //////////////////////////////////////////////////////////////////////

    struct filetypes
    {
        // file extension -> container format GUID
        std::map<std::wstring, GUID> container_formats;

        // file type friendly name -> filter spec (e.g. "BMP Files" -> "*.bmp;*.dib")
        std::map<std::wstring, std::wstring> filter_specs;

        // filter_specs for load/save dialogs
        std::vector<COMDLG_FILTERSPEC> comdlg_filterspecs;

        // default file type (PNG)
        uint default_index;
    };

    extern filetypes save_filetypes;
    extern filetypes load_filetypes;

    //////////////////////////////////////////////////////////////////////
    // a raw BGRA32 decoded image

    struct image_t
    {
        byte const *pixels;    // points at image_file::pixels vector or GlobalAlloc'ed buffer (copy/crop), just use it
        uint width;            // width in pixels
        uint height;           // height in pixels
        uint row_pitch;        // row_pitch of buffer, almost certainly redundant

        size_t size() const
        {
            return static_cast<size_t>(row_pitch) * height;
        }

        byte const *get_pixel(uint x, uint y) const
        {
            return pixels + row_pitch * y + x * 4llu;
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

    HRESULT can_load_file_extension(std::wstring const &extension, bool &is_supported);

    HRESULT init_filetypes();

    HRESULT get_size(std::wstring const &filename, uint32 &width, uint32 &height, uint64 &total_size);

    HRESULT decode(image_file *file);

    HRESULT copy_pixels_as_png(byte const *pixels, uint w, uint h);

    HRESULT save(std::wstring const &filename, byte const *bytes, uint width, uint height, uint pitch);
}
