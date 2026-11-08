// Original: library/modules/runtime/graphics/include/keen/graphics/rendergeometrywriter.hpp
// Implementation: library/modules/runtime/graphics/sources/global/rendergeometrywriter.cpp
//
// RenderGeometryWriter, writes vertices attribute by attribute (with format conversion) and indices into new geometry.
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/indexformat.hpp"
#include "keen/graphics/vertexattribute.hpp"

namespace keen
{
    class GraphicsCommandWriter;
    struct RenderGeometry;
    struct Vector2;
    struct Vector3;
    struct Vector4;
    struct VertexFormat;
    struct float4;

    // size 0x20
    class RenderGeometryWriter
    {
        RenderGeometryWriter(const RenderGeometryWriter&);
    public:
        RenderGeometryWriter(); // 0x00C5EF20 (:15)
        ~RenderGeometryWriter(); // 0x00C5EF30 (:20)
        bool open(GraphicsCommandWriter* pCommandWriter, RenderGeometry* pRenderGeometry); // 0x00C5EF40 (:25)
        void close(GraphicsCommandWriter* pCommandWriter); // 0x00C5F010 (:67)
        void beginVertex(); // 0x00C5F070 (:83)
        void endVertex(); // 0x00C5F090 (:91)
        void writeAttributeConverter(VertexAttributeId, const Vector4&);
        void writeAttributeConverter(VertexAttributeId, const Vector3&);
        void writeAttributeConverter(VertexAttributeId, const Vector2&);
        void writeAttributeConvert(VertexAttributeId id, uint32 color); // 0x00C5F0A0 (:100)
        void writeAttributeConvert(VertexAttributeId id, float x, float y, float z, float w); // 0x00C5F3B0 (:219)
        void writeAttributeConvert(VertexAttributeId id, float x, float y, float z); // 0x00C5F310 (:210)
        void writeAttributeConvert(VertexAttributeId id, float x, float y); // 0x00C5F270 (:201)
        void writeAttributeConvert(VertexAttributeId id, float x); // 0x00C5F1E0 (:192)
        void writeAttribute(VertexAttributeId id, uint16 x, uint16 y, uint16 z, uint16 w); // 0x00C5F5F0 (:315)
        void writeAttribute(VertexAttributeId id, uint16 x, uint16 y); // 0x00C5F5B0 (:302)
        void writeAttribute(VertexAttributeId id, uint32 color); // 0x00C5F570 (:290)
        void writeAttribute(VertexAttributeId id, float x, float y, float z, float w); // 0x00C5F520 (:273)
        void writeAttribute(VertexAttributeId id, float x, float y, float z); // 0x00C5F4D0 (:257)
        void writeAttribute(VertexAttributeId id, float x, float y); // 0x00C5F490 (:242)
        void writeAttribute(VertexAttributeId id, float x); // 0x00C5F450 (:228)
        void writeIndex(uint32 index); // 0x00C66E50 (:330)
    private:
        RenderGeometryWriter& operator=(const RenderGeometryWriter&);
        void writeAttributeWithConversion(uint8** ppTarget, VertexAttributeFormat format, float4 value); // 0x00C5F110 (:120)
        bool isValidVertexDatadWritePointer(void* pPointer, uint32 size) const; // 0x00C5F1A0 (:179)

        RenderGeometry* m_pGeometry; // 0x00
        const VertexFormat* m_pVertexFormat; // 0x04
        IndexFormat m_indexFormat; // 0x08
        uint32 m_currentVertexIndex; // 0x0C
        uint8* m_pCurrentVertexData; // 0x10
        uint8* m_pLockedVertexData; // 0x14
        uint8* m_pLockedIndexData; // 0x18
        uint32 m_currentIndex; // 0x1C
    };

    void writeFloats(uint8** ppTarget, const float* pSource, uint32 count); // 0x00C5F0E0 (:113)
}
