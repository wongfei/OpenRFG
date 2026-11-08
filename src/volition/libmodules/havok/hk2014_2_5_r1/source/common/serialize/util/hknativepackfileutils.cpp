// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hknativepackfileutils.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE), from rfg.exe (LTCG passes the fixup helpers' arguments in
// registers; read from the asm): loading binary packfiles saved in the host layout, either into a separate buffer
// (an InplaceDataHeader, the section headers, the sections' data with their exports and imports, then the tracked
// objects) or in place (fixups applied in the file's own memory). RFG's data has no packfiles (stub_impl.md S4).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hknativepackfileutils.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/attributes/hkattributes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hktypeinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkpackfileheader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkpackfilesectionheader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkstructurelayout.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionutil.h"

#include <new>

namespace LOCALNAMESPACE
{
    // size 0x8: a finished object of a loaded buffer
    struct TrackedObjectsArray_Element
    {
        hkTypeInfo* m_typeInfo; // 0x00
        int m_offset; // 0x04: from the buffer start
    };

    // size 0x30: the start of a buffer loaded by load(); the arrays point into the buffer
    struct InplaceDataHeader
    {
        // 0x00F65740
        ~InplaceDataHeader() {}

        int m_copy_loaded_flag; // 0x00: COPY_LOADED_MAGIC while loaded
        int m_contentsOffset; // 0x04
        hkArray<TrackedObjectsArray_Element,hkContainerHeapAllocator> m_trackedObjects; // 0x08
        hkArray<hkPackfileSectionHeader,hkContainerHeapAllocator> m_sections; // 0x14
        hkTypeInfo* m_contentsType; // 0x20
        int m_pad[3]; // 0x24
    };

    const int COPY_LOADED_MAGIC = 0xD5109142;

    // size 0x1C: a load() buffer owned as a resource
    struct hkNativeResource : public hkResource
    {
        // 0x00F654C0 (:67): takes the buffer
        hkNativeResource(hkArray<char,hkContainerHeapAllocator>& buf, void* contents, const char* contentsTypeName)
            : m_contents(contents), m_contentsTypeName(contentsTypeName)
        {
            m_data.swap(buf);
        }

        // 0x00F657D0 (:72)
        virtual void callDestructors()
        {
            if (m_contents != nullptr)
            {
                hkNativePackfileUtils::unload(m_data.begin(), m_data.getSize());
                m_contents = nullptr;
                m_contentsTypeName = nullptr;
            }
        }

        // 0x00F65800 (:82)
        virtual ~hkNativeResource()
        {
            callDestructors();
        }

        virtual const char* getName() const { return "hkNativeResource"; } // 0x00F65510 (:87)

        // 0x00F65530 (:91): null when the contents are not of the requested class (both known to the builtin registry)
        virtual void* getContentsPointer(const char* typeName, const hkTypeInfoRegistry* ignoredBecauseAlreadyDone) const
        {
            if (typeName != nullptr)
            {
                const hkClassNameRegistry* reg = hkBuiltinTypeRegistry::getInstance().getClassNameRegistry();
                const hkClass* contentsClass = reg->getClassByName(m_contentsTypeName);
                const hkClass* requestedClass = reg->getClassByName(typeName);
                if (contentsClass != nullptr && requestedClass != nullptr && !requestedClass->isSuperClass(*contentsClass))
                {
                    return nullptr;
                }
            }
            return m_contents;
        }

        // 0x00F655A0 (:111)
        virtual void getImportsExports(hkArray<hkResource::Import,hkContainerHeapAllocator>& impOut, hkArray<hkResource::Export,hkContainerHeapAllocator>& expOut) const
        {
            hkNativePackfileUtils::getImportsExports(m_data.begin(), expOut, impOut);
        }

        virtual const char* getContentsTypeName() const { return m_contentsTypeName; } // 0x00F65520 (:115)

        hkArray<char,hkContainerHeapAllocator> m_data; // 0x08
        void* m_contents; // 0x14
        const char* m_contentsTypeName; // 0x18
    };
}

using namespace LOCALNAMESPACE;

// 0x00F64B00 (:129): a load() into a new buffer, owned by the resource. The contents' type info must exist (rfg.exe
// reads its name unchecked).
hkResource* hkNativePackfileUtils::load(const void* packfileData, int dataSize, const hkTypeInfoRegistry* userRegistry)
{
    hkArray<char,hkContainerHeapAllocator> buf;
    const int bufSize = getRequiredBufferSize(packfileData, dataSize);
    buf.setSize(bufSize);
    void* contents = load(packfileData, dataSize, buf.begin(), bufSize, userRegistry);
    const InplaceDataHeader* header = reinterpret_cast<const InplaceDataHeader*>(buf.begin());
    return new hkNativeResource(buf, contents, header->m_contentsType->getTypeName());
}

