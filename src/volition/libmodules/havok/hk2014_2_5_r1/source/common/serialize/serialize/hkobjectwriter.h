// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkobjectwriter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;
class hkRelocationInfo;
struct hkResult;
class hkStreamWriter;

// size 0x8
class hkObjectWriter : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F72C30
    virtual ~hkObjectWriter() {} // vtbl+0x00 0x00F72BE0 (inline, stub_impl.md S4 SE)
    virtual hkResult writeObject(hkStreamWriter*, const void*, const hkClass&, hkRelocationInfo&) = 0; // vtbl+0x10
    hkObjectWriter() {} // 0x00F72BD0
    virtual hkResult writeRaw(hkStreamWriter*, const void*, int) = 0; // vtbl+0x14

};
