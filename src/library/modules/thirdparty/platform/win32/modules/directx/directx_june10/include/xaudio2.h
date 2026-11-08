// Original: library/modules/thirdparty/platform/win32/modules/directx/directx_june10/include/xaudio2.h
//
// The XAudio2 2.7 API (DirectX SDK June 2010) as rfg.exe uses it (SFX.md 2.4): the COM interfaces, structs and enums
// recovered from rfg.pdb, the GUIDs from rfg.exe's .rdata, and the one inline function rfg.exe compiled from this
// header (XAudio2Create). Declarations only, written from the binary: the SDK header is not used. The Windows SDK's
// <xaudio2.h> is the 2.8/2.9 API (another vtable, no device enumeration, XAudio2Create exported by the DLL), so this
// directory has to come first on the include path of every TU that includes <xaudio2.h> (keen_sound, tests/layout).
//
// Constants marked "API value" are not in rfg.pdb (macros); they are the documented values of the 2.7 API, added
// as the ported code needs them.
#pragma once

#include <windows.h>
#include <objbase.h>
#include <mmreg.h>

struct IXAudio2;
struct IXAudio2Voice;
struct IXAudio2SourceVoice;
struct IXAudio2SubmixVoice;
struct IXAudio2MasteringVoice;
struct IXAudio2EngineCallback;
struct IXAudio2VoiceCallback;

// ---- GUIDs (rfg.exe .rdata; selectany so every TU can include this header)
__declspec(selectany) extern const GUID CLSID_XAudio2 = {0x5a508685, 0xa254, 0x4fba, {0x9b, 0x82, 0x9a, 0x24, 0xb0, 0x03, 0x06, 0xaf}}; // 0x01340B88
__declspec(selectany) extern const GUID CLSID_XAudio2_Debug = {0xdb05ea35, 0x0329, 0x4d4b, {0xa5, 0x3a, 0x6d, 0xea, 0xd0, 0x3d, 0x38, 0x52}}; // 0x01340B98
__declspec(selectany) extern const GUID IID_IXAudio2 = {0x8bcf1f58, 0x9fe7, 0x4583, {0x8a, 0xc6, 0xe2, 0xad, 0xc4, 0x65, 0xc8, 0xbb}}; // 0x01340B78

// ---- constants (API values)
#define XAUDIO2_DEBUG_ENGINE 0x0001 // Flags of XAudio2Create: the debug engine (CLSID_XAudio2_Debug)
#define XAUDIO2_VOICE_NOPITCH 0x0002
#define XAUDIO2_VOICE_NOSRC 0x0004
#define XAUDIO2_VOICE_USEFILTER 0x0008
#define XAUDIO2_VOICE_MUSIC 0x0010 // keen's background music voices (SoundProviderWaveXAudio2::startSound)
#define XAUDIO2_PLAY_TAILS 0x0020
#define XAUDIO2_END_OF_STREAM 0x0040
#define XAUDIO2_SEND_USEFILTER 0x0080
#define XAUDIO2_COMMIT_NOW 0
#define XAUDIO2_COMMIT_ALL 0
#define XAUDIO2_DEFAULT_CHANNELS 0
#define XAUDIO2_DEFAULT_SAMPLERATE 0
#define XAUDIO2_DEFAULT_FREQ_RATIO 2.0f
#define XAUDIO2_MAX_FREQ_RATIO 1024.0f
#define XAUDIO2_NO_LOOP_REGION 0
#define XAUDIO2_MAX_LOOP_COUNT 254
#define XAUDIO2_LOOP_INFINITE 255
#define XAUDIO2_E_INVALID_CALL ((HRESULT)0x88960001)
#define XAUDIO2_E_XMA_DECODER_ERROR ((HRESULT)0x88960002)
#define XAUDIO2_E_XAPO_CREATION_FAILED ((HRESULT)0x88960003)
#define XAUDIO2_E_DEVICE_INVALIDATED ((HRESULT)0x88960004)

