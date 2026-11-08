// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkcustomattributes.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"

class hkClass;
struct hkInternalClassMember;

// size 0x8
class hkCustomAttributes
{
public:
    // size 0xC
    struct Attribute
    {
        static const hkClass& staticClass(); // 0x00CEB700
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Attribute);

        static const hkInternalClassMember Members[];
        const char* m_name; // 0x00
        hkVariant m_value; // 0x04
    };

    static const hkClass& staticClass(); // 0x00CEB720
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkCustomAttributes);
    // 0x00CF2950 (.h:24): the value of the first attribute named id
    const hkVariant* getAttribute(const char* id) const
    {
        for (int i = 0; i < m_numAttributes; ++i)
        {
            if (hkString::strCmp(m_attributes[i].m_name, id) == 0)
            {
                return &m_attributes[i].m_value;
            }
        }
        return nullptr;
    }

    static const hkInternalClassMember Members[]; // 0x0134B428
public:
    const Attribute* m_attributes; // 0x00
    int m_numAttributes; // 0x04
};
