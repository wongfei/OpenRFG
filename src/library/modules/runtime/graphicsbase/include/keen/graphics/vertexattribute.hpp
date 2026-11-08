// Original: library/modules/runtime/graphicsbase/include/keen/graphics/vertexattribute.hpp
// Implementation: library/modules/runtime/graphicsbase/sources/global/vertexattribute.cpp
//
// Vertex attribute semantics (VertexAttributeId) and storage formats (VertexAttributeFormat) with their info tables,
// and VertexAttributeDescription (one entry of a VertexFormat). decodeVertexAttribute is in vertexattribute.inl.
// Placement of the types inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct float4;

    enum VertexAttributeId
    {
        VertexAttributeId_Position = 0,
        VertexAttributeId_Normal = 1,
        VertexAttributeId_Tangent = 2,
        VertexAttributeId_BiTangent = 3,
        VertexAttributeId_Color0 = 4,
        VertexAttributeId_Color1 = 5,
        VertexAttributeId_Color2 = 6,
        VertexAttributeId_TexCoord0 = 7,
        VertexAttributeId_TexCoord1 = 8,
        VertexAttributeId_TexCoord2 = 9,
        VertexAttributeId_TexCoord3 = 10,
        VertexAttributeId_TexCoord4 = 11,
        VertexAttributeId_TexCoord5 = 12,
        VertexAttributeId_TexCoord6 = 13,
        VertexAttributeId_TexCoord7 = 14,
        VertexAttributeId_BoneIndex = 15,
        VertexAttributeId_BoneWeight = 16,
        VertexAttributeId_Count = 17,
        VertexAttributeId_Invalid = 255,
    };

    enum VertexAttributeFormat
    {
        VertexAttributeFormat_x32_float = 0,
        VertexAttributeFormat_x32y32_float = 1,
        VertexAttributeFormat_x32y32z32_float = 2,
        VertexAttributeFormat_x32y32z32w32_float = 3,
        VertexAttributeFormat_x16y16z16w16_float = 4,
        VertexAttributeFormat_x16y16z16w16_sint = 5,
        VertexAttributeFormat_x16y16z16w16_uint = 6,
        VertexAttributeFormat_x16y16z16w16_snorm = 7,
        VertexAttributeFormat_x16y16z16w16_unorm = 8,
        VertexAttributeFormat_x8y8z8w8_unorm = 9,
        VertexAttributeFormat_x8y8z8w8_uint = 10,
        VertexAttributeFormat_x8y8z8w8_snorm = 11,
        VertexAttributeFormat_x8y8z8w8_sint = 12,
        VertexAttributeFormat_x16y16_float = 13,
        VertexAttributeFormat_x16y16_sint = 14,
        VertexAttributeFormat_x16y16_snorm = 15,
        VertexAttributeFormat_x16y16_uint = 16,
        VertexAttributeFormat_x16y16_unorm = 17,
        VertexAttributeFormat_x10y10z10_snorm = 18,
        VertexAttributeFormat_x10y10z10w2_snorm = 19,
        VertexAttributeFormat_x11y11z10_snorm = 20,
        VertexAttributeFormat_z8y8x8w8_unorm = 21,
        VertexAttributeFormat_D3dColor = 22,
        VertexAttributeFormat_x16y16z16_snorm = 23,
        VertexAttributeFormat_x8y8z8_snorm = 24,
        VertexAttributeFormat_Count = 25,
        VertexAttributeFormat_Default_Position3d = 2,
        VertexAttributeFormat_Default_Normal = 2,
        VertexAttributeFormat_Default_TangentFlip = 3,
        VertexAttributeFormat_Default_TexCoord2d = 13,
        VertexAttributeFormat_Default_Color = 9,
        VertexAttributeFormat_Invalid = 255,
    };

    // size 0x4
    struct VertexAttributeDescription
    {
        void set(VertexAttributeId, VertexAttributeFormat, uint32, uint32);

        uint8 id; // 0x00
        uint8 format; // 0x01
        uint8 inputStreamIndex; // 0x02
        uint8 instanceStepRate; // 0x03
    };

    // size 0xC
    struct VertexAttributeFormatInfo
    {
        const char* pName; // 0x00
        uint32 sizeInBytes; // 0x04
        uint32 componentCount; // 0x08
    };

    // size 0x4
    struct VertexAttributeSemanticInfo
    {
        const char* pName; // 0x00
    };

    namespace graphics
    {
        VertexAttributeFormat getFloat32VertexAttributeFormat(uint32 dimension); // 0x00CE3B20 (:75)
        VertexAttributeFormat getFloat16VertexAttributeFormat(uint32 dimension); // 0x00CE3B40 (:87)
        const VertexAttributeSemanticInfo* getVertexAttributeSemanticInfo(VertexAttributeId semanticId); // 0x00CE3B60 (:99)
        const VertexAttributeFormatInfo* getVertexAttributeFormatInfo(VertexAttributeFormat format); // 0x00CE3B80 (:111)
        float4 decodeVertexAttribute(const uint8* pAttributeData, VertexAttributeFormat format); // 0x00C5B120 (vertexattribute.inl:8)
    }
}
