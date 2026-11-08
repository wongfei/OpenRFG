// Original: volition/render_lib/code/shader/gaussblurdata.hpp
//
// Constant buffer layout(s) shared with the shaders (GaussBlurFragmentShaderConstants); keen namespace. Constant
// buffer structs are 16-byte aligned (rl_renderer::m_commonVertexData sits at 0x30 after members ending at 0x28;
// tools/hdrgen/aligncheck.py); alignas(16) is inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0xB0
    struct alignas(16) GaussBlurFragmentShaderConstants
    {
        float4 tintColor; // 0x00
        float4 axis; // 0x10
        float4 samplingOffsetsAndWeights[9]; // 0x20
    };
}
