// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/properties/hksimpleproperty.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;

// size 0x8
struct hkSimplePropertyValue
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkSimplePropertyValue);
    static const hkClass& staticClass(); // 0x00CEA070
    hkSimplePropertyValue(); // 0x0080CCE0 (.h:31)
    hkSimplePropertyValue(uint64_t);
    hkSimplePropertyValue(void* p); // 0x0080CD20 (hksimpleproperty.inl:67)
    hkSimplePropertyValue(const float);
    hkSimplePropertyValue(const unsigned int i); // 0x00F20C40
    hkSimplePropertyValue(const int i); // 0x0080CD10 (hksimpleproperty.inl:52)
    void setReal(const float);
    void setInt(const int i); // 0x0080CCF0 (hksimpleproperty.inl:35)
    void setPtr(void* p); // 0x0080CD00 (hksimpleproperty.inl:45)
    void setUint64(uint64_t);
    float getReal() const;
    int getInt() const;
    void* getPtr() const; // 0x0080CD40 (hksimpleproperty.inl:105)
    uint64_t getUint64() const;

    static const hkInternalClassMember Members[];
    uint64_t m_data; // 0x00
};

// size 0x10
class hkSimpleProperty
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkSimpleProperty); // new(unsigned int n, void* p) 0x00CEA020
    static const hkClass& staticClass(); // 0x00CEA040
    hkSimpleProperty(hkFinishLoadedObjectFlag flag); // 0x00CEA030
    hkSimpleProperty(unsigned int, hkSimplePropertyValue);
    hkSimpleProperty(unsigned int, int);
    hkSimpleProperty(); // 0x00DF4A40
    static void mapStringToKey(const char*, unsigned int&);

    static const hkInternalClassMember Members[];
    unsigned int m_key; // 0x00
    unsigned int m_alignmentPadding; // 0x04
    hkSimplePropertyValue m_value; // 0x08
};
