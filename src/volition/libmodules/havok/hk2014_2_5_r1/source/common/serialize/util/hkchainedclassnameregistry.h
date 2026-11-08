// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkchainedclassnameregistry.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkchainedclassnameregistry.cpp
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkdynamicclassnameregistry.h"

class hkClass;
class hkClassNameRegistry;

// size 0x20
class hkChainedClassNameRegistry : public hkDynamicClassNameRegistry
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00FAA970, delete(void* p, unsigned int nbytes) 0x01054A70
    hkChainedClassNameRegistry(const hkClassNameRegistry* nextReg); // 0x010549C0
    virtual ~hkChainedClassNameRegistry(); // vtbl+0x00 0x01054A10
    virtual const hkClass* getClassByName(const char* className) const; // vtbl+0x14 0x01054980
    void setNextRegistry(const hkClassNameRegistry* nextRegistry); // 0x01054A40

public:
    const hkClassNameRegistry* m_nextRegistry; // 0x1C
};
