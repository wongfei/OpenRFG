// Original: library/modules/runtime/base/include/keen/base/framenumber.hpp
// Implementation: library/modules/runtime/base/sources/global/framenumber.cpp
//
// The global frame number and last frame duration.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    void setInitialFrameNumber(uint32 currentTimeInMs, uint32 expectedGameFrameRate); // 0x00BF0370 (:13)
    void advanceFrameNumber(uint32 currentTimeInMs); // 0x00BF03A0 (:20)
    uint32 getCurrentFrameNumber(); // 0x00BF0400 (:36)
    float getLastFrameDurationInSeconds(); // 0x00BF0410 (:41)
}
