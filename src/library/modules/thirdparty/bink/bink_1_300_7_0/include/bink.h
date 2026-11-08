// Original: library/modules/thirdparty/bink/bink_1_300_7_0/include/bink.h
//
// Recovery scope: Bink 1.300.7.0 (RAD Game Tools), third party, through the import library of the binkw32.dll the game
// ships (stub_impl.md decision 4). What the video player uses: the frame buffer description it allocates the YUV(A)
// planes of, the summary and real time statistics, the head of the movie handle, and the 23 functions rfg.exe imports.
#pragma once

// size 0x600 (the DLL allocates it; rfg.exe reads only the fields named here)
struct alignas(8) BINK
{
    unsigned int Width; // 0x00
    unsigned int Height; // 0x04
    unsigned int Frames; // 0x08
    unsigned int FrameNum; // 0x0C
    unsigned int LastFrameNum; // 0x10
    unsigned int FrameRate; // 0x14
    unsigned int FrameRateDiv; // 0x18
    unsigned int ReadError; // 0x1C
    unsigned int OpenFlags; // 0x20
    unsigned int BinkType; // 0x24
    unsigned int LargestFrameSize; // 0x28
    unsigned int FrameSize; // 0x2C
    unsigned int SndSize; // 0x30
    unsigned int FrameChangePercent; // 0x34
    int NumTracks; // 0x38
    unsigned char rest[0x600 - 0x3C]; // 0x3C
};
static_assert(sizeof(BINK) == 0x600, "BINK");

// size 0xC
struct BINKPLANE
{
    void* Buffer; // 0x00
    int Allocate; // 0x04
    unsigned int BufferPitch; // 0x08
};

// size 0x3C
struct BINKFRAMEPLANESET
{
    BINKPLANE YPlane; // 0x00
    BINKPLANE cRPlane; // 0x0C
    BINKPLANE cBPlane; // 0x18
    BINKPLANE APlane; // 0x24
    BINKPLANE HPlane; // 0x30
};

// size 0xCC
struct BINKFRAMEBUFFERS
{
    int TotalFrames; // 0x00
    unsigned int YABufferWidth; // 0x04
    unsigned int YABufferHeight; // 0x08
    unsigned int cRcBBufferWidth; // 0x0C
    unsigned int cRcBBufferHeight; // 0x10
    unsigned int FrameNum; // 0x14
    BINKFRAMEPLANESET Frames[3]; // 0x18
};

// size 0x48
struct BINKREALTIME
{
    unsigned long long ReadBufferSize; // 0x00
    unsigned long long ReadBufferUsed; // 0x08
    unsigned long long FramesDataRate; // 0x10
    unsigned int FrameNum; // 0x18
    unsigned int FrameRate; // 0x1C
    unsigned int FrameRateDiv; // 0x20
    unsigned int Frames; // 0x24
    unsigned int FramesTime; // 0x28
    unsigned int FramesVideoDecompTime; // 0x2C
    unsigned int FramesAudioDecompTime; // 0x30
    unsigned int FramesReadTime; // 0x34
    unsigned int FramesIdleReadTime; // 0x38
    unsigned int FramesThreadReadTime; // 0x3C
    unsigned int FramesBlitTime; // 0x40
};
static_assert(sizeof(BINKREALTIME) == 0x48, "BINKREALTIME");

// size 0x90
struct BINKSUMMARY
{
    unsigned long long TotalReadSpeed; // 0x00
    unsigned long long HighestMemAmount; // 0x08
    unsigned long long TotalIOMemory; // 0x10
    unsigned long long HighestIOUsed; // 0x18
    unsigned long long Highest1SecRate; // 0x20
    unsigned int Width; // 0x28
    unsigned int Height; // 0x2C
    unsigned int TotalTime; // 0x30
    unsigned int FileFrameRate; // 0x34
    unsigned int FileFrameRateDiv; // 0x38
    unsigned int FrameRate; // 0x3C
    unsigned int FrameRateDiv; // 0x40
    unsigned int TotalOpenTime; // 0x44
    unsigned int TotalFrames; // 0x48
    unsigned int TotalPlayedFrames; // 0x4C
    unsigned int SkippedFrames; // 0x50
    unsigned int SkippedBlits; // 0x54
    unsigned int SoundSkips; // 0x58
    unsigned int TotalBlitTime; // 0x5C
    unsigned int TotalReadTime; // 0x60
    unsigned int TotalVideoDecompTime; // 0x64
    unsigned int TotalAudioDecompTime; // 0x68
    unsigned int TotalIdleReadTime; // 0x6C
    unsigned int TotalBackReadTime; // 0x70
    unsigned int SlowestFrameTime; // 0x74
    unsigned int Slowest2FrameTime; // 0x78
    unsigned int SlowestFrameNum; // 0x7C
    unsigned int Slowest2FrameNum; // 0x80
    unsigned int AverageDataRate; // 0x84
    unsigned int AverageFrameSize; // 0x88
    unsigned int Highest1SecFrame; // 0x8C
};
static_assert(sizeof(BINKSUMMARY) == 0x90, "BINKSUMMARY");

// rfg.exe's imports from binkw32.dll, decorated __stdcall exports (_BinkOpen@8, tools/dll_implib/dll_implib.py).
extern "C"
{
BINK* __stdcall BinkOpen(const char* name, unsigned int flags);
void __stdcall BinkClose(BINK* bink);
int __stdcall BinkDoFrame(BINK* bink);
int __stdcall BinkDoFrameAsync(BINK* bink, unsigned int yplane_thread_index, unsigned int other_work_thread_index);
int __stdcall BinkDoFrameAsyncWait(BINK* bink, int us);
void __stdcall BinkNextFrame(BINK* bink);
int __stdcall BinkWait(BINK* bink);
int __stdcall BinkShouldSkip(BINK* bink);
int __stdcall BinkPause(BINK* bink, int pause);
void __stdcall BinkGoto(BINK* bink, unsigned int frame, int flags);
unsigned int __stdcall BinkGetKeyFrame(BINK* bink, unsigned int frame, int flags);
void __stdcall BinkGetFrameBuffersInfo(BINK* bink, BINKFRAMEBUFFERS* buffers);
void __stdcall BinkRegisterFrameBuffers(BINK* bink, BINKFRAMEBUFFERS* buffers);
void __stdcall BinkGetSummary(BINK* bink, BINKSUMMARY* summary);
void __stdcall BinkGetRealtime(BINK* bink, BINKREALTIME* realtime, unsigned int frames);
unsigned int __stdcall BinkControlBackgroundIO(BINK* bink, unsigned int control);
void __stdcall BinkSetVolume(BINK* bink, unsigned int track, int volume);
void __stdcall BinkSetIOSize(unsigned long long size);
void __stdcall BinkSetSoundTrack(unsigned int total_tracks, const unsigned int* tracks);
int __stdcall BinkSetSoundSystem(void* open, unsigned int param);
void* __stdcall BinkOpenDirectSound(unsigned int param);
int __stdcall BinkRequestStopAsyncThread(unsigned int thread_index);
int __stdcall BinkWaitStopAsyncThread(unsigned int thread_index);
}
