// Original: library/modules/runtime/graphics/include/keen/graphics/vertexformat.hpp
// Implementation: library/modules/runtime/graphics/sources/global/vertexformat.cpp
//
// VertexFormat (a cached state object: up to 16 attribute descriptions with offsets and stride, per stream) and
// VertexInputBinding (a vertex format bound to a vertex shader input signature; its D3D11 input layout lives in
// sources/d3d11/graphicssystem_d3d11.hpp).
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/graphicsstateobjects.hpp"
#include "keen/graphics/vertexattribute.hpp"

namespace keen
{
    // size 0xF4
    struct VertexFormat : public GraphicsStateObject
    {
        enum
        {
            MaxStreamCount = 6,
            MaxAttributeCount = 17,
            UnusedInstanceDataStreamIndex = 6,
        };

        VertexFormat(); // 0x00C69970 (:7)
        bool fill(const VertexAttributeDescription* pAttributes, uint32 count); // 0x00C69980 (:12)
        bool addAttribute(const VertexAttributeDescription& attribute); // 0x00C60440 (:44)
        void clear(); // 0x00C604E0 (:71)
        uint32 getVertexStride(uint32 streamIndex) const; // 0x004B2220 (.h:31)
        bool hasAttribute(VertexAttributeId id) const; // 0x00C5B520 (.h:32)
        uint32 getAttributeOffset(VertexAttributeId id) const; // 0x00C5B540 (.h:34)
        VertexAttributeFormat getAttributeFormat(VertexAttributeId id) const; // 0x00C5B560 (.h:35)

        VertexAttributeDescription attributes[17]; // 0x08
        uint32 attributeOffsets[17]; // 0x4C
        uint32 attributeCount; // 0x90
        uint32 attributeIndices[17]; // 0x94
        uint32 streamStride[6]; // 0xD8
        uint32 instanceDataStreamIndex; // 0xF0
    };
}
