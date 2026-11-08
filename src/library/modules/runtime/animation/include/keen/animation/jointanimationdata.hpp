// Original: library/modules/runtime/animation/include/keen/animation/jointanimationdata.hpp
//
// JointAnimationData, the serialized joint animation (header, flags, compressed keys). Placement of the flags and
// header inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum JointAnimationFlags
    {
        JointAnimationFlags_Clamp = 0,
        JointAnimationFlags_Looped = 1,
    };

    // size 0x24
    struct JointAnimationData
    {
        uint32 hierarchyHash; // 0x00
        float positionFactor; // 0x04
        float positionTangent; // 0x08
        float scaleFactor; // 0x0C
        uint32 dataOffset; // 0x10
        uint32 dataLength; // 0x14
        uint16 headerOffset; // 0x18
        uint16 headerCount; // 0x1A
        uint16 fps; // 0x1C
        uint16 frameCount; // 0x1E
        uint16 jointCount; // 0x20
        uint16 flags; // 0x22
    };

    // size 0x14
    struct JointAnimationHeader
    {
        uint32 dataOffset; // 0x00
        uint16 startTime; // 0x04
        uint16 endTime; // 0x06
        uint16 usedOrientationJoints; // 0x08
        uint16 nonConstOrientationJoints; // 0x0A
        uint16 usedPositionJoints; // 0x0C
        uint16 nonConstPositionJoints; // 0x0E
        uint16 usedScaleJoints; // 0x10
        uint16 nonConstScaleJoints; // 0x12
    };

    namespace animation
    {
        const uint8* getJointAnimationData(const JointAnimationData& animation); // 0x00CBB480 (.h:61)
        const JointAnimationHeader* getJointAnimationHeader(const JointAnimationData& animation); // 0x00CBB490 (.h:62)
    }
}
