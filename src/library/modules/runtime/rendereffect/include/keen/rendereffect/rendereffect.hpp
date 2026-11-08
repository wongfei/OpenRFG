// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/rendereffect.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/rendereffect.cpp
//
// Render effects: an effect id (RenderEffectId, user ids start at RenderEffectId_FirstUserId) and the material data
// header every effect's material starts with (BaseMaterialData). Placement inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum RenderEffectId
    {
        RenderEffectId_Invalid = 0,
        RenderEffectId_Fallback = 1,
        RenderEffectId_ParticleLight = 2,
        RenderEffectId_ParticleLineLight = 3,
        RenderEffectId_Callback = 4,
        RenderEffectId_FirstUserId = 8,
    };

    // size 0x8
    struct BaseMaterialData
    {
        enum
        {
            TypeSignatureCrc = 0x79456914,
        };

        static uint32 getAlignment();

        uint32 renderEffectId; // 0x00
        uint32 mainTextureHandleOffset; // 0x04
    };
}
