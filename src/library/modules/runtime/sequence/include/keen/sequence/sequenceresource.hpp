// Original: library/modules/runtime/sequence/include/keen/sequence/sequenceresource.hpp
// Implementation: library/modules/runtime/sequence/sources/global/sequenceresource.cpp
//
// SequenceResource, a timed event track (SequenceData of SequenceEvents). Placement of the data types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/staticarray.hpp"
#include "keen/resources/resource.hpp"

namespace keen
{
    struct SequenceData;
    struct SequenceInitializationData;

    // size 0xC
    class SequenceResource : public Resource
    {
    public:
        enum
        {
            ResourceClassType = 0x434E5153,
        };

    private:
        SequenceResource(const SequenceResource&);
    public:
        SequenceResource(uint32 key); // 0x00CE58A0 (.h:22)
        ~SequenceResource(); // 0x00CE58C0 (.h:23)
        const SequenceData* getSequenceData() const;
    protected:
        bool initialize(const SequenceInitializationData* pInitializationData); // 0x00CE59E0 (:9)
    private:
        SequenceResource& operator=(const SequenceResource&);

        SequenceData* m_pData; // 0x08
    };

    // size 0x1
    struct SequenceResourceHandleType
    {
        enum
        {
            TypeSignatureCrc = -1395699152,
        };

        static uint32 getAlignment();

    };

    // size 0x8
    struct SequenceEventData
    {
        enum
        {
            TypeSignatureCrc = -421182058,
        };

        static uint32 getAlignment();

        uint16 time; // 0x00
        uint16 type; // 0x02
        const void* pData; // 0x04
    };

    // size 0x8
    struct SequenceEvent
    {
        uint16 time; // 0x00
        uint16 type; // 0x02
        const void* pData; // 0x04
    };

    // size 0xC
    struct SequenceData
    {
        enum
        {
            TypeSignatureCrc = -1218097879,
        };

        static uint32 getAlignment();

        StaticArray<SequenceEventData> events; // 0x00
        uint16 length; // 0x08
        bool mustComplete; // 0x0A
    };
}
