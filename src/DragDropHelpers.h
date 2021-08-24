// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

#include <strsafe.h>
#include <commoncontrols.h>

// declare a static CLIPFORMAT and pass that that by ref as the first param

inline CLIPFORMAT GetClipboardFormat(CLIPFORMAT *pcf, PCWSTR pszForamt)
{
    if(*pcf == 0) {
        *pcf = (CLIPFORMAT)RegisterClipboardFormat(pszForamt);
    }
    return *pcf;
}

inline HRESULT SetBlob(IDataObject *pdtobj, CLIPFORMAT cf, const void *pvBlob, UINT cbBlob)
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

inline void SetDropTip(IDataObject *pdtobj, DROPIMAGETYPE type, PCWSTR pszMsg, PCWSTR pszInsert)
{
    DROPDESCRIPTION dd = { type };
    StringCchCopyW(dd.szMessage, ARRAYSIZE(dd.szMessage), pszMsg);
    StringCchCopyW(dd.szInsert, ARRAYSIZE(dd.szInsert), pszInsert ? pszInsert : L"");

    static CLIPFORMAT s_cfDropDescription = 0;
    SetBlob(pdtobj, GetClipboardFormat(&s_cfDropDescription, CFSTR_DROPDESCRIPTION), &dd, sizeof(dd));
}

inline void ClearDropTip(IDataObject *pdtobj)
{
    SetDropTip(pdtobj, DROPIMAGE_INVALID, L"", NULL);
}

// encapsulation of the shell drag drop presentation and Drop handling functionality
// this provides the following features 1) drag images, 2) drop tips,
// 3) ints OLE and registers drop target, 4) conversion of the data object item into shell items
//
// to use this
//      1) have the object that represents the main window of your app derive
//         from CDragDropHelper exposing it as public.
//         "class CAppClass : public CDragDropHelper"
//      2) add IDropTarget to the QueryInterface() implementation of your class
//         that is a COM object itself
//      3) in your WM_INITDIALOG handling call
//         InitializeDragDropHelper(_hdlg, DROPIMAGE_LINK, NULL) passing
//         the dialog window and drop tip template and type
//      4) in your WM_DESTROY handler add a call to TerminateDragDropHelper(). note not
//         doing this will lead to a leak of your object so this is important
//      5) add the delaration and implementation of OnDrop() to your class, this
//         gets called when the drop happens

class CDragDropHelper : public IDropTarget
{
public:
    CDragDropHelper() : _pdth(NULL), _pdtobj(NULL), _hwndRegistered(NULL), _dropImageType(DROPIMAGE_LABEL), _pszDropTipTemplate(NULL)
    {
        // if exceptions are not enabled then constructors are dumb (they can't do anything that might fail)

        if(FAILED(OleInitialize(null))) {
            return;
        }
        if(FAILED(CoCreateInstance(CLSID_DragDropHelper, NULL, CLSCTX_INPROC, IID_PPV_ARGS(&_pdth)))) {
        }
    }

    ~CDragDropHelper()
    {
        SafeRelease(&_pdth);
    }

    void SetDropTipTemplate(PCWSTR pszDropTipTemplate)
    {
        _pszDropTipTemplate = pszDropTipTemplate;
    }

    void InitializeDragDropHelper(HWND hwnd, DROPIMAGETYPE dropImageType = DROPIMAGE_LABEL, PCWSTR pszDropTipTemplate = L"View")
    {
        _dropImageType = dropImageType;
        _pszDropTipTemplate = pszDropTipTemplate;
        if(FAILED(RegisterDragDrop(hwnd, this))) {
            log_win32_error(L"?");
        } else {
            _hwndRegistered = hwnd;
        }
    }

    void TerminateDragDropHelper()
    {
        if(_hwndRegistered) {
            RevokeDragDrop(_hwndRegistered);
            _hwndRegistered = NULL;
        }
    }

    HRESULT GetDragDropHelper(REFIID riid, void **ppv)
    {
        *ppv = NULL;
        return _pdth ? _pdth->QueryInterface(riid, ppv) : E_NOINTERFACE;
    }

    // direct access to the data object, if you don't want to use the shell item array
    IDataObject *GetDataObject()
    {
        return _pdtobj;
    }

    // IDropTarget
    IFACEMETHODIMP DragEnter(IDataObject *pdtobj, DWORD /* grfKeyState */, POINTL pt, DWORD *pdwEffect)
    {
        // leave *pdwEffect unchanged, we support all operations
        if(_pdth) {
            POINT ptT = { pt.x, pt.y };
            _pdth->DragEnter(_hwndRegistered, pdtobj, &ptT, *pdwEffect);
        }
        SetInterface(&_pdtobj, pdtobj);

        IShellItem *psi;
        HRESULT hr = create_shell_item_from_object(pdtobj, IID_PPV_ARGS(&psi));
        if(SUCCEEDED(hr)) {
            PWSTR pszName;
            hr = psi->GetDisplayName(SIGDN_NORMALDISPLAY, &pszName);
            if(SUCCEEDED(hr)) {
                SetDropTip(pdtobj, _dropImageType, _pszDropTipTemplate ? _pszDropTipTemplate : L"%1", pszName);
                CoTaskMemFree(pszName);
            }
            psi->Release();
        }
        return S_OK;
    }

    IFACEMETHODIMP DragOver(DWORD /* grfKeyState */, POINTL pt, DWORD *pdwEffect)
    {
        // leave *pdwEffect unchanged, we support all operations
        if(_pdth) {
            POINT ptT = { pt.x, pt.y };
            _pdth->DragOver(&ptT, *pdwEffect);
        }
        return S_OK;
    }

    IFACEMETHODIMP DragLeave()
    {
        if(_pdth) {
            _pdth->DragLeave();
        }
        ClearDropTip(_pdtobj);
        SafeRelease(&_pdtobj);
        return S_OK;
    }

    IFACEMETHODIMP Drop(IDataObject *pdtobj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
    {
        if(_pdth) {
            POINT ptT = { pt.x, pt.y };
            _pdth->Drop(pdtobj, &ptT, *pdwEffect);
        }

        IShellItemArray *psia;
        if(SUCCEEDED(SHCreateShellItemArrayFromDataObject(_pdtobj, IID_PPV_ARGS(&psia)))) {

            defer(psia->Release());
            CHK_HR(on_drop_shell_item(psia, grfKeyState));
            return S_OK;
        }

        FORMATETC f{ 0 };
        f.cfFormat = CF_UNICODETEXT;
        f.dwAspect = DVASPECT_CONTENT;
        f.tymed = TYMED_HGLOBAL;
        f.lindex = -1;
        STGMEDIUM s{ 0 };
        CHK_HR(pdtobj->GetData(&f, &s));

        if(s.tymed == TYMED_HGLOBAL) {

            void *p = GlobalLock(s.hGlobal);
            defer(GlobalUnlock(p));
            return on_drop_string(reinterpret_cast<wchar const *>(p));
        }

        CHK_HR(OnDropError(pdtobj));
        return S_OK;
    }

private:
    // client provides
    virtual HRESULT on_drop_shell_item(IShellItemArray *psia, DWORD grfKeyState) = 0;
    virtual HRESULT on_drop_string(wchar const *str) = 0;
    virtual HRESULT OnDropError(IDataObject * /* pdtobj */)
    {
        return S_OK;
    }

    IDropTargetHelper *_pdth;
    IDataObject *_pdtobj;
    DROPIMAGETYPE _dropImageType;
    PCWSTR _pszDropTipTemplate;
    HWND _hwndRegistered;
};
