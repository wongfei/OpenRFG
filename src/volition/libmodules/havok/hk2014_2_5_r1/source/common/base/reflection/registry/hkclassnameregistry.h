// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkclassnameregistry.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;
struct hkContainerHeapAllocator;

// size 0x8
class hkClassNameRegistry : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F3C630
    virtual ~hkClassNameRegistry() {} // vtbl+0x00 0x00F3C0A0 (inline, emitted in hkBuiltinTypeRegistry.obj)
    virtual const char* getName() const = 0; // vtbl+0x10
    virtual const hkClass* getClassByName(const char*) const = 0; // vtbl+0x14
    hkClassNameRegistry() {} // 0x00F3C310 (inline, emitted in hkBuiltinTypeRegistry.obj)
    virtual void getClasses(hkArray<hkClass const*,hkContainerHeapAllocator>&) const = 0; // vtbl+0x18

};
