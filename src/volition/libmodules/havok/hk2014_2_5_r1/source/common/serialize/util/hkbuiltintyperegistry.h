// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD): the registry of the classes linked into the program (type infos,
// class names, vtables), a singleton (2014.1 hkBuiltinTypeRegistry.h). The two lists are defined by
// hkbuiltintyperegistry.cxx, which rfg.exe compiles into rfg_havok_unity.cpp (havok_cm_manager.cpp).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.h"

class hkClass;
class hkClassNameRegistry;
class hkTypeInfo;
class hkTypeInfoRegistry;
class hkVtableClassRegistry;

// size 0x8
// The constructor (0x00F3C1E0) and destructor (vtbl+0x00 0x00F3BFF0) are compiler-generated.
class hkBuiltinTypeRegistry : public hkReferencedObject, public hkSingleton<hkBuiltinTypeRegistry>
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F3C1F0, delete(void* p, unsigned int nbytes) 0x00F3C210 (.h:24)
    virtual hkTypeInfoRegistry* getTypeInfoRegistry() = 0; // vtbl+0x10
    hkTypeInfoRegistry* getLoadedObjectRegistry() { return getTypeInfoRegistry(); } // 0x0080F270 (.h:27)
    virtual hkClassNameRegistry* getClassNameRegistry() = 0; // vtbl+0x14
    virtual hkVtableClassRegistry* getVtableClassRegistry() = 0; // vtbl+0x18
    virtual void addType(const hkTypeInfo* info, const hkClass* klass); // vtbl+0x1C 0x00F3C0C0
    virtual void reinitialize() = 0; // vtbl+0x20

public:
    static const hkTypeInfo* const StaticLinkedTypeInfos[]; // 0x012F8628
    static const hkClass* const StaticLinkedClasses[]; // 0x012F8A48
};
