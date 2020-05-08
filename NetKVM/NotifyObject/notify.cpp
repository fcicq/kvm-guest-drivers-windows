/*
 * Copyright (c) 2008-2017 Red Hat, Inc.
 * Copyright (c) 2020 Oracle and/or its affiliates
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

#define  VFDEV          0x0001
#define  VIOPRO         0x0010

#define SET_FLAGS(Flag, Val)      (Flag) = ((Flag) | (Val))
#define TEST_FLAGS(Flag, Val)     ((Flag) & (Val))

CNotifyObject::CNotifyObject(VOID)
{
}

CNotifyObject::~CNotifyObject(VOID)
{
}

//----------------------------------------------------------------------------
// INetCfgComponentControl
//----------------------------------------------------------------------------

STDMETHODIMP CNotifyObject::Initialize(INetCfgComponent *pNetCfgCom,
                                       INetCfg          *pNetCfg,
                                       BOOL              fInstalling)
{
    UNREFERENCED_PARAMETER(pNetCfgCom);
    UNREFERENCED_PARAMETER(pNetCfg);
    UNREFERENCED_PARAMETER(fInstalling);

    return S_OK;
}

STDMETHODIMP CNotifyObject::CancelChanges(VOID)
{
    return S_OK;
}

STDMETHODIMP CNotifyObject::ApplyRegistryChanges(VOID)
{
    return S_OK;
}

STDMETHODIMP CNotifyObject::ApplyPnpChanges(
                            INetCfgPnpReconfigCallback* pfCallback)
{
    UNREFERENCED_PARAMETER(pfCallback);

    return S_OK;
}

//----------------------------------------------------------------------------
// INetCfgComponentSetup
//----------------------------------------------------------------------------

STDMETHODIMP CNotifyObject::Install(DWORD dwSetupFlags)
{
    UNREFERENCED_PARAMETER(dwSetupFlags);

    return S_OK;
}

STDMETHODIMP CNotifyObject::Upgrade(IN DWORD dwSetupFlags,
                                    IN DWORD dwUpgradeFromBuildNo)
{
    UNREFERENCED_PARAMETER(dwSetupFlags);
    UNREFERENCED_PARAMETER(dwUpgradeFromBuildNo);

    return S_OK;
}

STDMETHODIMP CNotifyObject::ReadAnswerFile(PCWSTR pszAnswerFile,
                                           PCWSTR pszAnswerSection)
{
    UNREFERENCED_PARAMETER(pszAnswerFile);
    UNREFERENCED_PARAMETER(pszAnswerSection);

    return S_OK;
}

STDMETHODIMP CNotifyObject::Removing(VOID)
{
    return S_OK;
}

//----------------------------------------------------------------------------
// INetCfgComponentNotifyBinding
//----------------------------------------------------------------------------

STDMETHODIMP CNotifyObject::QueryBindingPath(IN DWORD dwChangeFlag,
                                             IN INetCfgBindingPath *pNetCfgBindPath)
{
    UNREFERENCED_PARAMETER(dwChangeFlag);
    UNREFERENCED_PARAMETER(pNetCfgBindPath);

    return S_OK;
}

/*
 * Check whether the binding is the one we need - the upper is the VirtIO
 * protocol and the lower is the VF miniport driver.
 * Return value - iRet, its bitmap(VFDEV/VIOPRO) represents existence
 * of the VirtIO protocol and VF miniport in the binding.
 */
