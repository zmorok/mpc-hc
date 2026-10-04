/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015, 2017 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"

#include <d3d9.h>
#include <dxva.h>
#include <dxva2api.h>
#include <evr.h>
#include <afxmt.h>
#include "moreuuids.h"

#include "IPinHook.h"
#include "AllocatorCommon.h"

#include "../../../mpc-hc/FGFilterLAV.h"
#include "Variables.h"

REFERENCE_TIME g_tSegmentStart = 0;
REFERENCE_TIME g_tSampleStart = 0;
extern double g_dRate;

IPinCVtbl* g_pPinCVtbl_NewSegment = nullptr;
IPinCVtbl* g_pPinCVtbl_ReceiveConnection = nullptr;
IMemInputPinCVtbl* g_pMemInputPinCVtbl = nullptr;
IPinC* g_pPinC_NewSegment = nullptr;

struct D3DFORMAT_TYPE {
    int Format;
    LPCTSTR Description;
};

const D3DFORMAT_TYPE D3DFormatType[] = {
    { D3DFMT_UNKNOWN, _T("D3DFMT_UNKNOWN      ") },
    { D3DFMT_R8G8B8, _T("D3DFMT_R8G8B8       ") },
    { D3DFMT_A8R8G8B8, _T("D3DFMT_A8R8G8B8     ") },
    { D3DFMT_X8R8G8B8, _T("D3DFMT_X8R8G8B8     ") },
    { D3DFMT_R5G6B5, _T("D3DFMT_R5G6B5       ") },
    { D3DFMT_X1R5G5B5, _T("D3DFMT_X1R5G5B5     ") },
    { D3DFMT_A1R5G5B5, _T("D3DFMT_A1R5G5B5     ") },
    { D3DFMT_A4R4G4B4, _T("D3DFMT_A4R4G4B4     ") },
    { D3DFMT_R3G3B2, _T("D3DFMT_R3G3B2       ") },
    { D3DFMT_A8, _T("D3DFMT_A8           ") },
    { D3DFMT_A8R3G3B2, _T("D3DFMT_A8R3G3B2     ") },
    { D3DFMT_X4R4G4B4, _T("D3DFMT_X4R4G4B4     ") },
    { D3DFMT_A2B10G10R10, _T("D3DFMT_A2B10G10R10  ") },
    { D3DFMT_A8B8G8R8, _T("D3DFMT_A8B8G8R8     ") },
    { D3DFMT_X8B8G8R8, _T("D3DFMT_X8B8G8R8     ") },
    { D3DFMT_G16R16, _T("D3DFMT_G16R16       ") },
    { D3DFMT_A2R10G10B10, _T("D3DFMT_A2R10G10B10  ") },
    { D3DFMT_A16B16G16R16, _T("D3DFMT_A16B16G16R16 ") },
    { D3DFMT_A8P8, _T("D3DFMT_A8P8         ") },
    { D3DFMT_P8, _T("D3DFMT_P8           ") },
    { D3DFMT_L8, _T("D3DFMT_L8           ") },
    { D3DFMT_A8L8, _T("D3DFMT_A8L8         ") },
    { D3DFMT_A4L4, _T("D3DFMT_A4L4         ") },
    { D3DFMT_X8L8V8U8, _T("D3DFMT_X8L8V8U8     ") },
    { D3DFMT_Q8W8V8U8, _T("D3DFMT_Q8W8V8U8     ") },
    { D3DFMT_V16U16, _T("D3DFMT_V16U16       ") },
    { D3DFMT_A2W10V10U10, _T("D3DFMT_A2W10V10U10  ") },
    { D3DFMT_UYVY, _T("D3DFMT_UYVY         ") },
    { D3DFMT_R8G8_B8G8, _T("D3DFMT_R8G8_B8G8    ") },
    { D3DFMT_YUY2, _T("D3DFMT_YUY2         ") },
    { D3DFMT_G8R8_G8B8, _T("D3DFMT_G8R8_G8B8    ") },
    { D3DFMT_DXT1, _T("D3DFMT_DXT1         ") },
    { D3DFMT_DXT2, _T("D3DFMT_DXT2         ") },
    { D3DFMT_DXT3, _T("D3DFMT_DXT3         ") },
    { D3DFMT_DXT4, _T("D3DFMT_DXT4         ") },
    { D3DFMT_DXT5, _T("D3DFMT_DXT5         ") },
    { D3DFMT_D16_LOCKABLE, _T("D3DFMT_D16_LOCKABLE ") },
    { D3DFMT_D32, _T("D3DFMT_D32          ") },
    { D3DFMT_D15S1, _T("D3DFMT_D15S1        ") },
    { D3DFMT_D24S8, _T("D3DFMT_D24S8        ") },
    { D3DFMT_D24X8, _T("D3DFMT_D24X8        ") },
    { D3DFMT_D24X4S4, _T("D3DFMT_D24X4S4      ") },
    { D3DFMT_D16, _T("D3DFMT_D16          ") },
    { D3DFMT_D32F_LOCKABLE, _T("D3DFMT_D32F_LOCKABLE") },
    { D3DFMT_D24FS8, _T("D3DFMT_D24FS8       ") },
    { D3DFMT_L16, _T("D3DFMT_L16          ") },
    { D3DFMT_VERTEXDATA, _T("D3DFMT_VERTEXDATA   ") },
    { D3DFMT_INDEX16, _T("D3DFMT_INDEX16      ") },
    { D3DFMT_INDEX32, _T("D3DFMT_INDEX32      ") },
    { D3DFMT_Q16W16V16U16, _T("D3DFMT_Q16W16V16U16 ") },

    { MAKEFOURCC('N', 'V', '1', '2'), _T("D3DFMT_NV12") },
    { MAKEFOURCC('N', 'V', '2', '4'), _T("D3DFMT_NV24") },
};

