// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/properties/hksimpleproperty.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): a 64-bit property value (integers are sign-extended, pointers zero-extended)
// and a key/value property, with the inline functions of hksimpleproperty.h/.inl that have out-of-line copies.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/properties/hksimpleproperty.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// 0x0080CCE0 (.h:31)
hkSimplePropertyValue::hkSimplePropertyValue()
{
}

// 0x0080CD10 (.inl:52)
hkSimplePropertyValue::hkSimplePropertyValue(const int i)
{
    setInt(i);
}

// 0x00F20C40 (.inl:57)
// The unsigned value is sign-extended as well.
hkSimplePropertyValue::hkSimplePropertyValue(const unsigned int i)
{
    m_data = (uint64_t)(int64_t)(int)i;
}

// 0x0080CD20 (.inl:67)
hkSimplePropertyValue::hkSimplePropertyValue(void* p)
{
    setPtr(p);
}

// 0x0080CCF0 (.inl:35)
void hkSimplePropertyValue::setInt(const int i)
{
    m_data = (uint64_t)(int64_t)i;
}

// 0x0080CD00 (.inl:45)
void hkSimplePropertyValue::setPtr(void* p)
{
    m_data = (uint64_t)(uintptr_t)p;
}

// 0x0080CD40 (.inl:105)
void* hkSimplePropertyValue::getPtr() const
{
    return (void*)(uintptr_t)m_data;
}

// 0x00DF4A40
hkSimpleProperty::hkSimpleProperty()
{
}

// 0x00CEA030
hkSimpleProperty::hkSimpleProperty(hkFinishLoadedObjectFlag flag)
{
}
