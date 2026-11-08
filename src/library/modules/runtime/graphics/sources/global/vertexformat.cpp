// Original: library/modules/runtime/graphics/sources/global/vertexformat.cpp
//
// VertexFormat: up to 17 attributes over up to 6 vertex streams. Attributes are packed in the order they are added;
// attributeIndices maps an attribute id to its slot (17 = not present). At most one stream may carry per-instance
// data. The members marked (.h:N) are inline in vertexformat.hpp in the original.

#include "keen/graphics/vertexformat.hpp"
#include "keen/base/trace.hpp"

namespace keen
{
    // 0x00C69970 (:7)
    VertexFormat::VertexFormat()
    {
        clear();
    }

    // 0x00C69980 (:12)
    bool VertexFormat::fill(const VertexAttributeDescription* pAttributes, uint32 count)
    {
        clear();
        instanceDataStreamIndex = UnusedInstanceDataStreamIndex;
        for (uint32 i = 0; i < count; ++i)
        {
            if (!addAttribute(pAttributes[i]))
            {
                return false;
            }
            if (pAttributes[i].instanceStepRate != 0)
            {
                if (instanceDataStreamIndex == UnusedInstanceDataStreamIndex)
                {
                    instanceDataStreamIndex = pAttributes[i].inputStreamIndex;
                }
                else if (instanceDataStreamIndex != pAttributes[i].inputStreamIndex)
                {
                    debug::nativePrintArgs("[graphics] Vertexformat supports a single instance Data Stream only!\n");
                    return false;
                }
            }
        }
        return true;
    }

    // 0x00C60440 (:44)
    bool VertexFormat::addAttribute(const VertexAttributeDescription& attribute)
    {
        if (attributeCount >= MaxAttributeCount)
        {
            return false;
        }
        if (attributeIndices[attribute.id] != MaxAttributeCount)
        {
            debug::nativePrintArgs("[graphics] Duplicate vertex format attribute found!\n");
            return false;
        }
        attributes[attributeCount] = attribute;
        attributeOffsets[attributeCount] = streamStride[attribute.inputStreamIndex];
        attributeIndices[attribute.id] = attributeCount;
        streamStride[attribute.inputStreamIndex] += graphics::getVertexAttributeFormatInfo((VertexAttributeFormat)attribute.format)->sizeInBytes;
        ++attributeCount;
        return true;
    }

    // 0x00C604E0 (:71)
    void VertexFormat::clear()
    {
        attributeCount = 0;
        for (uint32 i = 0; i < MaxStreamCount; ++i)
        {
            streamStride[i] = 0;
        }
        for (uint32 i = 0; i < MaxAttributeCount; ++i)
        {
            attributeIndices[i] = MaxAttributeCount;
        }
        instanceDataStreamIndex = 0;
    }

    // 0x004B2220 (.h:31)
    uint32 VertexFormat::getVertexStride(uint32 streamIndex) const
    {
        return streamStride[streamIndex];
    }

    // 0x00C5B520 (.h:32)
    bool VertexFormat::hasAttribute(VertexAttributeId id) const
    {
        return attributeIndices[id] < MaxAttributeCount;
    }

    // 0x00C5B540 (.h:34)
    uint32 VertexFormat::getAttributeOffset(VertexAttributeId id) const
    {
        if (attributeIndices[id] < MaxAttributeCount)
        {
            return attributeOffsets[attributeIndices[id]];
        }
        return 0xffffffffu;
    }

    // 0x00C5B560 (.h:35)
    VertexAttributeFormat VertexFormat::getAttributeFormat(VertexAttributeId id) const
    {
        if (attributeIndices[id] < MaxAttributeCount)
        {
            return (VertexAttributeFormat)attributes[attributeIndices[id]].format;
        }
        return VertexAttributeFormat_Invalid;
    }
}
