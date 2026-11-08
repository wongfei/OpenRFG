// Original: volition/render_lib/code/shader/ssaocalcdata.hpp
//
// Constant buffer layout(s) shared with the shaders (SsaoCalcFragmentShaderConstants); keen namespace. Constant buffer
// structs are 16-byte aligned (rl_renderer::m_commonVertexData sits at 0x30 after members ending at 0x28;
// tools/hdrgen/aligncheck.py); alignas(16) is inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x80
    struct alignas(16) SsaoCalcFragmentShaderConstants
    {
        float intensity; // 0x00
        float aoClamp; // 0x04
        float bias; // 0x08
        float epsilon; // 0x0C
        float projScale; // 0x10
        float _dummy0; // 0x14
        float2 minMaxDepth; // 0x18
        float depthThreshold; // 0x20
        float depthThresholdRcp; // 0x24
        float depthFadeStart; // 0x28
        float depthVisibleRangeRcp; // 0x2C
        float4 radius; // 0x30
        float4 projInfo; // 0x40
        float4 clipInfo; // 0x50
        float4 targetSize; // 0x60
        float4 sourceSize; // 0x70
    };
}
