//////////////////////////////////////////////////////////////////////
// Always decodes to 32 bit BGRA
// Always ignores SRGB
// Handles exif rotate transforms
// Doesn't create D3D texture, just gets the pixels (create texture in main thread)
// Using D3D Feature Level 11 allows non-square, non-power-of-2 texture sizes up to 16384

#include "pch.h"

//////////////////////////////////////////////////////////////////////

// TODO (chs): use this table to set up the save dialog file types filter

std::map<std::wstring, output_image_format> save_formats{
    { L"PNG", { GUID_ContainerFormatPng, GUID_WICPixelFormat32bppBGRA, with_alpha } },
    { L"JPEG", { GUID_ContainerFormatJpeg, GUID_WICPixelFormat24bppRGB, without_alpha } },
    { L"JPG", { GUID_ContainerFormatJpeg, GUID_WICPixelFormat24bppRGB, without_alpha } },
    { L"BMP", { GUID_ContainerFormatBmp, GUID_WICPixelFormat24bppRGB, without_alpha } },
    { L"TIFF", { GUID_ContainerFormatTiff, GUID_WICPixelFormat32bppBGRA, with_alpha } }
};

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

//////////////////////////////////////////////////////////////////////
// get width, height in pixels and size of output in bytes for an image file

HRESULT get_image_size(wchar const *filename, uint32 &width, uint32 &height, uint64 &total_size)
{
    auto wic = get_wic();

    if(wic == null) {
        return E_NOINTERFACE;
    }

    // it does have to open the file which might be very slow under certain
    // conditions but that's ok, this is all happening in a thread

    ComPtr<IWICBitmapDecoder> decoder;
    CHK_HR(wic->CreateDecoderFromFilename(filename, null, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder));

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
        Log(L"Can't encode as PNG, format not supported");
        return E_FAIL;
    }

    UINT stride = static_cast<UINT>(bytes_per_row(w));
    UINT img_size = stride * h;
    CHK_HR(frame->WritePixels(h, stride, img_size, const_cast<BYTE *>(pixels)));

    frame->Commit();
    png_encoder->Commit();

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

    ComPtr<IDataObject> data_object;

    CHK_BOOL(SetClipboardData(RegisterClipboardFormat(TEXT("PNG")), hData));

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// Decode an image into a pixel buffer and get dimensions

HRESULT decode_image(byte const *bytes,
                     size_t file_size,
                     std::vector<byte> &pixels,
                     uint &texture_width,
                     uint &texture_height,
                     uint &row_pitch)
{
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

    pixels.resize((size_t)total_bytes);

    // if CopyPixels fails, free the buffer on exit

    auto release_pixels = deferred([&]() { pixels.clear(); });

    // actually get the pixels

    CHK_HR(bmp_src->CopyPixels(null, (uint32)t_row_pitch, (uint32)total_bytes, &pixels[0]));

    // don't free the buffer now, it's full of good stuff

    release_pixels.cancel();

    // return dimensions to caller

    texture_width = dst_size.w;
    texture_height = dst_size.h;
    row_pitch = (uint32)t_row_pitch;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT save_image_file(wchar_t const *filename, byte const *bytes, uint width, uint height, uint pitch)
{
    std::wstring extension;
    CHK_HR(file_get_extension(filename, extension));

    if(extension[0] == L'.') {
        extension = extension.substr(1);
    }

    make_uppercase(extension);

    auto found = save_formats.find(extension);

    if(found == save_formats.end()) {
        return ERROR_UNSUPPORTED_TYPE;
    }

    output_image_format const &format = found->second;

    auto wic = get_wic();

    if(wic == null) {
        return E_NOINTERFACE;
    }

    ComPtr<IWICStream> file_stream;
    CHK_HR(wic->CreateStream(&file_stream));
    CHK_HR(file_stream->InitializeFromFilename(filename, GENERIC_WRITE));

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
        return ERROR_NOT_SUPPORTED;
    }
    std::vector<byte> buffer(dst_size);
    byte *dst = buffer.data();
    byte const *src = bytes;

    if(format.alpha == alpha_supported::with_alpha) {
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
