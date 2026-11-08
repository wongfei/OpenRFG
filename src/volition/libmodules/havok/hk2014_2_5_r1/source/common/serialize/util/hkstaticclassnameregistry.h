// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkstaticclassnameregistry.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkstaticclassnameregistry.cpp
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkclassnameregistry.h"

class hkClass;
struct hkContainerHeapAllocator;

// size 0x18
class hkStaticClassNameRegistry : public hkClassNameRegistry
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F45AE0
    hkStaticClassNameRegistry(const hkClass* const * classes, int classVersion, const char* name); // 0x00F45940
    hkStaticClassNameRegistry(hkClass* const * classes, int classVersion, const char* name); // 0x00F459D0
    virtual const char* getName() const; // vtbl+0x10 0x00F45900
    virtual const hkClass* getClassByName(const char* className) const; // vtbl+0x14 0x00F45970
    virtual void getClasses(hkArray<hkClass const*,hkContainerHeapAllocator>& classes) const; // vtbl+0x18 0x00F45A00
public:
    void checkIfReady() const; // 0x00F45910
public:
    virtual ~hkStaticClassNameRegistry() {} // vtbl+0x00 0x00F3C0B0 (implicit, emitted in hkBuiltinTypeRegistry.obj)

public:
    const char* m_name; // 0x08
    const hkClass* const * m_classes; // 0x0C
    int m_classVersion; // 0x10
    mutable unsigned int m_ready; // 0x14 (hkBool32): set by checkIfReady() const
};
