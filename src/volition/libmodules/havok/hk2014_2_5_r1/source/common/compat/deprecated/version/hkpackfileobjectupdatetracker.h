// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/version/hkpackfileobjectupdatetracker.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/version/hkobjectupdatetracker.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializemultimap.h"

class hkClass;
class hkPackfileData;

// size 0x38
class hkPackfileObjectUpdateTracker : public hkObjectUpdateTracker
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00FAA990, delete(void* p, unsigned int nbytes) 0x00FAA9B0
    hkPackfileObjectUpdateTracker(hkPackfileData* data); // 0x00FB3300
    virtual ~hkPackfileObjectUpdateTracker(); // vtbl+0x00 0x00FB3120
    void setTopLevelObject(void* topLevel, const char* topClass); // 0x00FB2FE0
    const char* getTopLevelClassName(); // 0x00FB3020
    virtual void addAllocation(void* p); // vtbl+0x10 0x00FB3030
    virtual void addChunk(void* p, int n, HK_MEMORY_CLASS c); // vtbl+0x14 0x00FB30B0
    virtual void objectPointedBy(void* newObject, void* fromWhere); // vtbl+0x18 0x00FB31A0
    virtual void replaceObject(void* oldObject, void* newObject, const hkClass* newClass); // vtbl+0x1C 0x00FB3200
    virtual void addFinish(void* newObject, const char* className); // vtbl+0x20 0x00FB3070
    virtual void removeFinish(void* oldObject); // vtbl+0x24 0x00FB3090

    hkPackfileData* m_packfileData; // 0x08
    hkSerializeMultiMap<void*,void*,hkPointerMap<void*,int,hkContainerHeapAllocator> > m_pointers; // 0x0C
    hkPointerMap<void*,char const*,hkContainerHeapAllocator> m_finish; // 0x28
    void* m_topLevelObject; // 0x34
};
