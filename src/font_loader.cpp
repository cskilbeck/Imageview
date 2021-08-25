#include "pch.h"

// This entire file is the boilerplate for using an embedded font in D2D in Windows 7 and later


// Ignore unreferenced parameters, since they are very common when implementing callbacks.
#pragma warning(disable : 4100)

////////////////////////////////////////
// COM inheritance helpers.

// Acquires an additional reference, if non-null.
template <typename InterfaceType> inline InterfaceType *SafeAcquire(InterfaceType *newObject)
{
    if(newObject != NULL)
        newObject->AddRef();

    return newObject;
}


// Sets a new COM object, releasing the old one.
template <typename InterfaceType> inline void SafeSet(InterfaceType **currentObject, InterfaceType *newObject)
{
    SafeAcquire(newObject);
    SafeRelease(&currentObject);
    currentObject = newObject;
}

//////////////////////////////////////////////////////////////////////
// ResourceFontCollectionLoader
//////////////////////////////////////////////////////////////////////

IDWriteFontCollectionLoader *ResourceFontCollectionLoader::instance_(new(std::nothrow) ResourceFontCollectionLoader());

HRESULT STDMETHODCALLTYPE ResourceFontCollectionLoader::QueryInterface(REFIID iid, void **ppvObject)
{
    if(iid == IID_IUnknown || iid == __uuidof(IDWriteFontCollectionLoader)) {
        *ppvObject = this;
        AddRef();
        return S_OK;
    } else {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }
}

ULONG STDMETHODCALLTYPE ResourceFontCollectionLoader::AddRef()
{
    return InterlockedIncrement(&refCount_);
}

ULONG STDMETHODCALLTYPE ResourceFontCollectionLoader::Release()
{
    ULONG newCount = InterlockedDecrement(&refCount_);
    if(newCount == 0)
        delete this;

    return newCount;
}

HRESULT STDMETHODCALLTYPE ResourceFontCollectionLoader::CreateEnumeratorFromKey(IDWriteFactory *factory,
                                                                                void const *collectionKey,    // [collectionKeySize] in bytes
                                                                                UINT32 collectionKeySize, OUT IDWriteFontFileEnumerator **fontFileEnumerator)
{
    *fontFileEnumerator = NULL;

    HRESULT hr = S_OK;

    if(collectionKeySize % sizeof(UINT) != 0)
        return E_INVALIDARG;

    ResourceFontFileEnumerator *enumerator = new(std::nothrow) ResourceFontFileEnumerator(factory);
    if(enumerator == NULL)
        return E_OUTOFMEMORY;

    UINT const *resourceIDs = static_cast<UINT const *>(collectionKey);
    UINT32 const resourceCount = collectionKeySize / sizeof(UINT);

    hr = enumerator->Initialize(resourceIDs, resourceCount);

    if(FAILED(hr)) {
        delete enumerator;
        return hr;
    }

    *fontFileEnumerator = SafeAcquire(enumerator);

    return hr;
}

//////////////////////////////////////////////////////////////////////
// ResourceFontContext
//////////////////////////////////////////////////////////////////////

ResourceFontContext::~ResourceFontContext()
{
    if(dwrite_factory != null) {
        dwrite_factory->UnregisterFontCollectionLoader(ResourceFontCollectionLoader::GetLoader());
        dwrite_factory->UnregisterFontFileLoader(ResourceFontFileLoader::GetLoader());
    }
}

HRESULT ResourceFontContext::Initialize(IDWriteFactory *factory)
{
    dwrite_factory = factory;
    if(hr_ == S_FALSE) {
        hr_ = InitializeInternal();
    }
    return hr_;
}

HRESULT ResourceFontContext::InitializeInternal()
{
    HRESULT hr = S_OK;

    if(!ResourceFontFileLoader::IsLoaderInitialized() || !ResourceFontCollectionLoader::IsLoaderInitialized()) {
        return E_FAIL;
    }

    // Register our custom loaders with the factory object.
    //
    // Note: For this application we just use the shared DWrite factory object which is accessed via
    //       a global variable. If we were using fonts embedded in *documents* then it might make sense
    //       to create an isolated factory for each document. When unloading the document, one would
    //       also release the isolated factory, thus ensuring that all cached font data specific to
    //       that document would be promptly disposed of.
    //
    if(FAILED(hr = dwrite_factory->RegisterFontFileLoader(ResourceFontFileLoader::GetLoader())))
        return hr;

    hr = dwrite_factory->RegisterFontCollectionLoader(ResourceFontCollectionLoader::GetLoader());

    return hr;
}

