// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hktypedunion.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"

class hkClass;
class hkClassEnum;

// size 0x50
class hkTypedUnion
{
public:
    // size 0x8
    struct EnumVariant
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(EnumVariant);

        int m_value; // 0x00
        const hkClassEnum* m_enum; // 0x04
    };

    // size 0x40
    union alignas(16) Storage
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Storage);

        union
        {
            bool m_bool; // 0x00
            char m_char; // 0x00
            signed char m_int8; // 0x00
            unsigned char m_uint8; // 0x00
            short m_int16; // 0x00
            unsigned short m_uint16; // 0x00
            int m_int32; // 0x00
            unsigned int m_uint32; // 0x00
            int64_t m_int64; // 0x00
            uint64_t m_uint64; // 0x00
            float m_float; // 0x00
            float m_float4[4]; // 0x00
            float m_float12[12]; // 0x00
            float m_float16[16]; // 0x00
            hkVariant m_variant; // 0x00
            EnumVariant m_enumVariant; // 0x00
            unsigned long m_ulong; // 0x00
            int m_maxSize[16]; // 0x00
        };
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkTypedUnion);
    hkTypedUnion(); // 0x00FA7B70
    void setObject(const void* orig, const hkClass* klass); // 0x00CF3640
    void setEnum(int val, const hkClassEnum* eklass); // 0x00CF3660
    hkResult setSimple(const void* orig, const hkClassMember::Type type); // 0x00CF3680
    hkClassMember::Type getType() const;
    const Storage& getStorage() const; // 0x00FA3DF0

public:
    hkEnum<enum hkClassMember::Type,int> m_type; // 0x00
    Storage m_elem; // 0x10
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hktypedunion.inl"