// 0x00F63FE0 (:139): null for a section without data
static void* _getSectionDataByIndex(void* base, const hkArrayBase<hkPackfileSectionHeader*>& sections, int sectionIndex, int offset)
{
    const hkPackfileSectionHeader* s = sections[sectionIndex];
    if (s->getDataSize() != 0)
    {
        return static_cast<char*>(base) + s->m_absoluteDataStart + offset;
    }
    return nullptr;
}

// 0x00F63D60 (:151): the header must be the host's layout and the current version, the data 4-aligned
hkResult hkNativePackfileUtils::validatePackfileHeader(const void* packfileData, const char** errOut)
{
    if (packfileData == nullptr)
    {
        if (errOut != nullptr)
        {
            *errOut = "Pointer is null";
        }
        return HK_FAILURE;
    }
    const hkPackfileHeader* header = static_cast<const hkPackfileHeader*>(packfileData);
    hkPackfileHeader magic;
    if (header->m_magic[0] != magic.m_magic[0] || header->m_magic[1] != magic.m_magic[1])
    {
        if (errOut != nullptr)
        {
            *errOut = "Missing packfile magic header. Is this from a binary file?";
        }
        return HK_FAILURE;
    }
    const hkStructureLayout::LayoutRules& host = hkStructureLayout::HostLayoutRules;
    if (header->m_layoutRules[0] != host.m_bytesInPointer)
    {
        if (errOut != nullptr)
        {
            *errOut = "Trying to process a binary file with a different pointer size than this platform.";
        }
        return HK_FAILURE;
    }
    if (header->m_layoutRules[1] != host.m_littleEndian)
    {
        if (errOut != nullptr)
        {
            *errOut = "Trying to process a binary file with a different endian than this platform.";
        }
        return HK_FAILURE;
    }
    if (header->m_layoutRules[2] != host.m_reusePaddingOptimization)
    {
        if (errOut != nullptr)
        {
            *errOut = "Trying to process a binary file with a different padding optimization than this platform.";
        }
        return HK_FAILURE;
    }
    if (header->m_layoutRules[3] != host.m_emptyBaseClassOptimization)
    {
        if (errOut != nullptr)
        {
            *errOut = "Trying to process a binary file with a different empty base class optimization than this platform.";
        }
        return HK_FAILURE;
    }
    if ((reinterpret_cast<hkUlong>(packfileData) & 3) != 0)
    {
        if (errOut != nullptr)
        {
            *errOut = "Packfile data source needs to be 4 byte aligned";
        }
        return HK_FAILURE;
    }
    if (header->m_contentsVersion[0] == -1) // unset (all ones) before the version was stored
    {
        if (errOut != nullptr)
        {
            *errOut = "Packfile file format is too old";
        }
        return HK_FAILURE;
    }
    if (hkString::strCmp(header->m_contentsVersion, hkVersionUtil::getCurrentVersion()) != 0)
    {
        if (errOut != nullptr)
        {
            *errOut = "Packfile contents are not up to date";
        }
        return HK_FAILURE;
    }
    return HK_SUCCESS;
}

// 0x00F63F10 (:170): the header, the section headers, each section's data, exports and imports, and a tracked
// object per virtual fixup (12-byte records)
int hkNativePackfileUtils::getRequiredBufferSize(const void* packfileData, int packfileSize)
{
    const hkPackfileHeader* header = static_cast<const hkPackfileHeader*>(packfileData);
    int dataSize = 0;
    int numFinish = 0;
    for (int i = 0; i < header->m_numSections; i++)
    {
        const hkPackfileSectionHeader* s = header->getSectionHeader(packfileData, i);
        dataSize += s->getDataSize() + s->getExportsSize() + s->getImportsSize();
        numFinish += s->getFinishSize() / 12;
    }
    return dataSize + static_cast<int>(sizeof(InplaceDataHeader)) + numFinish * static_cast<int>(sizeof(TrackedObjectsArray_Element)) +
           header->m_numSections * static_cast<int>(sizeof(hkPackfileSectionHeader));
}

