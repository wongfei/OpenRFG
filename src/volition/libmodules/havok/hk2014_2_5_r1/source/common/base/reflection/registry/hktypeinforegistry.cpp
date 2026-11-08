// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SB): the type info registry singleton, from rfg.exe
// (HK_SINGLETON_IMPLEMENTATION: created empty by hkBaseSystem::initSingletons; hkBuiltinTypeRegistry fills it).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.h"

// 0x00FBF760 (:12)
static hkReferencedObject* hkTypeInfoRegistrycreate()
{
    return new hkTypeInfoRegistry();
}

template<> hkTypeInfoRegistry* hkSingleton<hkTypeInfoRegistry>::s_instance = nullptr; // 0x03BAC6C0
// 0x01A3557C
static hkSingletonInitNode hkSingletonRegisterhkTypeInfoRegistry("hkTypeInfoRegistry", hkTypeInfoRegistrycreate,
    (void**)&hkSingleton<hkTypeInfoRegistry>::s_instance);
