// Original: volition/render_lib/code/shader/clbprojectordata.hpp
//
// Constant buffer layout(s) shared with the shaders (ClbProjectorFragmentConstants); keen namespace. Constant buffer
// structs are 16-byte aligned (rl_renderer::m_commonVertexData sits at 0x30 after members ending at 0x28;
// tools/hdrgen/aligncheck.py); alignas(16) is inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/graphicsmatrix.hpp"

namespace keen
{
    // size 0x140
    struct alignas(16) ClbProjectorFragmentConstants
    {
        GraphicsMatrix44 shadowMapMatrix0; // 0x00
        float4 nearClipParams; // 0x40
        float4 nearFarDepthParams; // 0x50
        float4 minMaxDepthParams; // 0x60
        float4 shadowFadeParams; // 0x70
        float4 shadowPercentParams0; // 0x80
        float4 shadowMapLoOffsets; // 0x90
        float4 shadowMapHiOffsets; // 0xA0
        float4 shadowMapDimensions; // 0xB0
        float4 viewport; // 0xC0
        float4 _dummy0; // 0xD0
        float4 _dummy1; // 0xE0
        float4 shadowPercentParams1; // 0xF0
        GraphicsMatrix44 shadowMapMatrix1; // 0x100
    };
}
