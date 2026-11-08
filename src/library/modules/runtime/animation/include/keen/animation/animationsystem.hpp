// Original: library/modules/runtime/animation/include/keen/animation/animationsystem.hpp
// Implementation: library/modules/runtime/animation/sources/global/animationsystem.cpp
//
// AnimationSystem: created with AnimationSystemParameters, hands out animation command buffers that sample and blend
// animations into poses on worker tasks. Placement of the parameters inferred.
#pragma once

#include "keen/base/types.hpp"
#include "library/modules/runtime/animation/sources/global/animationcommandbuffer.hpp"
#include "keen/base/array.hpp"

namespace keen
{
    class MemoryAllocator;

    // size 0xC
    struct AnimationSystem
    {
        AnimationSystem(); // 0x00CC3620
        ~AnimationSystem(); // 0x00CBE980

        Array<AnimationCommandBuffer> buffer; // 0x00
        uint32 usedBuffer; // 0x08
    };

    // size 0xC
    struct AnimationSystemParameters
    {
        AnimationSystemParameters();

        uint32 bufferCount; // 0x00
        uint32 commandBufferSize; // 0x04
        uint32 commandBufferStackSize; // 0x08
    };

    namespace animation
    {
        void allocateCommandBuffer(AnimationCommandBuffer* pBuffer, MemoryAllocator* pAllocator, const AnimationSystemParameters& params); // 0x00CC7EC0 (:19)
        AnimationSystem* createAnimationSystem(MemoryAllocator* pAllocator, const AnimationSystemParameters& initParams); // 0x00CC8120 (:32)
        void destroyAnimationSystem(AnimationSystem* pSystem, MemoryAllocator* pAllocator); // 0x00CC8260 (:48)
        AnimationCommandBuffer* createAnimationCommandBuffer(AnimationSystem* pSystem); // 0x00CC3630 (:63)
        void destroyAnimationCommandBuffer(AnimationCommandBuffer* pCommandBuffer); // 0x00CC3670 (:81)
    }
}