// 0x00F64000 (:199): the data, exports and imports (the fixup tables are not copied: their offsets collapse to the
// end of the data)
static void _copySection(const hkPackfileSectionHeader& inSection, hkPackfileSectionHeader& outSection, int outCurOffset, const void* packfileData, void* outBuffer)
{
    hkString::memCpy(outSection.m_sectionTag, inSection.m_sectionTag, sizeof(inSection.m_sectionTag));
    outSection.m_nullByte = inSection.m_nullByte;
    outSection.m_absoluteDataStart = outCurOffset;
    outSection.m_localFixupsOffset = inSection.m_localFixupsOffset;
    outSection.m_globalFixupsOffset = inSection.m_localFixupsOffset;
    outSection.m_virtualFixupsOffset = inSection.m_localFixupsOffset;
    outSection.m_exportsOffset = inSection.m_localFixupsOffset;
    outSection.m_importsOffset = inSection.m_localFixupsOffset - inSection.m_exportsOffset + inSection.m_importsOffset;
    outSection.m_endOffset = inSection.m_localFixupsOffset - inSection.m_exportsOffset + inSection.m_endOffset;
    char* dst = static_cast<char*>(outBuffer) + outCurOffset;
    const char* src = static_cast<const char*>(packfileData) + inSection.m_absoluteDataStart;
    hkString::memCpy(dst, src, inSection.m_localFixupsOffset);
    hkString::memCpy(dst + outSection.m_exportsOffset, src + inSection.m_exportsOffset, inSection.m_importsOffset - inSection.m_exportsOffset);
    hkString::memCpy(dst + outSection.m_importsOffset, src + inSection.m_importsOffset, inSection.m_endOffset - inSection.m_importsOffset);
}

// 0x00F640A0 (:231): {from, to} pairs within the section, -1 padding
static void _applyLocalFixups(const hkPackfileSectionHeader& inSection, int outCurOffset, const void* packfileData, void* outBuffer)
{
    char* base = static_cast<char*>(outBuffer) + outCurOffset;
    const int* fixups = reinterpret_cast<const int*>(static_cast<const char*>(packfileData) + inSection.m_absoluteDataStart + inSection.m_localFixupsOffset);
    for (int i = 0; i < inSection.getLocalSize() / 4; i += 2)
    {
        if (fixups[i] != -1)
        {
            *reinterpret_cast<char**>(base + fixups[i]) = base + fixups[i + 1];
        }
    }
}

// 0x00F640F0 (:253): {from, section index, to} triples; a target in a section without data is null
static void _applyGlobalFixups(const hkPackfileSectionHeader& inSection, const hkPackfileSectionHeader& outSection, const void* packfileData, void* outBuffer, const hkArrayBase<hkPackfileSectionHeader*>& outSectionAddresses)
{
    char* base = static_cast<char*>(outBuffer) + outSection.m_absoluteDataStart;
    const int* fixups = reinterpret_cast<const int*>(static_cast<const char*>(packfileData) + inSection.m_absoluteDataStart + inSection.m_globalFixupsOffset);
    for (int i = 0; i < inSection.getGlobalSize() / 4; i += 3)
    {
        if (fixups[i] != -1)
        {
            *reinterpret_cast<void**>(base + fixups[i]) = _getSectionDataByIndex(outBuffer, outSectionAddresses, fixups[i + 1], fixups[i + 2]);
        }
    }
}

