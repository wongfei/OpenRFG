// Original: library/modules/runtime/graphicsbase/sources/global/vertexattribute.cpp
//
// Vertex attribute format sizes and semantic names. The format table is as shipped: entries 22 and 23 carry the
// Xbox 360 names ("xb360_color", "xb360_skinning_position") although the enum calls them D3dColor and
// x16y16z16_snorm.

#include "keen/graphics/vertexattribute.hpp"
#include "keen/base/float16.hpp"

#include <stdlib.h>
#include <string.h>

namespace keen
{
    static const VertexAttributeFormat s_f32DimensionToFormat[5] =
    {
        VertexAttributeFormat_Invalid,
        VertexAttributeFormat_x32_float,
        VertexAttributeFormat_x32y32_float,
        VertexAttributeFormat_x32y32z32_float,
        VertexAttributeFormat_x32y32z32w32_float,
    };

    static const VertexAttributeFormat s_f16DimensionToFormat[5] =
    {
        VertexAttributeFormat_Invalid,
        VertexAttributeFormat_x16y16_float, // one and three components are padded to two and four
        VertexAttributeFormat_x16y16_float,
        VertexAttributeFormat_x16y16z16w16_float,
        VertexAttributeFormat_x16y16z16w16_float,
    };

    static const VertexAttributeSemanticInfo s_semanticInfo[VertexAttributeId_Count] =
    {
        { "Position" }, { "Normal" }, { "Tangent" }, { "BiTangent" },
        { "Color0" }, { "Color1" }, { "Color2" },
        { "TexCoord0" }, { "TexCoord1" }, { "TexCoord2" }, { "TexCoord3" },
        { "TexCoord4" }, { "TexCoord5" }, { "TexCoord6" }, { "TexCoord7" },
        { "BoneIndex" }, { "BoneWeight" },
    };

    static const VertexAttributeFormatInfo s_formatInfos[VertexAttributeFormat_Count] =
    {
        { "x32_float", 4, 1 },
        { "x32y32_float", 8, 2 },
        { "x32y32z32_float", 12, 3 },
        { "x32y32z32w32_float", 16, 4 },
        { "x16y16z16w16_float", 8, 4 },
        { "x16y16z16w16_sint", 8, 4 },
        { "x16y16z16w16_uint", 8, 4 },
        { "x16y16z16w16_snorm", 8, 4 },
        { "x16y16z16w16_unorm", 8, 4 },
        { "x8y8z8w8_unorm", 4, 4 },
        { "x8y8z8w8_uint", 4, 4 },
        { "x8y8z8w8_snorm", 4, 4 },
        { "x8y8z8w8_sint", 4, 4 },
        { "x16y16_float", 4, 2 },
        { "x16y16_sint", 4, 2 },
        { "x16y16_snorm", 4, 2 },
        { "x16y16_uint", 4, 2 },
        { "x16y16_unorm", 4, 2 },
        { "x10y10z10_snorm", 4, 3 },
        { "x10y10z10w2_snorm", 4, 4 },
        { "x11y11z10_snorm", 4, 3 },
        { "z8y8x8w8_unorm", 4, 4 },
        { "xb360_color", 4, 4 },
        { "xb360_skinning_position", 6, 3 },
        { "x8y8z8_snorm", 3, 3 },
    };

    namespace graphics
    {
        // 0x00CE3B20 (:75)
        VertexAttributeFormat getFloat32VertexAttributeFormat(uint32 dimension)
        {
            if (dimension < 5)
            {
                return s_f32DimensionToFormat[dimension];
            }
            return VertexAttributeFormat_Count;
        }

        // 0x00CE3B40 (:87)
        VertexAttributeFormat getFloat16VertexAttributeFormat(uint32 dimension)
        {
            if (dimension < 5)
            {
                return s_f16DimensionToFormat[dimension];
            }
            return VertexAttributeFormat_Count;
        }

        // 0x00CE3B60 (:99)
        const VertexAttributeSemanticInfo* getVertexAttributeSemanticInfo(VertexAttributeId semanticId)
        {
            if (semanticId < VertexAttributeId_Count)
            {
                return &s_semanticInfo[semanticId];
            }
            return nullptr;
        }

        // 0x00CE3B80 (:111)
        const VertexAttributeFormatInfo* getVertexAttributeFormatInfo(VertexAttributeFormat format)
        {
            if (format < VertexAttributeFormat_Count)
            {
                return &s_formatInfos[format];
            }
            return nullptr;
        }

