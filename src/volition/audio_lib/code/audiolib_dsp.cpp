// Original: volition/audio_lib/code/audiolib_dsp.cpp
//
// DSP system init and shutdown (SFX.md SX2). The keen-backed game does its low pass and reverb in the hardware layer
// (stub/audiolib_stub.cpp), so nothing is set up here. Compiled in audio_lib_unity.cpp.

#include "volition/audio_lib/code/audiolib_dsp.h"

// ---- file-static globals
// bool Audio_DSP_Inited (0x01A789C7) is used here too; placed in audiolib.cpp.

// 0x00485D70 (:61)
bool audiolib_dsp_system_init()
{
    if (Audio_DSP_Inited)
    {
        return false;
    }
    // Original: the flag is set back to false, never to true, so a second init succeeds too.
    Audio_DSP_Inited = false;
    return true;
}

// 0x00485D90 (:74)
void audiolib_dsp_system_shutdown()
{
    Audio_DSP_Inited = false;
}
