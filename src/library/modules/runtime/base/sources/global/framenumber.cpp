// Original: library/modules/runtime/base/sources/global/framenumber.cpp
//
// The global frame number and the duration of the last frame, advanced once per frame by GameFramework::update.

#include "keen/base/framenumber.hpp"

namespace keen
{
    static uint32 s_lastTimeInMs; // 0x038B3C70
    static float s_lastFrameDuration; // 0x038B3C74
    static uint32 s_frameNumber; // 0x038B3C78
    static bool s_firstUpdate = true; // 0x016EBF08

    // 0x00BF0370 (:13)
    void setInitialFrameNumber(uint32 currentTimeInMs, uint32 expectedGameFrameRate)
    {
        s_lastTimeInMs = currentTimeInMs;
        s_firstUpdate = false;
        s_lastFrameDuration = 1.0f / (float)expectedGameFrameRate;
    }

    // 0x00BF03A0 (:20)
    void advanceFrameNumber(uint32 currentTimeInMs)
    {
        if (s_firstUpdate)
        {
            ++s_frameNumber;
            // Magic number: the first frame is taken as 1/30 s.
            s_lastFrameDuration = 1.0f / 30.0f;
            s_firstUpdate = false;
            s_lastTimeInMs = currentTimeInMs;
            return;
        }
        const float elapsedMs = (float)(currentTimeInMs - s_lastTimeInMs);
        ++s_frameNumber;
        s_lastTimeInMs = currentTimeInMs;
        s_lastFrameDuration = elapsedMs / 1000.0f;
    }

    // 0x00BF0400 (:36)
    uint32 getCurrentFrameNumber()
    {
        return s_frameNumber;
    }

    // 0x00BF0410 (:41)
    float getLastFrameDurationInSeconds()
    {
        return s_lastFrameDuration;
    }
}
