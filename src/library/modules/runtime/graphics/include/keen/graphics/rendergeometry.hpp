// Original: library/modules/runtime/graphics/include/keen/graphics/rendergeometry.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.cpp
//
// Render geometry descriptions (vertex/index counts, formats, access mode) and creation data, plus the vertex/index
// buffer and primitive data passed to draw commands. RenderGeometry, VertexBufferData and IndexBufferData are defined
// in sources/d3d11/rendergeometry_d3d11.hpp. Placement of the enums and PrimitiveData inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/indexformat.hpp"
#include "keen/graphics/primitivetype.hpp"

enum D3D_PRIMITIVE_TOPOLOGY;

namespace keen
{
    struct GraphicsSystem;
    struct IndexBufferData;
    class MemoryAllocator;
    struct RenderGeometry;
    struct VertexFormat;

    enum RenderGeometryAccessMode
    {
        RenderGeometryAccessMode_None = 0,
        RenderGeometryAccessMode_Read = 1,
        RenderGeometryAccessMode_Write = 2,
        RenderGeometryAccessMode_ReadWrite = 3,
        RenderGeometryAccessMode_Count = 4,
    };

    // size 0x1C
    struct RenderGeometryDescription
    {
        RenderGeometryDescription(); // 0x004B22F0 (.h:83)

        const VertexFormat* pVertexFormat; // 0x00
        uint32 vertexCount; // 0x04
        uint32 indexCount; // 0x08
        PrimitiveType primitiveType; // 0x0C
        IndexFormat indexFormat; // 0x10
        uint8 cpuAccessMode; // 0x14
        uint32 streamIndex; // 0x18
    };

    // size 0x14
    struct RenderGeometryInitializationData
    {
        RenderGeometryInitializationData(); // 0x004B2320 (.h:105)

        const void* pVertexData; // 0x00
        const void* pIndexData; // 0x04
        const void* pSkinningData; // 0x08
        const void* pMorphingData; // 0x0C
        const void* pPlatformData; // 0x10
    };

    // size 0x8
    struct PrimitiveData
    {
        uint32 primitiveTopology; // 0x00
        uint32 vertexDrawCount; // 0x04
    };

    // size 0xC
    struct MappedRenderGeometryInfo
    {
        uint32 startOffset; // 0x00
        uint32 numBytes; // 0x04
        uint32 mapFlags; // 0x08
    };

    D3D_PRIMITIVE_TOPOLOGY getPrimitiveTopologyFromPrimitiveType(PrimitiveType type); // 0x00C5C8B0 (rendergeometry_d3d11.cpp:40)

    namespace graphics
    {
        void setIndexFormatOnBuffer(IndexBufferData* pBuffer, IndexFormat indexFormat); // 0x00C5C8C0 (rendergeometry_d3d11.cpp:46)
        void setPrimitiveTypeOnPrimitiveData(PrimitiveData* pData, PrimitiveType type); // 0x00C5C8E0 (rendergeometry_d3d11.cpp:52)
        bool allocateRenderGeometry(RenderGeometry* pGeometry, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pDataAllocator, const RenderGeometryDescription& description, const char* pDebugName); // 0x00C62580 (rendergeometry_d3d11.cpp:57)
        bool initializeRenderGeometry(RenderGeometry* pGeometry, GraphicsSystem* pGraphicsSystem, const RenderGeometryDescription& description, const RenderGeometryInitializationData& initData, const char* pDebugName); // 0x00C62710 (rendergeometry_d3d11.cpp:182)
        void freeRenderGeometry(RenderGeometry* pGeometry, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pDataAllocator); // 0x00C5C900 (rendergeometry_d3d11.cpp:300)
        IndexFormat getIndexFormat(const RenderGeometry* pGeometry); // 0x00C5C930 (rendergeometry_d3d11.cpp:322)
        PrimitiveType getPrimitiveType(const RenderGeometry* pGeometry); // 0x00C5C950 (rendergeometry_d3d11.cpp:338)
        bool getRenderGeometryDescription(RenderGeometryDescription* pTarget, const RenderGeometry* pGeometry); // 0x00C5DCD0 (:8)
    }
}