HRESULT ResourceFontContext::CreateFontCollection(UINT const *fontCollectionKey,    // [keySize] in bytes
                                                  UINT32 keySize, OUT IDWriteFontCollection **result)
{
    *result = NULL;

    if(dwrite_factory == nullptr) {
        return E_UNEXPECTED;
    }

    return dwrite_factory->CreateCustomFontCollection(ResourceFontCollectionLoader::GetLoader(), fontCollectionKey, keySize, result);
}

//////////////////////////////////////////////////////////////////////
// ResourceFontFileEnumerator
//////////////////////////////////////////////////////////////////////

ResourceFontFileEnumerator::ResourceFontFileEnumerator(IDWriteFactory *factory) : refCount_(0), factory_(SafeAcquire(factory)), currentFile_(), nextIndex_(0)
{
}

HRESULT ResourceFontFileEnumerator::Initialize(UINT const *resourceIDs,    // [resourceCount]
                                               UINT32 resourceCount)
{
    resourceIDs_.assign(resourceIDs, resourceIDs + resourceCount);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ResourceFontFileEnumerator::QueryInterface(REFIID iid, OUT void **ppvObject)
{
    if(iid == IID_IUnknown || iid == __uuidof(IDWriteFontFileEnumerator)) {
        *ppvObject = this;
        AddRef();
        return S_OK;
    } else {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }
}

ULONG STDMETHODCALLTYPE ResourceFontFileEnumerator::AddRef()
{
    return InterlockedIncrement(&refCount_);
}

ULONG STDMETHODCALLTYPE ResourceFontFileEnumerator::Release()
{
    ULONG newCount = InterlockedDecrement(&refCount_);
    if(newCount == 0)
        delete this;

    return newCount;
}

HRESULT STDMETHODCALLTYPE ResourceFontFileEnumerator::MoveNext(OUT BOOL *hasCurrentFile)
{
    HRESULT hr = S_OK;

    *hasCurrentFile = FALSE;
    SafeRelease(&currentFile_);

    if(nextIndex_ < resourceIDs_.size()) {
        hr = factory_->CreateCustomFontFileReference(&resourceIDs_[nextIndex_], sizeof(UINT), ResourceFontFileLoader::GetLoader(), &currentFile_);

        if(SUCCEEDED(hr)) {
            *hasCurrentFile = TRUE;

            ++nextIndex_;
        }
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE ResourceFontFileEnumerator::GetCurrentFontFile(OUT IDWriteFontFile **fontFile)
{
    *fontFile = SafeAcquire(currentFile_);

    return (currentFile_ != NULL) ? S_OK : E_FAIL;
}

//////////////////////////////////////////////////////////////////////
// ResourceFontFileLoader
//////////////////////////////////////////////////////////////////////

IDWriteFontFileLoader *ResourceFontFileLoader::instance_(new(std::nothrow) ResourceFontFileLoader());

// QueryInterface
HRESULT STDMETHODCALLTYPE ResourceFontFileLoader::QueryInterface(REFIID iid, void **ppvObject)
{
    if(iid == IID_IUnknown || iid == __uuidof(IDWriteFontFileLoader)) {
        *ppvObject = this;
        AddRef();
        return S_OK;
    } else {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }
}

// AddRef
ULONG STDMETHODCALLTYPE ResourceFontFileLoader::AddRef()
{
    return InterlockedIncrement(&refCount_);
}

// Release
ULONG STDMETHODCALLTYPE ResourceFontFileLoader::Release()
{
    ULONG newCount = InterlockedDecrement(&refCount_);
    if(newCount == 0)
        delete this;

    return newCount;
}

// CreateStreamFromKey
//
//      Creates an IDWriteFontFileStream from a key that identifies the file. The
//      format and meaning of the key is entirely up to the loader implementation.
//      The only requirements imposed by DWrite are that a key must remain valid
//      for as long as the loader is registered. The same key must also uniquely
//      identify the same file, so for example you must not recycle keys so that
//      a key might represent different files at different times.
//
//      In this case the key is a UINT which identifies a font resources.
//
HRESULT STDMETHODCALLTYPE ResourceFontFileLoader::CreateStreamFromKey(void const *fontFileReferenceKey,    // [fontFileReferenceKeySize] in bytes
                                                                      UINT32 fontFileReferenceKeySize, OUT IDWriteFontFileStream **fontFileStream)
{
    *fontFileStream = NULL;

    // Make sure the key is the right size.
    if(fontFileReferenceKeySize != sizeof(UINT))
        return E_INVALIDARG;

    UINT resourceID = *static_cast<UINT const *>(fontFileReferenceKey);

    // Create the stream object.
    ResourceFontFileStream *stream = new(std::nothrow) ResourceFontFileStream(resourceID);
    if(stream == NULL)
        return E_OUTOFMEMORY;

    if(!stream->IsInitialized()) {
        delete stream;
        return E_FAIL;
    }

    *fontFileStream = SafeAcquire(stream);

    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// ResourceFontFileStream
//////////////////////////////////////////////////////////////////////

HMODULE const ResourceFontFileStream::moduleHandle_(GetCurrentModule());

// GetCurrentModule
//
//      Helper to get the module handle for the application.
//
HMODULE ResourceFontFileStream::GetCurrentModule()
{
    HMODULE handle = NULL;

    // Determine the module handle from the address of this function.
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCTSTR>(&GetCurrentModule), &handle);

    return handle;
}

ResourceFontFileStream::ResourceFontFileStream(UINT resourceID) : refCount_(0), resourcePtr_(NULL), resourceSize_(0)
{
    HRSRC resource = FindResource(moduleHandle_, MAKEINTRESOURCE(resourceID), RT_FONT);
    if(resource != NULL) {
        HGLOBAL memHandle = LoadResource(moduleHandle_, resource);
        if(memHandle != NULL) {
            resourcePtr_ = LockResource(memHandle);
            if(resourcePtr_ != NULL) {
                resourceSize_ = SizeofResource(moduleHandle_, resource);
            }
        }
    }
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE ResourceFontFileStream::QueryInterface(REFIID iid, void **ppvObject)
{
    if(iid == IID_IUnknown || iid == __uuidof(IDWriteFontFileStream)) {
        *ppvObject = this;
        AddRef();
        return S_OK;
    } else {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }
}

ULONG STDMETHODCALLTYPE ResourceFontFileStream::AddRef()
{
    return InterlockedIncrement(&refCount_);
}

ULONG STDMETHODCALLTYPE ResourceFontFileStream::Release()
{
    ULONG newCount = InterlockedDecrement(&refCount_);
    if(newCount == 0)
        delete this;

    return newCount;
}

// IDWriteFontFileStream methods
HRESULT STDMETHODCALLTYPE ResourceFontFileStream::ReadFileFragment(void const **fragmentStart,    // [fragmentSize] in bytes
                                                                   UINT64 fileOffset, UINT64 fragmentSize, OUT void **fragmentContext)
{
    // The loader is responsible for doing a bounds check.
    if(fileOffset <= resourceSize_ && fragmentSize <= resourceSize_ - fileOffset) {
        *fragmentStart = static_cast<BYTE const *>(resourcePtr_) + static_cast<size_t>(fileOffset);
        *fragmentContext = NULL;
        return S_OK;
    } else {
        *fragmentStart = NULL;
        *fragmentContext = NULL;
        return E_FAIL;
    }
}

void STDMETHODCALLTYPE ResourceFontFileStream::ReleaseFileFragment(void *fragmentContext)
{
}

HRESULT STDMETHODCALLTYPE ResourceFontFileStream::GetFileSize(OUT UINT64 *fileSize)
{
    *fileSize = resourceSize_;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ResourceFontFileStream::GetLastWriteTime(OUT UINT64 *lastWriteTime)
{
    // The concept of last write time does not apply to this loader.
    *lastWriteTime = 0;
    return E_NOTIMPL;
}
