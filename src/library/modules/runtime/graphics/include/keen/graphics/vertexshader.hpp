// Original: library/modules/runtime/graphics/include/keen/graphics/vertexshader.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/vertexshader_d3d11.cpp
//
// Vertex shader creation. VertexShader is defined in sources/d3d11/vertexshader_d3d11.hpp.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct VertexShader;

    namespace graphics
    {
        uint32 getVertexShaderInputSignatureHash(const VertexShader* pVertexShader); // 0x00C5CE90 (vertexshader_d3d11.cpp:127)
    }
}
