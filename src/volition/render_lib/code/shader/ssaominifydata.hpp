// Original: volition/render_lib/code/shader/ssaominifydata.hpp
//
// Constant buffer layout(s) shared with the shaders (SsaoMinifyFragmentShaderConstants); keen namespace. Constant
// buffer structs are 16-byte aligned (rl_renderer::m_commonVertexData sits at 0x30 after members ending at 0x28;
// tools/hdrgen/aligncheck.py); alignas(16) is inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x30
    struct alignas(16) SsaoMinifyFragmentShaderConstants
    {
        float4 targetSize; // 0x00
        float4 sourceSize; // 0x10
        float4 projInfo; // 0x20
    };
}
