#pragma once

//////////////////////////////////////////////////////////////////////
// ResourceFontCollectionLoader
//////////////////////////////////////////////////////////////////////

// ResourceFontCollectionLoader
//
//      Implements the IDWriteFontCollectionLoader interface for collections
//      of fonts embedded in the application as resources. The font collection
//      key is an array of resource IDs.
//
class ResourceFontCollectionLoader : public IDWriteFontCollectionLoader
{
public:
    ResourceFontCollectionLoader() : refCount_(0)
    {
    }

    // IUnknown methods
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IDWriteFontCollectionLoader methods
    COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE
    CreateEnumeratorFromKey(IDWriteFactory *factory,
                            void const *collectionKey,    // [collectionKeySize] in bytes
                            UINT32 collectionKeySize,
                            OUT IDWriteFontFileEnumerator **fontFileEnumerator) override;

    // Gets the singleton loader instance.
    static IDWriteFontCollectionLoader *GetLoader()
    {
        return instance_;
    }

    static bool IsLoaderInitialized()
    {
        return instance_ != NULL;
    }

private:
    ULONG refCount_;

    static IDWriteFontCollectionLoader *instance_;
};

//////////////////////////////////////////////////////////////////////
// ResourceFontContext
//////////////////////////////////////////////////////////////////////

class ResourceFontContext
{
public:
    ResourceFontContext() = default;
    ~ResourceFontContext();

    HRESULT Initialize(IDWriteFactory *factory);

    HRESULT CreateFontCollection(UINT const *fontCollectionKey,    // [keySize] in bytes
                                 UINT32 keySize,
                                 OUT IDWriteFontCollection **result);

private:
    // Not copyable or assignable.
    ResourceFontContext(ResourceFontContext const &) = delete;
    void operator=(ResourceFontContext const &) = delete;

    HRESULT InitializeInternal();

    // Error code from Initialize().
    HRESULT hr_{ S_FALSE };
    IDWriteFactory *dwrite_factory{ nullptr };
};

//////////////////////////////////////////////////////////////////////
// ResourceFontFileEnumerator
//////////////////////////////////////////////////////////////////////

// ResourceFontFileEnumerator
//
//      Implements the IDWriteFontFileEnumerator interface for a collection
//      of fonts embedded in the application as resources. The font collection
//      key is an array of resource IDs.
//
class ResourceFontFileEnumerator : public IDWriteFontFileEnumerator
{
public:
    ResourceFontFileEnumerator(IDWriteFactory *factory);

    HRESULT Initialize(UINT const *resourceIDs,    // [resourceCount]
                       UINT32 resourceCount);

    ~ResourceFontFileEnumerator()
    {
        SafeRelease(&currentFile_);
        SafeRelease(&factory_);
    }

    // IUnknown methods
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IDWriteFontFileEnumerator methods
    COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE MoveNext(OUT BOOL *hasCurrentFile) override;
    COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE GetCurrentFontFile(OUT IDWriteFontFile **fontFile) override;

private:
    ULONG refCount_;

    IDWriteFactory *factory_;
    IDWriteFontFile *currentFile_;
    std::vector<UINT> resourceIDs_;
    size_t nextIndex_;
};

//////////////////////////////////////////////////////////////////////
// ResourceFontFileLoader
//////////////////////////////////////////////////////////////////////

// ResourceFontFileLoader
//
//      Implements the IDWriteFontFileLoader interface for fonts
//      embedded as a resources in the application. The font file key is
//      a UINT resource identifier.
//
class ResourceFontFileLoader : public IDWriteFontFileLoader
{
public:
    ResourceFontFileLoader() : refCount_(0)
    {
    }

    // IUnknown methods
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IDWriteFontFileLoader methods
    COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE
    CreateStreamFromKey(void const *fontFileReferenceKey,    // [fontFileReferenceKeySize] in bytes
                        UINT32 fontFileReferenceKeySize,
                        OUT IDWriteFontFileStream **fontFileStream) override;

    // Gets the singleton loader instance.
    static IDWriteFontFileLoader *GetLoader()
    {
        return instance_;
    }

    static bool IsLoaderInitialized()
    {
        return instance_ != NULL;
    }

private:
    ULONG refCount_;

    static IDWriteFontFileLoader *instance_;
};

//////////////////////////////////////////////////////////////////////
// ResourceFontFileStream
//////////////////////////////////////////////////////////////////////

// ResourceFontFileStream
//
//      Implements the IDWriteFontFileStream interface in terms of a font
//      embedded as a resource in the application. The font file key is
//      a UINT resource identifier.
//
class ResourceFontFileStream : public IDWriteFontFileStream
{
public:
    explicit ResourceFontFileStream(UINT resourceID);

    // IUnknown methods
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IDWriteFontFileStream methods
    COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE
    ReadFileFragment(void const **fragmentStart,    // [fragmentSize] in bytes
                     UINT64 fileOffset,
                     UINT64 fragmentSize,
                     OUT void **fragmentContext) override;

    COM_DECLSPEC_NOTHROW void STDMETHODCALLTYPE ReleaseFileFragment(void *fragmentContext) override;

    COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE GetFileSize(OUT UINT64 *fileSize) override;

    COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE GetLastWriteTime(OUT UINT64 *lastWriteTime) override;

    bool IsInitialized()
    {
        return resourcePtr_ != NULL;
    }

private:
    ULONG refCount_;
    void const *resourcePtr_;    // [resourceSize_] in bytes
    DWORD resourceSize_;

    static HMODULE const moduleHandle_;
    static HMODULE GetCurrentModule();
};
