// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkclasspointervtable.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkclasspointervtable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hktypeinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkdynamicclassnameregistry.h"

// 0x01054470 (:13)
hkClassPointerVtable::TypeInfoRegistry::TypeInfoRegistry(const hkClassNameRegistry* classes) : m_classes(nullptr)
{
    m_classes = classes;
}

// 0x010544F0 (:18): a dynamic registry of the classes, owned
hkClassPointerVtable::TypeInfoRegistry::TypeInfoRegistry(const hkStringMap<hkClass const*,hkContainerHeapAllocator>& classes) : m_classes(nullptr)
{
    hkDynamicClassNameRegistry* reg = new hkDynamicClassNameRegistry(nullptr);
    reg->merge(classes);
    m_classes.setAndDontIncrementRefCount(reg);
}

// 0x010545B0 (:25)
hkClassPointerVtable::TypeInfoRegistry::~TypeInfoRegistry()
{
    for (hkStringMap<hkTypeInfo*,hkContainerHeapAllocator>::Iterator it = m_typeInfos.getIterator(); m_typeInfos.isValid(it);
         it = m_typeInfos.getNext(it))
    {
        delete m_typeInfos.getValue(it);
    }
    m_classes = nullptr; // no hkRefPtr destructor in this port
}

// 0x010546A0 (:35): the object's vtable slot gets its class (when the class has one); the type info returned has no
// functions. The reflection classes share a dummy.
const hkTypeInfo* hkClassPointerVtable::TypeInfoRegistry::finishLoadedObject(void* obj, const char* className) const
{
    if (hkString::strCmp(className, "hkClass") == 0 || hkString::strCmp(className, "hkClassMember") == 0 ||
        hkString::strCmp(className, "hkClassEnum") == 0 || hkString::strCmp(className, "hkClassEnumItem") == 0)
    {
        static hkTypeInfo s_dummyTypeInfo("dummyTypeInfo", nullptr, nullptr, nullptr, nullptr, 0);
        return &s_dummyTypeInfo;
    }
    const hkClass* klass = m_classes->getClassByName(className);
    if (klass->hasVtable())
    {
        *static_cast<const hkClass**>(obj) = klass;
    }
    hkTypeInfo* typeInfo = m_typeInfos.getWithDefault(className, nullptr);
    if (typeInfo == nullptr)
    {
        typeInfo = new hkTypeInfo(className, nullptr, nullptr, nullptr, nullptr, 0);
        m_typeInfos.insert(className, typeInfo);
    }
    return typeInfo;
}

// 0x01054450 (:60): nothing to register, the objects carry their class
void hkClassPointerVtable::VtableRegistry::registerVtable(const void* vtable, const hkClass* klass)
{
}

// 0x01054460 (:65)
const hkClass* hkClassPointerVtable::VtableRegistry::getClassFromVirtualInstance(const void* obj) const
{
    return *static_cast<const hkClass* const*>(obj);
}