LPCTSTR FindD3DFormat(const D3DFORMAT Format)
{
    for (int i = 0; i < _countof(D3DFormatType); i++) {
        if (Format == D3DFormatType[i].Format) {
            return D3DFormatType[i].Description;
        }
    }

    return D3DFormatType[0].Description;
}

// === DirectShow hooks
static void SetNativeSampleFlushing(bool flushing);
static HRESULT(STDMETHODCALLTYPE* BeginFlushOrg)(IPinC* This) = nullptr;
static HRESULT(STDMETHODCALLTYPE* EndFlushOrg)(IPinC* This) = nullptr;

static HRESULT STDMETHODCALLTYPE BeginFlushMine(IPinC* This)
{
    if (g_pPinC_NewSegment == This) {
        // Wake a blocked producer before EVR waits for its streaming thread.
        // The presenter keeps the samples until the mixer finishes its read.
        SetNativeSampleFlushing(true);
    }
    return BeginFlushOrg(This);
}

static HRESULT STDMETHODCALLTYPE EndFlushMine(IPinC* This)
{
    const HRESULT hr = EndFlushOrg(This);
    if (g_pPinC_NewSegment == This && SUCCEEDED(hr)) {
        SetNativeSampleFlushing(false);
    }
    return hr;
}

static HRESULT(STDMETHODCALLTYPE* NewSegmentOrg)(IPinC* This, /* [in] */ REFERENCE_TIME tStart, /* [in] */ REFERENCE_TIME tStop, /* [in] */ double dRate) = nullptr;

static HRESULT STDMETHODCALLTYPE NewSegmentMine(IPinC* This, /* [in] */ REFERENCE_TIME tStart, /* [in] */ REFERENCE_TIME tStop, /* [in] */ double dRate)
{
    if (g_pPinC_NewSegment == This) {
        g_tSegmentStart = tStart;
        g_dRate = dRate;
    }

    return NewSegmentOrg(This, tStart, tStop, dRate);
}

static HRESULT(STDMETHODCALLTYPE* ReceiveConnectionOrg)(IPinC* This, /* [in] */ IPinC* pConnector, /* [in] */ const AM_MEDIA_TYPE* pmt) = nullptr;

