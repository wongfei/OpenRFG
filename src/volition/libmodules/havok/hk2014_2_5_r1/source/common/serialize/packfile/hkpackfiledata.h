// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/hkpackfiledata.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/hkpackfiledata.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD): the resource of a loaded packfile (or of a data world copied to
// native objects): the objects it tracks (finished and destructed through their type infos), its memory, imports
// and exports. Layout from rfg.pdb; the inline members from rfg.exe.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkresource.h"

class hkTypeInfo;
class hkTypeInfoRegistry;

// size 0x70
class hkPackfileData : public hkResource
{
public:
    typedef hkPointerMap<void*,char const*,hkContainerHeapAllocator> TrackedObjectMap;
    typedef hkStringMap<hkTypeInfo const*,hkContainerHeapAllocator> TrackedTypeMap;

    // size 0xC
    struct Chunk
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Chunk);
        Chunk() {}
        Chunk(void* p, int n, HK_MEMORY_CLASS c) : pointer(p), numBytes(n), memClass(c) {} // 0x00F66490 (.h:58)

        void* pointer; // 0x00
        int numBytes; // 0x04
        HK_MEMORY_CLASS memClass; // 0x08
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F67980, delete(void* p, unsigned int nbytes) 0x00F7A630 (.h:21)
    hkPackfileData(const hkClassNameRegistry* reg); // 0x00F799B0
    virtual ~hkPackfileData(); // vtbl+0x00 0x00F79A60
    virtual const char* getName() const { return m_name; } // vtbl+0x10 0x00F7A620 (.h:35)
    virtual void callDestructors(); // vtbl+0x14 0x00F79600
    virtual void getImportsExports(hkArray<hkResource::Import,hkContainerHeapAllocator>& impOut, hkArray<hkResource::Export,hkContainerHeapAllocator>& expOut) const; // vtbl+0x18 0x00F796A0
    void setName(const char* n); // 0x00F794E0
    void disableDestructors() { m_destructorsEnabled = false; } // 0x00F66480 (.h:45)
    virtual void* getContentsPointer(const char* typeName, const hkTypeInfoRegistry* typeRegistry) const; // vtbl+0x1C 0x00F79860
    virtual const char* getContentsTypeName() const; // vtbl+0x20 0x00F79990
    void addAllocation(void* p) { m_memory.pushBack(p); } // 0x00F68570 (.h:69)
    bool isDirty() const;
    void addChunk(void* p, int n, HK_MEMORY_CLASS c) { m_chunks.pushBack(Chunk(p, n, c)); } // 0x00F68890 (.h:75)
    void trackObject(void* o, const char* n) { m_trackedObjects.insert(o, n); } // 0x00F688F0 (.h:78)
    void addExport(const char* symbolName, void* object); // 0x00F797E0
    void removeExport(void* object); // 0x00F79520
    void addImport(const char* symbolName, void** location); // 0x00F79820
    void removeImport(void** location); // 0x00F79570
    void setContentsWithName(void* topLevelObject, const char* typeName); // 0x00F794D0
    void setPackfileClassNameRegistry(const hkClassNameRegistry* classReg); // 0x00F795D0
    hkBool32 finishedObjects() const; // 0x00F795C0

public:
    void* m_topLevelObject; // 0x08
    char* m_name; // 0x0C
    hkPointerMap<void*,char const*,hkContainerHeapAllocator> m_trackedObjects; // 0x10: object -> class name
    mutable hkStringMap<hkTypeInfo const*,hkContainerHeapAllocator> m_trackedTypes; // 0x1C: class name -> type info, filled by finishing
    hkRefPtr<hkClassNameRegistry const > m_packfileClassRegistry; // 0x2C
    hkBool32 m_destructorsEnabled; // 0x30
    hkArray<void*,hkContainerHeapAllocator> m_memory; // 0x34: freed with easyFree
    hkArray<Chunk,hkContainerHeapAllocator> m_chunks; // 0x40: freed with their sizes
    hkArray<hkResource::Export,hkContainerHeapAllocator> m_exports; // 0x4C
    hkArray<hkResource::Import,hkContainerHeapAllocator> m_imports; // 0x58
    hkArray<hkVariant,hkContainerHeapAllocator> m_postFinishObjects; // 0x64
};
