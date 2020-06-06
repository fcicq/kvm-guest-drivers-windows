#pragma once

#include "ndis56common.h"

void ParaNdis_ProtocolInitialize(NDIS_HANDLE DriverHandle);
void ParaNdis_ProtocolRegisterAdapter(PARANDIS_ADAPTER *pContext);
void ParaNdis_ProtocolUnregisterAdapter(PARANDIS_ADAPTER *pContext);
bool ParaNdis_ProtocolSend(PARANDIS_ADAPTER *pContext, PNET_BUFFER_LIST pNBL);
void ParaNdis_ProtocolReturnNbls(PARANDIS_ADAPTER *pContext, PNET_BUFFER_LIST pNBL, ULONG numNBLs);
void ParaNdis_ProtocolActive();
