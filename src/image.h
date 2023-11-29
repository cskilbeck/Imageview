#pragma once

struct image
{
    byte const *pixels;    // could be pointing into a image_file::pixels vector or a GlobalAlloc'ed buffer (in case of
                           // copy or crop), just use it
    uint width;            // width in pixels
    uint height;           // height in pixels
    uint row_pitch;        // row_pitch of buffer, almost certainly redundant

    size_t size() const
    {
        return row_pitch * height;
    }

    HRESULT create_texture(ID3D11Device *d3d_device,
                           ID3D11DeviceContext *d3d_context,
                           ID3D11Texture2D **texture,
                           ID3D11ShaderResourceView **srv);
};
