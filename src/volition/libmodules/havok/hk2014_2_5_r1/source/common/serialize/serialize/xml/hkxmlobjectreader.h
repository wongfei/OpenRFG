// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectreader.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); the destructor's VA noted (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkobjectreader.h"

class hkClass;
struct hkContainerHeapAllocator;
class hkRelocationInfo;
struct hkResult;
class hkStreamReader;
class hkXmlParser;

// size 0x10: reads one "<hkobject>" (its "<hkparam>" members) into a packfile style buffer with relocations
class hkXmlObjectReader : public hkObjectReader
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x01057050
    hkXmlObjectReader(hkXmlParser* parser, hkStringMap<void*,hkContainerHeapAllocator>* nameToObject); // 0x01054E80
    virtual ~hkXmlObjectReader(); // vtbl+0x00 0x01054E10
    hkResult readObject(hkStreamReader* reader, hkArray<char,hkContainerHeapAllocator>& array, const hkClass& klass, hkRelocationInfo& reloc); // 0x01056CD0
    virtual int readObject(hkStreamReader* reader, void* buf, int bufSize, const hkClass& klass, hkRelocationInfo& reloc); // vtbl+0x10 0x01056E80
    virtual hkResult readRaw(hkStreamReader* reader, void* buf, int bufLen); // vtbl+0x14 0x01054E30
    static hkResult base64read(hkStreamReader* sr, void* buf, int len); // 0x01054C60

public:
    hkXmlParser* m_parser; // 0x08
    hkStringMap<void*,hkContainerHeapAllocator>* m_nameToObject; // 0x0C
};
