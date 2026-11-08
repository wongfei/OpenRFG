// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/hkstepinfo.h
//
// Havok 2014.2.5: layout from rfg.pdb, inline members from the binary (havok_impl.md HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x10
class hkStepInfo
{
public:
    hkStepInfo(float startTime, float endTime) { set(startTime, endTime); } // 0x00DE9D70
    hkStepInfo() {} // 0x00814230 (.h:18)
    // 0x00B71210 (.h:21): a zero step has a zero inverse
    void set(float startTime, float endTime)
    {
        m_startTime = startTime;
        m_endTime = endTime;
        const float dt = endTime - startTime;
        m_deltaTime = dt;
        m_invDeltaTime = (dt == 0.0f) ? 0.0f : 1.0f / dt;
    }

    hkPadSpu<float> m_startTime; // 0x00
    hkPadSpu<float> m_endTime; // 0x04
    hkPadSpu<float> m_deltaTime; // 0x08
    hkPadSpu<float> m_invDeltaTime; // 0x0C
};
