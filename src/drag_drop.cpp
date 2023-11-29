//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

HRESULT create_shell_item_from_object(IUnknown *punk, REFIID riid, void **ppv)
{
    *ppv = NULL;

    // try the basic default first
    PIDLIST_ABSOLUTE pidl;
    if(SUCCEEDED(SHGetIDListFromObject(punk, &pidl))) {
        DEFER(ILFree(pidl));
        CHK_HR(SHCreateItemFromIDList(pidl, riid, ppv));
        return S_OK;
    }

    // perhaps the input is from IE and if so we can construct an item from the URL
    IDataObject *pdo;
    CHK_HR(punk->QueryInterface(IID_PPV_ARGS(&pdo)));
    DEFER(pdo->Release());

    CLIPFORMAT g_cfURL = 0;
    FORMATETC fmte = { get_clipboard_format(&g_cfURL, CFSTR_SHELLURL), NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium;
    CHK_HR(pdo->GetData(&fmte, &medium));

    auto cleanup = SCOPED[&]()
    {
        ReleaseStgMedium(&medium);
    };

    PCSTR pszURL = (PCSTR)GlobalLock(medium.hGlobal);
    if(pszURL == null) {
        return ERROR_CANTREAD;
    }
    DEFER(GlobalUnlock(medium.hGlobal));

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
    void *pv;

    CHK_NULL(pv = GlobalAlloc(GPTR, cbBlob));

    auto cleanup = SCOPED[=]()
    {
        GlobalFree(pv);
    };

    CopyMemory(pv, pvBlob, cbBlob);

    FORMATETC fmte = { cf, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };

    STGMEDIUM medium = {};
    medium.tymed = TYMED_HGLOBAL;
    medium.hGlobal = pv;

    CHK_HR(pdtobj->SetData(&fmte, &medium, TRUE));

    cleanup.cancel();

    return S_OK;
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
