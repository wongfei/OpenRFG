// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/rendercontext.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/rendercontext.cpp
//
// RenderContext, the per-pass data passed to render effects (effect system, camera, view-projection matrix).
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/matrix44.hpp"

namespace keen
{
    class Camera;
    struct RenderEffectSystem;

    // size 0x4C
    struct RenderContext
    {
        RenderContext();

        const RenderEffectSystem* pRenderEffectSystem; // 0x00
        const Camera* pActiveCamera; // 0x04
        Matrix44 viewProjectionMatrix; // 0x08
        float activeCameraReciprocFarPlane; // 0x48
    };
}
