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

    std::mutex formats_mutex;

    struct filetypes
    {
        // file extension -> container format GUID
        std::map<std::wstring, GUID> container_formats;

        // file type friendly name -> filter spec (e.g. "BMP Files" -> "*.bmp;*.dib")
        std::map<std::wstring, std::wstring> filter_specs;

        // filter_specs for load/save dialogs
        std::vector<COMDLG_FILTERSPEC> comdlg_filterspecs;
    };

    filetypes save_filetypes;
    filetypes load_filetypes;

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
    // scan WIC supported file types for Decode or Encode

    HRESULT enum_codecs(uint32 codec_type, filetypes &results)
    {
        auto wic = get_wic();

        if(wic == null) {
            return E_NOINTERFACE;
        }

        ComPtr<IEnumUnknown> enumerator;

        DWORD enumerate_options = WICComponentEnumerateDefault;

        CHK_HR(wic->CreateComponentEnumerator(codec_type, enumerate_options, &enumerator));

        ULONG fetched = 0;
        ComPtr<IUnknown> current = NULL;

        std::map<std::wstring, std::vector<std::wstring>> filters;

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

            GUID container_format;
            CHK_HR(codecinfo->GetContainerFormat(&container_format));

            // strip " Encoder" , " Decoder" from friendly name
            // TODO (chs): find a better way!
            name = name.substr(0, name.find_last_of(L' '));

            extensions = imageview::make_lowercase(extensions);

            std::vector<std::wstring> file_extensions;
            imageview::tokenize(extensions, file_extensions, L",", imageview::discard_empty);

            for(auto const &ext : file_extensions) {
                results.container_formats[ext] = container_format;
                filters[name].push_back(ext);
            }
        }

        // TODO (chs): default for loading and saving is PNG - hard code the container format GUID,
        // notice it in the enum and somehow... something

        // make map of file type -> extensions filter spec

        for(auto &n : filters) {
            std::wstring pattern;
            wchar const *sep = L"";
            for(auto &e : n.second) {
                pattern = std::format(L"{}{}*{}", pattern, sep, e);
                sep = L";";
            }
            std::wstring filetype = std::format(L"{} files", n.first);
            results.filter_specs[filetype] = pattern;
        }

        // make COMDLG_FILTERSPEC vector for load/save dialogs

        for(auto const &r : results.filter_specs) {
            results.comdlg_filterspecs.emplace_back(COMDLG_FILTERSPEC{ r.first.c_str(), r.second.c_str() });
        }

        // add in "All Files", seems to be the normal thing to do

        results.comdlg_filterspecs.emplace_back(COMDLG_FILTERSPEC{ L"All files", L"*.*" });

        return S_OK;
    }
}

namespace imageview::image
{
    //////////////////////////////////////////////////////////////////////

    HRESULT init_filetypes()
    {
        auto iflock{ std::lock_guard(formats_mutex) };

        CHK_HR(enum_codecs(WICEncoder, save_filetypes));
        CHK_HR(enum_codecs(WICDecoder, load_filetypes));

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
    // create a WIC memory bitmap in 32bpp BGRA
    // if necessary, interpose a WICFormatConverter
    // then use a WICBitmapEncoder to save the file

    HRESULT save(std::wstring const &filename, byte const *bytes, uint width, uint height, uint pitch)
    {
        auto wic = get_wic();

        if(wic == null) {
            return E_NOINTERFACE;
        }

        std::wstring extension;
        CHK_HR(file::get_extension(filename, extension));

        auto f = save_filetypes.container_formats.find(extension);
        if(f == save_filetypes.container_formats.end()) {
            return WEB_E_UNSUPPORTED_FORMAT;    // unknown format
        }

        GUID const &container_format = f->second;

        uint64 dst_size = static_cast<uint64>(pitch) * height;

        if(dst_size > UINT_MAX) {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        uint32 size = static_cast<uint32>(dst_size);

        std::vector<byte> data(size);
        memcpy(data.data(), bytes, size);

        GUID src_format = GUID_WICPixelFormat32bppBGRA;

        ComPtr<IWICBitmap> memory_bitmap;
        CHK_HR(wic->CreateBitmapFromMemory(width, height, src_format, pitch, size, data.data(), &memory_bitmap));

        ComPtr<IWICBitmapSource> bmp_src;
        bmp_src.Attach(memory_bitmap.Detach());

        ComPtr<IWICStream> file_stream;
        CHK_HR(wic->CreateStream(&file_stream));
        CHK_HR(file_stream->InitializeFromFilename(filename.c_str(), GENERIC_WRITE));

        ComPtr<IWICBitmapEncoder> encoder;
        CHK_HR(wic->CreateEncoder(container_format, NULL, &encoder));
        CHK_HR(encoder->Initialize(file_stream.Get(), WICBitmapEncoderNoCache));

        ComPtr<IWICBitmapFrameEncode> frame;
        ComPtr<IPropertyBag2> property_bag;
        CHK_HR(encoder->CreateNewFrame(&frame, &property_bag));
        CHK_HR(frame->Initialize(NULL));
        CHK_HR(frame->SetSize(width, height));

        WICPixelFormatGUID pixel_format = src_format;
        WICPixelFormatGUID original_pixel_format = pixel_format;

        CHK_HR(frame->SetPixelFormat(&pixel_format));

        if(memcmp(&pixel_format, &original_pixel_format, sizeof(GUID)) != 0) {

            ComPtr<IWICFormatConverter> format_converter;

            CHK_HR(wic->CreateFormatConverter(&format_converter));

            CHK_HR(format_converter->Initialize(bmp_src.Get(),
                                                pixel_format,
                                                WICBitmapDitherTypeErrorDiffusion,
                                                null,
                                                0.0f,
                                                WICBitmapPaletteTypeMedianCut));

            bmp_src.Attach(format_converter.Detach());
        }

        CHK_HR(frame->WriteSource(bmp_src.Get(), null));

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

    HRESULT can_load_file_extension(std::wstring const &extension, bool &is_supported)
    {
        std::wstring ext = make_lowercase(extension);

        auto iflock{ std::lock_guard(formats_mutex) };

        auto found = load_filetypes.container_formats.find(ext);
        is_supported = found != load_filetypes.container_formats.end();

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_load_filter_specs(COMDLG_FILTERSPEC *&filter_specs, uint &num_filter_specs)
    {
        filter_specs = load_filetypes.comdlg_filterspecs.data();
        num_filter_specs = static_cast<uint>(load_filetypes.comdlg_filterspecs.size());
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////

    HRESULT get_save_filter_specs(COMDLG_FILTERSPEC *&filter_specs, uint &num_filter_specs)
    {
        filter_specs = save_filetypes.comdlg_filterspecs.data();
        num_filter_specs = static_cast<uint>(save_filetypes.comdlg_filterspecs.size());
        return S_OK;
    }
}
