//////////////////////////////////////////////////////////////////////
// Always decodes to 32 bit BGRA
// Handles exif rotate transforms
// Doesn't create D3D texture, just gets the pixels (create texture in main thread)
// Using D3D Feature Level 11 allows non-square, non-power-of-2 texture sizes up to 16384

#include "pch.h"

LOG_CONTEXT("image_decoder");

//////////////////////////////////////////////////////////////////////

namespace
{
    struct size_u
    {
        uint w;
        uint h;
    };

    //////////////////////////////////////////////////////////////////////

    // d3d11 specifies 16384 as max texture dimension

    uint max_texture_size = 16384;

    // output format is fixed so this is too

    uint64 constexpr bits_per_pixel = 32llu;

    //////////////////////////////////////////////////////////////////////
    // note exif is counter clockwise, WIC is clockwise, so 90/270 swapped
    // also flip/transpose/transverse not supported, but seem to be rarely used

    struct exif_transform_translator_t
    {
        WICBitmapTransformOptions opt;
        uint32 orientation;
    };

    exif_transform_translator_t exif_transform_translation[] = {
        //
        { WICBitmapTransformRotate0, PHOTO_ORIENTATION_NORMAL },
        { WICBitmapTransformRotate90, PHOTO_ORIENTATION_ROTATE270 },
        { WICBitmapTransformRotate180, PHOTO_ORIENTATION_ROTATE180 },
        { WICBitmapTransformRotate270, PHOTO_ORIENTATION_ROTATE90 },
        { WICBitmapTransformFlipHorizontal, PHOTO_ORIENTATION_FLIPHORIZONTAL },
        { WICBitmapTransformFlipVertical, PHOTO_ORIENTATION_FLIPVERTICAL }
    };

    WICBitmapTransformOptions convert_exif_to_wic_transform(uint32 exif)
    {
        for(auto const &t : exif_transform_translation) {
            if(t.orientation == exif) {
                return t.opt;
            }
        }
        return WICBitmapTransformRotate0;
    }

    //////////////////////////////////////////////////////////////////////
    // wic factory admin

    INIT_ONCE init_wic_once = INIT_ONCE_STATIC_INIT;
    IWICImagingFactory *wic_factory = null;

    BOOL WINAPI init_wic_factory(PINIT_ONCE, PVOID, PVOID *ifactory) noexcept
    {
        return SUCCEEDED(CoCreateInstance(
            CLSID_WICImagingFactory, null, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory), ifactory));
    }

    //////////////////////////////////////////////////////////////////////

    IWICImagingFactory *get_wic() noexcept
    {
        if(!InitOnceExecuteOnce(&init_wic_once, init_wic_factory, null, reinterpret_cast<LPVOID *>(&wic_factory))) {

            return null;
        }
        return wic_factory;
    }

    //////////////////////////////////////////////////////////////////////
    // assumes pitch can be byte aligned? probably doesn't matter in
    // this case because bpp == 32 so will be dword aligned which probly fine

    uint64 bytes_per_row(uint width)
    {
        return (width * bits_per_pixel + 7u) / 8u;
    }

    //////////////////////////////////////////////////////////////////////
    // get new width and height where neither exceeds max_texture_size

    size_u constrain_dimensions(size_u const &s)
    {
        float f = std::min(1.0f, (float)max_texture_size / std::max(s.w, s.h));
        return { (uint)(s.w * f), (uint)(s.h * f) };
    }
}

namespace imageview::image
{
    std::mutex formats_mutex;

    // TODO(chs): get this table of image formats from WIC

