// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkclasspointervtable.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkclasspointervtable.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE): registries for objects whose vtable slot holds their hkClass
// pointer instead of a vtable (objects loaded without native types, e.g. by the version tools). Declarations as
// the 2014.1 SDK's hkClassPointerVtable.h; layouts from rfg.pdb.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkvtableclassregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"

class hkClass;
class hkTypeInfo;

namespace hkClassPointerVtable
{
    // size 0x34
    class TypeInfoRegistry : public hkTypeInfoRegistry
    {
    public:
        HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x010548F0 (.h:28)
        TypeInfoRegistry(const hkStringMap<hkClass const*,hkContainerHeapAllocator>& classes); // 0x010544F0
        TypeInfoRegistry(const hkClassNameRegistry* classes); // 0x01054470
        virtual ~TypeInfoRegistry(); // vtbl+0x00 0x010545B0
        virtual const hkTypeInfo* finishLoadedObject(void* obj, const char* className) const; // vtbl+0x14 0x010546A0

    protected:
        hkRefPtr<hkClassNameRegistry const> m_classes; // 0x20
        mutable hkStringMap<hkTypeInfo*,hkContainerHeapAllocator> m_typeInfos; // 0x24: one empty type info per class name
    };

    // size 0x14
    class VtableRegistry : public hkVtableClassRegistry
    {
    public:
        HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00FA1900 (.h:45)
        virtual void registerVtable(const void* vtable, const hkClass* klass); // vtbl+0x10 0x01054450
        virtual const hkClass* getClassFromVirtualInstance(const void* obj) const; // vtbl+0x18 0x01054460
    };
}
