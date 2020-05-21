/*
 * Copyright (c) 2020 Red Hat, Inc.
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

#include "stdafx.h"

void Trace (LPCSTR szFormat, ...)
{
    va_list arglist;
    va_start(arglist, szFormat);
    CStringA s;
    s.FormatV(szFormat, arglist);
    s += '\n';
    OutputDebugString(s.GetBuffer());
    va_end(arglist);
}

static LPCWSTR SupportedAdapters[] =
{
    L"ven_8086&dev_1515", //Intel X540 Virtual Function
    L"ven_8086&dev_10ca", //Intel 82576 Virtual Function
    L"ven_8086&dev_15a8", //Intel Ethernet Connection X552
    L"ven_15b3&dev_101a", //Mellanox MT28800 Family

    L"ven_8086&dev_10d3", // Local test only: Intel e1000
};

bool IsSupportedSRIOVAdapter(LPCWSTR pnpId)
{
    for (UINT i = 0; i < ELEMENTS_IN(SupportedAdapters); ++i)
    {
        if (wcsstr(pnpId, SupportedAdapters[i]))
        {
            return true;
        }
    }
    return false;
}
