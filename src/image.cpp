//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////
// create a d3d texture for an image

HRESULT image::create_texture(ID3D11Device *d3d_device,
                              ID3D11DeviceContext *d3d_context,
                              ID3D11Texture2D **texture,
                              ID3D11ShaderResourceView **srv)
{
    if(d3d_device == null || d3d_context == null || texture == null || srv == null) {
        return E_INVALIDARG;
    }

    // object state
    if(pixels == null || width == 0 || height == 0 || row_pitch == 0) {
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }

    DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;

    // See if format supports auto-gen mipmaps (varies by feature level, but guaranteed for
    // BGRA32/D3D11 which we're using so this is kind of redundant)

    bool autogen = false;
    UINT format_support = 0;
    if(SUCCEEDED(d3d_device->CheckFormatSupport(format, &format_support))) {
        if((format_support & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN) != 0) {
            autogen = true;
        }
    }

    // Create texture

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1u;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = desc.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = 1u;

    D3D11_SUBRESOURCE_DATA initData;
    initData.pSysMem = pixels;
    initData.SysMemPitch = row_pitch;
    initData.SysMemSlicePitch = (uint)size();

    D3D11_SUBRESOURCE_DATA *id = &initData;

    if(autogen) {
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
        desc.MipLevels = 0u;
        SRVDesc.Texture2D.MipLevels = (uint)-1;
        id = null;
    }

    ComPtr<ID3D11Texture2D> tex;
    CHK_HR(d3d_device->CreateTexture2D(&desc, id, &tex));

    ComPtr<ID3D11ShaderResourceView> texture_view;
    CHK_HR(d3d_device->CreateShaderResourceView(tex.Get(), &SRVDesc, &texture_view));

    if(autogen) {
        d3d_context->UpdateSubresource(tex.Get(), 0, null, pixels, row_pitch, (uint)size());
        d3d_context->GenerateMips(texture_view.Get());
    }

    *texture = tex.Detach();
    *srv = texture_view.Detach();

    return S_OK;
}