// ---- enums (rfg.pdb)
enum XAUDIO2_DEVICE_ROLE
{
    NotDefaultDevice = 0,
    DefaultConsoleDevice = 1,
    DefaultMultimediaDevice = 2,
    DefaultCommunicationsDevice = 4,
    DefaultGameDevice = 8,
    GlobalDefaultDevice = 15,
    InvalidDeviceRole = -16, // ~GlobalDefaultDevice
};

enum XAUDIO2_WINDOWS_PROCESSOR_SPECIFIER
{
    Processor1 = 1,
    Processor2 = 2,
    Processor3 = 4,
    Processor4 = 8,
    Processor5 = 16,
    Processor6 = 32,
    Processor7 = 64,
    Processor8 = 128,
    Processor9 = 256,
    Processor10 = 512,
    Processor11 = 1024,
    Processor12 = 2048,
    Processor13 = 4096,
    Processor14 = 8192,
    Processor15 = 16384,
    Processor16 = 32768,
    Processor17 = 0x10000,
    Processor18 = 0x20000,
    Processor19 = 0x40000,
    Processor20 = 0x80000,
    Processor21 = 0x100000,
    Processor22 = 0x200000,
    Processor23 = 0x400000,
    Processor24 = 0x800000,
    Processor25 = 0x1000000,
    Processor26 = 0x2000000,
    Processor27 = 0x4000000,
    Processor28 = 0x8000000,
    Processor29 = 0x10000000,
    Processor30 = 0x20000000,
    Processor31 = 0x40000000,
    Processor32 = (int)0x80000000,
    XAUDIO2_ANY_PROCESSOR = -1,
    XAUDIO2_DEFAULT_PROCESSOR = -1, // XAUDIO2_ANY_PROCESSOR
};

// Not in rfg.pdb under this name; the 2.7 API's four filter types.
enum XAUDIO2_FILTER_TYPE
{
    LowPassFilter = 0,
    BandPassFilter = 1,
    HighPassFilter = 2,
    NotchFilter = 3,
};

// ---- structs (rfg.pdb; the API packs them to 1 byte, which changes nothing on x86 for these member lists)
#pragma pack(push, 1)

// size 0x42C
struct XAUDIO2_DEVICE_DETAILS
{
    WCHAR DeviceID[256]; // 0x00
    WCHAR DisplayName[256]; // 0x200
    XAUDIO2_DEVICE_ROLE Role; // 0x400
    WAVEFORMATEXTENSIBLE OutputFormat; // 0x404
};

// size 0xC
struct XAUDIO2_VOICE_DETAILS
{
    UINT32 CreationFlags; // 0x00
    UINT32 InputChannels; // 0x04
    UINT32 InputSampleRate; // 0x08
};

// size 0x8
struct XAUDIO2_SEND_DESCRIPTOR
{
    UINT32 Flags; // 0x00
    IXAudio2Voice* pOutputVoice; // 0x04
};

// size 0x8
struct XAUDIO2_VOICE_SENDS
{
    UINT32 SendCount; // 0x00
    XAUDIO2_SEND_DESCRIPTOR* pSends; // 0x04
};

// size 0xC
struct XAUDIO2_EFFECT_DESCRIPTOR
{
    IUnknown* pEffect; // 0x00
    BOOL InitialState; // 0x04
    UINT32 OutputChannels; // 0x08
};

// size 0x8
struct XAUDIO2_EFFECT_CHAIN
{
    UINT32 EffectCount; // 0x00
    XAUDIO2_EFFECT_DESCRIPTOR* pEffectDescriptors; // 0x04
};

// size 0xC
struct XAUDIO2_FILTER_PARAMETERS
{
    XAUDIO2_FILTER_TYPE Type; // 0x00
    float Frequency; // 0x04
    float OneOverQ; // 0x08
};

