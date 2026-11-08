// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkresource.h
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD): a loaded resource (packfile contents or one top-level object), as
// 2014.1 hkResource.h; every member inline.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkTypeInfoRegistry;

// size 0x8
// The constructor (0x00F63D40, emitted in hkNativePackfileUtils.obj) is compiler-generated.
class hkResource : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F65110, delete(void* p, unsigned int nbytes) 0x00F65130 (.h:20)

    // size 0x8
    struct Export
    {
        const char* name; // 0x00
        void* data; // 0x04
    };

    // size 0x8
    struct Import
    {
        const char* name; // 0x00
        void** location; // 0x04
    };

    virtual ~hkResource() {} // vtbl+0x00 0x00F64EE0 (.h:51)
    virtual const char* getName() const = 0; // vtbl+0x10
    virtual void callDestructors() {} // vtbl+0x14 0x00F63D50 (.h:58)
    virtual void getImportsExports(hkArray<Import,hkContainerHeapAllocator>& impOut, hkArray<Export,hkContainerHeapAllocator>& expOut) const = 0; // vtbl+0x18
    virtual void* getContentsPointer(const char* typeName, const hkTypeInfoRegistry* typeRegistry) const = 0; // vtbl+0x1C
    virtual const char* getContentsTypeName() const = 0; // vtbl+0x20
};
