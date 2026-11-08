// Original: library/modules/runtime/graphics/include/keen/graphics/rendertarget.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.cpp
//
// Render target part masks (which color/depth/stencil parts a render pass restores or resolves). RenderTarget and
// RenderTargetBuffer are defined in sources/d3d11/rendertarget_d3d11.hpp. Placement of the enum inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum RenderTargetPartFlagMask
    {
        RenderTargetPartFlagMask_None = 0,
        RenderTargetPartFlagMask_Target0 = 1,
        RenderTargetPartFlagMask_Target1 = 2,
        RenderTargetPartFlagMask_Target2 = 4,
        RenderTargetPartFlagMask_Target3 = 8,
        RenderTargetPartFlagMask_Depth = 16,
        RenderTargetPartFlagMask_Stencil = 32,
        RenderTargetPartFlagMask_Color = 15,
        RenderTargetPartFlagMask_DepthStencil = 48,
        RenderTargetPartFlagMask_All = 63,
    };
}