// size 0x24
struct XAUDIO2_BUFFER
{
    UINT32 Flags; // 0x00
    UINT32 AudioBytes; // 0x04
    const BYTE* pAudioData; // 0x08
    UINT32 PlayBegin; // 0x0C
    UINT32 PlayLength; // 0x10
    UINT32 LoopBegin; // 0x14
    UINT32 LoopLength; // 0x18
    UINT32 LoopCount; // 0x1C
    void* pContext; // 0x20
};

// size 0x8
struct XAUDIO2_BUFFER_WMA
{
    const UINT32* pDecodedPacketCumulativeBytes; // 0x00
    UINT32 PacketCount; // 0x04
};

// size 0x10
struct XAUDIO2_VOICE_STATE
{
    void* pCurrentBufferContext; // 0x00
    UINT32 BuffersQueued; // 0x04
    UINT64 SamplesPlayed; // 0x08
};

// size 0x40
struct XAUDIO2_PERFORMANCE_DATA
{
    UINT64 AudioCyclesSinceLastQuery; // 0x00
    UINT64 TotalCyclesSinceLastQuery; // 0x08
    UINT32 MinimumCyclesPerQuantum; // 0x10
    UINT32 MaximumCyclesPerQuantum; // 0x14
    UINT32 MemoryUsageInBytes; // 0x18
    UINT32 CurrentLatencyInSamples; // 0x1C
    UINT32 GlitchesSinceEngineStarted; // 0x20
    UINT32 ActiveSourceVoiceCount; // 0x24
    UINT32 TotalSourceVoiceCount; // 0x28
    UINT32 ActiveSubmixVoiceCount; // 0x2C
    UINT32 ActiveResamplerCount; // 0x30
    UINT32 ActiveMatrixMixCount; // 0x34
    UINT32 ActiveXmaSourceVoices; // 0x38
    UINT32 ActiveXmaStreams; // 0x3C
};

// size 0x18
struct XAUDIO2_DEBUG_CONFIGURATION
{
    UINT32 TraceMask; // 0x00
    UINT32 BreakMask; // 0x04
    BOOL LogThreadID; // 0x08
    BOOL LogFileline; // 0x0C
    BOOL LogFunctionName; // 0x10
    BOOL LogTiming; // 0x14
};

#pragma pack(pop)

// ---- interfaces (rfg.pdb vtable order)
// size 0x4
struct IXAudio2 : public IUnknown
{
    virtual HRESULT __stdcall GetDeviceCount(UINT32* pCount) = 0; // vtbl+0x0C
    virtual HRESULT __stdcall GetDeviceDetails(UINT32 Index, XAUDIO2_DEVICE_DETAILS* pDeviceDetails) = 0; // vtbl+0x10
    virtual HRESULT __stdcall Initialize(UINT32 Flags, XAUDIO2_WINDOWS_PROCESSOR_SPECIFIER XAudio2Processor) = 0; // vtbl+0x14
    virtual HRESULT __stdcall RegisterForCallbacks(IXAudio2EngineCallback* pCallback) = 0; // vtbl+0x18
    virtual void __stdcall UnregisterForCallbacks(IXAudio2EngineCallback* pCallback) = 0; // vtbl+0x1C
    virtual HRESULT __stdcall CreateSourceVoice(IXAudio2SourceVoice** ppSourceVoice, const WAVEFORMATEX* pSourceFormat, UINT32 Flags, float MaxFrequencyRatio, IXAudio2VoiceCallback* pCallback, const XAUDIO2_VOICE_SENDS* pSendList, const XAUDIO2_EFFECT_CHAIN* pEffectChain) = 0; // vtbl+0x20
    virtual HRESULT __stdcall CreateSubmixVoice(IXAudio2SubmixVoice** ppSubmixVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags, UINT32 ProcessingStage, const XAUDIO2_VOICE_SENDS* pSendList, const XAUDIO2_EFFECT_CHAIN* pEffectChain) = 0; // vtbl+0x24
    virtual HRESULT __stdcall CreateMasteringVoice(IXAudio2MasteringVoice** ppMasteringVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags, UINT32 DeviceIndex, const XAUDIO2_EFFECT_CHAIN* pEffectChain) = 0; // vtbl+0x28
    virtual HRESULT __stdcall StartEngine() = 0; // vtbl+0x2C
    virtual void __stdcall StopEngine() = 0; // vtbl+0x30
    virtual HRESULT __stdcall CommitChanges(UINT32 OperationSet) = 0; // vtbl+0x34
    virtual void __stdcall GetPerformanceData(XAUDIO2_PERFORMANCE_DATA* pPerfData) = 0; // vtbl+0x38
    virtual void __stdcall SetDebugConfiguration(const XAUDIO2_DEBUG_CONFIGURATION* pDebugConfiguration, void* pReserved) = 0; // vtbl+0x3C
};

