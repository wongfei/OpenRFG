// Original: library/modules/runtime/graphics/include/keen/graphics/rendergeometryreader.hpp
// Implementation: library/modules/runtime/graphics/sources/global/rendergeometryreader.cpp
//
// RenderGeometryReader, reads vertex attributes and indices back from mapped geometry (IndexDecoder per index format).
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/indexformat.hpp"
#include "keen/graphics/vertexattribute.hpp"

namespace keen
{
    struct AxisAlignedBox;
    class GraphicsCommandWriter;
    struct RenderGeometry;
    struct Vector3;
    struct Vector4;
    struct VertexFormat;
    struct float4;

    // size 0x8
    class IndexDecoder
    {
    public:
        IndexDecoder(const uint8* pData, IndexFormat format); // 0x00C5E770 (:130)
        uint32 decodeIndex(uint32 index); // 0x00C5E790 (:134)

    private:
        const uint8* m_pData; // 0x00
        IndexFormat m_format; // 0x04
    };

    // size 0xAC
    class RenderGeometryReader
    {
    public:
        RenderGeometryReader(); // 0x00C5E6B0 (:12)
        ~RenderGeometryReader(); // 0x00C65FF0 (:16)
        bool open(GraphicsCommandWriter* pCommandWriter, const RenderGeometry* pGeometry); // 0x00C66000 (:21)
        void close(); // 0x00C5E6C0 (:94)
        const VertexFormat* getVertexFormat() const;
        uint32 getTriangleCount() const; // 0x00C5E750 (:114)
        uint32 getVertexCount() const; // 0x00C5E760 (:120)
        float4 readAttributeData(uint32 vertexIndex, VertexAttributeId attributeId) const; // 0x00C5DD50 (.h:33)
        Vector3 readVertexPosition(uint32 vertexIndex) const; // 0x00C5E180 (.h:46)
        Vector3 readVertexNormal(uint32) const;
        Vector4 readVertexTangentFlip(uint32) const;
        Vector4 readVertexTextureCoordinate(uint32, uint32) const;
        Vector4 readVertexColor0(uint32) const;
        bool getTriangleVertexPositions(Vector3* pVertexStorage, uint32 triangleIndex); // 0x00C66170 (:152)
        bool getTriangleIndices(uint32* pIndices, uint32 triangleIndex) const; // 0x00C5E7B0 (:167)
        void getAABB(AxisAlignedBox* pAabb); // 0x00C5E810 (:178)

    private:
        GraphicsCommandWriter* m_pCommandWriter; // 0x00
        const RenderGeometry* m_pGeometry; // 0x04
        const VertexFormat* m_pVertexFormat; // 0x08
        uint32 m_vertexStride; // 0x0C
        IndexFormat m_indexFormat; // 0x10
        uint32 m_currentPrimitiveIndex; // 0x14
        uint32 m_currentPrimitiveStartVertexIndex; // 0x18
        const uint8* m_attributeData[17]; // 0x1C
        VertexAttributeFormat m_attributeFormat[17]; // 0x60
        const uint8* m_pLockedIndexData; // 0xA4
        uint32 m_currentIndex; // 0xA8
    };
}