static HRESULT STDMETHODCALLTYPE ReceiveConnectionMine(IPinC* This, /* [in] */ IPinC* pConnector, /* [in] */ const AM_MEDIA_TYPE* pmt)
{
    // Force-reject P010 and P016 pixel formats due to Microsoft bug ...
    if (pmt && (pmt->subtype == MEDIASUBTYPE_P010 || pmt->subtype == MEDIASUBTYPE_P016)) {
        // ... but allow LAV Video Decoder to do that itself in order to support 10bit DXVA.
        if (GetCLSID((IPin*)pConnector) != GUID_LAVVideo) {
            return VFW_E_TYPE_NOT_ACCEPTED;
        }
    }

    return ReceiveConnectionOrg(This, pConnector, pmt);
}


static HRESULT(STDMETHODCALLTYPE* ReceiveOrg)(IMemInputPinC* This, IMediaSample* pSample) = nullptr;
static HRESULT(STDMETHODCALLTYPE* ReceiveCanBlockOrg)(IMemInputPinC* This) = nullptr;

static CCritSec g_nativeSampleLock;
static bool g_retainNativeSamples = false;
static bool g_evrMixerReady = false;
static REFERENCE_TIME g_nativeSampleDeadline = -1;
static UINT g_nativeRepaintIntervalMs = 0;
static ULONGLONG g_lastNativeInputTick = 0;
static bool g_nativeSkippedSamples = false;
static IMemInputPinC* g_pNativeInputPin = nullptr;
static bool g_nativeSampleFlushing = false;
static HRESULT g_nativeSampleError = S_OK;
static CEvent g_nativeSampleChanged(FALSE, FALSE);
struct NativeSample {
    CComPtr<IMediaSample> sample;
    REFERENCE_TIME time;
    bool processed = false;
    bool discontinuity = false;
};
static std::vector<NativeSample> g_nativeSamples;

static void SetNativeSampleFlushing(bool flushing)
{
    CAutoLock lock(&g_nativeSampleLock);
    g_nativeSampleFlushing = flushing;
    g_nativeSampleChanged.SetEvent();
}

bool HasNativeSamples()
{
    CAutoLock lock(&g_nativeSampleLock);
    return !g_nativeSamples.empty();
}

bool IsEVRMixerReady()
{
    CAutoLock lock(&g_nativeSampleLock);
    return !g_retainNativeSamples || g_evrMixerReady;
}

void MarkEVRMixerReady()
{
    CAutoLock lock(&g_nativeSampleLock);
    g_evrMixerReady = true;
}

void SetNativeSampleDeadline(REFERENCE_TIME time, UINT repaintIntervalMs)
{
    CAutoLock lock(&g_nativeSampleLock);
    g_nativeSampleDeadline = time;
    g_nativeRepaintIntervalMs = repaintIntervalMs;
}

void FailNativeSamples(HRESULT error)
{
    CAutoLock lock(&g_nativeSampleLock);
    g_nativeSampleError = error;
    g_nativeSampleChanged.SetEvent();
}

bool MarkNativeSamplesProcessed(REFERENCE_TIME sampleTime)
{
    CAutoLock lock(&g_nativeSampleLock);
    bool discontinuity = false;
    for (auto& sample : g_nativeSamples) {
        if (!sample.processed && sample.time <= sampleTime) {
            sample.processed = true;
            discontinuity |= sample.discontinuity;
        }
    }
    g_nativeSampleChanged.SetEvent();
    return discontinuity;
}

void ReleaseNativeSamplesBefore(REFERENCE_TIME sampleTime)
{
    CAutoLock lock(&g_nativeSampleLock);
    g_nativeSamples.erase(std::remove_if(g_nativeSamples.begin(), g_nativeSamples.end(),
        [sampleTime](const NativeSample& sample) { return sample.time < sampleTime; }), g_nativeSamples.end());
    g_nativeSampleChanged.SetEvent();
}

void ClearNativeSamples()
{
    CAutoLock lock(&g_nativeSampleLock);
    g_nativeSamples.clear();
    g_evrMixerReady = false;
    g_nativeSampleDeadline = -1;
    g_lastNativeInputTick = 0;
    g_nativeSkippedSamples = false;
    g_nativeSampleError = S_OK;
    g_nativeSampleChanged.SetEvent();
}