    std::map<std::wstring, image_format> formats{

        { L"PNG", { GUID_ContainerFormatPng, GUID_WICPixelFormat32bppBGRA, format_flags{ with_alpha | is_default } } },
        { L"JPEG",
          { GUID_ContainerFormatJpeg, GUID_WICPixelFormat24bppRGB, format_flags{ without_alpha | use_name } } },
        { L"JPG", { GUID_ContainerFormatJpeg, GUID_WICPixelFormat24bppRGB, format_flags{ without_alpha } } },
        { L"BMP", { GUID_ContainerFormatBmp, GUID_WICPixelFormat24bppRGB, format_flags{ without_alpha } } },
        { L"TIFF", { GUID_ContainerFormatTiff, GUID_WICPixelFormat32bppBGRA, format_flags{ with_alpha } } },
        { L"ICO", { GUID_ContainerFormatIco, GUID_WICPixelFormat24bppRGB, format_flags{ with_alpha } } },
        { L"DNG", { GUID_ContainerFormatAdng, GUID_WICPixelFormat24bppRGB, format_flags{ without_alpha } } },
        { L"WMP", { GUID_ContainerFormatWmp, GUID_WICPixelFormat24bppRGB, format_flags{ without_alpha } } },
        { L"WEBP", { GUID_ContainerFormatWebp, GUID_WICPixelFormat24bppRGB, format_flags{ with_alpha } } },
        { L"RAW", { GUID_ContainerFormatRaw, GUID_WICPixelFormat24bppRGB, format_flags{ without_alpha } } },
        { L"DDS", { GUID_ContainerFormatDds, GUID_WICPixelFormat32bppBGRA, format_flags{ with_alpha } } },
    };

    //////////////////////////////////////////////////////////////////////
    // add heif file support if it's installed
    // only need to do this before showing a load/save dialog...

    HRESULT check_heif_support()
    {
        CHK_HR(MFStartup(MF_VERSION));
        DEFER(MFShutdown());

        IMFActivate **activate{};
        uint32 count{};

        MFT_REGISTER_TYPE_INFO input;
        input.guidMajorType = MFMediaType_Video;
        input.guidSubtype = MFVideoFormat_HEVC;

        CHK_HR(MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, MFT_ENUM_FLAG_SYNCMFT, &input, null, &activate, &count));
        DEFER(CoTaskMemFree(activate));

        for(uint32 i = 0; i < count; i++) {
            activate[i]->Release();
        }

        if(count > 0) {

            LOG_INFO(L"HEIF support is enabled");

            auto iflock{ std::lock_guard(formats_mutex) };

            image::formats[L"HEIF"] = { GUID_ContainerFormatHeif,
                                        GUID_WICPixelFormat32bppBGRA,
                                        format_flags{ with_alpha | use_name } };

            image::formats[L"HEIC"] = { GUID_ContainerFormatHeif,
                                        GUID_WICPixelFormat32bppBGRA,
                                        format_flags{ with_alpha } };
        }

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // get width, height in pixels and size of output in bytes for an image file

    HRESULT get_size(std::wstring const &filename, uint32 &width, uint32 &height, uint64 &total_size)
    {
        auto wic = get_wic();

        if(wic == null) {
            return E_NOINTERFACE;
        }

        // it does have to open the file which might be very slow under certain
        // conditions but that's ok, this is all happening in a thread

        ComPtr<IWICBitmapDecoder> decoder;
        CHK_HR(wic->CreateDecoderFromFilename(
            filename.c_str(), null, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder));

        // assumption here is that GetFrame() does _NOT_ actually decode the pixels...
        // that's what the docs seem to say, but... well, here's hoping

        ComPtr<IWICBitmapFrameDecode> frame;
        CHK_HR(decoder->GetFrame(0, &frame));

        size_u s;

        CHK_HR(frame->GetSize(&s.w, &s.h));

        s = constrain_dimensions(s);
        width = s.w;
        height = s.h;
        total_size = bytes_per_row(s.w) * s.h;

        return S_OK;
    }

    /////////////////////////////////////////////////////////////////////
    // copy image as a png to the clipboard

    HRESULT copy_pixels_as_png(byte const *pixels, uint w, uint h)
    {
        auto wic = get_wic();

        if(wic == null) {
            return E_NOINTERFACE;
        }

        ComPtr<IWICBitmapEncoder> png_encoder;
        CHK_HR(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, png_encoder.GetAddressOf()));

        ComPtr<IStream> stream;

        stream.Attach(SHCreateMemStream(nullptr, 0));

