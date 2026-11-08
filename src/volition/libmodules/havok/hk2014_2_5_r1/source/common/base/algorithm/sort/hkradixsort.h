// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hkradixsort.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>

// size 0x1
class hkRadixSort
{
public:
    // size 0x4
    struct SortData16
    {
        typedef unsigned short KeyType;

        HK_DECLARE_POD_TYPE();

        union
        {
            unsigned short m_key; // 0x00
            unsigned char m_keys[2]; // 0x00
        };
        unsigned short m_userData; // 0x02
    };

    // size 0x8
    struct SortData32
    {
        typedef unsigned int KeyType;

        HK_DECLARE_POD_TYPE();

        union
        {
            unsigned int m_key; // 0x00
            unsigned char m_keys[4]; // 0x00
        };
        unsigned int m_userData; // 0x04
    };

    // size 0x10
    struct SortData64
    {
        typedef uint64_t KeyType;

        HK_DECLARE_POD_TYPE();

        union
        {
            uint64_t m_key; // 0x00
            unsigned char m_keys[8]; // 0x00
        };
        unsigned int m_userData; // 0x08
    };

    static void sort16(SortData16* data, int numObjects, SortData16* buffer); // 0x00FB98C0
    static void sort32(SortData32* data, int numObjects, SortData32* buffer); // 0x00FB9BC0
    static void sort64(SortData64* data, int numObjects, SortData64* buffer); // 0x00FBA190

};