// size 0x4
struct IXAudio2Voice
{
    virtual void __stdcall GetVoiceDetails(XAUDIO2_VOICE_DETAILS* pVoiceDetails) = 0; // vtbl+0x00
    virtual HRESULT __stdcall SetOutputVoices(const XAUDIO2_VOICE_SENDS* pSendList) = 0; // vtbl+0x04
    virtual HRESULT __stdcall SetEffectChain(const XAUDIO2_EFFECT_CHAIN* pEffectChain) = 0; // vtbl+0x08
    virtual HRESULT __stdcall EnableEffect(UINT32 EffectIndex, UINT32 OperationSet) = 0; // vtbl+0x0C
    virtual HRESULT __stdcall DisableEffect(UINT32 EffectIndex, UINT32 OperationSet) = 0; // vtbl+0x10
    virtual void __stdcall GetEffectState(UINT32 EffectIndex, BOOL* pEnabled) = 0; // vtbl+0x14
    virtual HRESULT __stdcall SetEffectParameters(UINT32 EffectIndex, const void* pParameters, UINT32 ParametersByteSize, UINT32 OperationSet) = 0; // vtbl+0x18
    virtual HRESULT __stdcall GetEffectParameters(UINT32 EffectIndex, void* pParameters, UINT32 ParametersByteSize) = 0; // vtbl+0x1C
    virtual HRESULT __stdcall SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS* pParameters, UINT32 OperationSet) = 0; // vtbl+0x20
    virtual void __stdcall GetFilterParameters(XAUDIO2_FILTER_PARAMETERS* pParameters) = 0; // vtbl+0x24
    virtual HRESULT __stdcall SetOutputFilterParameters(IXAudio2Voice* pDestinationVoice, const XAUDIO2_FILTER_PARAMETERS* pParameters, UINT32 OperationSet) = 0; // vtbl+0x28
    virtual void __stdcall GetOutputFilterParameters(IXAudio2Voice* pDestinationVoice, XAUDIO2_FILTER_PARAMETERS* pParameters) = 0; // vtbl+0x2C
    virtual HRESULT __stdcall SetVolume(float Volume, UINT32 OperationSet) = 0; // vtbl+0x30
    virtual void __stdcall GetVolume(float* pVolume) = 0; // vtbl+0x34
    virtual HRESULT __stdcall SetChannelVolumes(UINT32 Channels, const float* pVolumes, UINT32 OperationSet) = 0; // vtbl+0x38
    virtual void __stdcall GetChannelVolumes(UINT32 Channels, float* pVolumes) = 0; // vtbl+0x3C
    virtual HRESULT __stdcall SetOutputMatrix(IXAudio2Voice* pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float* pLevelMatrix, UINT32 OperationSet) = 0; // vtbl+0x40
    virtual void __stdcall GetOutputMatrix(IXAudio2Voice* pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, float* pLevelMatrix) = 0; // vtbl+0x44
    virtual void __stdcall DestroyVoice() = 0; // vtbl+0x48
};