// 0x00F642B0 (:279): {object, class name section, class name offset} triples: each object is finished through the
// registry; a copy-loaded buffer tracks it (and which is the contents), an in-place one stores the type info in the
// record ({object, 0, type info}, read by unloadInPlace). Objects whose class has a post-finish function are
// collected.
static void _applyVirtualFixups(const hkPackfileSectionHeader& inSection, hkPackfileSectionHeader& outSection, const void* packfileData, void* outBuffer,
                                const hkArrayBase<hkPackfileSectionHeader*>& outSectionAddresses, hkArray<TrackedObjectsArray_Element,hkContainerHeapAllocator>* map,
                                const hkTypeInfoRegistry* finishRegistry, const hkClassNameRegistry* classNameRegistry, hkArray<hkVariant,hkContainerHeapAllocator>& postFinishObjects)
{
    char* sectionBase = static_cast<char*>(outBuffer) + outSection.m_absoluteDataStart;
    InplaceDataHeader* header = static_cast<InplaceDataHeader*>(outBuffer);
    const char* contents = static_cast<char*>(outBuffer) + header->m_contentsOffset; // only meaningful for a copy load
    const int* fixups = reinterpret_cast<const int*>(static_cast<const char*>(packfileData) + inSection.m_absoluteDataStart + inSection.m_virtualFixupsOffset);
    for (int i = 0; i < inSection.getFinishSize() / 4; i += 3)
    {
        const int objectOffset = fixups[i];
        if (objectOffset == -1)
        {
            continue;
        }
        const char* className = static_cast<const char*>(_getSectionDataByIndex(outBuffer, outSectionAddresses, fixups[i + 1], fixups[i + 2]));
        void* object = sectionBase + objectOffset;
        const hkTypeInfo* typeInfo = finishRegistry->finishLoadedObject(object, className);
        if (typeInfo == nullptr)
        {
            continue;
        }
        if (map != nullptr)
        {
            TrackedObjectsArray_Element& e = map->expandOne();
            e.m_typeInfo = const_cast<hkTypeInfo*>(typeInfo);
            e.m_offset = outSection.m_absoluteDataStart + objectOffset;
            if (object == contents)
            {
                header->m_contentsType = const_cast<hkTypeInfo*>(typeInfo);
            }
        }
        else
        {
            int* record = reinterpret_cast<int*>(static_cast<char*>(outBuffer) + outSection.m_absoluteDataStart + outSection.m_virtualFixupsOffset) + i;
            record[1] = 0;
            *reinterpret_cast<const hkTypeInfo**>(&record[2]) = typeInfo;
        }
        if (classNameRegistry != nullptr)
        {
            const hkClass* klass = classNameRegistry->getClassByName(typeInfo->getTypeName());
            if (klass != nullptr && klass->getAttribute("hk.PostFinish") != nullptr)
            {
                hkVariant& v = postFinishObjects.expandOne();
                v.m_object = object;
                v.m_class = klass;
            }
        }
    }
}

// 0x00F64490 (:355): the buffer must hold getRequiredBufferSize() bytes; the contents pointer
void* hkNativePackfileUtils::load(const void* packfileData, int packfileSize, void* outBuffer, int outBufferSize, const hkTypeInfoRegistry* userRegistry)
{
    const hkPackfileHeader* fileHeader = static_cast<const hkPackfileHeader*>(packfileData);
    InplaceDataHeader* header = static_cast<InplaceDataHeader*>(outBuffer);
    header->m_copy_loaded_flag = COPY_LOADED_MAGIC;
    // the section headers follow the buffer header, not owned
    hkPackfileSectionHeader* outSectionHeaders = reinterpret_cast<hkPackfileSectionHeader*>(static_cast<char*>(outBuffer) + sizeof(InplaceDataHeader));
    new (&header->m_sections) hkArray<hkPackfileSectionHeader,hkContainerHeapAllocator>();
    header->m_sections.setDataUserFree(outSectionHeaders, fileHeader->m_numSections, fileHeader->m_numSections);
    int outCurOffset = fileHeader->m_numSections * static_cast<int>(sizeof(hkPackfileSectionHeader)) + static_cast<int>(sizeof(InplaceDataHeader));

    hkLocalArray<hkPackfileSectionHeader*> outSections(fileHeader->m_numSections);
    for (int i = 0; i < header->m_sections.getSize(); i++)
    {
        outSections.pushBack(&header->m_sections[i]);
    }
    for (int i = 0; i < fileHeader->m_numSections; i++)
    {
        const hkPackfileSectionHeader* inSection = fileHeader->getSectionHeader(packfileData, i);
        hkPackfileSectionHeader& outSection = header->m_sections[i];
        _copySection(*inSection, outSection, outCurOffset, packfileData, outBuffer);
        if (i == fileHeader->m_contentsSectionIndex)
        {
            header->m_contentsOffset = fileHeader->m_contentsSectionOffset + outCurOffset;
        }
        _applyLocalFixups(*inSection, outCurOffset, packfileData, outBuffer);
        outCurOffset += outSection.m_endOffset;
    }
    for (int i = 0; i < fileHeader->m_numSections; i++)
    {
        const hkPackfileSectionHeader* inSection = fileHeader->getSectionHeader(packfileData, i);
        if (inSection->getGlobalSize() != 0)
        {
            _applyGlobalFixups(*inSection, header->m_sections[i], packfileData, outBuffer, outSections);
        }
    }
    // the tracked objects use the rest of the buffer. Original bug kept: the capacity is the remaining byte count,
    // not an element count (the buffer holds exactly the finished objects, so it is never exceeded).
    new (&header->m_trackedObjects) hkArray<TrackedObjectsArray_Element,hkContainerHeapAllocator>();
    header->m_trackedObjects.setDataUserFree(reinterpret_cast<TrackedObjectsArray_Element*>(static_cast<char*>(outBuffer) + outCurOffset), 0, outBufferSize - outCurOffset);

    if (userRegistry == nullptr)
    {
        userRegistry = hkBuiltinTypeRegistry::getInstance().getTypeInfoRegistry();
    }
    const hkClassNameRegistry* classNameRegistry = hkBuiltinTypeRegistry::getInstance().getClassNameRegistry();
    header->m_contentsType = nullptr;
    hkArray<hkVariant,hkContainerHeapAllocator> postFinishObjects;
    for (int i = 0; i < fileHeader->m_numSections; i++)
    {
        const hkPackfileSectionHeader* inSection = fileHeader->getSectionHeader(packfileData, i);
        if (inSection->getFinishSize() != 0)
        {
            _applyVirtualFixups(*inSection, header->m_sections[i], packfileData, outBuffer, outSections, &header->m_trackedObjects, userRegistry,
                                classNameRegistry, postFinishObjects);
        }
    }
    for (int i = 0; i < postFinishObjects.getSize(); i++)
    {
        const hkVariant* attr = postFinishObjects[i].m_class->getAttribute("hk.PostFinish");
        static_cast<const hkPostFinishAttribute*>(attr->m_object)->m_postFinishFunction(postFinishObjects[i].m_object);
    }
    return static_cast<char*>(outBuffer) + header->m_contentsOffset;
}

