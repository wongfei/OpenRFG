// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0), with the inline members of Havok 2014.1
// (hkpLinearCastCollisionInput.inl). Hand-maintained since HK5 (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"

struct hkpCollisionAgentConfig;
struct hkpProcessCollisionInput;

// size 0x70
struct alignas(16) hkpLinearCastCollisionInput : public hkpCollisionInput
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpLinearCastCollisionInput);
    // 0x0081BCE0 (hkplinearcastcollisioninput.inl:10)
    hkpLinearCastCollisionInput()
    {
        m_maxExtraPenetration = 1.192092896e-07f; // HK_REAL_EPSILON
    }

    void set(const hkpProcessCollisionInput& input); // 0x0081BD00 (hkplinearcastcollisioninput.inl:23): HK6

    // 0x0082B010 (hkplinearcastcollisioninput.inl:16)
    void setPathAndTolerance(const hkVector4f& path, float tolerance)
    {
        m_path = path;
        m_cachedPathLength = path.length<3>().getReal();
        m_tolerance = tolerance;
    }

    hkVector4f m_path; // 0x50
    float m_maxExtraPenetration; // 0x60
    float m_cachedPathLength; // 0x64
    hkpCollisionAgentConfig* m_config; // 0x68
};
