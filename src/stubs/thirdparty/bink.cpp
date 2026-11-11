// Stand-in for binkw32.dll (bink.h), new code: built instead of linking lib/win32/binkw32.lib when CMake's
// OPENRFG_STUB_BINK is on. Every movie opens as a black 16x16 movie of one frame, so the video player takes its normal
// path: it allocates the frame buffers, creates the Y/cR/cB textures and draws the video's rectangle in black. A video
// played once ends on its first decoded frame (video_player.cpp, bink_do_frame_sync: FrameNum == Frames) and its stop
// callback runs; a looping one stays black. Returning no movie instead (a failed play) left the rectangle undrawn, so
// whatever was under it showed, and skipped the stop callbacks the menus wait on.
//
// Only the 23 functions rfg.exe imports are provided (bink.h); the DLL's other exports are not used. The program does
// not need binkw32.dll next to it.
#include "library/modules/thirdparty/bink/bink_1_300_7_0/include/bink.h"

#include <intrin.h>
#include <string.h>

namespace
{
// The fake movie: 16x16, one frame at 30 frames per second.
constexpr unsigned int Fake_width = 16;
constexpr unsigned int Fake_height = 16;
constexpr unsigned int Fake_frames = 1;
constexpr unsigned int Fake_frame_rate = 30;

// Black in YCbCr: Y 0 with both chroma planes at their midpoint 0x80. Y 0 (not video-range 16) stays black whether the
// bink shader expands video range (the negative result clamps to 0) or reads the planes as full range.
constexpr unsigned char Black_y = 0x00;
constexpr unsigned char Black_chroma = 0x80;

// Magic number: one handle per video tracker (video_player.cpp, Video_tracker_pool holds 8).
constexpr int Max_open_movies = 8;

// Magic number: the read buffer size reported by BinkGetRealtime. The buffer is reported full, so the video player's
// throttle (bink_maybe_throttle) never holds the streaming system's disc processing paused for a movie that reads
// nothing (100 * used / (size + 1) = 99, above its 80% resume mark).
constexpr unsigned long long Fake_read_buffer_size = 0x10000;

BINK Movies[Max_open_movies];
volatile long Movie_in_use[Max_open_movies];

int movie_index(const BINK* bink)
{
    if (bink == nullptr)
    {
        return -1;
    }
    const int index = (int)(bink - Movies);
    return (index >= 0 && index < Max_open_movies) ? index : -1;
}

// Chroma planes are half the size, rounded up (as video_player_play sizes the textures).
unsigned int chroma_size(unsigned int size)
{
    return size / 2 + size % 2;
}

void fill_plane(const BINKPLANE& plane, unsigned int height, unsigned char value)
{
    if (plane.Allocate != 0 && plane.Buffer != nullptr)
    {
        memset(plane.Buffer, value, (size_t)plane.BufferPitch * height);
    }
}
} // namespace

