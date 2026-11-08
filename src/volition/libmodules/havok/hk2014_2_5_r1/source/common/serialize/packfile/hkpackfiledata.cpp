// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/hkpackfiledata.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD; the class is the packfile pass's, SE, ported here because the
// data world copier tracks its copies in one), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/hkpackfiledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/attributes/hkattributes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hktypeinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.h"

// 0x00F799B0 (:27): the builtin class name registry unless one is given
hkPackfileData::hkPackfileData(const hkClassNameRegistry* reg)
    : m_topLevelObject(nullptr), m_name(nullptr), m_packfileClassRegistry(nullptr), m_destructorsEnabled(true)
{
    if (reg == nullptr)
    {
        reg = hkBuiltinTypeRegistry::getInstance().getClassNameRegistry();
    }
    m_packfileClassRegistry = reg;
}

// 0x00F79600 (:39): the finished objects are destructed through their type infos
void hkPackfileData::callDestructors()
{
    if (m_destructorsEnabled && m_trackedTypes.getSize() > 0)
    {
        for (TrackedObjectMap::Iterator it = m_trackedObjects.getIterator(); m_trackedObjects.isValid(it);
             it = m_trackedObjects.getNext(it))
        {
            const hkTypeInfo* typeInfo = m_trackedTypes.getWithDefault(m_trackedObjects.getValue(it), nullptr);
            if (typeInfo != nullptr)
            {
                typeInfo->cleanupLoadedObject(m_trackedObjects.getKey(it));
            }
        }
    }
    m_topLevelObject = nullptr;
    m_trackedObjects.clear();
    m_trackedTypes.clear();
}

// 0x00F79A60 (:61)
hkPackfileData::~hkPackfileData()
{
    callDestructors();
    for (int i = 0; i < m_memory.getSize(); i++)
    {
        hkMemoryRouter::easyFree(hkMemoryRouter::getInstance().heap(), m_memory[i]);
    }
    for (int i = 0; i < m_chunks.getSize(); i++)
    {
        hkMemoryRouter::getInstance().heap().blockFree(m_chunks[i].pointer, m_chunks[i].numBytes);
    }
    hkMemoryRouter::easyFree(hkMemoryRouter::getInstance().heap(), m_name);
    m_packfileClassRegistry = nullptr;
}

// 0x00F794E0 (:77)
void hkPackfileData::setName(const char* n)
{
    hkMemoryRouter::easyFree(hkMemoryRouter::getInstance().heap(), m_name);
    m_name = hkString::strDup(n);
}

// 0x00F796A0 (:83): copies of the lists (the outputs grow as hkArray's reserve does)
void hkPackfileData::getImportsExports(hkArray<hkResource::Import,hkContainerHeapAllocator>& impOut, hkArray<hkResource::Export,hkContainerHeapAllocator>& expOut) const
{
    impOut.reserve(m_imports.getSize());
    for (int i = 0; i < m_imports.getSize(); i++)
    {
        impOut.begin()[i] = m_imports[i];
    }
    impOut.setSizeUnchecked(m_imports.getSize());
    expOut.reserve(m_exports.getSize());
    for (int i = 0; i < m_exports.getSize(); i++)
    {
        expOut.begin()[i] = m_exports[i];
    }
    expOut.setSizeUnchecked(m_exports.getSize());
}

// 0x00F797E0 (:89)
void hkPackfileData::addExport(const char* symbolName, void* object)
{
    hkResource::Export& e = m_exports.expandOne();
    e.name = symbolName;
    e.data = object;
}

// 0x00F79520 (:96): every export of the object
void hkPackfileData::removeExport(void* object)
{
    for (int i = m_exports.getSize() - 1; i >= 0; i--)
    {
        if (m_exports[i].data == object)
        {
            m_exports.removeAt(i);
        }
    }
}

// 0x00F79820 (:108)
void hkPackfileData::addImport(const char* symbolName, void** location)
{
    hkResource::Import& imp = m_imports.expandOne();
    imp.name = symbolName;
    imp.location = location;
}

// 0x00F79570 (:122): the last import of the location
void hkPackfileData::removeImport(void** location)
{
    for (int i = m_imports.getSize() - 1; i >= 0; i--)
    {
        if (m_imports[i].location == location)
        {
            m_imports.removeAt(i);
            return;
        }
    }
}

// 0x00F795C0 (:135)
hkBool32 hkPackfileData::finishedObjects() const
{
    return m_trackedTypes.getSize() != 0;
}

// 0x00F79490 (:140)
static bool baseOrSameClass(const char* typeName, const char* topLevelClassName, const hkClassNameRegistry* classReg)
{
    const hkClass* topLevel = classReg->getClassByName(topLevelClassName);
    const hkClass* type = classReg->getClassByName(typeName);
    return type->isSuperClass(*topLevel);
}

// 0x00F79860 (:151): the tracked objects finished on the first request with a type registry (none when one has no
// type info), then the post-finish functions run
void* hkPackfileData::getContentsPointer(const char* typeName, const hkTypeInfoRegistry* typeRegistry) const
{
    if (m_topLevelObject == nullptr)
    {
        return nullptr;
    }
    const char* topLevelClassName = m_trackedObjects.getWithDefault(m_topLevelObject, nullptr);
    if (typeName != nullptr && !baseOrSameClass(typeName, topLevelClassName, m_packfileClassRegistry))
    {
        return nullptr;
    }
    if (!finishedObjects() && typeRegistry != nullptr)
    {
        TrackedObjectMap::Iterator it = m_trackedObjects.getIterator();
        if (m_trackedObjects.isValid(it))
        {
            bool failed = false;
            for (; m_trackedObjects.isValid(it); it = m_trackedObjects.getNext(it))
            {
                const char* className = m_trackedObjects.getValue(it);
                const hkTypeInfo* typeInfo = typeRegistry->finishLoadedObject(m_trackedObjects.getKey(it), className);
                if (typeInfo == nullptr)
                {
                    failed = true;
                }
                else
                {
                    m_trackedTypes.insert(className, typeInfo);
                }
            }
            if (failed)
            {
                return nullptr;
            }
        }
        for (const hkVariant* v = m_postFinishObjects.begin(); v < m_postFinishObjects.begin() + m_postFinishObjects.getSize(); v++)
        {
            const hkVariant* attr = v->m_class->getAttribute("hk.PostFinish");
            static_cast<const hkPostFinishAttribute*>(attr->m_object)->m_postFinishFunction(v->m_object);
        }
    }
    return m_topLevelObject;
}

// 0x00F794D0 (:207): the type name is not kept (the tracked objects know it)
void hkPackfileData::setContentsWithName(void* topLevelObject, const char* typeName)
{
    m_topLevelObject = topLevelObject;
}

// 0x00F79990 (:216)
const char* hkPackfileData::getContentsTypeName() const
{
    if (m_topLevelObject != nullptr)
    {
        return m_trackedObjects.getWithDefault(m_topLevelObject, nullptr);
    }
    return nullptr;
}

// 0x00F795D0 (:227)
void hkPackfileData::setPackfileClassNameRegistry(const hkClassNameRegistry* classReg)
{
    m_packfileClassRegistry = classReg;
}
