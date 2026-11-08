// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkstaticclassnameregistry.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD), from rfg.exe: a class name registry over a null-terminated class
// list, searched linearly. A list given as non-const classes has its member offsets recomputed on first use.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkstaticclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionutil.h"

// 0x00F459D0 (:14)
hkStaticClassNameRegistry::hkStaticClassNameRegistry(hkClass* const * classes, int classVersion, const char* name)
    : m_name(name), m_classes(classes), m_classVersion(classVersion), m_ready(0)
{
}

// 0x00F45940 (:20)
hkStaticClassNameRegistry::hkStaticClassNameRegistry(const hkClass* const * classes, int classVersion, const char* name)
    : m_name(name), m_classes(classes), m_classVersion(classVersion), m_ready(1)
{
}

// 0x00F45900 (:25)
const char* hkStaticClassNameRegistry::getName() const
{
    return m_name;
}

// 0x00F45A00 (:30)
void hkStaticClassNameRegistry::getClasses(hkArray<hkClass const*,hkContainerHeapAllocator>& classes) const
{
    checkIfReady();
    int n = 0;
    while (m_classes[n] != nullptr)
    {
        n++;
    }
    const hkClass** c = classes.expandBy(n);
    for (int i = 0; m_classes[i] != nullptr; i++)
    {
        c[i] = m_classes[i];
    }
}

// 0x00F45970 (:45)
const hkClass* hkStaticClassNameRegistry::getClassByName(const char* className) const
{
    checkIfReady();
    for (int i = 0; m_classes[i] != nullptr; i++)
    {
        if (hkString::strCmp(className, m_classes[i]->getName()) == 0)
        {
            return m_classes[i];
        }
    }
    return nullptr;
}

// 0x00F45910 (:58)
void hkStaticClassNameRegistry::checkIfReady() const
{
    if (m_ready == 0)
    {
        hkVersionUtil::recomputeClassMemberOffsets(const_cast<hkClass* const*>(m_classes), m_classVersion);
        m_ready = 1;
    }
}