static HRESULT STDMETHODCALLTYPE ReceiveCanBlockMine(IMemInputPinC* This)
{
    return g_retainNativeSamples && This == g_pNativeInputPin ? S_OK : ReceiveCanBlockOrg(This);
}

static HRESULT STDMETHODCALLTYPE ReceiveMine(IMemInputPinC* This, IMediaSample* pSample)
{
    REFERENCE_TIME rtStart = 0, rtStop = 0;
    const bool hasTime = pSample && SUCCEEDED(pSample->GetTime(&rtStart, &rtStop));
    if (hasTime) {
        g_tSampleStart = rtStart;
    }

    bool retained = false;
    if (g_retainNativeSamples && This == g_pNativeInputPin && hasTime
            && rtStop > 0 && pSample->IsPreroll() != S_OK) {
        CComPtr<IMFGetService> pGS;
        if (SUCCEEDED(pSample->QueryInterface(IID_PPV_ARGS(&pGS))) && pGS) {
            CComPtr<IDirect3DSurface9> pSurface;
            if (SUCCEEDED(pGS->GetService(MR_BUFFER_SERVICE, IID_PPV_ARGS(&pSurface))) && pSurface) {
                // A surface reference alone does not reserve the decoder allocator's
                // slot. Keep its media sample until the mixer's GPU read completes.
                CAutoLock lock(&g_nativeSampleLock);
                if (g_nativeSampleFlushing) {
                    return S_FALSE;
                }
                if (FAILED(g_nativeSampleError)) {
                    return g_nativeSampleError;
                }
                const ULONGLONG now = GetTickCount64();
                if (rtStop < g_nativeSampleDeadline && pSample->IsDiscontinuity() != S_OK
                        && g_lastNativeInputTick && now - g_lastNativeInputTick < g_nativeRepaintIntervalMs) {
                    // The decoded picture is already late. Skip its expensive GPU
                    // conversion, while still forwarding an image each refresh.
                    g_nativeSkippedSamples = true;
                    return S_OK;
                }
                g_lastNativeInputTick = now;
                g_nativeSamples.push_back({pSample, rtStart, false, g_nativeSkippedSamples});
                g_nativeSkippedSamples = false;
                retained = true;
            }
        }
    }

    HRESULT hr = ReceiveOrg(This, pSample);
    if (retained && hr == S_OK) {
        // Let the next input arrive once the mixer has submitted this one.
        // Its allocator sample remains retained until GPU completion; waiting
        // for that completion here would serialize decode and processing.
        for (;;) {
            {
                CAutoLock lock(&g_nativeSampleLock);
                if (g_nativeSampleFlushing) {
                    return S_FALSE;
                }
                if (FAILED(g_nativeSampleError)) {
                    return g_nativeSampleError;
                }
                auto it = std::find_if(g_nativeSamples.begin(), g_nativeSamples.end(),
                    [pSample](const NativeSample& sample) { return sample.sample == pSample; });
                if (it == g_nativeSamples.end() || it->processed) {
                    break;
                }
            }
            WaitForSingleObject(g_nativeSampleChanged, 10);
        }
    }
    if (retained && hr != S_OK) {
        CAutoLock lock(&g_nativeSampleLock);
        g_nativeSamples.erase(std::remove_if(g_nativeSamples.begin(), g_nativeSamples.end(),
            [pSample](const NativeSample& sample) { return sample.sample == pSample; }), g_nativeSamples.end());
    }
    return hr;
}

