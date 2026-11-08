// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkchainedclassnameregistry.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD), from rfg.exe: a dynamic class name registry that falls back to
// another registry (which it holds a reference to).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkchainedclassnameregistry.h"

// 0x010549C0 (:14)
hkChainedClassNameRegistry::hkChainedClassNameRegistry(const hkClassNameRegistry* nextReg)
    : hkDynamicClassNameRegistry(nullptr), m_nextRegistry(nextReg)
{
    if (nextReg != nullptr)
    {
        nextReg->addReference();
    }
}

// 0x01054A10 (:22)
hkChainedClassNameRegistry::~hkChainedClassNameRegistry()
{
    if (m_nextRegistry != nullptr)
    {
        m_nextRegistry->removeReference();
    }
}

// 0x01054A40 (:30)
void hkChainedClassNameRegistry::setNextRegistry(const hkClassNameRegistry* nextRegistry)
{
    if (nextRegistry != nullptr)
    {
        nextRegistry->addReference();
    }
    if (m_nextRegistry != nullptr)
    {
        m_nextRegistry->removeReference();
    }
    m_nextRegistry = nextRegistry;
}

// 0x01054980 (:43)
const hkClass* hkChainedClassNameRegistry::getClassByName(const char* className) const
{
    const hkClass* c = m_map.getWithDefault(className, nullptr);
    if (c == nullptr && m_nextRegistry != nullptr)
    {
        return m_nextRegistry->getClassByName(className);
    }
    return c;
}
