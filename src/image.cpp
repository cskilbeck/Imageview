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

    //////////////////////////////////////////////////////////////////////

    using imageview::GUID_compare;

    std::map<GUID const, wchar const *, GUID_compare> pixel_format_names{
        { GUID_WICPixelFormatDontCare, L"DontCare" },
        { GUID_WICPixelFormat1bppIndexed, L"1bppIndexed" },
        { GUID_WICPixelFormat2bppIndexed, L"2bppIndexed" },
        { GUID_WICPixelFormat4bppIndexed, L"4bppIndexed" },
        { GUID_WICPixelFormat8bppIndexed, L"8bppIndexed" },
        { GUID_WICPixelFormatBlackWhite, L"BlackWhite" },
        { GUID_WICPixelFormat2bppGray, L"2bppGray" },
        { GUID_WICPixelFormat4bppGray, L"4bppGray" },
        { GUID_WICPixelFormat8bppGray, L"8bppGray" },
        { GUID_WICPixelFormat8bppAlpha, L"8bppAlpha" },
        { GUID_WICPixelFormat16bppBGR555, L"16bppBGR555" },
        { GUID_WICPixelFormat16bppBGR565, L"16bppBGR565" },
        { GUID_WICPixelFormat16bppBGRA5551, L"16bppBGRA5551" },
        { GUID_WICPixelFormat16bppGray, L"16bppGray" },
        { GUID_WICPixelFormat24bppBGR, L"24bppBGR" },
        { GUID_WICPixelFormat24bppRGB, L"24bppRGB" },
        { GUID_WICPixelFormat32bppBGR, L"32bppBGR" },
        { GUID_WICPixelFormat32bppBGRA, L"32bppBGRA" },
        { GUID_WICPixelFormat32bppPBGRA, L"32bppPBGRA" },
        { GUID_WICPixelFormat32bppGrayFloat, L"32bppGrayFloat" },
        //        { GUID_WICPixelFormat32bppRGB, L"32bppRGB" },
        { GUID_WICPixelFormat32bppRGBA, L"32bppRGBA" },
        { GUID_WICPixelFormat32bppPRGBA, L"32bppPRGBA" },
        { GUID_WICPixelFormat48bppRGB, L"48bppRGB" },
        { GUID_WICPixelFormat48bppBGR, L"48bppBGR" },
        //        { GUID_WICPixelFormat64bppRGB, L"64bppRGB" },
        { GUID_WICPixelFormat64bppRGBA, L"64bppRGBA" },
        { GUID_WICPixelFormat64bppBGRA, L"64bppBGRA" },
        { GUID_WICPixelFormat64bppPRGBA, L"64bppPRGBA" },
        { GUID_WICPixelFormat64bppPBGRA, L"64bppPBGRA" },
        { GUID_WICPixelFormat16bppGrayFixedPoint, L"16bppGrayFixedPoint" },
        { GUID_WICPixelFormat32bppBGR101010, L"32bppBGR101010" },
        { GUID_WICPixelFormat48bppRGBFixedPoint, L"48bppRGBFixedPoint" },
        { GUID_WICPixelFormat48bppBGRFixedPoint, L"48bppBGRFixedPoint" },
        { GUID_WICPixelFormat96bppRGBFixedPoint, L"96bppRGBFixedPoint" },
        //        { GUID_WICPixelFormat96bppRGBFloat, L"96bppRGBFloat" },
        { GUID_WICPixelFormat128bppRGBAFloat, L"128bppRGBAFloat" },
        { GUID_WICPixelFormat128bppPRGBAFloat, L"128bppPRGBAFloat" },
        { GUID_WICPixelFormat128bppRGBFloat, L"128bppRGBFloat" },
        { GUID_WICPixelFormat32bppCMYK, L"32bppCMYK" },
        { GUID_WICPixelFormat64bppRGBAFixedPoint, L"64bppRGBAFixedPoint" },
        { GUID_WICPixelFormat64bppBGRAFixedPoint, L"64bppBGRAFixedPoint" },
        { GUID_WICPixelFormat64bppRGBFixedPoint, L"64bppRGBFixedPoint" },
        { GUID_WICPixelFormat128bppRGBAFixedPoint, L"128bppRGBAFixedPoint" },
        { GUID_WICPixelFormat128bppRGBFixedPoint, L"128bppRGBFixedPoint" },
        { GUID_WICPixelFormat64bppRGBAHalf, L"64bppRGBAHalf" },
        //        { GUID_WICPixelFormat64bppPRGBAHalf, L"64bppPRGBAHalf" },
        { GUID_WICPixelFormat64bppRGBHalf, L"64bppRGBHalf" },
        { GUID_WICPixelFormat48bppRGBHalf, L"48bppRGBHalf" },
        { GUID_WICPixelFormat32bppRGBE, L"32bppRGBE" },
        { GUID_WICPixelFormat16bppGrayHalf, L"16bppGrayHalf" },
        { GUID_WICPixelFormat32bppGrayFixedPoint, L"32bppGrayFixedPoint" },
        { GUID_WICPixelFormat32bppRGBA1010102, L"32bppRGBA1010102" },
        { GUID_WICPixelFormat32bppRGBA1010102XR, L"32bppRGBA1010102XR" },
        { GUID_WICPixelFormat32bppR10G10B10A2, L"32bppR10G10B10A2" },
        { GUID_WICPixelFormat32bppR10G10B10A2HDR10, L"32bppR10G10B10A2HDR10" },
        { GUID_WICPixelFormat64bppCMYK, L"64bppCMYK" },
        { GUID_WICPixelFormat24bpp3Channels, L"24bpp3Channels" },
        { GUID_WICPixelFormat32bpp4Channels, L"32bpp4Channels" },
        { GUID_WICPixelFormat40bpp5Channels, L"40bpp5Channels" },
        { GUID_WICPixelFormat48bpp6Channels, L"48bpp6Channels" },
        { GUID_WICPixelFormat56bpp7Channels, L"56bpp7Channels" },
        { GUID_WICPixelFormat64bpp8Channels, L"64bpp8Channels" },
        { GUID_WICPixelFormat48bpp3Channels, L"48bpp3Channels" },
        { GUID_WICPixelFormat64bpp4Channels, L"64bpp4Channels" },
        { GUID_WICPixelFormat80bpp5Channels, L"80bpp5Channels" },
        { GUID_WICPixelFormat96bpp6Channels, L"96bpp6Channels" },
        { GUID_WICPixelFormat112bpp7Channels, L"112bpp7Channels" },
        { GUID_WICPixelFormat128bpp8Channels, L"128bpp8Channels" },
        { GUID_WICPixelFormat40bppCMYKAlpha, L"40bppCMYKAlpha" },
        { GUID_WICPixelFormat80bppCMYKAlpha, L"80bppCMYKAlpha" },
        { GUID_WICPixelFormat32bpp3ChannelsAlpha, L"32bpp3ChannelsAlpha" },
        { GUID_WICPixelFormat40bpp4ChannelsAlpha, L"40bpp4ChannelsAlpha" },
        { GUID_WICPixelFormat48bpp5ChannelsAlpha, L"48bpp5ChannelsAlpha" },
        { GUID_WICPixelFormat56bpp6ChannelsAlpha, L"56bpp6ChannelsAlpha" },
        { GUID_WICPixelFormat64bpp7ChannelsAlpha, L"64bpp7ChannelsAlpha" },
        { GUID_WICPixelFormat72bpp8ChannelsAlpha, L"72bpp8ChannelsAlpha" },
        { GUID_WICPixelFormat64bpp3ChannelsAlpha, L"64bpp3ChannelsAlpha" },
        { GUID_WICPixelFormat80bpp4ChannelsAlpha, L"80bpp4ChannelsAlpha" },
        { GUID_WICPixelFormat96bpp5ChannelsAlpha, L"96bpp5ChannelsAlpha" },
        { GUID_WICPixelFormat112bpp6ChannelsAlpha, L"112bpp6ChannelsAlpha" },
        { GUID_WICPixelFormat128bpp7ChannelsAlpha, L"128bpp7ChannelsAlpha" },
        { GUID_WICPixelFormat144bpp8ChannelsAlpha, L"144bpp8ChannelsAlpha" },
        { GUID_WICPixelFormat8bppY, L"8bppY" },
        { GUID_WICPixelFormat8bppCb, L"8bppCb" },
        { GUID_WICPixelFormat8bppCr, L"8bppCr" },
        { GUID_WICPixelFormat16bppCbCr, L"16bppCbCr" },
        { GUID_WICPixelFormat16bppYQuantizedDctCoefficients, L"16bppYQuantizedDctCoefficients" },
        { GUID_WICPixelFormat16bppCbQuantizedDctCoefficients, L"16bppCbQuantizedDctCoefficients" },
        { GUID_WICPixelFormat16bppCrQuantizedDctCoefficients, L"16bppCrQuantizedDctCoefficients" },
    };

    HRESULT enum_codecs(uint32 codec_type)
    {
        auto wic = get_wic();

        ComPtr<IEnumUnknown> enumerator;

        DWORD enumerate_options = WICComponentEnumerateDefault;

        CHK_HR(wic->CreateComponentEnumerator(codec_type, enumerate_options, &enumerator));

        ULONG fetched = 0;
        ComPtr<IUnknown> current = NULL;

        while(enumerator->Next(1, &current, &fetched) == S_OK) {

            ComPtr<IWICBitmapCodecInfo> codecinfo;
            CHK_HR(current.As(&codecinfo));
            current.Reset();

            // two step dance to get the len and then the text
            auto get_string = [](auto o, auto fn, std::wstring &str) {
                UINT str_len = 0;
                CHK_HR(fn(o, 0, null, &str_len));
                str.resize(str_len);
                CHK_HR(fn(o, str_len, str.data(), &str_len));
                str.pop_back();    // remove null terminator
                return S_OK;
            };

            std::wstring name;
            std::wstring extensions;

            CHK_HR(get_string(codecinfo.Get(), std::mem_fn(&IWICBitmapCodecInfo::GetFriendlyName), name));
            CHK_HR(get_string(codecinfo.Get(), std::mem_fn(&IWICBitmapCodecInfo::GetFileExtensions), extensions));

            uint num_pixel_formats;
            std::vector<GUID> pixel_formats;
            CHK_HR(codecinfo->GetPixelFormats(0, null, &num_pixel_formats));
            pixel_formats.resize(num_pixel_formats);
            CHK_HR(codecinfo->GetPixelFormats(num_pixel_formats, pixel_formats.data(), &num_pixel_formats));

            LOG_DEBUG(L"WIC CODEC {} supports {}", name, extensions);

            for(auto const &g : pixel_formats) {
                std::wstring pixel_format_name = L"?UNKNOWN?";
                auto found = pixel_format_names.find(g);
                if(found != pixel_format_names.end()) {
                    pixel_format_name = found->second;
                }
                LOG_DEBUG(L"    WIC CODEC {} : {}", name, pixel_format_name);
            }
        }
        return S_OK;
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

        enum_codecs(WICEncoder);

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

        CHK_BOOL(SetClipboardData(RegisterClipboardFormatW(L"PNG"), hData));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // Decode an image into a pixel buffer and get dimensions

    HRESULT decode(image_file *file)
    {
        LOG_DEBUG(L"DECODE {}", file->filename);

        byte const *bytes = file->bytes.data();
        size_t file_size = file->bytes.size();

        if(file_size == 0) {
            return HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT);
        }

        if(bytes == null) {
            return E_INVALIDARG;
        }

        if(file_size > UINT32_MAX) {
            return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        }

        auto wic = get_wic();

        if(!wic) {
            return E_NOINTERFACE;
        }

        ComPtr<IWICColorContext> color_context;

        CHK_HR(wic->CreateColorContext(&color_context));

        CHK_HR(color_context->InitializeFromMemory(bytes, (uint)file_size));

        WICColorContextType color_context_type;
        CHK_HR(color_context->GetType(&color_context_type));
        LOG_DEBUG(L"Color Context for {} is {}", file->filename, (uint)color_context_type);

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

        LOG_DEBUG(L"DECODE {}", file->filename);

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
    // alternatively....
    // create a WIC bitmap on memory of a known format
    // use a WICFormatConverter to create the final one

    HRESULT save(std::wstring const &filename, byte const *bytes, uint width, uint height, uint pitch)
    {
        std::wstring extension;
        CHK_HR(file::get_extension(filename, extension));

        if(extension[0] == L'.') {
            extension = extension.substr(1);
        }

        extension = make_uppercase(extension);

        decltype(image::formats)::const_iterator found;

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

        CD3D11_TEXTURE2D_DESC desc(format, image.width, image.height);

        CD3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc(D3D11_SRV_DIMENSION_TEXTURE2D, desc.Format);

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

        decltype(image::formats)::const_iterator found;

        {
            auto iflock{ std::lock_guard(formats_mutex) };
            found = image::formats.find(ext);
        }

        is_supported = found != image::formats.end();

        return S_OK;
    }
}
