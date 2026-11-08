// Original: library/modules/runtime/animation/include/keen/animation/lveanimationdata.hpp
//
// LveAnimationData, the serialized LVE animation (orientation and position keys). Placement of the flags and key types
// inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum LveAnimationFlags
    {
        LveAnimationFlags_Clamp = 0,
        LveAnimationFlags_Looped = 1,
    };

    // size 0x28
    struct LveAnimationData
    {
        uint32 trackName; // 0x00
        uint32 orientationOffset; // 0x04
        uint32 orientationKeyCount; // 0x08
        uint32 positionOffset; // 0x0C
        uint32 positionKeyCount; // 0x10
        float3 startToEnd; // 0x14
        uint16 fps; // 0x20
        uint16 frameCount; // 0x22
        uint16 flags; // 0x24
    };

    // size 0x24
    struct LveOrientationKey
    {
        uint32 time; // 0x00
        float4 values; // 0x04
        float4 controls; // 0x14
    };

    // size 0x1C
    struct LvePositionKey
    {
        uint32 time; // 0x00
        float3 values; // 0x04
        float3 controls; // 0x10
    };

    namespace animation
    {
        const LveOrientationKey* getLveAnimationOrientation(const LveAnimationData& animation); // 0x00CBB0E0 (.h:53)
        const LvePositionKey* getLveAnimationPosition(const LveAnimationData& animation); // 0x00CBB0F0 (.h:54)
    }
}
