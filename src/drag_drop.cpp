//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

HRESULT create_shell_item_from_object(IUnknown *punk, REFIID riid, void **ppv)
{
    *ppv = NULL;

    // try the basic default first
    PIDLIST_ABSOLUTE pidl;
    if(SUCCEEDED(SHGetIDListFromObject(punk, &pidl))) {
        defer(ILFree(pidl));
        CHK_HR(SHCreateItemFromIDList(pidl, riid, ppv));
        return S_OK;
    }

    // perhaps the input is from IE and if so we can construct an item from the URL
    IDataObject *pdo;
    CHK_HR(punk->QueryInterface(IID_PPV_ARGS(&pdo)));
    defer(pdo->Release());

    CLIPFORMAT g_cfURL = 0;
    FORMATETC fmte = { get_clipboard_format(&g_cfURL, CFSTR_SHELLURL), NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium;
    CHK_HR(pdo->GetData(&fmte, &medium));
    scoped([&]() { ReleaseStgMedium(&medium); });

    PCSTR pszURL = (PCSTR)GlobalLock(medium.hGlobal);
    if(pszURL == null) {
        return ERROR_CANTREAD;
    }
    defer(GlobalUnlock(medium.hGlobal));

    WCHAR szURL[2048];
    if(SHAnsiToUnicode(pszURL, szURL, ARRAYSIZE(szURL)) == 0) {
        return ERROR_ILLEGAL_CHARACTER;
    }

    CHK_HR(SHCreateItemFromParsingName(szURL, NULL, riid, ppv));
    return S_OK;
}

//////////////////////////////////////////////////////////////////////
// declare a static CLIPFORMAT and pass that that by ref as the first param

CLIPFORMAT get_clipboard_format(CLIPFORMAT *pcf, PCWSTR pszForamt)
{
    if(*pcf == 0) {
        *pcf = (CLIPFORMAT)RegisterClipboardFormat(pszForamt);
    }
    return *pcf;
}

//////////////////////////////////////////////////////////////////////

HRESULT set_blob(IDataObject *pdtobj, CLIPFORMAT cf, const void *pvBlob, UINT cbBlob)
{
    void *pv = GlobalAlloc(GPTR, cbBlob);
    HRESULT hr = pv ? S_OK : E_OUTOFMEMORY;
    if(SUCCEEDED(hr)) {
        CopyMemory(pv, pvBlob, cbBlob);

        FORMATETC fmte = { cf, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };

        // The STGMEDIUM structure is used to define how to handle a global memory transfer.
        // This structure includes a flag, tymed, which indicates the medium
        // to be used, and a union comprising pointers and a handle for getting whichever
        // medium is specified in tymed.
        STGMEDIUM medium = {};
        medium.tymed = TYMED_HGLOBAL;
        medium.hGlobal = pv;

        hr = pdtobj->SetData(&fmte, &medium, TRUE);
        if(FAILED(hr)) {
            GlobalFree(pv);
        }
    }
    return hr;
}

//////////////////////////////////////////////////////////////////////

void set_drop_tip(IDataObject *pdtobj, DROPIMAGETYPE type, PCWSTR pszMsg, PCWSTR pszInsert)
{
    DROPDESCRIPTION dd = { type };
    StringCchCopyW(dd.szMessage, ARRAYSIZE(dd.szMessage), pszMsg);
    StringCchCopyW(dd.szInsert, ARRAYSIZE(dd.szInsert), pszInsert ? pszInsert : L"");

    static CLIPFORMAT s_cfDropDescription = 0;
    set_blob(pdtobj, get_clipboard_format(&s_cfDropDescription, CFSTR_DROPDESCRIPTION), &dd, sizeof(dd));
}

//////////////////////////////////////////////////////////////////////

void clear_drop_tip(IDataObject *pdtobj)
{
    set_drop_tip(pdtobj, DROPIMAGE_INVALID, L"", NULL);
}