        // 0x00C5B120 (vertexattribute.inl:8)
        float4 decodeVertexAttribute(const uint8* pAttributeData, VertexAttributeFormat format)
        {
            float4 result;
            result.x = 0.0f;
            result.y = 0.0f;
            result.z = 0.0f;
            result.w = 0.0f;
            switch (format)
            {
            case VertexAttributeFormat_x32_float:
                memcpy(&result.x, pAttributeData, 4);
                break;
            case VertexAttributeFormat_x32y32_float:
                memcpy(&result.x, pAttributeData, 8);
                break;
            case VertexAttributeFormat_x32y32z32_float:
                memcpy(&result.x, pAttributeData, 12);
                break;
            case VertexAttributeFormat_x32y32z32w32_float:
                memcpy(&result.x, pAttributeData, 16);
                break;
            case VertexAttributeFormat_x16y16z16w16_float:
            {
                const uint16* pHalf = (const uint16*)pAttributeData;
                result.x = convertFloat16ToFloat32(pHalf[0]);
                result.y = convertFloat16ToFloat32(pHalf[1]);
                result.z = convertFloat16ToFloat32(pHalf[2]);
                result.w = convertFloat16ToFloat32(pHalf[3]);
                break;
            }
            case VertexAttributeFormat_x8y8z8w8_unorm:
            {
                const uint32 value = *(const uint32*)pAttributeData;
                result.x = (float)(value & 0xffu) / 255.0f;
                result.y = (float)((value >> 8) & 0xffu) / 255.0f;
                result.z = (float)((value >> 16) & 0xffu) / 255.0f;
                result.w = (float)(value >> 24) / 255.0f;
                break;
            }
            case VertexAttributeFormat_x8y8z8w8_uint:
            {
                const uint32 value = *(const uint32*)pAttributeData;
                result.x = (float)(value & 0xffu);
                result.y = (float)((value >> 8) & 0xffu);
                result.z = (float)((value >> 16) & 0xffu);
                result.w = (float)(value >> 24);
                break;
            }
            case VertexAttributeFormat_x16y16_float:
            {
                const uint16* pHalf = (const uint16*)pAttributeData;
                result.x = convertFloat16ToFloat32(pHalf[0]);
                result.y = convertFloat16ToFloat32(pHalf[1]);
                break;
            }
            case VertexAttributeFormat_x10y10z10w2_snorm:
            {
                // Magic numbers: 10-bit signed fields scaled by 1/511, x in the top bits; w is left at 0.
                const sint32 value = *(const sint32*)pAttributeData;
                result.x = (float)(value >> 22) / 511.0f;
                result.y = (float)((value << 10) >> 22) / 511.0f;
                result.z = (float)((value << 20) >> 22) / 511.0f;
                break;
            }
            case VertexAttributeFormat_x11y11z10_snorm:
            {
                // Magic numbers: 11/11/10-bit signed fields scaled by 1/1023, 1/1023 and 1/511, x in the top bits.
                const sint32 value = *(const sint32*)pAttributeData;
                result.x = (float)(value >> 21) / 1023.0f;
                result.y = (float)((value << 11) >> 21) / 1023.0f;
                result.z = (float)((value << 22) >> 22) / 511.0f;
                break;
            }
            case VertexAttributeFormat_x16y16z16w16_sint:
            case VertexAttributeFormat_x16y16z16w16_uint:
            case VertexAttributeFormat_x16y16z16w16_snorm:
            case VertexAttributeFormat_x16y16z16w16_unorm:
            case VertexAttributeFormat_x8y8z8w8_snorm:
            case VertexAttributeFormat_x8y8z8w8_sint:
            case VertexAttributeFormat_x16y16_sint:
            case VertexAttributeFormat_x16y16_snorm:
            case VertexAttributeFormat_x16y16_uint:
            case VertexAttributeFormat_x16y16_unorm:
            case VertexAttributeFormat_x10y10z10_snorm:
            case VertexAttributeFormat_z8y8x8w8_unorm:
            case VertexAttributeFormat_D3dColor:
                // Unsupported formats terminate (KEEN_BREAK in the shipped build).
                exit(1);
            default:
                break;
            }
            return result;
        }
    }
}
