// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassenum.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SB): reflected enums (value and name lookups, flag decomposition,
// signatures), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassenum.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkcustomattributes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/oarchive/hkoarchive.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/crc/hkcrcstreamwriter.h"

// 0x00D0F350 (:16): the names of the items whose bits are all set, highest item first; the bits no item covers are
// left in bitsOver (failure when there are any)
hkResult hkClassEnum::decomposeFlags(int flagValue, hkArray<char const*,hkContainerHeapAllocator>& bitsOut,
                                     int& bitsOver) const
{
    bitsOut.clear();
    for (int i = m_numItems - 1; i >= 0 && flagValue != 0; --i)
    {
        int val = m_items[i].getValue();
        if ((val & flagValue) == val)
        {
            bitsOut.pushBack(m_items[i].getName());
            flagValue &= ~val;
        }
    }
    bitsOver = flagValue;
    return flagValue == 0 ? HK_SUCCESS : HK_FAILURE;
}

// 0x00D0F1B0 (:46)
hkResult hkClassEnum::getNameOfValue(int val, const char** name) const
{
    for (int i = 0; i < m_numItems; ++i)
    {
        if (m_items[i].getValue() == val)
        {
            *name = m_items[i].getName();
            return HK_SUCCESS;
        }
    }
    return HK_FAILURE;
}

// 0x00D0F200 (:59): the name compared without case
hkResult hkClassEnum::getValueOfName(const char* name, int* val) const
{
    for (int i = 0; i < m_numItems; ++i)
    {
        if (hkString::strCasecmp(name, m_items[i].getName()) == 0)
        {
            *val = m_items[i].getValue();
            return HK_SUCCESS;
        }
    }
    return HK_FAILURE;
}

// 0x00D0F320 (:72)
unsigned int hkClassEnum::getSignature() const
{
    hkCrc32StreamWriter crc;
    writeSignature(&crc);
    return crc.getCrc();
}

// 0x00D0F260 (:79): the name, each item's name and value, the item count
void hkClassEnum::writeSignature(hkStreamWriter* w) const
{
    hkOArchive oa(w, false);
    oa.writeRaw(m_name, hkString::strLen(m_name));
    int numItems = m_numItems;
    for (int i = 0; i < numItems; ++i)
    {
        const Item& item = m_items[i];
        oa.writeRaw(item.getName(), hkString::strLen(item.getName()));
        oa.write32(item.getValue());
    }
    oa.write32(numItems);
}

// 0x00D0F300 (:95)
const hkVariant* hkClassEnum::getAttribute(const char* id) const
{
    return m_attributes != nullptr ? m_attributes->getAttribute(id) : nullptr;
}