extern "C"
{
    // A black movie of one frame for any name, nullptr only once all 8 handles are open.
    BINK* __stdcall BinkOpen(const char* name, unsigned int flags)
    {
        (void)name;
        for (int i = 0; i < Max_open_movies; i++)
        {
            if (_InterlockedCompareExchange(&Movie_in_use[i], 1, 0) == 0)
            {
                BINK* bink = &Movies[i];
                memset(bink, 0, sizeof(*bink));
                bink->Width = Fake_width;
                bink->Height = Fake_height;
                bink->Frames = Fake_frames;
                // Bink numbers frames from 1; FrameNum is the frame the next BinkDoFrame decodes.
                bink->FrameNum = 1;
                bink->FrameRate = Fake_frame_rate;
                bink->FrameRateDiv = 1;
                bink->OpenFlags = flags;
                return bink;
            }
        }
        return nullptr;
    }

    void __stdcall BinkClose(BINK* bink)
    {
        const int index = movie_index(bink);
        if (index >= 0)
        {
            _InterlockedExchange(&Movie_in_use[index], 0);
        }
    }

    // 0: the frame was decoded (the planes were filled black when registered and never change).
    int __stdcall BinkDoFrame(BINK* bink)
    {
        if (bink != nullptr)
        {
            bink->LastFrameNum = bink->FrameNum;
        }
        return 0;
    }

    int __stdcall BinkDoFrameAsync(BINK* bink, unsigned int yplane_thread_index, unsigned int other_work_thread_index)
    {
        (void)yplane_thread_index;
        (void)other_work_thread_index;
        return BinkDoFrame(bink);
    }

    // 1: the asynchronous work is done, so a caller never waits on it.
    int __stdcall BinkDoFrameAsyncWait(BINK* bink, int us)
    {
        (void)bink;
        (void)us;
        return 1;
    }

    // Past the last frame the movie wraps to the first (a looping video).
    void __stdcall BinkNextFrame(BINK* bink)
    {
        if (bink != nullptr)
        {
            bink->FrameNum = bink->FrameNum >= bink->Frames ? 1 : bink->FrameNum + 1;
        }
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
        (void)flags;
        if (bink != nullptr)
        {
            bink->FrameNum = (frame < 1 || frame > bink->Frames) ? 1 : frame;
        }
    }

    // Every frame is a key frame.
    unsigned int __stdcall BinkGetKeyFrame(BINK* bink, unsigned int frame, int flags)
    {
        (void)flags;
        if (bink == nullptr || frame < 1 || frame > bink->Frames)
        {
            return 1;
        }
        return frame;
    }

    // One set of Y, cR and cB planes (no alpha); the video player allocates them and sets their pitches.
    void __stdcall BinkGetFrameBuffersInfo(BINK* bink, BINKFRAMEBUFFERS* buffers)
    {
        if (buffers == nullptr)
        {
            return;
        }
        memset(buffers, 0, sizeof(*buffers));
        if (bink == nullptr)
        {
            return;
        }
        buffers->TotalFrames = 1;
        buffers->YABufferWidth = bink->Width;
        buffers->YABufferHeight = bink->Height;
        buffers->cRcBBufferWidth = chroma_size(bink->Width);
        buffers->cRcBBufferHeight = chroma_size(bink->Height);
        buffers->FrameNum = 0;
        buffers->Frames[0].YPlane.Allocate = 1;
        buffers->Frames[0].cRPlane.Allocate = 1;
        buffers->Frames[0].cBPlane.Allocate = 1;
    }

    // The planes are filled black once: the video player uploads the buffers it registered (frame set FrameNum 0) to
    // its textures, and nothing writes them again.
    void __stdcall BinkRegisterFrameBuffers(BINK* bink, BINKFRAMEBUFFERS* buffers)
    {
        (void)bink;
        if (buffers == nullptr)
        {
            return;
        }
        for (int i = 0; i < buffers->TotalFrames && i < 3; i++)
        {
            const BINKFRAMEPLANESET& frame = buffers->Frames[i];
            fill_plane(frame.YPlane, buffers->YABufferHeight, Black_y);
            fill_plane(frame.cRPlane, buffers->cRcBBufferHeight, Black_chroma);
            fill_plane(frame.cBPlane, buffers->cRcBBufferHeight, Black_chroma);
        }
    }

    void __stdcall BinkGetSummary(BINK* bink, BINKSUMMARY* summary)
    {
        if (summary == nullptr)
        {
            return;
        }
        memset(summary, 0, sizeof(*summary));
        if (bink == nullptr)
        {
            return;
        }
        summary->Width = bink->Width;
        summary->Height = bink->Height;
        summary->FileFrameRate = bink->FrameRate;
        summary->FileFrameRateDiv = bink->FrameRateDiv;
        summary->FrameRate = bink->FrameRate;
        summary->FrameRateDiv = bink->FrameRateDiv;
        summary->TotalFrames = bink->Frames;
    }

    // The read buffer is reported full (see Fake_read_buffer_size).
    void __stdcall BinkGetRealtime(BINK* bink, BINKREALTIME* realtime, unsigned int frames)
    {
        (void)frames;
        if (realtime == nullptr)
        {
            return;
        }
        memset(realtime, 0, sizeof(*realtime));
        realtime->ReadBufferSize = Fake_read_buffer_size;
        realtime->ReadBufferUsed = Fake_read_buffer_size;
        if (bink != nullptr)
        {
            realtime->FrameNum = bink->FrameNum;
            realtime->FrameRate = bink->FrameRate;
            realtime->FrameRateDiv = bink->FrameRateDiv;
        }
    }

    unsigned int __stdcall BinkControlBackgroundIO(BINK* bink, unsigned int control)
    {
        (void)bink;
        (void)control;
        return 0;
    }

    // The movie has no sound.
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
