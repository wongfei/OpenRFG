// Original: library/modules/runtime/graphics/sources/d3d11/geometryinstancing_d3d11.hpp
//
// GeometryInstancingBuffer (a dynamic instance vertex buffer) and GeometryInstancingBatch (a range of it).
#pragma once

#include "keen/base/types.hpp"
#include <d3d11.h>

struct ID3D11Buffer;
struct ID3D11DeviceContext;

namespace keen
{
    // size 0x10
    struct GeometryInstancingBuffer
    {
        ID3D11DeviceContext* pContext; // 0x00
        ID3D11Buffer* pVertexBuffer; // 0x04
        uint32 currentPosition; // 0x08
        uint32 size; // 0x0C
    };

    // size 0x10
    struct GeometryInstancingBatch
    {
        ID3D11Buffer* pVertexBuffer; // 0x00
        uint32 instanceCount; // 0x04
        uint32 vertexBufferByteOffset; // 0x08
        uint32 vertexBufferStride; // 0x0C
    };

    namespace graphics
    {
        void fillGeometryInstancingBatch(GeometryInstancingBatch* pOut, const GeometryInstancingBatch* pSource, uint32 offset, uint32 instanceCount); // 0x004B23D0 (.h:41)
    }
}