        if(!stream) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        png_encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);

        ComPtr<IPropertyBag2> property_bag;
        ComPtr<IWICBitmapFrameEncode> frame;

        CHK_HR(png_encoder->CreateNewFrame(frame.GetAddressOf(), property_bag.GetAddressOf()));

        CHK_HR(frame->Initialize(property_bag.Get()));

        CHK_HR(frame->SetSize(w, h));

        auto format = GUID_WICPixelFormat32bppBGRA;
        auto requested_format = format;

        // encoder sets format to the best one it can manage
        CHK_HR(frame->SetPixelFormat(&format));

        if(format != requested_format) {
            LOG_ERROR(L"Can't encode as PNG, format not supported");
            return E_FAIL;
        }

        UINT stride = static_cast<UINT>(bytes_per_row(w));
        UINT img_size = stride * h;
        CHK_HR(frame->WritePixels(h, stride, img_size, const_cast<BYTE *>(pixels)));

        CHK_HR(frame->Commit());
        CHK_HR(png_encoder->Commit());

        uint64_t stream_size;

        CHK_HR(stream->Seek({ 0 }, STREAM_SEEK_END, reinterpret_cast<ULARGE_INTEGER *>(&stream_size)));
        CHK_HR(stream->Seek({ 0 }, STREAM_SEEK_SET, nullptr));

        HANDLE hData = GlobalAlloc(GHND | GMEM_SHARE, stream_size);

        if(hData == null) {
            return HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);
        }
        byte *pData = reinterpret_cast<byte *>(GlobalLock(hData));

        if(pData == null) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        ULONG got;
        CHK_HR(stream->Read(pData, static_cast<ULONG>(stream_size), &got));
        if(got != stream_size) {
            return E_FAIL;
        }

        GlobalUnlock(hData);

        CHK_BOOL(SetClipboardData(RegisterClipboardFormatA("PNG"), hData));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // Decode an image into a pixel buffer and get dimensions

    HRESULT decode(image_file *file)
    {
        LOG_DEBUG(L"DECODE {}", file->filename);

        byte const *bytes = file->bytes.data();
        size_t file_size = file->bytes.size();

        if(bytes == null || file_size == 0) {
            return E_INVALIDARG;
        }

        if(file_size > UINT32_MAX) {
            return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        }

        auto wic = get_wic();

        if(!wic) {
            return E_NOINTERFACE;
        }

        // get frame decoder

        ComPtr<IWICStream> stream;
        CHK_HR(wic->CreateStream(&stream));

        CHK_HR(stream->InitializeFromMemory(const_cast<byte *>(bytes), (DWORD)file_size));

        ComPtr<IWICBitmapDecoder> decoder;
        WICDecodeOptions options = WICDecodeMetadataCacheOnDemand;
        CHK_HR(wic->CreateDecoderFromStream(stream.Get(), null, options, &decoder));

        ComPtr<IWICBitmapFrameDecode> frame;
        CHK_HR(decoder->GetFrame(0, &frame));

        // get EXIF tag for image orientation

        WICBitmapTransformOptions transform = WICBitmapTransformRotate0;

        ComPtr<IWICMetadataQueryReader> mqr;
        if(SUCCEEDED(frame->GetMetadataQueryReader(&mqr))) {

            wchar const *orientation_flag = L"/app1/ifd/{ushort=274}";

            PROPVARIANT var;
            PropVariantInit(&var);

            if(SUCCEEDED(mqr->GetMetadataByName(orientation_flag, &var))) {
                if(var.vt == VT_UI2) {
                    transform = convert_exif_to_wic_transform(var.uiVal);
                }
            }
        }

        // line up any necessary transforms:

        // first upcast frame decoder to a bitmap source

        ComPtr<IWICBitmapSource> bmp_src = frame.Detach();

        // 1. rescale if src image exceeds max_texture_size

        size_u src_size;

        CHK_HR(bmp_src->GetSize(&src_size.w, &src_size.h));

        size_u dst_size = constrain_dimensions(src_size);

        if(dst_size.w != src_size.w || dst_size.h != src_size.h) {

            ComPtr<IWICBitmapScaler> scaler;
            CHK_HR(wic->CreateBitmapScaler(&scaler));

            WICBitmapInterpolationMode interp_mode = WICBitmapInterpolationModeFant;
            CHK_HR(scaler->Initialize(bmp_src.Get(), dst_size.w, dst_size.h, interp_mode));

            bmp_src.Attach(scaler.Detach());
        }

        // 2. convert pixel format if necessary

        // Force 32 bpp BGRA dest format
        WICPixelFormatGUID dst_format = GUID_WICPixelFormat32bppBGRA;

        // get source pixel format
        WICPixelFormatGUID src_format;
        CHK_HR(bmp_src->GetPixelFormat(&src_format));

        if(dst_format != src_format) {

            ComPtr<IWICFormatConverter> fmt_converter;
            CHK_HR(wic->CreateFormatConverter(&fmt_converter));

            // some formats have > 4 channels, in which case we're out of luck

            BOOL can_convert = FALSE;
            CHK_HR(fmt_converter->CanConvert(src_format, dst_format, &can_convert));
            if(!can_convert) {
                return HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE);
            }

            WICBitmapDitherType dither = WICBitmapDitherTypeErrorDiffusion;
            WICBitmapPaletteType palette = WICBitmapPaletteTypeMedianCut;
            CHK_HR(fmt_converter->Initialize(bmp_src.Get(), dst_format, dither, null, 0, palette));

            bmp_src.Attach(fmt_converter.Detach());
        }

        // 3. apply exif transform if necessary

        if(transform != WICBitmapTransformRotate0) {

            // pre-decode the pixels as recommended here
            // https://docs.microsoft.com/en-us/windows/win32/api/wincodec/nn-wincodec-iwicbitmapfliprotator

            // "IWICBitmapFipRotator requests data on a per-pixel basis, while WIC codecs provide data
            // on a per-scanline basis. This causes the fliprotator object to exhibit n² behavior if
            // there is no buffering. This occurs because each pixel in the transformed image requires
            // an entire scanline to be decoded in the file. It is recommended that you buffer the image
            // using IWICBitmap, or flip/rotate the image using Direct2D."

            // TODO(chs): ditch this and just maintain the transform as a member of the image struct,
            // although that makes drawing/copying from it more of a hassle

            uint64 pitch = bytes_per_row(dst_size.w);

            if(pitch * dst_size.h >= UINT32_MAX) {
                return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
            }

            ComPtr<IWICBitmap> decoded_src;
            CHK_HR(wic_factory->CreateBitmap(dst_size.w, dst_size.h, dst_format, WICBitmapCacheOnDemand, &decoded_src));

            // decode the image as-is into decoded_src
            {
                ComPtr<IWICBitmapLock> lock;
                CHK_HR(decoded_src->Lock(null, WICBitmapLockWrite, &lock));

                uint buffer_size;
                WICInProcPointer data;
                CHK_HR(lock->GetDataPointer(&buffer_size, &data));

                assert(data != null);    // satisfy the analyzer...

                CHK_HR(bmp_src->CopyPixels(null, (uint32)pitch, buffer_size, data));
            }

            // add in the transformer from the decoded pixels

            ComPtr<IWICBitmapFlipRotator> flip_rotater;
            CHK_HR(wic->CreateBitmapFlipRotator(&flip_rotater));

            flip_rotater->Initialize(decoded_src.Get(), transform);

            bmp_src.Attach(flip_rotater.Detach());
        }

        // transforms are all lined up, get final size because rotate might have swapped width, height

        CHK_HR(bmp_src->GetSize(&dst_size.w, &dst_size.h));

        if(dst_size.w == 0 || dst_size.h == 0) {
            return E_UNEXPECTED;
        }

        // allocate output buffer

        uint64 t_row_pitch = bytes_per_row(dst_size.w);
        uint64 total_bytes = t_row_pitch * dst_size.h;

        if(total_bytes > UINT32_MAX) {
            return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
        }

        file->pixels.resize((size_t)total_bytes);

        // if CopyPixels fails, free the buffer on exit

        auto release_pixels = defer::deferred([&]() { file->pixels.clear(); });

        // actually get the pixels

        CHK_HR(bmp_src->CopyPixels(null, (uint32)t_row_pitch, (uint32)total_bytes, file->pixels.data()));

        // don't free the buffer now, it's full of good stuff

        release_pixels.cancel();

        // return dimensions to caller

        file->img.width = dst_size.w;
        file->img.height = dst_size.h;
        file->img.row_pitch = (uint32)t_row_pitch;

        file->img.pixels = file->pixels.data();

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT save(std::wstring const &filename, byte const *bytes, uint width, uint height, uint pitch)
    {
        std::wstring extension;
        CHK_HR(file::get_extension(filename, extension));

        if(extension[0] == L'.') {
            extension = extension.substr(1);
        }

        extension = make_uppercase(extension);

        decltype(image::formats.begin()) found;

        {
            auto iflock{ std::lock_guard(formats_mutex) };
            found = image::formats.find(extension);
        }

        if(found == image::formats.end()) {
            return HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE);
        }

        image_format const &format = found->second;

        auto wic = get_wic();

        if(wic == null) {
            return E_NOINTERFACE;
        }

        ComPtr<IWICStream> file_stream;
        CHK_HR(wic->CreateStream(&file_stream));
        CHK_HR(file_stream->InitializeFromFilename(filename.c_str(), GENERIC_WRITE));

        ComPtr<IWICBitmapEncoder> encoder;
        CHK_HR(wic->CreateEncoder(format.file_format, NULL, &encoder));
        CHK_HR(encoder->Initialize(file_stream.Get(), WICBitmapEncoderNoCache));

        ComPtr<IWICBitmapFrameEncode> frame;
        ComPtr<IPropertyBag2> property_bag;
        CHK_HR(encoder->CreateNewFrame(&frame, &property_bag));
        CHK_HR(frame->Initialize(NULL));
        CHK_HR(frame->SetSize(width, height));

        WICPixelFormatGUID pixel_format = format.pixel_format;

        CHK_HR(frame->SetPixelFormat(&pixel_format));

        // create a new wicbitmap using the new pixel format

        uint dst_row_pitch = width * sizeof(uint32);
        uint64 dst_size = (uint64)dst_row_pitch * height;
        if(dst_size > UINT_MAX) {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }
        std::vector<byte> buffer(dst_size);
        byte *dst = buffer.data();
        byte const *src = bytes;

        if(format.supports_alpha()) {

            for(uint y = 0; y < height; ++y) {

                memcpy(dst, src, dst_row_pitch);
                dst += dst_row_pitch;
                src += pitch;
            }

        } else {

            for(uint y = 0; y < height; ++y) {

                uint32 const *src_pixel = reinterpret_cast<uint32 const *>(src);
                byte *dst_pixel = dst;
                for(uint x = 0; x < width; ++x) {
                    uint32 pixel = *src_pixel++;
                    byte red = pixel & 0xff;
                    byte grn = (pixel >> 8) & 0xff;
                    byte blu = (pixel >> 16) & 0xff;
                    *dst_pixel++ = red;
                    *dst_pixel++ = grn;
                    *dst_pixel++ = blu;
                }

                dst += dst_row_pitch;
                src += pitch;
            }
        }

        CHK_HR(frame->WritePixels(height, dst_row_pitch, (uint)dst_size, buffer.data()));
        CHK_HR(frame->Commit());
        CHK_HR(encoder->Commit());

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // create a d3d texture for an image

    HRESULT create_texture(ID3D11Device *d3d_device,
                           ID3D11DeviceContext *d3d_context,
                           ID3D11Texture2D **texture,
                           ID3D11ShaderResourceView **srv,
                           image_t &image)
    {
        if(d3d_device == null || d3d_context == null || texture == null || srv == null) {
            return E_INVALIDARG;
        }

        // object state
        if(image.pixels == null || image.width == 0 || image.height == 0 || image.row_pitch == 0) {
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
        desc.Width = image.width;
        desc.Height = image.height;
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
        initData.pSysMem = image.pixels;
        initData.SysMemPitch = image.row_pitch;
        initData.SysMemSlicePitch = (uint)image.size();

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
            d3d_context->UpdateSubresource(tex.Get(), 0, null, image.pixels, image.row_pitch, (uint)image.size());
            d3d_context->GenerateMips(texture_view.Get());
        }

        *texture = tex.Detach();
        *srv = texture_view.Detach();

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT is_file_extension_supported(std::wstring const &extension, bool &is_supported)
    {
        std::wstring ext;

        if(extension[0] == L'.') {

            ext = extension.substr(1);

        } else {

            ext = extension;
        }

        ext = make_uppercase(ext);

        decltype(image::formats.begin()) found;

        {
            auto iflock{ std::lock_guard(formats_mutex) };
            found = image::formats.find(ext);
        }

        is_supported = found != image::formats.end();

        return S_OK;
    }
}