bool HookReceiveConnection(IBaseFilter* pBF)
{
    if (!pBF || g_pPinCVtbl_ReceiveConnection) {
        return false;
    }

    if (CComPtr<IPin> pPin = GetFirstPin(pBF)) {
        IPinC* pPinC = (IPinC*)(IPin*)pPin;

        DWORD flOldProtect = 0;
        if (VirtualProtect(pPinC->lpVtbl, sizeof(IPinCVtbl), PAGE_EXECUTE_WRITECOPY, &flOldProtect)) {
            if (ReceiveConnectionOrg == nullptr) {
                ReceiveConnectionOrg = pPinC->lpVtbl->ReceiveConnection;
            }
            pPinC->lpVtbl->ReceiveConnection = ReceiveConnectionMine;
            FlushInstructionCache(GetCurrentProcess(), pPinC->lpVtbl, sizeof(IPinCVtbl));
            VirtualProtect(pPinC->lpVtbl, sizeof(IPinCVtbl), flOldProtect, &flOldProtect);
            g_pPinCVtbl_ReceiveConnection = pPinC->lpVtbl;
            return true;
        } else {
            TRACE(_T("HookWorkAroundVideoDriversBug: Could not hook the VTable"));
            ASSERT(FALSE);
        }
    }
    return false;
}

void UnhookReceiveConnection()
{
    // Unhook previous VTable
    if (g_pPinCVtbl_ReceiveConnection) {
        DWORD flOldProtect = 0;
        if (VirtualProtect(g_pPinCVtbl_ReceiveConnection, sizeof(IPinCVtbl), PAGE_EXECUTE_WRITECOPY, &flOldProtect)) {
            if (g_pPinCVtbl_ReceiveConnection->ReceiveConnection == ReceiveConnectionMine) {
                g_pPinCVtbl_ReceiveConnection->ReceiveConnection = ReceiveConnectionOrg;
            }
            ReceiveConnectionOrg = nullptr;
            FlushInstructionCache(GetCurrentProcess(), g_pPinCVtbl_ReceiveConnection, sizeof(IPinCVtbl));
            VirtualProtect(g_pPinCVtbl_ReceiveConnection, sizeof(IPinCVtbl), flOldProtect, &flOldProtect);
            g_pPinCVtbl_ReceiveConnection = nullptr;
        } else {
            TRACE(_T("UnhookReceiveConnection: Could not unhook previous VTable"));
            ASSERT(FALSE);
        }
    }
}

void UnhookNewSegment()
{
    // Unhook previous VTables
    if (g_pPinCVtbl_NewSegment) {
        DWORD flOldProtect = 0;
        if (VirtualProtect(g_pPinCVtbl_NewSegment, sizeof(IPinCVtbl), PAGE_EXECUTE_WRITECOPY, &flOldProtect)) {
            if (g_pPinCVtbl_NewSegment->NewSegment == NewSegmentMine) {
                g_pPinCVtbl_NewSegment->NewSegment = NewSegmentOrg;
            }
            if (g_pPinCVtbl_NewSegment->BeginFlush == BeginFlushMine) {
                g_pPinCVtbl_NewSegment->BeginFlush = BeginFlushOrg;
            }
            if (g_pPinCVtbl_NewSegment->EndFlush == EndFlushMine) {
                g_pPinCVtbl_NewSegment->EndFlush = EndFlushOrg;
            }
            FlushInstructionCache(GetCurrentProcess(), g_pPinCVtbl_NewSegment, sizeof(IPinCVtbl));
            VirtualProtect(g_pPinCVtbl_NewSegment, sizeof(IPinCVtbl), flOldProtect, &flOldProtect);
            g_pPinCVtbl_NewSegment = nullptr;
            g_pPinC_NewSegment = nullptr;
            NewSegmentOrg = nullptr;
            BeginFlushOrg = nullptr;
            EndFlushOrg = nullptr;
        } else {
            TRACE(_T("UnhookNewSegment: Could not unhook g_pPinCVtbl VTable"));
            ASSERT(FALSE);
        }
    }
}

