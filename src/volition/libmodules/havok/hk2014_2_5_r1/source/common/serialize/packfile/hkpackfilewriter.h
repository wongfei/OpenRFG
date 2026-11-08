// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/hkpackfilewriter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer; Options inline in SE, from rfg.exe).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializemultimap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkstructurelayout.h"

class hkClass;
class hkStreamWriter;
class hkVtableClassRegistry;

// size 0xE0
class hkPackfileWriter : public hkReferencedObject
{
public:
    enum
    {
        INDEX_IGNORE = -1,
        INDEX_IMPORT = -2,
        INDEX_ERROR = -3,
    };

    typedef const char* SectionTag;

    // size 0x8
    class AddObjectListener : public hkReferencedObject
    {
    public:
        typedef const void* ObjectPointer;
        typedef const hkClass* ClassPointer;

        HK_DECLARE_CLASS_ALLOCATOR();
        virtual void addObjectCallback(const void*&, const hkClass*&) = 0; // vtbl+0x10

    };

    // size 0x1C
    struct Options
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Options);
        // 0x00F454F0 (.h:67)
        Options(const Options& rhs)
            : m_userTag(rhs.m_userTag), m_layout(rhs.m_layout), m_writeMetaInfo(rhs.m_writeMetaInfo),
              m_writeSerializedFalse(rhs.m_writeSerializedFalse), m_contentsVersion(rhs.m_contentsVersion)
        {
            m_truePredicates.append(rhs.m_truePredicates.begin(), rhs.m_truePredicates.getSize());
        }
        // 0x00FA2070 (.h:61): the host layout, no metadata, serialize-ignored members skipped
        Options() : m_userTag(0), m_writeMetaInfo(false), m_writeSerializedFalse(false), m_contentsVersion(nullptr) {}
        ~Options() {} // 0x00F45550

        unsigned int m_userTag; // 0x00
        hkStructureLayout m_layout; // 0x04
        hkBool m_writeMetaInfo; // 0x08
        hkBool m_writeSerializedFalse; // 0x09
        const char* m_contentsVersion; // 0x0C
        hkArray<unsigned short,hkContainerHeapAllocator> m_truePredicates; // 0x10
    };

    // size 0x18
    struct PendingWrite
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(PendingWrite);

        const void* m_pointer; // 0x00
        const hkClass* m_klass; // 0x04
        const void* m_origPointer; // 0x08
        const hkClass* m_origClass; // 0x0C
        int m_sectionIndex; // 0x10
        int m_dataSize; // 0x14
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F6AA90
    hkPackfileWriter(const Options& options); // 0x00F6A0F0
    virtual ~hkPackfileWriter(); // vtbl+0x00 0x00F69B90
    virtual void setContentsWithRegistry(const void* pointer, const hkClass& klass, const hkVtableClassRegistry* classRegistry, AddObjectListener* addListener); // vtbl+0x10 0x00F6A280
    virtual void setContents(const void* object, const hkClass& klass, AddObjectListener* addListen); // vtbl+0x14 0x00F69720
    virtual void addImport(const void* object, const char* id); // vtbl+0x18 0x00F697C0
    virtual void addExport(const void* object, const char* id); // vtbl+0x1C 0x00F69810
    virtual hkResult save(hkStreamWriter*, const Options&) = 0; // vtbl+0x20
    virtual void addSection(const char* sectionTag); // vtbl+0x24 0x00F69A60
    virtual void setSectionForPointer(const void* ptr, const char* sectionTag); // vtbl+0x28 0x00F69A70
    virtual void setSectionForClass(const hkClass& k, const char* sectionTag); // vtbl+0x2C 0x00F69AA0
    static void getCurrentVersion(char* buf, int bufLen); // 0x00F69700
public:
    unsigned int findSectionFor(const void* pointer, const hkClass& klass, const char* sectionTag); // 0x00F698B0
    void addObject(const void* pointerIn, const hkClass& klassIn, const hkVtableClassRegistry* classRegistry, AddObjectListener* addListener, const char* sectionTag); // 0x00F69DA0
    void addPendingWrite(const void* pointer, const hkClass& klass, const void* pointerIn, const hkClass& klassIn, const char* sectionTag); // 0x00F69AD0
    int notDuplicateMetaData(const void* pointer, const hkClass* klass); // 0x00F69930
    unsigned int sectionTagToIndex(const char* sectionTag); // 0x00F69830

public:
    static const char* SECTION_TAG_DATA; // 0x016FA9E8
    static const char* SECTION_TAG_TYPES; // 0x016FA9EC
public:
    hkArray<PendingWrite,hkContainerHeapAllocator> m_pendingWrites; // 0x08
    hkPointerMap<void const*,int,hkContainerHeapAllocator> m_knownObjects; // 0x14
    hkPointerMap<void const*,char const*,hkContainerHeapAllocator> m_imports; // 0x20
    hkPointerMap<void const*,char const*,hkContainerHeapAllocator> m_exports; // 0x2C
    hkStringMap<hkClass const*,hkContainerHeapAllocator> m_knownClasses; // 0x38
    hkPointerMap<void const*,void const*,hkContainerHeapAllocator> m_replacements; // 0x48
    hkArray<char*,hkContainerHeapAllocator> m_knownSections; // 0x54
    hkStringMap<int,hkContainerHeapAllocator> m_sectionTagToIndex; // 0x60
    hkArray<hkVariant,hkContainerHeapAllocator> m_objectsWithUnregisteredClass; // 0x70
    int m_contentsPtrPWIndex; // 0x7C
    int m_contentsClassPWIndex; // 0x80
    int m_numDataInstances; // 0x84
    int m_numClassInstances; // 0x88
    hkPointerMap<void const*,unsigned int,hkContainerHeapAllocator> m_sectionOverrideByPointer; // 0x8C
    hkStringMap<unsigned int,hkContainerHeapAllocator> m_sectionOverrideByType; // 0x98
    hkSerializeMultiMap<void const*,int,hkPointerMap<void const*,int,hkContainerHeapAllocator> > m_pwIndexesFromReferencedPointer; // 0xA8
    Options m_startOptions; // 0xC4
};
