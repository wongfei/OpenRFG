// Original: library/modules/runtime/resources/include/keen/resources/blobresource.hpp
// Implementation: library/modules/runtime/resources/sources/blobresource.cpp
//
// BlobResource, a resource that is a raw block of data, and its handle tag. Placement of the handle tag inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resource.hpp"

namespace keen
{
    // size 0x10
    class BlobResource : public Resource
    {
    public:
        enum
        {
            ResourceClassType = 0x424F4C42,
        };

        const uint8* getData() const;
        uint32 getSize() const;
    protected:
        BlobResource(uint32 key); // 0x00CDD6C0 (:7)
    public:
        ~BlobResource(); // 0x00CDD3A0

    protected:
        const uint8* m_pData; // 0x08
        uint32 m_size; // 0x0C
    };

    struct BlobResourceHandleType; // no definition in PDB
}