void UnhookReceive()
{
    g_retainNativeSamples = false;
    g_evrMixerReady = false;
    g_pNativeInputPin = nullptr;
    ClearNativeSamples();
    // Unhook previous VTables
    if (g_pMemInputPinCVtbl) {
        DWORD flOldProtect = 0;
        if (VirtualProtect(g_pMemInputPinCVtbl, sizeof(IMemInputPinCVtbl), PAGE_EXECUTE_WRITECOPY, &flOldProtect)) {
            if (g_pMemInputPinCVtbl->Receive == ReceiveMine) {
                g_pMemInputPinCVtbl->Receive = ReceiveOrg;
            }
            if (g_pMemInputPinCVtbl->ReceiveCanBlock == ReceiveCanBlockMine) {
                g_pMemInputPinCVtbl->ReceiveCanBlock = ReceiveCanBlockOrg;
            }
            FlushInstructionCache(GetCurrentProcess(), g_pMemInputPinCVtbl, sizeof(IMemInputPinCVtbl));
            VirtualProtect(g_pMemInputPinCVtbl, sizeof(IMemInputPinCVtbl), flOldProtect, &flOldProtect);
            g_pMemInputPinCVtbl = nullptr;
            ReceiveOrg = nullptr;
            ReceiveCanBlockOrg = nullptr;
        } else {
            TRACE(_T("UnhookReceive: Could not unhook g_pMemInputPinCVtbl VTable"));
            ASSERT(FALSE);
        }
    }
}

bool HookNewSegment(IPinC* pPinC)
{
    if (!pPinC || g_pPinCVtbl_NewSegment) {
        return false;
    }

    DWORD flOldProtect = 0;
    if (VirtualProtect(pPinC->lpVtbl, sizeof(IPinCVtbl), PAGE_EXECUTE_WRITECOPY, &flOldProtect)) {
        g_tSegmentStart = 0;
        g_dRate = 1.0;
        if (NewSegmentOrg == nullptr) {
            NewSegmentOrg = pPinC->lpVtbl->NewSegment;
        }
        BeginFlushOrg = pPinC->lpVtbl->BeginFlush;
        EndFlushOrg = pPinC->lpVtbl->EndFlush;
        pPinC->lpVtbl->BeginFlush = BeginFlushMine;
        pPinC->lpVtbl->EndFlush = EndFlushMine;
        pPinC->lpVtbl->NewSegment = NewSegmentMine; // Function sets global variable(s)
        FlushInstructionCache(GetCurrentProcess(), pPinC->lpVtbl, sizeof(IPinCVtbl));
        VirtualProtect(pPinC->lpVtbl, sizeof(IPinCVtbl), flOldProtect, &flOldProtect);
        g_pPinCVtbl_NewSegment = pPinC->lpVtbl;
        g_pPinC_NewSegment = pPinC;
        return true;
    } else {
        TRACE(_T("HookNewSegment: Could not unhook VTable"));
        ASSERT(FALSE);
    }

    return false;
}

bool HookReceive(IMemInputPinC* pMemInputPinC, bool retainNativeSamples)
{
    if (!pMemInputPinC || g_pMemInputPinCVtbl) {
        return false;
    }

    DWORD flOldProtect = 0;
    if (VirtualProtect(pMemInputPinC->lpVtbl, sizeof(IMemInputPinCVtbl), PAGE_EXECUTE_WRITECOPY, &flOldProtect)) {
        g_tSampleStart = 0;
        g_retainNativeSamples = retainNativeSamples;
        g_evrMixerReady = false;
        g_pNativeInputPin = pMemInputPinC;
        SetNativeSampleFlushing(false);
        if (ReceiveOrg == nullptr) {
            ReceiveOrg = pMemInputPinC->lpVtbl->Receive;
        }
        ReceiveCanBlockOrg = pMemInputPinC->lpVtbl->ReceiveCanBlock;
        pMemInputPinC->lpVtbl->ReceiveCanBlock = ReceiveCanBlockMine;
        pMemInputPinC->lpVtbl->Receive = ReceiveMine; // Function sets global variable(s)
        FlushInstructionCache(GetCurrentProcess(), pMemInputPinC->lpVtbl, sizeof(IMemInputPinCVtbl));
        VirtualProtect(pMemInputPinC->lpVtbl, sizeof(IMemInputPinCVtbl), flOldProtect, &flOldProtect);
        g_pMemInputPinCVtbl = pMemInputPinC->lpVtbl;
        return true;
    } else {
        TRACE(_T("HookReceive: Could not unhook VTable"));
        ASSERT(FALSE);
    }

    return false;
}
