// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.cpp
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer). The members are inline in Havok (code
// from rfg.exe; the destructor is emitted in hkBuiltinTypeRegistry.obj); the .cpp holds the singleton.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hktypeinfo.h"

// size 0x20
class hkTypeInfoRegistry : public hkReferencedObject, public hkSingleton<hkTypeInfoRegistry>
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F3CAE0, delete(void* p, unsigned int nbytes) 0x00F3CB00

    // 0x00F3C340 (.h:23)
    hkTypeInfoRegistry(int finishFlag = 1, int informMemoryTracker = 1)
        : m_finishFlag(finishFlag), m_informMemoryTracker(informMemoryTracker)
    {
    }

    virtual ~hkTypeInfoRegistry() {} // vtbl+0x00 0x00F3C530

    // vtbl+0x10 0x00F3C410 (.h:29): keyed by the type name
    virtual void registerTypeInfo(const hkTypeInfo* info)
    {
        m_map.insert(info->getTypeName(), info);
    }

    // vtbl+0x14 0x00F3C430 (.h:37): finished with the registry's flag; the type info, null for an unknown class
    virtual const hkTypeInfo* finishLoadedObject(void* obj, const char* className) const
    {
        if (const hkTypeInfo* t = m_map.getWithDefault(className, nullptr))
        {
            t->finishLoadedObject(obj, m_finishFlag);
            return t;
        }
        return nullptr;
    }

    // 0x00F3BFC0 (.h:67): a null-terminated list
    void registerList(const hkTypeInfo* const * infos)
    {
        const hkTypeInfo* const * ti = infos;
        while (*ti != nullptr)
        {
            registerTypeInfo(*ti);
            ++ti;
        }
    }

    // vtbl+0x18 0x00F3C470 (.h:52)
    virtual const hkTypeInfo* cleanupLoadedObject(void* obj, const char* className) const
    {
        const hkTypeInfo* t = m_map.getWithDefault(className, nullptr);
        if (t != nullptr)
        {
            t->cleanupLoadedObject(obj);
        }
        return t;
    }

    // vtbl+0x1C 0x00F3C4A0 (.h:78)
    virtual void merge(const hkTypeInfoRegistry& mergeFrom)
    {
        const hkStringMap<const hkTypeInfo*>& map = mergeFrom.m_map;
        for (hkStringMap<const hkTypeInfo*>::Iterator it = map.getIterator(); map.isValid(it); it = map.getNext(it))
        {
            m_map.insert(map.getKey(it), map.getValue(it));
        }
    }

    // vtbl+0x20 0x00F3C510 (.h:89)
    virtual const hkTypeInfo* getTypeInfo(const char* typeName) const
    {
        return m_map.getWithDefault(typeName, nullptr);
    }

public:
    hkStringMap<hkTypeInfo const*,hkContainerHeapAllocator> m_map; // 0x08
    int m_finishFlag; // 0x18
    int m_informMemoryTracker; // 0x1C
};
