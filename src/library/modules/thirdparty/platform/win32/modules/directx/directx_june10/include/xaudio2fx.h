// Original: library/modules/thirdparty/platform/win32/modules/directx/directx_june10/include/xaudio2fx.h
//
// The XAudio2 2.7 reverb effect as rfg.exe uses it (keen's bus effect chains, SFX.md 2.4): the parameter structs from
// rfg.pdb, the GUIDs from rfg.exe's .rdata, and the two inline functions rfg.exe compiled from this header, written
// from their disassembly. Declarations only otherwise; the SDK header is not used (see xaudio2.h).
#pragma once

#include <math.h>
#include "xaudio2.h"

// ---- GUIDs (rfg.exe .rdata)
__declspec(selectany) extern const GUID CLSID_AudioReverb = {0x6a93130e, 0x1d53, 0x41d1, {0xa9, 0xcf, 0xe7, 0x58, 0x80, 0x0b, 0xb1, 0x79}}; // 0x01340C24
__declspec(selectany) extern const GUID CLSID_AudioReverb_Debug = {0xc4f82dd4, 0xcb4e, 0x4ce1, {0x8b, 0xdb, 0xee, 0x32, 0xd4, 0x19, 0x82, 0x69}}; // 0x01340C34

#define XAUDIO2FX_DEBUG 1 // API value: Flags of XAudio2CreateReverb, the debug reverb

#pragma pack(push, 1)

// size 0x34
struct XAUDIO2FX_REVERB_PARAMETERS
{
    float WetDryMix; // 0x00
    UINT32 ReflectionsDelay; // 0x04
    BYTE ReverbDelay; // 0x08
    BYTE RearDelay; // 0x09
    BYTE PositionLeft; // 0x0A
    BYTE PositionRight; // 0x0B
    BYTE PositionMatrixLeft; // 0x0C
    BYTE PositionMatrixRight; // 0x0D
    BYTE EarlyDiffusion; // 0x0E
    BYTE LateDiffusion; // 0x0F
    BYTE LowEQGain; // 0x10
    BYTE LowEQCutoff; // 0x11
    BYTE HighEQGain; // 0x12
    BYTE HighEQCutoff; // 0x13
    float RoomFilterFreq; // 0x14
    float RoomFilterMain; // 0x18
    float RoomFilterHF; // 0x1C
    float ReflectionsGain; // 0x20
    float ReverbGain; // 0x24
    float DecayTime; // 0x28
    float Density; // 0x2C
    float RoomSize; // 0x30
};

// size 0x34
struct XAUDIO2FX_REVERB_I3DL2_PARAMETERS
{
    float WetDryMix; // 0x00
    int Room; // 0x04
    int RoomHF; // 0x08
    float RoomRolloffFactor; // 0x0C
    float DecayTime; // 0x10
    float DecayHFRatio; // 0x14
    int Reflections; // 0x18
    float ReflectionsDelay; // 0x1C
    int Reverb; // 0x20
    float ReverbDelay; // 0x24
    float Diffusion; // 0x28
    float Density; // 0x2C
    float HFReference; // 0x30
};

#pragma pack(pop)

// 0x00C33AA0 (:136)
// CoCreateInstance of the release or debug reverb (Flags bit 0) as a plain IUnknown (the XAPO).
inline HRESULT XAudio2CreateReverb(IUnknown** ppApo, UINT32 Flags = 0)
{
    return CoCreateInstance((Flags & XAUDIO2FX_DEBUG) ? CLSID_AudioReverb_Debug : CLSID_AudioReverb, nullptr, CLSCTX_INPROC_SERVER, IID_IUnknown, (void**)ppApo);
}

// 0x00C33AD0 (:323)
// I3DL2 (millibels, seconds) to native reverb parameters. Float/double steps follow rfg.exe's SSE code: the divisions by
// 100 and the delay multiplications are done in double; log10 goes through the x87 CRT and is rounded to float first.
inline void ReverbConvertI3DL2ToNative(const XAUDIO2FX_REVERB_I3DL2_PARAMETERS* pI3DL2, XAUDIO2FX_REVERB_PARAMETERS* pNative)
{
    // Magic numbers: the API's fixed values for room size 100, rear delay 5, positions 6, matrix positions 27, EQ
    // cutoffs 4 (low) and 6 (high).
    pNative->RoomSize = 100.0f;
    pNative->RearDelay = 5;
    pNative->PositionLeft = 6;
    pNative->PositionRight = 6;
    pNative->PositionMatrixLeft = 27;
    pNative->PositionMatrixRight = 27;
    pNative->LowEQCutoff = 4;
    pNative->HighEQCutoff = 6;
    pNative->RoomFilterMain = (float)((double)(float)pI3DL2->Room / 100.0);
    pNative->RoomFilterHF = (float)((double)(float)pI3DL2->RoomHF / 100.0);
    // The decay HF ratio becomes an EQ gain: 4 dB per decade of the ratio, clamped to -8, as a 0-8 gain index.
    if (pI3DL2->DecayHFRatio >= 1.0f)
    {
        INT32 index = (INT32)(-4.0 * (double)(float)log10((double)pI3DL2->DecayHFRatio));
        if (index < -8)
        {
            index = -8;
        }
        pNative->LowEQGain = (BYTE)((index < 0) ? index + 8 : 8);
        pNative->HighEQGain = 8;
        pNative->DecayTime = (float)((double)pI3DL2->DecayTime * (double)pI3DL2->DecayHFRatio);
    }
    else
    {
        INT32 index = (INT32)(4.0 * (double)(float)log10((double)pI3DL2->DecayHFRatio));
        if (index < -8)
        {
            index = -8;
        }
        pNative->LowEQGain = 8;
        pNative->HighEQGain = (BYTE)((index < 0) ? index + 8 : 8);
        pNative->DecayTime = pI3DL2->DecayTime;
    }
    // Delays in ms: reflections 1-299 (truncated through a 64-bit integer), reverb below 85.
    float reflectionsDelay = (float)((double)pI3DL2->ReflectionsDelay * 1000.0);
    if (reflectionsDelay >= 300.0f)
    {
        reflectionsDelay = 299.0f;
    }
    else if (reflectionsDelay <= 1.0f)
    {
        reflectionsDelay = 1.0f;
    }
    pNative->ReflectionsDelay = (UINT32)(INT64)reflectionsDelay;
    float reverbDelay = (float)((double)pI3DL2->ReverbDelay * 1000.0);
    if (reverbDelay >= 85.0f)
    {
        reverbDelay = 84.0f;
    }
    pNative->ReverbDelay = (BYTE)(int)reverbDelay;
    pNative->ReflectionsGain = (float)((double)(float)pI3DL2->Reflections / 100.0);
    pNative->ReverbGain = (float)((double)(float)pI3DL2->Reverb / 100.0);
    // Diffusion 0-100 % to 0-15.
    const BYTE diffusion = (BYTE)(int)((double)pI3DL2->Diffusion * 15.0 / 100.0);
    pNative->EarlyDiffusion = diffusion;
    pNative->LateDiffusion = diffusion;
    pNative->Density = pI3DL2->Density;
    pNative->RoomFilterFreq = pI3DL2->HFReference;
    pNative->WetDryMix = pI3DL2->WetDryMix;
}
