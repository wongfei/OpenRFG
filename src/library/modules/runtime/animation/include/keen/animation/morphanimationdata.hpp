// Original: library/modules/runtime/animation/include/keen/animation/morphanimationdata.hpp
//
// MorphAnimationData, the serialized morph animation (tracks of morph keys). Placement of the track and key types
// inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0xC
    struct MorphAnimationData
    {
        uint32 numTracks; // 0x00
        uint16 fps; // 0x04
        uint16 frameCount; // 0x06
        uint16 flags; // 0x08
        uint16 _pad; // 0x0A
    };

    // size 0xC
    struct MorphKey
    {
        uint32 time; // 0x00
        float value; // 0x04
        float control; // 0x08
    };

    // size 0xC
    struct MorphAnimationTrack
    {
        uint32 targetName; // 0x00
        uint32 keyCount; // 0x04
        uint32 dataOffset; // 0x08
    };
}
