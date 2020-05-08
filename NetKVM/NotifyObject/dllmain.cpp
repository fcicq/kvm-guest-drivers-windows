/*
 * Copyright (c) 2008-2017 Red Hat, Inc.
 * Copyright (c) 2020 Oracle Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met :
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and / or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of their contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "notify.h"
#include "notifyn_i.c"

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
    OBJECT_ENTRY(CLSID_CNotifyObject, CNotifyObject)
END_OBJECT_MAP()

// required DLL export
extern "C"
BOOL WINAPI DllMain (HINSTANCE hInstance, DWORD dwReason, LPVOID)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            Trace("%s: Attach =>", __FUNCTION__);
            _Module.Init(ObjectMap, hInstance);
            DisableThreadLibraryCalls(hInstance);
            break;
        case DLL_PROCESS_DETACH:
            Trace("%s: Detach =>", __FUNCTION__);
            _Module.Term();
    }
    Trace("%s: <=", __FUNCTION__);
    return true;
}

// required DLL export
STDAPI DllCanUnloadNow(void)
{
    HRESULT hr = _Module.GetLockCount() ? S_OK : S_FALSE;
    Trace("%s hr %d", __FUNCTION__, hr);
    return hr;
}

// required DLL export
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID* ppv)
{
    HRESULT hr = _Module.GetClassObject(rclsid, riid, ppv);
    Trace("%s hr %d", __FUNCTION__, hr);
    return hr;
}

// required DLL export
STDAPI DllRegisterServer(void)
{
    HRESULT hr = _Module.RegisterServer(true);
    Trace("%s hr %d", __FUNCTION__, hr);
    return hr;
}

// required DLL export
STDAPI DllUnregisterServer(void)
{
    HRESULT hr = _Module.UnregisterServer();
    Trace("%s hr %d", __FUNCTION__, hr);
    return S_OK;
}
