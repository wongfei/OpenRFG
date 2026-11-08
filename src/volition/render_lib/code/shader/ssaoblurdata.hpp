// Original: volition/render_lib/code/shader/ssaoblurdata.hpp
//
// Constant buffer layout(s) shared with the shaders (SsaoBlurFragmentShaderConstants); keen namespace. Constant buffer
// structs are 16-byte aligned (rl_renderer::m_commonVertexData sits at 0x30 after members ending at 0x28;
// tools/hdrgen/aligncheck.py); alignas(16) is inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x10
    struct alignas(16) SsaoBlurFragmentShaderConstants
    {
        float2 axis; // 0x00
        float edgeSharpness; // 0x08
        float __pad0; // 0x0C
    };
}
