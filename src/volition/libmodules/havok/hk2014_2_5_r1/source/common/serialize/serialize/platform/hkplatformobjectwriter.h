// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/platform/hkplatformobjectwriter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/copier/hkobjectcopier.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkobjectwriter.h"

class hkClass;
class hkRelocationInfo;
class hkStreamWriter;
class hkStructureLayout;

// size 0x10
class hkPlatformObjectWriter : public hkObjectWriter
{
public:
    // size 0x2C
    class Cache : public hkReferencedObject
    {
    public:
        HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F72C90, delete(void* p, unsigned int nbytes) 0x00F72E90
        Cache(hkFinishLoadedObjectFlag);
        Cache(); // 0x00F729F0
        virtual ~Cache(); // vtbl+0x00 0x00F72A30
        hkClass* get(const hkClass* klass, const hkStructureLayout& layout); // 0x00F727F0

        hkPointerMap<void const*,void*,hkContainerHeapAllocator> m_platformClassFromHostClass; // 0x08
        hkPointerMap<hkClass const*,int,hkContainerHeapAllocator> m_platformClassComputed; // 0x14
        hkArray<void*,hkContainerHeapAllocator> m_allocations; // 0x20
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F72D00
    hkPlatformObjectWriter(const hkStructureLayout& layout, Cache* cache, hkFlags<enum hkObjectCopier::ObjectCopierFlagBits,unsigned int> flags); // 0x00F72AD0
    virtual ~hkPlatformObjectWriter(); // vtbl+0x00 0x00F727C0
    virtual hkResult writeObject(hkStreamWriter* writer, const void* dataIn, const hkClass& klass, hkRelocationInfo& reloc); // vtbl+0x10 0x00F72890
    virtual hkResult writeRaw(hkStreamWriter* writer, const void* data, int dataLen); // vtbl+0x14 0x00F72780
    const hkStructureLayout& getLayout() const; // 0x00F727B0

public:
    hkObjectCopier* m_copier; // 0x08
    Cache* m_cache; // 0x0C
};