INT CNotifyObject::CheckProtocolandDevInf(INetCfgBindingPath *pNetCfgBindingPath,
                                          INetCfgComponent  **ppUpNetCfgCom,
                                          INetCfgComponent  **ppLowNetCfgCom
)
{
    IEnumNetCfgBindingInterface    *pEnumNetCfgBindIf;
    INetCfgBindingInterface        *pNetCfgBindIf;
    ULONG                           ulNum;
    LPWSTR                          pszwLowInfId;
    LPWSTR                          pszwUpInfId;
    INT                             ret = 0;

    Trace("%s", __FUNCTION__);

    if (S_OK != pNetCfgBindingPath->EnumBindingInterfaces(&pEnumNetCfgBindIf))
        goto end4;

    if (S_OK != pEnumNetCfgBindIf->Next(1, &pNetCfgBindIf, &ulNum))
        goto end3;

    if (S_OK != pNetCfgBindIf->GetUpperComponent(ppUpNetCfgCom))
        goto end2;

    if (S_OK != pNetCfgBindIf->GetLowerComponent(ppLowNetCfgCom))
        goto end2;

    if (S_OK != (*ppUpNetCfgCom)->GetId(&pszwUpInfId))
        goto end2;

    if (S_OK != (*ppLowNetCfgCom)->GetId(&pszwLowInfId))
        goto end1;

    // Upper is VIO protocol
    if (!_wcsicmp(pszwUpInfId, NETKVM_PROTOCOL_NAME_W))
        SET_FLAGS(ret, VIOPRO);
    // Lower is VF device miniport
    if (IsSupportedSRIOVAdapter(pszwLowInfId))
        SET_FLAGS(ret, VFDEV);
    Trace("pszwUpInfId %S, pszwLowInfId %S", pszwUpInfId, pszwLowInfId);

    CoTaskMemFree(pszwLowInfId);
end1:
    CoTaskMemFree(pszwUpInfId);
end2:
    ReleaseObj(pNetCfgBindIf);
end3:
    ReleaseObj(pEnumNetCfgBindIf);
end4:
    Trace("%s <=iRet 0x%X", __FUNCTION__, ret);
    return ret;
}

/*
 * When the new binding(VirtIO Protocol<->VF Miniport) is added, enumerate
 * all other bindings(non VirtIO Protocol<->VF Miniport) and disable them.
 * When the a binding(VirtIO Protocol<->VF Miniport) is removed, enumerate
 * all other bindings(non VirtIO Protocol<->VF Miniport) and enable them.
 */
STDMETHODIMP CNotifyObject::NotifyBindingPath(IN DWORD dwChangeFlag,
                                              IN INetCfgBindingPath *pNetCfgBindPath)
{
    INetCfgComponent     *pUpNetCfgCom;
    INetCfgComponent     *pLowNetCfgCom;
    BOOL                  bAdd, bRemove;
    INT                   iRet = 0;

    pUpNetCfgCom = NULL;
    pLowNetCfgCom = NULL;

    Trace("-->%s change flags 0x%X", __FUNCTION__, dwChangeFlag);

    bAdd = dwChangeFlag & NCN_ADD;
    bRemove = dwChangeFlag & NCN_REMOVE;

    // Check and operate when binding is being added or removed
    if (bAdd || bRemove) {
        iRet = CheckProtocolandDevInf(pNetCfgBindPath,
                                      &pUpNetCfgCom, &pLowNetCfgCom);
        if (TEST_FLAGS(iRet, VFDEV) && TEST_FLAGS(iRet, VIOPRO)) {
            if (bAdd) {
                // Enumerate and disable other bindings except for
                // VIOprotocol<->VF miniport
                if (EnableVFBindings(pLowNetCfgCom, FALSE))
                    Trace("Failed to disable non VIO protocol to VF miniport");
            }
            else {
                // Enumerate and enable other bindings except for
                // VIOprotocol<->VF miniport
                if (EnableVFBindings(pLowNetCfgCom, TRUE))
                    Trace("Failed to enable non VIO protocol to VF miniport");
            }
        }
        if (pUpNetCfgCom != NULL)
        {
            ReleaseObj(pUpNetCfgCom);
        }
        if (pLowNetCfgCom != NULL)
        {
            ReleaseObj(pLowNetCfgCom);
        }
    }
    Trace("<--%s", __FUNCTION__);

    return S_OK;
}

//----------------------------------------------------------------------------
// INetCfgComponentNotifyGlobal
//----------------------------------------------------------------------------

STDMETHODIMP CNotifyObject::GetSupportedNotifications(
                                  OUT DWORD *pdwNotificationFlag)
{
    Trace("-->%s", __FUNCTION__);

    *pdwNotificationFlag = NCN_NET | NCN_NETTRANS | NCN_ADD | NCN_REMOVE |
                           NCN_BINDING_PATH | NCN_ENABLE | NCN_DISABLE;

    Trace("<--%s", __FUNCTION__);

    return S_OK;
}

