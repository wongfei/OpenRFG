// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkvtableclassregistry.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SB): the vtable to class registry and its singleton, from rfg.exe
// (HK_SINGLETON_IMPLEMENTATION: created empty by hkBaseSystem::initSingletons; hkBuiltinTypeRegistry fills it).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkvtableclassregistry.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hktypeinfo.h"

// vtbl+0x10 0x00FBF7F0 (:22)
void hkVtableClassRegistry::registerVtable(const void* vtable, const hkClass* klass)
{
    m_map.insert(vtable, klass);
}

// 0x00FBF7B0 (:40): two parallel null-terminated lists; the non-virtual types (no vtable) are skipped
void hkVtableClassRegistry::registerList(const hkTypeInfo* const * infos, const hkClass* const * classes)
{
    const hkTypeInfo* const * ti = infos;
    const hkClass* const * ci = classes;
    while (*ti != nullptr && *ci != nullptr)
    {
        if (const void* vtable = (*ti)->getVtable())
        {
            registerVtable(vtable, *ci);
        }
        ++ti;
        ++ci;
    }
}

// 0x00FBF810 (:55)
void hkVtableClassRegistry::merge(const hkVtableClassRegistry& mergeFrom)
{
    const hkPointerMap<const void*, const hkClass*>& map = mergeFrom.m_map;
    for (hkPointerMap<const void*, const hkClass*>::Iterator it = map.getIterator(); map.isValid(it); it = map.getNext(it))
    {
        m_map.insert(map.getKey(it), map.getValue(it));
    }
}

// 0x00FBF890 (:68)
static hkReferencedObject* hkVtableClassRegistrycreate()
{
    return new hkVtableClassRegistry();
}

template<> hkVtableClassRegistry* hkSingleton<hkVtableClassRegistry>::s_instance = nullptr; // 0x03BAC6C4
// 0x01A3558C
static hkSingletonInitNode hkSingletonRegisterhkVtableClassRegistry("hkVtableClassRegistry",
    hkVtableClassRegistrycreate, (void**)&hkSingleton<hkVtableClassRegistry>::s_instance);
