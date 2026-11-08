// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkdefaultclassnameregistry.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SB): the default class name registry singleton, from rfg.exe
// (HK_SINGLETON_IMPLEMENTATION: created empty by hkBaseSystem::initSingletons; hkBuiltinTypeRegistry fills it).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkdefaultclassnameregistry.h"

// 0x00FBF700 (:12)
static hkReferencedObject* hkDefaultClassNameRegistrycreate()
{
    return new hkDefaultClassNameRegistry();
}

template<> hkDefaultClassNameRegistry* hkSingleton<hkDefaultClassNameRegistry>::s_instance = nullptr; // 0x03BAC6BC
// 0x01A3556C
static hkSingletonInitNode hkSingletonRegisterhkDefaultClassNameRegistry("hkDefaultClassNameRegistry",
    hkDefaultClassNameRegistrycreate, (void**)&hkSingleton<hkDefaultClassNameRegistry>::s_instance);
