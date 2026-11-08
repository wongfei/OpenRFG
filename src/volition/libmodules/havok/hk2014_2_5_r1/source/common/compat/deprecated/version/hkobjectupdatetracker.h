// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/version/hkobjectupdatetracker.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;

// size 0x8
class hkObjectUpdateTracker : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00FAEDB0
    virtual ~hkObjectUpdateTracker(); // vtbl+0x00 0x00FAEA00
    virtual void addAllocation(void*) = 0; // vtbl+0x10
    virtual void addChunk(void*, int, HK_MEMORY_CLASS) = 0; // vtbl+0x14
    virtual void objectPointedBy(void*, void*) = 0; // vtbl+0x18
    virtual void replaceObject(void*, void*, const hkClass*) = 0; // vtbl+0x1C
    virtual void addFinish(void*, const char*) = 0; // vtbl+0x20
    hkObjectUpdateTracker(); // 0x00FAE9F0
    virtual void removeFinish(void*) = 0; // vtbl+0x24

};
