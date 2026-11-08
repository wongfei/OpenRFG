// Original: library/modules/runtime/base/include/keen/base/blob.hpp
//
// Blob, a self-relative serialized block: a size header followed by data that BlobMember<T> offsets point into.
// BlobMember<T> is written by hand; the addresses are of the <float3> instantiation.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x4
    struct Blob
    {
        uint32 size; // 0x00
    };

    // size 0x4
    template<typename T>
    struct BlobMember
    {
        const T* getValue(const Blob* pBlob) const; // <float3> 0x00C605C0 (.h:25)
        T* getValue(Blob* pBlob); // <float3> 0x00C605A0 (.h:24)

        uint32 offset; // 0x00
    };
}
