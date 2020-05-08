#pragma once

#include "ndis56common.h"

void ParaNdis_ProtocolInitialize(NDIS_HANDLE DriverHandle);
void ParaNdis_ProtocolRegisterAdapter(PARANDIS_ADAPTER *pContext);
void ParaNdis_ProtocolUnregisterAdapter(PARANDIS_ADAPTER *pContext);