// size 0x4
struct IXAudio2SourceVoice : public IXAudio2Voice
{
    virtual HRESULT __stdcall Start(UINT32 Flags, UINT32 OperationSet) = 0; // vtbl+0x4C
    virtual HRESULT __stdcall Stop(UINT32 Flags, UINT32 OperationSet) = 0; // vtbl+0x50
    virtual HRESULT __stdcall SubmitSourceBuffer(const XAUDIO2_BUFFER* pBuffer, const XAUDIO2_BUFFER_WMA* pBufferWMA) = 0; // vtbl+0x54
    virtual HRESULT __stdcall FlushSourceBuffers() = 0; // vtbl+0x58
    virtual HRESULT __stdcall Discontinuity() = 0; // vtbl+0x5C
    virtual HRESULT __stdcall ExitLoop(UINT32 OperationSet) = 0; // vtbl+0x60
    virtual void __stdcall GetState(XAUDIO2_VOICE_STATE* pVoiceState) = 0; // vtbl+0x64
    virtual HRESULT __stdcall SetFrequencyRatio(float Ratio, UINT32 OperationSet) = 0; // vtbl+0x68
    virtual void __stdcall GetFrequencyRatio(float* pRatio) = 0; // vtbl+0x6C
    virtual HRESULT __stdcall SetSourceSampleRate(UINT32 NewSourceSampleRate) = 0; // vtbl+0x70
};

// size 0x4
struct IXAudio2SubmixVoice : public IXAudio2Voice
{
};

// size 0x4
struct IXAudio2MasteringVoice : public IXAudio2Voice
{
};

// size 0x4
struct IXAudio2EngineCallback
{
    virtual void __stdcall OnProcessingPassStart() = 0; // vtbl+0x00
    virtual void __stdcall OnProcessingPassEnd() = 0; // vtbl+0x04
    virtual void __stdcall OnCriticalError(HRESULT Error) = 0; // vtbl+0x08
};

// size 0x4
struct IXAudio2VoiceCallback
{
    virtual void __stdcall OnVoiceProcessingPassStart(UINT32 BytesRequired) = 0; // vtbl+0x00
    virtual void __stdcall OnVoiceProcessingPassEnd() = 0; // vtbl+0x04
    virtual void __stdcall OnStreamEnd() = 0; // vtbl+0x08
    virtual void __stdcall OnBufferStart(void* pBufferContext) = 0; // vtbl+0x0C
    virtual void __stdcall OnBufferEnd(void* pBufferContext) = 0; // vtbl+0x10
    virtual void __stdcall OnLoopEnd(void* pBufferContext) = 0; // vtbl+0x14
    virtual void __stdcall OnVoiceError(void* pBufferContext, HRESULT Error) = 0; // vtbl+0x18
};

// 0x00C32C10 (:1230)
// CoCreateInstance of the release or debug engine (Flags bit 0), then IXAudio2::Initialize; the object is released
// again when Initialize fails. In 2.7 the DLL exports nothing: the CLSID must be registered (the DirectX runtime).
inline HRESULT XAudio2Create(IXAudio2** ppXAudio2, UINT32 Flags = 0, XAUDIO2_WINDOWS_PROCESSOR_SPECIFIER XAudio2Processor = XAUDIO2_DEFAULT_PROCESSOR)
{
    IXAudio2* pXAudio2;
    HRESULT hr = CoCreateInstance((Flags & XAUDIO2_DEBUG_ENGINE) ? CLSID_XAudio2_Debug : CLSID_XAudio2, nullptr, CLSCTX_INPROC_SERVER, IID_IXAudio2, (void**)&pXAudio2);
    if (SUCCEEDED(hr))
    {
        hr = pXAudio2->Initialize(Flags, XAudio2Processor);
        if (SUCCEEDED(hr))
        {
            *ppXAudio2 = pXAudio2;
        }
        else
        {
            pXAudio2->Release();
        }
    }
    return hr;
}
