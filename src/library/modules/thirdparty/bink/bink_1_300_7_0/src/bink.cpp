// Stand-in for binkw32.dll (bink.h), new code: built instead of linking lib/win32/binkw32.lib when CMake's
// OPENRFG_WITH_CUSTOM_BINK is on. No movie ever opens: BinkOpen returns nullptr, which the video player already treats
// as a failed play (video_player.cpp, play: the tracker is destroyed and 0 returned), so videos are skipped. The other
// functions are only reached with a handle the stub never gives out, or for global settings; they do nothing and
// report nothing done. The program does not need binkw32.dll next to it.
//
// Only the 23 functions rfg.exe imports are provided (bink.h); the DLL's other exports are not used.
#include "library/modules/thirdparty/bink/bink_1_300_7_0/include/bink.h"

#include <string.h>

extern "C"
{
    BINK* __stdcall BinkOpen(const char* name, unsigned int flags)
    {
        (void)name;
        (void)flags;
        return nullptr;
    }

    void __stdcall BinkClose(BINK* bink)
    {
        (void)bink;
    }

    // 0: no frame decompressed.
    int __stdcall BinkDoFrame(BINK* bink)
    {
        (void)bink;
        return 0;
    }

    int __stdcall BinkDoFrameAsync(BINK* bink, unsigned int yplane_thread_index, unsigned int other_work_thread_index)
    {
        (void)bink;
        (void)yplane_thread_index;
        (void)other_work_thread_index;
        return 0;
    }

    // 1: the (absent) asynchronous work is done, so a caller never waits on it.
    int __stdcall BinkDoFrameAsyncWait(BINK* bink, int us)
    {
        (void)bink;
        (void)us;
        return 1;
    }

    void __stdcall BinkNextFrame(BINK* bink)
    {
        (void)bink;
    }

    // 0: no need to wait for the next frame.
    int __stdcall BinkWait(BINK* bink)
    {
        (void)bink;
        return 0;
    }

    // 0: no frame to skip (the video player loops while this is non-zero).
    int __stdcall BinkShouldSkip(BINK* bink)
    {
        (void)bink;
        return 0;
    }

    int __stdcall BinkPause(BINK* bink, int pause)
    {
        (void)bink;
        return pause;
    }

    void __stdcall BinkGoto(BINK* bink, unsigned int frame, int flags)
    {
        (void)bink;
        (void)frame;
        (void)flags;
    }

    unsigned int __stdcall BinkGetKeyFrame(BINK* bink, unsigned int frame, int flags)
    {
        (void)bink;
        (void)flags;
        return frame;
    }

    // Zero frame buffers.
    void __stdcall BinkGetFrameBuffersInfo(BINK* bink, BINKFRAMEBUFFERS* buffers)
    {
        (void)bink;
        if (buffers != nullptr)
        {
            memset(buffers, 0, sizeof(*buffers));
        }
    }

    void __stdcall BinkRegisterFrameBuffers(BINK* bink, BINKFRAMEBUFFERS* buffers)
    {
        (void)bink;
        (void)buffers;
    }

    void __stdcall BinkGetSummary(BINK* bink, BINKSUMMARY* summary)
    {
        (void)bink;
        if (summary != nullptr)
        {
            memset(summary, 0, sizeof(*summary));
        }
    }

    void __stdcall BinkGetRealtime(BINK* bink, BINKREALTIME* realtime, unsigned int frames)
    {
        (void)bink;
        (void)frames;
        if (realtime != nullptr)
        {
            memset(realtime, 0, sizeof(*realtime));
        }
    }

    unsigned int __stdcall BinkControlBackgroundIO(BINK* bink, unsigned int control)
    {
        (void)bink;
        (void)control;
        return 0;
    }

    void __stdcall BinkSetVolume(BINK* bink, unsigned int track, int volume)
    {
        (void)bink;
        (void)track;
        (void)volume;
    }

    void __stdcall BinkSetIOSize(unsigned long long size)
    {
        (void)size;
    }

    void __stdcall BinkSetSoundTrack(unsigned int total_tracks, const unsigned int* tracks)
    {
        (void)total_tracks;
        (void)tracks;
    }

    // 0: no sound system installed (the video player ignores the result).
    int __stdcall BinkSetSoundSystem(void* open, unsigned int param)
    {
        (void)open;
        (void)param;
        return 0;
    }

    // Only passed to BinkSetSoundSystem, never called here.
    void* __stdcall BinkOpenDirectSound(unsigned int param)
    {
        (void)param;
        return nullptr;
    }

    // 1: no asynchronous thread runs, so stopping one succeeds at once.
    int __stdcall BinkRequestStopAsyncThread(unsigned int thread_index)
    {
        (void)thread_index;
        return 1;
    }

    int __stdcall BinkWaitStopAsyncThread(unsigned int thread_index)
    {
        (void)thread_index;
        return 1;
    }
}
