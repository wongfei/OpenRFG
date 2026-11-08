// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/packfile/binary/hkbinarypackfilereader.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/packfile/hkpackfilereader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/hkpackfiledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkchainedclassnameregistry.h"

class hkClassNameRegistry;
class hkObjectUpdateTracker;
class hkPackfileHeader;
class hkPackfileObjectUpdateTracker;
class hkPackfileSectionHeader;
struct hkResult;
class hkSeekableStreamReader;
class hkStreamReader;
class hkTypeInfoRegistry;
struct hkVariant;

// size 0x90
class hkBinaryPackfileReader : public hkPackfileReader
{
public:
    // size 0xC
    struct PackfileObject
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(PackfileObject);

        void* object; // 0x00
        int section; // 0x04
        int offset; // 0x08
    };

    // size 0x70
    class BinaryPackfileData : public hkPackfileData
    {
    public:
        HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00FAED00, delete(void* p, unsigned int nbytes) 0x00FAED20
        BinaryPackfileData(); // 0x00FAB600
        void freeSection(void* data); // 0x00FABF10
        void untrackAndDestructObject(void* obj); // 0x00FACE70
        virtual ~BinaryPackfileData(); // vtbl+0x00 0x00FAB620

    };

    // size 0x8
    class SectionHeaderArray
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(SectionHeaderArray);
        SectionHeaderArray(); // 0x00FAB970
        void init(void* baseSection, int sectionHeaderSize); // 0x00FAB980
        bool isValid() const; // 0x00FAB990
        const hkPackfileSectionHeader& operator[](int i) const; // 0x00FABEF0
        hkPackfileSectionHeader& operator[](int i); // 0x00FABEE0

    public:
        void* m_baseSection; // 0x00
        int m_sectionHeaderSize; // 0x04
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00FB01A0
    hkBinaryPackfileReader(); // 0x00FAD3A0
    virtual ~hkBinaryPackfileReader(); // vtbl+0x00 0x00FADCD0
    virtual hkResult loadEntireFile(hkStreamReader* reader); // vtbl+0x10 0x00FAE6E0
    virtual void* getContentsWithRegistry(const char* expectedClassName, const hkTypeInfoRegistry* finishRegistry); // vtbl+0x14 0x00FADFA0
    virtual const char* getContentsClassName() const; // vtbl+0x1C 0x00FAB950
    virtual hkArray<hkVariant,hkContainerHeapAllocator>& getLoadedObjects() const; // vtbl+0x20 0x00FAC960
    virtual hkVariant getTopLevelObject() const; // vtbl+0x24 0x00FADF00
    virtual hkObjectUpdateTracker& getUpdateTracker() const; // vtbl+0x28 0x00FAC3C0
    virtual const hkClassNameRegistry* getClassNameRegistry() const; // vtbl+0x34 0x00FAD450
    virtual hkPackfileData* getPackfileData() const; // vtbl+0x38 0x00FADDE0
    hkResult loadFileHeader(hkStreamReader* reader, hkPackfileHeader* dst); // 0x00FACC20
    const hkPackfileHeader& getFileHeader() const; // 0x00FAB880
    int getNumSections() const; // 0x00FAB890
    hkResult loadSectionHeadersNoSeek(hkStreamReader* reader, hkPackfileSectionHeader* dst); // 0x00FACD20
    const hkPackfileSectionHeader& getSectionHeader(int idx) const; // 0x00FABF00
    hkResult loadSection(hkSeekableStreamReader* reader, int sectionIndex, void* buf); // 0x00FAD890
    hkResult unloadSection(int unloadIndex); // 0x00FAD8F0
    hkResult fixupGlobalReferences(); // 0x00FAE0A0
    hkResult finishLoadedObjects(const hkTypeInfoRegistry& finishObjects); // 0x00FAB8A0
    hkResult loadSectionNoSeek(hkStreamReader* reader, int sectionIndex, void* buf); // 0x00FAD640
    virtual hkResult loadEntireFileInplace(void* data, int dataSize); // vtbl+0x3C 0x00FAE7B0
public:
    virtual int getSectionIndex(const char* sectionTag) const; // vtbl+0x40 0x00FABF70
    virtual void* getSectionDataByIndex(int sectionIndex, int offset) const; // vtbl+0x44 0x00FABEC0
    void* getOriginalContents() const; // 0x00FAB8F0
    const char* getOriginalContentsClassName() const; // 0x00FAB930
    void findClassLocations(hkArray<PackfileObject,hkContainerHeapAllocator>& classPtrsOut) const; // 0x00FAC6F0
    void useClassesFromRegistry(const hkClassNameRegistry& registry) const; // 0x00FADAC0
public:
    void updateSectionHeaders(); // 0x00FABFC0

    BinaryPackfileData* m_packfileData; // 0x18
    hkPackfileHeader* m_header; // 0x1C
    SectionHeaderArray m_sections; // 0x20
    hkInplaceArray<void*,16,hkContainerHeapAllocator> m_sectionData; // 0x28
    int m_streamOffset; // 0x74
    hkArray<hkVariant,hkContainerHeapAllocator>* m_loadedObjects; // 0x78
    hkPackfileObjectUpdateTracker* m_tracker; // 0x7C
    hkRefPtr<hkChainedClassNameRegistry> m_packfileClassRegistry; // 0x80
    hkArray<unsigned short,hkContainerHeapAllocator> m_predicates; // 0x84
};
