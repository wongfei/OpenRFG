// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x40
struct alignas(16) hkpCollisionQualityInfo
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCollisionQualityInfo);
    float calcMinSeparation(float) const;
    // 0x00D9ED60: leaves everything uninitialized
    hkpCollisionQualityInfo()
    {
    }

    float m_keepContact; // 0x00
    float m_create4dContact; // 0x04
    float m_createContact; // 0x08
    float m_manifoldTimDistance; // 0x0C
    hkPadSpu<unsigned int> m_useContinuousPhysics; // 0x10
    hkBool m_useSimpleToiHandling; // 0x14
    float m_minSeparation; // 0x18
    float m_minExtraSeparation; // 0x1C
    float m_minSafeDeltaTime; // 0x20
    float m_minAbsoluteSafeDeltaTime; // 0x24
    float m_toiSeparation; // 0x28
    float m_toiExtraSeparation; // 0x2C
    float m_toiAccuracy; // 0x30
    float m_maxContraintViolation; // 0x34
    float m_minToiDeltaTime; // 0x38
    unsigned short m_constraintPriority; // 0x3C
    hkBool m_enableToiWeldRejection; // 0x3E
};
