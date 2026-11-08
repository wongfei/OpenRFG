// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD), from rfg.exe: the default builtin type registry, which fills the
// class name, type info and vtable registries from hkBuiltinTypeRegistry's lists, and the current version's static
// class name registry over the same class list.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hktypeinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkdefaultclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkvtableclassregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkstaticclassnameregistry.h"

template<> hkBuiltinTypeRegistry* hkSingleton<hkBuiltinTypeRegistry>::s_instance = nullptr; // 0x03A380D8

namespace hkHavokCurrentClasses
{
    // 0x03A380DC (:26): the current version's classes, merged into the default class name registry by init()
    hkStaticClassNameRegistry hkHavokDefaultClassRegistry(hkBuiltinTypeRegistry::StaticLinkedClasses, -1,
                                                          "hk_2014.2.5-r1");
}

// 0x00F3C0C0 (:30)
void hkBuiltinTypeRegistry::addType(const hkTypeInfo* info, const hkClass* klass)
{
    hkDefaultClassNameRegistry::getInstance().registerClass(klass, klass->getName());
    hkTypeInfoRegistry::getInstance().registerTypeInfo(info);
    if (klass->getNumInterfaces() > 0)
    {
        hkVtableClassRegistry::getInstance().registerVtable(info->getVtable(), klass);
    }
}

// size 0x8
class hkDefaultBuiltinTypeRegistry : public hkBuiltinTypeRegistry
{
public:
    // 0x00F3C250 (:43)
    hkDefaultBuiltinTypeRegistry()
    {
        init();
    }

    // 0x00F3C180 (:49)
    void init()
    {
        hkDefaultClassNameRegistry::getInstance().merge(hkHavokCurrentClasses::hkHavokDefaultClassRegistry);
        hkTypeInfoRegistry::getInstance().registerList(StaticLinkedTypeInfos);
        hkVtableClassRegistry::getInstance().registerList(StaticLinkedTypeInfos, StaticLinkedClasses);
    }

    // 0x00F3C570 (:56)
    hkTypeInfoRegistry* getTypeInfoRegistry() override
    {
        return &hkTypeInfoRegistry::getInstance();
    }

    // 0x00F3C580 (:61)
    hkClassNameRegistry* getClassNameRegistry() override
    {
        return &hkDefaultClassNameRegistry::getInstance();
    }

    // 0x00F3C590 (:66)
    hkVtableClassRegistry* getVtableClassRegistry() override
    {
        return &hkVtableClassRegistry::getInstance();
    }

    // 0x00F3D100 (:71): fresh registries, then filled again
    void reinitialize() override
    {
        hkDefaultClassNameRegistry::replaceInstance(new hkDefaultClassNameRegistry());
        hkTypeInfoRegistry::replaceInstance(new hkTypeInfoRegistry());
        hkVtableClassRegistry::replaceInstance(new hkVtableClassRegistry());
        init();
    }
    // the destructor (vtbl+0x00 0x00F3C290) is compiler-generated
};

// 0x00F3C120 (:81): the singleton's creation function; null until the three registries it fills exist
// (hkBaseSystem::initSingletons retries it after the others)
static hkReferencedObject* hkCreateBuiltInTypeRegistry()
{
    if (hkSingleton<hkVtableClassRegistry>::s_instance != nullptr &&
        hkSingleton<hkDefaultClassNameRegistry>::s_instance != nullptr &&
        hkSingleton<hkTypeInfoRegistry>::s_instance != nullptr)
    {
        return new hkDefaultBuiltinTypeRegistry();
    }
    return nullptr;
}

// 0x016FA178
static hkSingletonInitNode hkSingletonRegisterhkBuiltinTypeRegistry("hkBuiltinTypeRegistry",
    hkCreateBuiltInTypeRegistry, (void**)&hkSingleton<hkBuiltinTypeRegistry>::s_instance);
