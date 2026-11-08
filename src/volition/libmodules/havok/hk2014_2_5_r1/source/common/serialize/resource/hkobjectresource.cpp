// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD), from rfg.exe: the top-level object of a load and the registries
// it was built with (the builtin ones until set otherwise).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.h"

extern const hkClass hkReferencedObjectClass;

// 0x00F79120 (:13): typeName names the top-level class or one of its bases
static bool baseOrSameClass(const char* typeName, const char* topLevelClassName, const hkClassNameRegistry* classReg)
{
    const hkClass* topLevel = classReg->getClassByName(topLevelClassName);
    const hkClass* type = classReg->getClassByName(typeName);
    return type->isSuperClass(*topLevel);
}

// 0x00F79220 (:27)
hkObjectResource::hkObjectResource(const hkVariant& v)
    : m_topLevelObject(v),
      m_classRegistry(hkBuiltinTypeRegistry::getInstance().getClassNameRegistry()),
      m_typeRegistry(hkBuiltinTypeRegistry::getInstance().getTypeInfoRegistry())
{
}

// 0x00F79290 (:32): a referenced object loses the resource's reference; any other is destructed and freed
hkObjectResource::~hkObjectResource()
{
    if (m_topLevelObject.m_class != nullptr)
    {
        if (hkReferencedObjectClass.isSuperClass(*m_topLevelObject.m_class))
        {
            static_cast<hkReferencedObject*>(m_topLevelObject.m_object)->removeReference();
        }
        else
        {
            if (m_typeRegistry != nullptr)
            {
                m_typeRegistry->cleanupLoadedObject(m_topLevelObject.m_object, m_topLevelObject.m_class->getName());
            }
            hkMemoryRouter::getInstance().heap().blockFree(m_topLevelObject.m_object,
                                                            m_topLevelObject.m_class->getObjectSize());
        }
    }
    m_typeRegistry = nullptr;
    m_classRegistry = nullptr;
}

// 0x00F79340 (:51)
void hkObjectResource::setClassNameRegistry(const hkClassNameRegistry* classReg)
{
    m_classRegistry = classReg;
}

// 0x00F79370 (:56)
void hkObjectResource::setTypeInfoRegistry(const hkTypeInfoRegistry* typeReg)
{
    m_typeRegistry = typeReg;
}

// 0x00F79160 (:61)
const char* hkObjectResource::getName() const
{
    return nullptr;
}

// 0x00F79170 (:66)
void hkObjectResource::getImportsExports(hkArray<hkResource::Import,hkContainerHeapAllocator>& impOut, hkArray<hkResource::Export,hkContainerHeapAllocator>& expOut) const
{
}

// 0x00F791C0 (:70): the object only when typeName is its class or a base of it; the registry argument is unused
// (the object was finished when it was built)
void* hkObjectResource::getContentsPointer(const char* typeName, const hkTypeInfoRegistry* typeRegistry) const
{
    if (typeName != nullptr && !baseOrSameClass(typeName, m_topLevelObject.m_class->getName(), m_classRegistry))
    {
        return nullptr;
    }
    return m_topLevelObject.m_object;
}

// 0x00F79180 (:85)
void* hkObjectResource::stealContentsPointer(const char* typeName, const hkTypeInfoRegistry* typeRegistry)
{
    void* p = getContentsPointer(typeName, typeRegistry);
    if (p != nullptr)
    {
        m_topLevelObject.m_object = nullptr;
        m_topLevelObject.m_class = nullptr;
    }
    return p;
}

// 0x00F791B0 (:96)
const char* hkObjectResource::getContentsTypeName() const
{
    return m_topLevelObject.m_class->getName();
}
