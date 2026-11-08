// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/util/hkrenamedclassnameregistry.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/version/hkversionregistry.h"

class hkClass;

// size 0x1C
class hkRenamedClassNameRegistry : public hkClassNameRegistry
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00FA35C0
    hkRenamedClassNameRegistry(const hkVersionRegistry::ClassRename* renames, const hkClassNameRegistry* originalRegistry); // 0x00FA3870
    virtual ~hkRenamedClassNameRegistry(); // vtbl+0x00 0x00FA3550
    virtual const char* getName() const; // vtbl+0x10 0x00FA3580
    virtual const hkClass* getClassByName(const char* oldname) const; // vtbl+0x14 0x00FA3600
    virtual void getClasses(hkArray<hkClass const*,hkContainerHeapAllocator>& classes) const; // vtbl+0x18 0x00FA35A0
    void setOriginalRegistry(const hkClassNameRegistry* originalRegistry); // 0x00FA3630
    const char* getRename(const char* oldName) const; // 0x00FA7B80
    void registerRenames(const hkVersionRegistry::ClassRename* renames); // 0x00FA3660

    hkStringMap<char const*,hkContainerHeapAllocator> m_renames; // 0x08
    const hkClassNameRegistry* m_originalRegistry; // 0x18
};
