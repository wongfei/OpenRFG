// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkinternalclassmember.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

class hkClass;
class hkClassEnum;

// size 0x8
struct hkInternalCustomAttributes
{
    // size 0xC
    struct Attribute
    {
        HK_DECLARE_PLACEMENT_ALLOCATOR();

        const char* m_name; // 0x00
        const void* m_data; // 0x04
        const hkClass* m_klass; // 0x08
    };

    HK_DECLARE_PLACEMENT_ALLOCATOR();

    const Attribute* m_attributes; // 0x00
    int m_numAttributes; // 0x04
};

// size 0x18
struct hkInternalClassMember
{
    HK_DECLARE_PLACEMENT_ALLOCATOR();

    const char* m_name; // 0x00
    const hkClass* m_class; // 0x04
    const hkClassEnum* m_enum; // 0x08
    unsigned char m_type; // 0x0C
    unsigned char m_subtype; // 0x0D
    unsigned short m_cArraySize; // 0x0E
    unsigned short m_flags; // 0x10
    unsigned short m_offset; // 0x12
    const hkInternalCustomAttributes* m_attributes; // 0x14
};

// size 0x8
struct hkInternalClassEnumItem
{
    HK_DECLARE_PLACEMENT_ALLOCATOR();

    int m_value; // 0x00
    const char* m_name; // 0x04
};

// size 0x14
struct hkInternalClassEnum
{
    HK_DECLARE_PLACEMENT_ALLOCATOR();

    const char* m_name; // 0x00
    const hkInternalClassEnumItem* m_items; // 0x04
    int m_numItems; // 0x08
    const hkInternalCustomAttributes* m_attributes; // 0x0C
    unsigned int m_flags; // 0x10
};
