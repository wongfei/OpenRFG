// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD): a resource holding one top-level object (hkSerializeUtil::
// loadOnHeap), as 2014.1 hkObjectResource.h.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.h"

class hkClassNameRegistry;

// size 0x8
struct hkVariant
{
    void* m_object; // 0x00
    const hkClass* m_class; // 0x04
};

// size 0x18
class hkObjectResource : public hkResource
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F679A0, delete(void* p, unsigned int nbytes) 0x00F79400 (.h:20)
    hkObjectResource(const hkVariant& v); // 0x00F79220
    virtual ~hkObjectResource(); // vtbl+0x00 0x00F79290
    virtual const char* getName() const; // vtbl+0x10 0x00F79160
    virtual void getImportsExports(hkArray<hkResource::Import,hkContainerHeapAllocator>& impOut, hkArray<hkResource::Export,hkContainerHeapAllocator>& expOut) const; // vtbl+0x18 0x00F79170
    virtual void* getContentsPointer(const char* typeName, const hkTypeInfoRegistry* typeRegistry) const; // vtbl+0x1C 0x00F791C0
    virtual const char* getContentsTypeName() const; // vtbl+0x20 0x00F791B0
    virtual void* stealContentsPointer(const char* typeName, const hkTypeInfoRegistry* typeRegistry); // vtbl+0x24 0x00F79180

    // <hkpListShape> 0x00821340 (.h:43): takes the top-level object out of the resource; cleanup is the caller's
    template<typename T>
    T* stealContents()
    {
        return static_cast<T*>(stealContentsPointer(T::staticClass().getName(),
                                                    hkBuiltinTypeRegistry::getInstance().getLoadedObjectRegistry()));
    }

    void setClassNameRegistry(const hkClassNameRegistry* classReg); // 0x00F79340
    void setTypeInfoRegistry(const hkTypeInfoRegistry* typeReg); // 0x00F79370

private:
    hkVariant m_topLevelObject; // 0x08
    hkRefPtr<hkClassNameRegistry const> m_classRegistry; // 0x10
    hkRefPtr<hkTypeInfoRegistry const> m_typeRegistry; // 0x14
};
