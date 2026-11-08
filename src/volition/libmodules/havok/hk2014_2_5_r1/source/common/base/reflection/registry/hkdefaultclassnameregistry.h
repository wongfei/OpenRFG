// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkdefaultclassnameregistry.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkdynamicclassnameregistry.h"

// size 0x20
class hkDefaultClassNameRegistry : public hkDynamicClassNameRegistry, public hkSingleton<hkDefaultClassNameRegistry>
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F3C9D0, delete(void* p, unsigned int nbytes) 0x00F3CA50
    // 0x00F3C9F0 (.h:21): named after the SDK version (HAVOK_SDK_VERSION_STRING)
    hkDefaultClassNameRegistry() : hkDynamicClassNameRegistry("hk_2014.2.5-r1") {}
    virtual ~hkDefaultClassNameRegistry() {} // vtbl+0x00 0x00F3CA30

};
