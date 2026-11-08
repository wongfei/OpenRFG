// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassenum.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkClass;
struct hkContainerHeapAllocator;
class hkCustomAttributes;
struct hkInternalClassMember;
class hkStreamWriter;
struct hkVariant;

// size 0x14
class hkClassEnum
{
public:
    enum FlagValues
    {
        FLAGS_NONE = 0,
    };

    typedef hkFlags<enum FlagValues,unsigned int> Flags;

    // size 0x8
    class Item
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Item);
        static const hkClass& staticClass(); // 0x00CEB7A0
        Item(int, const char*);
        const char* getName() const; // 0x00D0F170
        int getValue() const; // 0x00D0F180

        static const hkInternalClassMember Members[]; // 0x0134B6E4
    public:
        int m_value; // 0x00
        const char* m_name; // 0x04
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkClassEnum);
    static const hkClass& staticClass(); // 0x00CEB7C0
    hkClassEnum(const char*, const Item*, int);
    const char* getName() const; // 0x00CF2940
    int getNumItems() const; // 0x00D0F190
    const Item& getItem(int i) const; // 0x00D0F1A0
    hkResult getNameOfValue(int val, const char** name) const; // 0x00D0F1B0
    hkResult getValueOfName(const char* name, int* val) const; // 0x00D0F200
    hkResult decomposeFlags(int flagValue, hkArray<char const*,hkContainerHeapAllocator>& bitsOut, int& bitsOver) const; // 0x00D0F350
    unsigned int getSignature() const; // 0x00D0F320
    void writeSignature(hkStreamWriter* w) const; // 0x00D0F260
    const hkVariant* getAttribute(const char* id) const; // 0x00D0F300
    hkFlags<enum FlagValues,unsigned int>& getFlags();
    const hkFlags<enum FlagValues,unsigned int>& getFlags() const; // 0x00FA3DE0

    static const hkInternalClassMember Members[]; // 0x016F3E80
public:
    const char* m_name; // 0x00
    const Item* m_items; // 0x04
    int m_numItems; // 0x08
    hkCustomAttributes* m_attributes; // 0x0C
    hkFlags<enum FlagValues,unsigned int> m_flags; // 0x10
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassenum.inl"
