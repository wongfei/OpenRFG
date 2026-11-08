// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkdynamicclassnameregistry.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer). Every member is inline in Havok
// (code from rfg.exe; the destructor is emitted in hkBuiltinTypeRegistry.obj).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkclassnameregistry.h"

// size 0x1C
class hkDynamicClassNameRegistry : public hkClassNameRegistry
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x010548D0, delete(void* p, unsigned int nbytes) 0x00F3C8F0

    hkDynamicClassNameRegistry(const char* name) : m_name(name) {} // 0x00F3C670 (:23)
    virtual ~hkDynamicClassNameRegistry() {} // vtbl+0x00 0x00F3C320

    virtual const char* getName() const { return m_name; } // vtbl+0x10 0x00F3C6A0 (:27)

    // vtbl+0x14 0x00F3C7E0 (:39)
    virtual const hkClass* getClassByName(const char* className) const
    {
        return m_map.getWithDefault(className, nullptr);
    }

    // vtbl+0x18 0x00F3CCC0 (:45): every registered class appended
    virtual void getClasses(hkArray<hkClass const*,hkContainerHeapAllocator>& classes) const
    {
        for (hkStringMap<const hkClass*>::Iterator it = m_map.getIterator(); m_map.isValid(it); it = m_map.getNext(it))
        {
            classes.pushBack(m_map.getValue(it));
        }
    }

    virtual void setName(const char* name) { m_name = name; } // vtbl+0x1C 0x00F3C6B0 (:33)

    // vtbl+0x20 0x00F3C800 (:59): registered under its own name when name is null
    virtual void registerClass(const hkClass* klass, const char* name = nullptr)
    {
        if (name == nullptr)
        {
            name = klass->getName();
        }
        m_map.insert(name, klass);
    }

    // vtbl+0x24 0x00F3C6C0 (:65): a null-terminated list
    virtual void registerList(const hkClass* const * classes)
    {
        const hkClass* const * ci = classes;
        while (*ci != nullptr)
        {
            registerClass(*ci);
            ++ci;
        }
    }

    // vtbl+0x28 0x00F3D270 (:87)
    virtual void merge(const hkClassNameRegistry& mergeFrom)
    {
        hkArray<const hkClass*> classes;
        mergeFrom.getClasses(classes);
        for (int i = 0; i < classes.getSize(); ++i)
        {
            registerClass(classes[i]);
        }
    }

    // vtbl+0x2C 0x00F3C830 (:76): each class registered under its own name, not the map's key
    virtual void merge(const hkStringMap<hkClass const*,hkContainerHeapAllocator>& mergeFromMap)
    {
        for (hkStringMap<const hkClass*>::Iterator it = mergeFromMap.getIterator(); mergeFromMap.isValid(it);
             it = mergeFromMap.getNext(it))
        {
            registerClass(mergeFromMap.getValue(it));
        }
    }

public:
    const char* m_name; // 0x08
    hkStringMap<hkClass const*,hkContainerHeapAllocator> m_map; // 0x0C
};