// 0x00F64820 (:456): once per buffer (header flag bit 0); null when the header does not validate or was loaded.
// Original bug kept: without a contents section index match the result is packfileData - 1.
void* hkNativePackfileUtils::loadInPlace(void* packfileData, int dataSize, const hkTypeInfoRegistry* userRegistry, const char** errOut)
{
    if (errOut != nullptr)
    {
        *errOut = nullptr;
    }
    hkPackfileHeader* fileHeader = static_cast<hkPackfileHeader*>(packfileData);
    if (validatePackfileHeader(packfileData, errOut) != HK_SUCCESS || (fileHeader->m_flags & 1) != 0)
    {
        return nullptr;
    }
    fileHeader->m_flags |= 1;
    int contentsOffset = -1;
    hkLocalArray<hkPackfileSectionHeader*> sections(fileHeader->m_numSections);
    for (int i = 0; i < fileHeader->m_numSections; i++)
    {
        sections.pushBack(fileHeader->getSectionHeader(packfileData, i));
    }
    for (int i = 0; i < fileHeader->m_numSections; i++)
    {
        const int sectionStart = sections[i]->m_absoluteDataStart;
        hkString::strCmp(sections[i]->m_sectionTag, "__types__"); // result unused (an assert's, compiled out)
        if (i == fileHeader->m_contentsSectionIndex)
        {
            contentsOffset = fileHeader->m_contentsSectionOffset + sectionStart;
        }
        _applyLocalFixups(*sections[i], sectionStart, packfileData, packfileData);
    }
    for (int i = 0; i < fileHeader->m_numSections; i++)
    {
        if (sections[i]->getGlobalSize() != 0)
        {
            _applyGlobalFixups(*sections[i], *sections[i], packfileData, packfileData, sections);
        }
    }
    if (userRegistry == nullptr)
    {
        userRegistry = hkBuiltinTypeRegistry::getInstance().getTypeInfoRegistry();
    }
    const hkClassNameRegistry* classNameRegistry = hkBuiltinTypeRegistry::getInstance().getClassNameRegistry();
    hkArray<hkVariant,hkContainerHeapAllocator> postFinishObjects;
    for (int i = 0; i < fileHeader->m_numSections; i++)
    {
        if (sections[i]->getFinishSize() != 0)
        {
            _applyVirtualFixups(*sections[i], *sections[i], packfileData, packfileData, sections, nullptr, userRegistry, classNameRegistry, postFinishObjects);
        }
    }
    for (int i = 0; i < postFinishObjects.getSize(); i++)
    {
        const hkVariant* attr = postFinishObjects[i].m_class->getAttribute("hk.PostFinish");
        static_cast<const hkPostFinishAttribute*>(attr->m_object)->m_postFinishFunction(postFinishObjects[i].m_object);
    }
    return static_cast<char*>(packfileData) + contentsOffset;
}