//When addition of a binding path is about to occur,
//disable it if it is VirtIO protocol<->non VF miniport binding.
STDMETHODIMP CNotifyObject::SysQueryBindingPath(DWORD dwChangeFlag,
                                                INetCfgBindingPath *pNetCfgBindPath)
{
    INetCfgComponent     *pUpNetCfgCom;
    INetCfgComponent     *pLowNetCfgCom;
    INT                  iRet = 0;
    HRESULT              hResult = S_OK;

    pUpNetCfgCom = NULL;
    pLowNetCfgCom = NULL;
    Trace("-->%s", __FUNCTION__);

    if (dwChangeFlag & (NCN_ENABLE | NCN_ADD)) {
        iRet = CheckProtocolandDevInf(pNetCfgBindPath,
                                      &pUpNetCfgCom, &pLowNetCfgCom);
        if (!TEST_FLAGS(iRet, VFDEV) && TEST_FLAGS(iRet, VIOPRO)) {
            // Upper protocol is virtio protocol and lower id is not
            // vf device id, disable the binding.
            hResult = NETCFG_S_DISABLE_QUERY;
        }
        if (pUpNetCfgCom != NULL)
        {
            ReleaseObj(pUpNetCfgCom);
        }
        if (pLowNetCfgCom != NULL)
        {
            ReleaseObj(pLowNetCfgCom);
        }
    }
    Trace("<--%s HRESULT = %x", __FUNCTION__, hResult);

    return hResult;
}

STDMETHODIMP CNotifyObject::SysNotifyBindingPath(DWORD dwChangeFlag,
                                                 INetCfgBindingPath* pNetCfgBindPath)
{
    UNREFERENCED_PARAMETER(dwChangeFlag);
    UNREFERENCED_PARAMETER(pNetCfgBindPath);

    return S_OK;
}

STDMETHODIMP CNotifyObject::SysNotifyComponent(DWORD dwChangeFlag,
                                               INetCfgComponent* pNetCfgCom)
{
    UNREFERENCED_PARAMETER(dwChangeFlag);
    UNREFERENCED_PARAMETER(pNetCfgCom);

    return S_OK;
}

//Enable/Disable the bindings of non VIO protocols to the VF miniport
HRESULT CNotifyObject::EnableVFBindings(INetCfgComponent *pNetCfgCom,
                                        BOOL bEnable)
{
    IEnumNetCfgBindingPath      *pEnumNetCfgBindPath;
    INetCfgBindingPath          *pNetCfgBindPath;
    INetCfgComponentBindings    *pNetCfgComBind;
    HRESULT                     hResult;
    ULONG                       ulNum;

    Trace("-->%s bEnable = %d", __FUNCTION__, bEnable);

    // Get the binding path enumerator.
    pEnumNetCfgBindPath = NULL;
    pNetCfgComBind = NULL;
    hResult = pNetCfgCom->QueryInterface(IID_INetCfgComponentBindings,
                                         (PVOID *)&pNetCfgComBind);
    if (S_OK == hResult) {
        hResult = pNetCfgComBind->EnumBindingPaths(EBP_ABOVE,
                                                   &pEnumNetCfgBindPath);
        ReleaseObj(pNetCfgComBind);
    }
    else
        return hResult;

    if (hResult == S_OK) {
        pNetCfgBindPath = NULL;
        hResult = pEnumNetCfgBindPath->Next(1, &pNetCfgBindPath, &ulNum);
        // Enumerate every binding path.
        while (hResult == S_OK) {
            EnableBinding(pNetCfgBindPath, bEnable);
            ReleaseObj(pNetCfgBindPath);
            pNetCfgBindPath = NULL;
            hResult = pEnumNetCfgBindPath->Next(1, &pNetCfgBindPath, &ulNum);
        }
        ReleaseObj(pEnumNetCfgBindPath);
    }
    else {
        Trace("Failed to get the binding path enumerator, "
                 "bindings will not be %s.", bEnable ? "enabled" : "disabled");
    }

    Trace("%s\n", __FUNCTION__);

    return hResult;
}

//Enable or disable bindings with non VIO protocol
VOID CNotifyObject::EnableBinding(INetCfgBindingPath *pNetCfgBindPath,
                                  BOOL bEnable)
{
    INetCfgComponent     *pUpNetCfgCom;
    INetCfgComponent     *pLowNetCfgCom;
    INT                   iRet;

    pUpNetCfgCom = NULL;
    pLowNetCfgCom = NULL;

    iRet = CheckProtocolandDevInf(pNetCfgBindPath,
                                  &pUpNetCfgCom, &pLowNetCfgCom);
    if (!TEST_FLAGS(iRet, VIOPRO))
        pNetCfgBindPath->Enable(bEnable);
    if (pUpNetCfgCom != NULL)
        ReleaseObj(pUpNetCfgCom);
    if (pLowNetCfgCom != NULL)
        ReleaseObj(pLowNetCfgCom);

    return;
}
