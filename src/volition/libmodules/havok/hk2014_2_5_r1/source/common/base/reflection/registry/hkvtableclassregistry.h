// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkvtableclassregistry.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkvtableclassregistry.cpp
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer). The lookups are inline in Havok (code
// from rfg.exe; the destructor is emitted in hkBuiltinTypeRegistry.obj).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.h"

class hkClass;
class hkTypeInfo;

// size 0x14
class hkVtableClassRegistry : public hkReferencedObject, public hkSingleton<hkVtableClassRegistry>
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F3C550, delete(void* p, unsigned int nbytes) 0x00F3CE20

    hkVtableClassRegistry() {} // 0x00F3CDB0 (.h:24)
    virtual ~hkVtableClassRegistry() {} // vtbl+0x00 0x00F3D070

    virtual void registerVtable(const void* vtable, const hkClass* klass); // vtbl+0x10 0x00FBF7F0

    // vtbl+0x14 0x00F3CFA0 (.h:31)
    virtual const hkClass* getClassFromVtable(const void* vtable)
    {
        return m_map.getWithDefault(vtable, nullptr);
    }

    void registerList(const hkTypeInfo* const * infos, const hkClass* const * classes); // 0x00FBF7B0
    void merge(const hkVtableClassRegistry& mergeFrom); // 0x00FBF810

    // vtbl+0x18 0x00F3CFC0 (.h:41): the object's first word is its vtable
    virtual const hkClass* getClassFromVirtualInstance(const void* obj) const
    {
        return m_map.getWithDefault(*(const void**)obj, nullptr);
    }

    // vtbl+0x1C 0x00F3CFE0 (.h:59): every registered class appended
    virtual void getClasses(hkArray<hkClass const*,hkContainerHeapAllocator>& classes) const
    {
        for (hkPointerMap<const void*, const hkClass*>::Iterator it = m_map.getIterator(); m_map.isValid(it);
             it = m_map.getNext(it))
        {
            classes.pushBack(m_map.getValue(it));
        }
    }

public:
    hkPointerMap<void const*,hkClass const*,hkContainerHeapAllocator> m_map; // 0x08
};
