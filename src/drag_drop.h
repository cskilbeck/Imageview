//////////////////////////////////////////////////////////////////////
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

void clear_drop_tip(IDataObject *pdtobj);
void set_drop_tip(IDataObject *pdtobj, DROPIMAGETYPE type, PCWSTR pszMsg, PCWSTR pszInsert);
HRESULT set_blob(IDataObject *pdtobj, CLIPFORMAT cf, const void *pvBlob, UINT cbBlob);
CLIPFORMAT get_clipboard_format(CLIPFORMAT *pcf, PCSTR pszForamt);
HRESULT create_shell_item_from_object(IUnknown *punk, REFIID riid, void **ppv);

//////////////////////////////////////////////////////////////////////
// COM helpers

template <class T> void SafeRelease(T **ppT)
{
    if(*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

//////////////////////////////////////////////////////////////////////

template <class T> HRESULT SetInterface(T **ppT, IUnknown *punk)
{
    SafeRelease(ppT);

    if(punk == null) {
        return E_NOINTERFACE;
    }

    return punk->QueryInterface(ppT);
}

//////////////////////////////////////////////////////////////////////

class CDragDropHelper : public IDropTarget
{
public:
    CDragDropHelper()
        : _pdth(NULL), _pdtobj(NULL), _hwndRegistered(NULL), _dropImageType(DROPIMAGE_LABEL), _pszDropTipTemplate(NULL)
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

    void InitializeDragDropHelper(HWND hwnd,
                                  DROPIMAGETYPE dropImageType = DROPIMAGE_LABEL,
                                  PCWSTR pszDropTipTemplate = L"View")
    {
        _dropImageType = dropImageType;
        _pszDropTipTemplate = pszDropTipTemplate;
        if(FAILED(RegisterDragDrop(hwnd, this))) {
            log_win32_error("?");
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
                set_drop_tip(pdtobj, _dropImageType, _pszDropTipTemplate ? _pszDropTipTemplate : L"%1", pszName);
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
        clear_drop_tip(_pdtobj);
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

            DEFER(psia->Release());
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
            DEFER(GlobalUnlock(p));
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
