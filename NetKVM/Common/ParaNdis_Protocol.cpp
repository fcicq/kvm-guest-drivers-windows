/*
 * This file contains implementation of protocol for binding
 * virtio-net and SRIOV device
 *
 * Copyright (c) 2020 Red Hat, Inc.
 * Copyright (c) 2020 Oracle Corporation
 *
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
#include "ParaNdis_Protocol.h"
#include "ParaNdis-SM.h"
#include "Trace.h"

#if NDIS_SUPPORT_NDIS630

#ifdef NETKVM_WPP_ENABLED
#include "ParaNdis_Protocol.tmh"
#endif

class CAdapterEntry : public CNdisAllocatable<CAdapterEntry, '1ORP'>
{
public:
    CAdapterEntry(PARANDIS_ADAPTER *pContext) : m_Adapter(pContext), m_Binding(NULL)
    {
        NdisMoveMemory(m_MacAddress, pContext->CurrentMacAddress, sizeof(m_MacAddress));
    }
    CAdapterEntry(PVOID Binding, const UINT8 *MacAddress) : m_Adapter(NULL), m_Binding(Binding)
    {
        NdisMoveMemory(m_MacAddress, MacAddress, sizeof(m_MacAddress));
    }
    bool Match(PARANDIS_ADAPTER *pContext) const
    {
        return m_Adapter == pContext;
    }
    PARANDIS_ADAPTER *GetAdapter(UCHAR *MacAddress, PVOID BindTo)
    {
        bool same;
        ETH_COMPARE_NETWORK_ADDRESSES_EQ_SAFE(MacAddress, m_MacAddress, &same);
        if (!same || m_Binding)
        {
            return NULL;
        }
        m_Binding = BindTo;
        return m_Adapter;
    }
    PVOID GetBinding(PARANDIS_ADAPTER *pContext)
    {
        bool same;
        ETH_COMPARE_NETWORK_ADDRESSES_EQ_SAFE(pContext->CurrentMacAddress, m_MacAddress, &same);
        if (!same || m_Adapter)
        {
            return NULL;
        }
        m_Adapter = pContext;
        return m_Binding;
    }
    bool HasAdapter() const { return m_Adapter != NULL; }
    void PutAdapter()
    {
        m_Binding = NULL;
    }
    void PutBinding()
    {
        m_Adapter = NULL;
    }
    void NotifyRemoval()
    {
        if (m_Binding)
        {
            Notifier(m_Binding);
            m_Binding = NULL;
        }
    }
private:
    PARANDIS_ADAPTER *m_Adapter;
    PVOID m_Binding = 0;
    UINT8 m_MacAddress[ETH_HARDWARE_ADDRESS_SIZE];
    static void Notifier(PVOID Binding);
    DECLARE_CNDISLIST_ENTRY(CAdapterEntry);
};

class COidWrapper : public CNdisAllocatable <COidWrapper, '2ORP'>
{
public:
    COidWrapper(NDIS_REQUEST_TYPE RequestType, NDIS_OID oid)
    {
        NdisZeroMemory(&m_Request, sizeof(m_Request));
        PVOID data = this;
        NdisMoveMemory(m_Request.SourceReserved, &data, sizeof(data));
        m_Request.Header.Type = NDIS_OBJECT_TYPE_OID_REQUEST;
        m_Request.Header.Revision = NDIS_OID_REQUEST_REVISION_1;
        m_Request.Header.Size = NDIS_SIZEOF_OID_REQUEST_REVISION_1;
        m_Request.PortNumber = NDIS_DEFAULT_PORT_NUMBER;
        m_Request.RequestType = RequestType;
        m_Request.DATA.QUERY_INFORMATION.Oid = oid;
    }
    void Complete(NDIS_STATUS status)
    {
        m_Status = status;
        m_Event.Notify();
    }
    void Wait()
    {
        m_Event.Wait();
    }
    NDIS_STATUS Status() const { return m_Status; }
    NDIS_OID_REQUEST m_Request;
private:
    CNdisEvent m_Event;
    NDIS_STATUS m_Status;
};

class CParaNdisProtocol;

class CProtocolBinding : public CNdisAllocatable<CProtocolBinding, 'TORP'>, public CRefCountingObject
{
public:
    CProtocolBinding(CParaNdisProtocol& Protocol, NDIS_HANDLE BindContext, PNDIS_BIND_PARAMETERS BindParameters) :
        m_Protocol(Protocol),
        m_BindContext(BindContext),
        m_Parameters(BindParameters),
        m_Status(NDIS_STATUS_ADAPTER_NOT_OPEN),
        m_BindingHandle(NULL),
        m_BoundAdapter(NULL)
    {
        TraceNoPrefix(0, "[%s] %p\n", __FUNCTION__, this);
    }
    ~CProtocolBinding()
    {
        TraceNoPrefix(0, "[%s] %p\n", __FUNCTION__, this);
    }
    NDIS_STATUS Bind();
    NDIS_STATUS Unbind(NDIS_HANDLE UnbindContext);
    void Complete(NDIS_STATUS Status)
    {
        m_Status = Status;
        m_Event.Notify();
    }
    void OidComplete(PNDIS_OID_REQUEST OidRequest, NDIS_STATUS Status)
    {
        PVOID data;
        NdisMoveMemory(&data, OidRequest->SourceReserved, sizeof(data));
        COidWrapper *pOid = (COidWrapper *)data;
        pOid->Complete(Status);
    }
    void OnReceive(PNET_BUFFER_LIST Nbls, ULONG NofNbls, ULONG Flags);
    void OnSendCompletion(PNET_BUFFER_LIST Nbls, ULONG Flags);
    void OnAdapterRemoval()
    {
        m_BoundAdapter = NULL;
    }
    void OnStatusIndication(PNDIS_STATUS_INDICATION StatusIndication)
    {
        TraceNoPrefix(0, "[%s] code %X\n", __FUNCTION__, StatusIndication->StatusCode);
    }
    void OnPnPEvent(PNET_PNP_EVENT_NOTIFICATION NetPnPEventNotification)
    {
        TraceNoPrefix(0, "[%s] event %X\n", __FUNCTION__, NetPnPEventNotification->NetPnPEvent.NetEvent);
    }
    CFlowStateMachine m_RxStateMachine;
    CFlowStateMachine m_TxStateMachine;
private:
    void OnLastReferenceGone() override;
    void SetOid(ULONG oid, PVOID data, ULONG size);
    CParaNdisProtocol& m_Protocol;
    NDIS_HANDLE m_BindContext;
    NDIS_HANDLE m_BindingHandle;
    PNDIS_BIND_PARAMETERS m_Parameters;
    PARANDIS_ADAPTER *m_BoundAdapter;
    CNdisEvent m_Event;
    NDIS_STATUS m_Status;
};

static CParaNdisProtocol *ProtocolData = NULL;

class CParaNdisProtocol : public CNdisAllocatable<CParaNdisProtocol, 'TORP'>
{
public:
    CParaNdisProtocol(NDIS_HANDLE DriverHandle) :
        m_DriverHandle(DriverHandle),
        m_ProtocolHandle(NULL)
    {
        NDIS_PROTOCOL_DRIVER_CHARACTERISTICS pchs = {};
        pchs.Name = NDIS_STRING_CONST("netkvmp");
        //pchs.Name = NDIS_STRING_CONST("NDISPROT");
        pchs.Header.Type = NDIS_OBJECT_TYPE_PROTOCOL_DRIVER_CHARACTERISTICS;
        pchs.Header.Revision = NDIS_PROTOCOL_DRIVER_CHARACTERISTICS_REVISION_2;
        pchs.Header.Size = NDIS_SIZEOF_PROTOCOL_DRIVER_CHARACTERISTICS_REVISION_2;
        pchs.MajorNdisVersion = 6;
        pchs.MinorNdisVersion = 30;
        pchs.MajorDriverVersion = (UCHAR)(PARANDIS_MAJOR_DRIVER_VERSION & 0xFF);
        pchs.MinorDriverVersion = (UCHAR)(PARANDIS_MINOR_DRIVER_VERSION & 0xFF);
        pchs.BindAdapterHandlerEx = [](
            _In_ NDIS_HANDLE ProtocolDriverContext,
            _In_ NDIS_HANDLE BindContext,
            _In_ PNDIS_BIND_PARAMETERS BindParameters)
        {
            TraceNoPrefix(0, "[BindAdapterHandlerEx] binding %p\n", BindContext);
            return ((CParaNdisProtocol *)ProtocolDriverContext)->OnBindAdapter(BindContext, BindParameters);
        };
        pchs.OpenAdapterCompleteHandlerEx = [](
            _In_ NDIS_HANDLE ProtocolBindingContext,
            _In_ NDIS_STATUS Status)
        {
            TraceNoPrefix(0, "[OpenAdapterCompleteHandlerEx] %X, ctx %p\n", Status, ProtocolBindingContext);
            CProtocolBinding *binding = (CProtocolBinding *)ProtocolBindingContext;
            binding->Complete(Status);
        };
        pchs.UnbindAdapterHandlerEx = [](
            _In_ NDIS_HANDLE UnbindContext,
            _In_ NDIS_HANDLE ProtocolBindingContext)
        {
            TraceNoPrefix(0, "[UnbindAdapterHandlerEx] ctx %p, binding %p\n", UnbindContext, ProtocolBindingContext);
            CProtocolBinding *binding = (CProtocolBinding *)ProtocolBindingContext;
            return binding->Unbind(UnbindContext);
        };
        pchs.CloseAdapterCompleteHandlerEx = [](
            _In_ NDIS_HANDLE ProtocolBindingContext)
        {
            TraceNoPrefix(0, "[CloseAdapterCompleteHandlerEx] ctx %p\n", ProtocolBindingContext);
            CProtocolBinding *binding = (CProtocolBinding *)ProtocolBindingContext;
            binding->Complete(NDIS_STATUS_SUCCESS);
        };
        pchs.SendNetBufferListsCompleteHandler = [](
            _In_  NDIS_HANDLE             ProtocolBindingContext,
            _In_  PNET_BUFFER_LIST        NetBufferList,
            _In_  ULONG                   SendCompleteFlags)
        {
            TraceNoPrefix(0, "[SendNetBufferListsCompleteHandler] ctx %p\n", ProtocolBindingContext);
            CProtocolBinding *binding = (CProtocolBinding *)ProtocolBindingContext;
            binding->OnSendCompletion(NetBufferList, SendCompleteFlags);
        };
        pchs.ReceiveNetBufferListsHandler = [](
            _In_  NDIS_HANDLE             ProtocolBindingContext,
            _In_  PNET_BUFFER_LIST        NetBufferLists,
            _In_  NDIS_PORT_NUMBER        PortNumber,
            _In_  ULONG                   NumberOfNetBufferLists,
            _In_  ULONG                   ReceiveFlags)
        {
            UNREFERENCED_PARAMETER(PortNumber);
            TraceNoPrefix(0, "[ReceiveNetBufferListsHandler] ctx %p\n", ProtocolBindingContext);
            CProtocolBinding *binding = (CProtocolBinding *)ProtocolBindingContext;
            binding->OnReceive(NetBufferLists, NumberOfNetBufferLists, ReceiveFlags);
        };
        pchs.OidRequestCompleteHandler = [](
            _In_  NDIS_HANDLE             ProtocolBindingContext,
            _In_  PNDIS_OID_REQUEST       OidRequest,
            _In_  NDIS_STATUS             Status
            )
        {
            CProtocolBinding *binding = (CProtocolBinding *)ProtocolBindingContext;
            binding->OidComplete(OidRequest, Status);
        };
        pchs.UninstallHandler = []()
        {
            Destroy(ProtocolData, ProtocolData->DriverHandle());
            ProtocolData = NULL;
        };
        pchs.StatusHandlerEx = [](
            _In_  NDIS_HANDLE             ProtocolBindingContext,
            _In_  PNDIS_STATUS_INDICATION StatusIndication
            )
        {
            CProtocolBinding *binding = (CProtocolBinding *)ProtocolBindingContext;
            binding->OnStatusIndication(StatusIndication);
        };
        pchs.NetPnPEventHandler = [](
            _In_  NDIS_HANDLE                 ProtocolBindingContext,
            _In_  PNET_PNP_EVENT_NOTIFICATION NetPnPEventNotification
            )
        {
            CProtocolBinding *binding = (CProtocolBinding *)ProtocolBindingContext;
            binding->OnPnPEvent(NetPnPEventNotification);
            return NDIS_STATUS_SUCCESS;
        };
        NDIS_STATUS status = NdisRegisterProtocolDriver(this, &pchs, &m_ProtocolHandle);
        TraceNoPrefix(0, "[%s] Registering protocol %wZ = %X\n", __FUNCTION__, &pchs.Name, status);
    }
    ~CParaNdisProtocol()
    {
        if (m_ProtocolHandle)
        {
            TraceNoPrefix(0, "[%s] Deregistering protocol\n", __FUNCTION__);
            NdisDeregisterProtocolDriver(m_ProtocolHandle);
        }
    }
    NDIS_STATUS OnBindAdapter(_In_ NDIS_HANDLE BindContext, _In_ PNDIS_BIND_PARAMETERS BindParameters)
    {
        CProtocolBinding *binding = new (m_DriverHandle)CProtocolBinding(*this, BindContext, BindParameters);
        if (!binding)
        {
            return NDIS_STATUS_RESOURCES;
        }
        NDIS_STATUS status = binding->Bind();
        return status;
    }
    void AddAdapter(PARANDIS_ADAPTER *pContext)
    {
        CAdapterEntry *e = new (m_DriverHandle)CAdapterEntry(pContext);
        if (e)
        {
            TraceNoPrefix(0, "[%s] entry %p for adapter %p\n", __FUNCTION__, e, pContext);
            m_Adapters.PushBack(e);
        }
    }
    bool RemoveAdapter(PARANDIS_ADAPTER *pContext)
    {
        LPCSTR func = __FUNCTION__;
        m_Adapters.ForEachDetachedIf(
            [pContext](CAdapterEntry *e) { return e->Match(pContext); },
            [&](CAdapterEntry *e)
            {
                TraceNoPrefix(0, "[%s] entry %p for adapter %p\n", func, e, pContext);
                e->NotifyRemoval();
                e->Destroy(e, m_DriverHandle);
            }
        );
        bool bDelete = true;
        m_Adapters.ForEach([&](CAdapterEntry *e)
        {
            TraceNoPrefix(0, "[%s] entry %p for adapter %p\n", func, e, pContext);
            if (e->HasAdapter())
            {
                bDelete = false;
            }
        }
        );
        if (bDelete)
        {
            TraceNoPrefix(0, "[%s] no more adapters\n", func);
            Destroy(this, m_DriverHandle);
        }
        return bDelete;
    }
    PARANDIS_ADAPTER *FindAdapter(UCHAR *MacAddress, PVOID BindTo)
    {
        PARANDIS_ADAPTER *p = NULL;
        m_Adapters.ForEach(
            [&](CAdapterEntry *e)
            {
                if (!p)
                {
                    p = e->GetAdapter(MacAddress, BindTo);
                }
            }
        );
        return p;
    }
    NDIS_HANDLE DriverHandle() const { return m_DriverHandle; }
    NDIS_HANDLE ProtocolHandle() const { return m_ProtocolHandle; }
private:
    CNdisList<CAdapterEntry, CMutexProtectedAccess, CCountingObject> m_Adapters;
    NDIS_HANDLE m_DriverHandle;
    NDIS_HANDLE m_ProtocolHandle;
};

void ParaNdis_ProtocolInitialize(NDIS_HANDLE DriverHandle)
{
    ProtocolData = new (DriverHandle) CParaNdisProtocol(DriverHandle);
}

void ParaNdis_ProtocolRegisterAdapter(PARANDIS_ADAPTER *pContext)
{
    if (!ProtocolData)
        return;
    ProtocolData->AddAdapter(pContext);
}

void ParaNdis_ProtocolUnregisterAdapter(PARANDIS_ADAPTER *pContext)
{
    if (!ProtocolData)
        return;
    if (ProtocolData->RemoveAdapter(pContext))
    {
        ProtocolData = NULL;
    }
}

NDIS_STATUS CProtocolBinding::Bind()
{
    NDIS_OPEN_PARAMETERS openParams = {};
    NDIS_MEDIUM medium = NdisMedium802_3;
    UINT mediumIndex;
    NET_FRAME_TYPE frameTypes[2] = { NDIS_ETH_TYPE_802_1X, NDIS_ETH_TYPE_802_1Q };

    AddRef();

    openParams.Header.Type = NDIS_OBJECT_TYPE_OPEN_PARAMETERS;
    openParams.Header.Revision = NDIS_OPEN_PARAMETERS_REVISION_1;
    openParams.Header.Size = NDIS_SIZEOF_OPEN_PARAMETERS_REVISION_1;
    openParams.AdapterName = m_Parameters->AdapterName;
    openParams.MediumArray = &medium;
    openParams.MediumArraySize = 1;
    openParams.SelectedMediumIndex = &mediumIndex;
    openParams.FrameTypeArray = frameTypes;
    openParams.FrameTypeArraySize = ARRAYSIZE(frameTypes);

    NDIS_STATUS status = NdisOpenAdapterEx(m_Protocol.ProtocolHandle(), this, &openParams, m_BindContext, &m_BindingHandle);

    if (status == STATUS_SUCCESS)
    {
        TraceNoPrefix(0, "[%s] %p done\n", __FUNCTION__, this);
        m_Status = STATUS_SUCCESS;
    }
    else if (status == STATUS_PENDING)
    {
        m_Event.Wait();
        status = m_Status;
    }
    if (!NT_SUCCESS(status))
    {
        TraceNoPrefix(0, "[%s] %p, failed %X\n", __FUNCTION__, this, status);
        Unbind(m_BindContext);
    }

    PARANDIS_ADAPTER *pBound = ProtocolData->FindAdapter(m_Parameters->CurrentMacAddress, this);
    if (pBound)
    {
        TraceNoPrefix(0, "[%s] Found failover adapter %p\n", __FUNCTION__, pBound);
#if LATER
        ParaNdis_DoRedirection(pBound, this);
        m_BoundAdapter = pBound;
        m_RxStateMachine.Start();
        m_TxStateMachine.Start();
#endif
    }

    Release();

    return status;
}

NDIS_STATUS CProtocolBinding::Unbind(NDIS_HANDLE UnbindContext)
{
    NDIS_STATUS status = STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(UnbindContext);

#if LATER
    m_RxStateMachine.Stop();
    m_TxStateMachine.Stop();
#endif

    if (m_BindingHandle)
    {
        ULONG dummy = 0;
        SetOid(OID_GEN_CURRENT_PACKET_FILTER, &dummy, sizeof(dummy));
        SetOid(OID_802_3_MULTICAST_LIST, NULL, 0);

        m_Event.Clear();
        status = NdisCloseAdapterEx(m_BindingHandle);
        switch (status)
        {
            case NDIS_STATUS_PENDING:
                m_Event.Wait();
                status = NDIS_STATUS_SUCCESS;
                break;
            case NDIS_STATUS_SUCCESS:
                break;
            default:
                TraceNoPrefix(0, "[%s] %p, failed %X\n", __FUNCTION__, this, status);
                return status;
        }
        m_BindingHandle = NULL;
    }
    Release();
    return status;
}

void CProtocolBinding::OnLastReferenceGone()
{
    Destroy(this, m_Protocol.DriverHandle());
}

void CProtocolBinding::OnReceive(PNET_BUFFER_LIST Nbls, ULONG NofNbls, ULONG Flags)
{
    // Flags may contain RESOURCES  (but why this should make difference?)
    bool bDrop = true;
    UNREFERENCED_PARAMETER(Flags);
#if LATER
    if (m_RxStateMachine.RegisterOutstandingItems(NofNbls))
    {
        if (m_BoundAdapter)
        {
            NdisMIndicateReceiveNetBufferLists(m_BoundAdapter->MiniportHandle, Nbls, 0, NofNbls, Flags);
            bDrop = false;
        }
    }
#endif
    if (bDrop)
    {
        TraceNoPrefix(0, "[%s] dropped %d NBLs\n", __FUNCTION__, NofNbls);
        NdisReturnNetBufferLists(m_BindingHandle, Nbls, 0);
    }
}

void CProtocolBinding::OnSendCompletion(PNET_BUFFER_LIST Nbls, ULONG Flags)
{
    UNREFERENCED_PARAMETER(Nbls);
    UNREFERENCED_PARAMETER(Flags);
}

void CProtocolBinding::SetOid(ULONG oid, PVOID data, ULONG size)
{
    COidWrapper *p = new (m_Protocol.DriverHandle()) COidWrapper(NdisRequestSetInformation, oid);
    if (!p)
    {
        return;
    }
    p->m_Request.DATA.SET_INFORMATION.InformationBuffer = data;
    p->m_Request.DATA.SET_INFORMATION.InformationBufferLength = size;
    NDIS_STATUS status = NdisOidRequest(m_BindingHandle, &p->m_Request);
    if (status == NDIS_STATUS_PENDING)
    {
        p->Wait();
        status = p->Status();
    }
    p->Destroy(p, m_Protocol.DriverHandle());
}

void CAdapterEntry::Notifier(PVOID Binding)
{
    CProtocolBinding *pb = (CProtocolBinding *)Binding;
    pb->OnAdapterRemoval();
}

#else

void ParaNdis_ProtocolUnregisterAdapter(PARANDIS_ADAPTER *) { }
void ParaNdis_ProtocolRegisterAdapter(PARANDIS_ADAPTER *) { }
void ParaNdis_ProtocolInitialize(NDIS_HANDLE) { }

#endif //NDIS_SUPPORT_NDIS630
