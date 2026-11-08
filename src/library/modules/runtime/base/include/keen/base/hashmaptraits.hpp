// Original: library/modules/runtime/base/include/keen/base/hashmaptraits.hpp
//
// DefaultHashmapTraits<K, V>, the hash and key compare HashMap uses unless told otherwise. Written by hand; the
// addresses are of the <uint32, GraphicsStateObject*> instantiation (<GSO>).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x1
    template<typename K, typename V>
    struct DefaultHashmapTraits
    {
        enum
        {
            BucketSize = 4,
            MinBucketCount = 8,
        };

        static uint32 getHashValue(const K& key); // <GSO> 0x00C6A530 (.h:27)
        static bool isKeyEqual(const K& key0, const K& key1); // <GSO> 0x00C60D60 (.h:32)
    };
}
