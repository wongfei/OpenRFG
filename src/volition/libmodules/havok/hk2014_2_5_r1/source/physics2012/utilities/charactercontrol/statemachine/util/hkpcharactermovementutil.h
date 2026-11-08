// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/statemachine/util/hkpcharactermovementutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (player_minimal.md P0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x1
class hkpCharacterMovementUtil
{
public:
    // size 0x70
    struct hkpMovementUtilInput
    {
        enum DesiredVelocitySpace
        {
            SURFACE_SPACE = 0,
            WORLD_SPACE = 1,
            DEFAULT_SPACE = 0,
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpMovementUtilInput);
        hkpMovementUtilInput(); // 0x00814BE0 (.h:32)

        hkVector4f m_forward; // 0x00
        hkVector4f m_up; // 0x10
        hkVector4f m_surfaceNormal; // 0x20
        hkVector4f m_currentVelocity; // 0x30
        hkVector4f m_desiredVelocity; // 0x40
        hkVector4f m_surfaceVelocity; // 0x50
        float m_gain; // 0x60
        float m_maxVelocityDelta; // 0x64
        hkEnum<enum DesiredVelocitySpace,unsigned char> m_desiredVelocitySpace; // 0x68
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCharacterMovementUtil);
    static void calculateMovement(const hkpMovementUtilInput& input, hkVector4f& velocityOut); // 0x00F20890

};
