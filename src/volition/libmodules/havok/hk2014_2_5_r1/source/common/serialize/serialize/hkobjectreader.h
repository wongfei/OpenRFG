// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkobjectreader.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); constructor and destructor inline
// (stub_impl.md S4 SF, nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;
class hkRelocationInfo;
struct hkResult;
class hkStreamReader;

// size 0x8
class hkObjectReader : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x01057010
    virtual ~hkObjectReader() {} // vtbl+0x00 0x01056F80
    virtual int readObject(hkStreamReader*, void*, int, const hkClass&, hkRelocationInfo&) = 0; // vtbl+0x10
    hkObjectReader() {} // 0x01056F70
    virtual hkResult readRaw(hkStreamReader*, void*, int) = 0; // vtbl+0x14

};