// 0x00F63F80 (:554): the contents class name of an unloaded packfile, null when the data cannot hold it
const char* hkNativePackfileUtils::getContentsClassName(const void* packfileData, int dataSize)
{
    const hkPackfileHeader* header = static_cast<const hkPackfileHeader*>(packfileData);
    if (packfileData == nullptr || dataSize < static_cast<int>(sizeof(hkPackfileHeader)) || header->m_magic[0] != 0x57E0E057 || header->m_magic[1] != 0x10C0C010 ||
        header->m_numSections <= 0 || (header->m_numSections + 1) * static_cast<int>(sizeof(hkPackfileSectionHeader)) > dataSize)
    {
        return nullptr;
    }
    const hkPackfileSectionHeader* sections = header->getSectionHeader(packfileData, 0);
    const int offset = sections[header->m_contentsClassNameSectionIndex].m_absoluteDataStart + header->m_contentsClassNameSectionOffset;
    if (offset <= dataSize)
    {
        return static_cast<const char*>(packfileData) + offset;
    }
    return nullptr;
}

// 0x00F64C00 (:579): the tracked objects cleaned up and the arrays released (the buffer is the caller's)
void hkNativePackfileUtils::unload(void* buffer, int bufferSize)
{
    InplaceDataHeader* header = static_cast<InplaceDataHeader*>(buffer);
    header->m_copy_loaded_flag = 0;
    for (TrackedObjectsArray_Element* e = header->m_trackedObjects.begin(); e < header->m_trackedObjects.begin() + header->m_trackedObjects.getSize(); e++)
    {
        e->m_typeInfo->cleanupLoadedObject(static_cast<char*>(buffer) + e->m_offset);
    }
    header->m_sections.clearAndDeallocate();
    header->m_trackedObjects.clearAndDeallocate();
}

// 0x00F64170 (:598): the type infos loadInPlace stored in the virtual fixup records
void hkNativePackfileUtils::unloadInPlace(void* buffer, int bufferSize)
{
    const hkPackfileHeader* header = static_cast<const hkPackfileHeader*>(buffer);
    for (int s = 0; s < header->m_numSections; s++)
    {
        const hkPackfileSectionHeader* section = header->getSectionHeader(buffer, s);
        if (section->getFinishSize() == 0)
        {
            continue;
        }
        const int* records = reinterpret_cast<const int*>(static_cast<const char*>(buffer) + section->m_absoluteDataStart + section->m_virtualFixupsOffset);
        for (int i = 0; i < section->getFinishSize() / 4; i += 3)
        {
            const hkTypeInfo* typeInfo = *reinterpret_cast<const hkTypeInfo* const*>(&records[i + 2]);
            if (records[i] != -1 && typeInfo != nullptr)
            {
                typeInfo->cleanupLoadedObject(static_cast<char*>(buffer) + section->m_absoluteDataStart + records[i]);
            }
        }
    }
}

// 0x00F64200 (:633): of a buffer loaded by load()
void hkNativePackfileUtils::getImportsExports(const void* loadedBuffer, hkArray<hkResource::Export,hkContainerHeapAllocator>& expOut, hkArray<hkResource::Import,hkContainerHeapAllocator>& impOut)
{
    const InplaceDataHeader* header = static_cast<const InplaceDataHeader*>(loadedBuffer);
    for (int i = 0; i < header->m_sections.getSize(); i++)
    {
        const hkPackfileSectionHeader& section = header->m_sections[i];
        const void* sectionBegin = static_cast<const char*>(loadedBuffer) + section.m_absoluteDataStart;
        section.getExports(sectionBegin, expOut);
        section.getImports(sectionBegin, impOut);
    }
}

// 0x00F64250 (:647): of a buffer loaded in place
static void getImportsExportsInPlace(const void* loadedBuffer, hkArray<hkResource::Export,hkContainerHeapAllocator>& expOut, hkArray<hkResource::Import,hkContainerHeapAllocator>& impOut)
{
    const hkPackfileHeader* header = static_cast<const hkPackfileHeader*>(loadedBuffer);
    const hkPackfileSectionHeader* section = header->m_numSections >= 1 ? header->getSectionHeader(loadedBuffer, 0) : nullptr;
    for (int i = 0; i < header->m_numSections; i++, section++)
    {
        const void* sectionBegin = static_cast<const char*>(loadedBuffer) + section->m_absoluteDataStart;
        section->getExports(sectionBegin, expOut);
        section->getImports(sectionBegin, impOut);
    }
}
