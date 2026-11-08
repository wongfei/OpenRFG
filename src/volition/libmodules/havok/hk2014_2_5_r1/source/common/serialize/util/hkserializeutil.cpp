// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializeutil.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD), from rfg.exe (checkTagfileWorld, loadPackfile,
// loadNewPackfileOnHeap, classSignaturesUpToDate, detectFormat and isLoadable from the asm, their decompiles are
// LTCG-garbled). The format is detected on a peeking reader over the stream; tagfiles load into a dictionary world
// (checked against the class registry, then patched to the current versions) and are copied out to native objects;
// current packfiles load natively, older ones through hkSerializeDeprecated.
//
// Not ported yet (the packfile and tagfile writers, SE): savePackfile, saveTagfile, save (:605-:680) and the
// ForwardingPackfileListerer they use (:255-:287).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializeutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringbuf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/util/hkmemutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hktypeinforegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/istream/hkistream.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/hkstreamreader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobjectimpl.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/native/hkdataobjectnative.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataobjectutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkbinarypackfilewriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkpackfileheader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkpackfilesectionheader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/binary/hkbinarytagfilereader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/xml/hkxmltagfilecommon.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/xml/hkxmltagfilereader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hknativepackfileutils.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializedeprecated.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionpatchmanager.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionutil.h"

namespace
{
    // 0x01371AE0
    const char* loadVersion0FailedBoilerPlate =
        "No information about this class exists in the serialization system. This could happen if:\n"
        "* You have provided a custom classes file which does not include this class\n"
        "* You have not specified keycodes before including hkProductFeatures.cxx\n"
        "* Custom classes have not been registered.";
    // 0x01371C00
    const char* versioningFailedBoilerPlate =
        "Patching to latest version failed. Have you registered the necessary patches? Patches may be missing because "
        "your assets are older than HK_SERIALIZE_MIN_COMPATIBLE_VERSION (if defined). Make sure that the patches are "
        "registered properly (see hkRegisterPatches.cxx, included by hkProductFeatures.cxx). See the hkError output for "
        "more details.";

    // size 0x1C
    // Reads through a buffer of everything peeked so far, so the format detectors can each start from the beginning
    // (resetPos) without seeking the underlying stream.
    class PeekStreamReader : public hkStreamReader
    {
    public:
        HK_DECLARE_CLASS_ALLOCATOR(); // delete 0x00F45690 (:776)

        // 0x00F45600 (:780)
        PeekStreamReader(hkStreamReader* underlying) : m_underlyingStream(underlying), m_curPos(0)
        {
            expandPeekBuffer(512);
        }
        // the destructor (0x00F45440) is compiler-generated

        // 0x00F45640 (:785)
        hkBool isOk() const override
        {
            return m_curPos >= 0 && m_underlyingStream->isOk();
        }

        // 0x00F42B40 (:790)
        int read(void* buf, int nbytes) override
        {
            const int n = peek(buf, nbytes);
            m_curPos += n;
            return n;
        }

        // 0x00F456D0 (:798)
        int peek(void* buf, int nbytes) override
        {
            if (m_curPos + nbytes >= m_peekBuffer.getSize())
            {
                expandPeekBuffer(nbytes);
            }
            const int available = m_peekBuffer.getSize() - m_curPos;
            if (nbytes > available)
            {
                nbytes = available;
            }
            if (nbytes != 0)
            {
                hkString::memCpy(buf, m_peekBuffer.begin() + m_curPos, nbytes);
            }
            return nbytes;
        }

        hkSeekableStreamReader* isSeekTellSupported() override { return nullptr; } // 0x00F45680 (:814)

        void resetPos() { m_curPos = 0; } // 0x00F42B60 (:816)

    private:
        // 0x00F453A0 (:821): grown by half (at least by nbytes + 1), filled by peeking the underlying stream
        void expandPeekBuffer(const int nbytes)
        {
            int newSize = (int)(m_peekBuffer.getSize() * 1.5);
            const int minSize = m_peekBuffer.getSize() + 1 + nbytes;
            if (newSize < minSize)
            {
                newSize = minSize;
            }
            m_peekBuffer.setSize(newSize);
            const int n = m_underlyingStream->peek(m_peekBuffer.begin(), newSize);
            m_peekBuffer.setSize(n);
        }

        hkStreamReader* m_underlyingStream; // 0x08
        hkArray<char, hkContainerTempAllocator> m_peekBuffer; // 0x0C
        int m_curPos; // 0x18
    };

    // 0x00F437A0 (:60): every class of the world is current or has a patch
    hkBool32 checkTagfileWorld(hkDataWorld& world, hkSerializeUtil::ErrorDetails& errorOut, const hkVersionPatchManager& versionManager, const hkClassNameRegistry& classReg)
    {
        hkArray<hkDataClassImpl*, hkContainerTempAllocator> classes;
        world.findAllClasses(classes);
        for (int i = 0; i < classes.getSize(); i++)
        {
            const char* name = classes[i]->getName();
            const int version = classes[i]->getVersion();
            const hkClass* k = classReg.getClassByName(name);
            if (k == nullptr || k->getDescribedVersion() != version)
            {
                const hkUint64 uid = versionManager.getUid(name, version);
                if (versionManager.getPatchIndex(uid) == -1)
                {
                    hkStringBuf buf;
                    if (version == 0)
                    {
                        buf.printf("Unable to load class %s, version 0x0\n%s", name, loadVersion0FailedBoilerPlate);
                    }
                    else
                    {
                        buf.printf("Unable to version data of class %s, version 0x%p\n%s", name, version, versioningFailedBoilerPlate);
                    }
                    errorOut.raiseError(hkSerializeUtil::ErrorDetails::ERRORID_VERSIONING_FAILED, buf.cString());
                    return false;
                }
            }
        }
        return true;
    }

    // 0x00F43960 (:92)
    hkResult loadBinaryTagfileIntoWorld(hkStreamReader& streamIn, hkDataWorldDict& world, hkSerializeUtil::ErrorDetails* errorOut, const hkClassNameRegistry& classReg)
    {
        hkBinaryTagfileReader reader;
        hkDataObject contents = reader.load(&streamIn, world);
        if (contents.isNull())
        {
            if (errorOut != nullptr)
            {
                errorOut->raiseError(hkSerializeUtil::ErrorDetails::ERRORID_VERSIONING_FAILED, versioningFailedBoilerPlate);
            }
            return HK_FAILURE;
        }
        if (errorOut != nullptr && !checkTagfileWorld(world, *errorOut, hkVersionPatchManager::getInstance(), classReg))
        {
            return HK_FAILURE;
        }
        hkDefaultClassWrapper wrapper(&classReg);
        if (hkVersionPatchManager::getInstance().applyPatches(world, &wrapper) != HK_SUCCESS)
        {
            if (errorOut != nullptr)
            {
                errorOut->raiseError(hkSerializeUtil::ErrorDetails::ERRORID_VERSIONING_FAILED, versioningFailedBoilerPlate);
            }
            return HK_FAILURE;
        }
        return HK_SUCCESS;
    }

    // 0x00F43AE0 (:128)
    hkResult loadXmlTagfileIntoWorld(hkStreamReader& streamIn, hkDataWorldDict& world, hkSerializeUtil::ErrorDetails* errorOut, const hkClassNameRegistry& classReg)
    {
        hkXmlTagfileReader reader;
        hkDataObject contents = reader.load(&streamIn, world);
        if (contents.isNull())
        {
            if (errorOut != nullptr)
            {
                errorOut->raiseError(hkSerializeUtil::ErrorDetails::ERRORID_VERSIONING_FAILED, versioningFailedBoilerPlate);
            }
            return HK_FAILURE;
        }
        if (errorOut != nullptr && !checkTagfileWorld(world, *errorOut, hkVersionPatchManager::getInstance(), classReg))
        {
            return HK_FAILURE;
        }
        hkDefaultClassWrapper wrapper(&classReg);
        if (hkVersionPatchManager::getInstance().applyPatches(world, &wrapper) != HK_SUCCESS)
        {
            if (errorOut != nullptr)
            {
                errorOut->raiseError(hkSerializeUtil::ErrorDetails::ERRORID_VERSIONING_FAILED, versioningFailedBoilerPlate);
            }
            return HK_FAILURE;
        }
        return HK_SUCCESS;
    }

    hkBool classSignaturesUpToDate(const hkPackfileHeader* packfileHeader, const hkClassNameRegistry* classReg);

    // 0x00F42E20 (:160): a current packfile of this platform is read whole and loaded natively (its class signatures
    // checked); any other goes to the deprecated loader (with what was already read)
    hkResource* loadPackfile(hkStreamReader& streamIn, const hkSerializeUtil::FormatDetails& details, hkSerializeUtil::ErrorDetails* errorOut, const hkTypeInfoRegistry& typeReg, hkSerializeUtil::LoadOptions options)
    {
        hkArray<char, hkContainerTempAllocator> buf;
        if (details.m_formatType == hkSerializeUtil::FORMAT_PACKFILE_BINARY)
        {
            if (hkString::memCmp(&details.m_layoutRules, &hkStructureLayout::HostLayoutRules, 4) != 0)
            {
                if (errorOut != nullptr)
                {
                    errorOut->raiseError(hkSerializeUtil::ErrorDetails::ERRORID_PACKFILE_PLATFORM, "Wrong platform for packfile");
                }
                return nullptr;
            }
            if (details.m_version == hkVersionUtil::getCurrentVersion() &&
                details.m_formatVersion == (int)hkBinaryPackfileWriter::CURRENT_FILE_VERSION)
            {
                // the header, the predicates, the section headers, then the sections up to the last one's end
                streamIn.read(buf.expandBy(sizeof(hkPackfileHeader)), sizeof(hkPackfileHeader));
                const hkPackfileHeader* header = reinterpret_cast<const hkPackfileHeader*>(buf.begin());
                const int numSections = header->m_numSections;
                const int sectionsSize = numSections * (int)sizeof(hkPackfileSectionHeader);
                const int sectionsOffset = (int)(reinterpret_cast<char*>(header->getSectionHeader(buf.begin(), 0)) - buf.begin());
                if (header->m_fileVersion >= 10)
                {
                    const int predicatesSize = header->m_predicateArraySizePlusPadding;
                    streamIn.read(buf.expandBy(predicatesSize), predicatesSize);
                }
                buf.expandBy(sectionsSize);
                hkPackfileSectionHeader* sections = reinterpret_cast<hkPackfileSectionHeader*>(buf.begin() + sectionsOffset);
                streamIn.read(sections, sectionsSize);
                int dataSize = 0;
                for (int i = 0; i < numSections; i++)
                {
                    dataSize += sections[i].m_endOffset;
                }
                const int dataOffset = sectionsOffset + sectionsSize;
                buf.reserve(buf.getSize() + dataSize);
                buf.expandBy(dataSize);
                streamIn.read(buf.begin() + dataOffset, dataSize);
                if (classSignaturesUpToDate(reinterpret_cast<const hkPackfileHeader*>(buf.begin()), options.getClassNameRegistry()))
                {
                    return hkNativePackfileUtils::load(buf.begin(), buf.getSize(), &typeReg);
                }
                if (options.anyIsSet(hkSerializeUtil::LOAD_FAIL_IF_VERSIONING))
                {
                    if (errorOut != nullptr)
                    {
                        errorOut->raiseError(hkSerializeUtil::ErrorDetails::ERRORID_VERSIONING_FAILED, "Class signatures not up to date.");
                    }
                    return nullptr;
                }
            }
            if (buf.getSize() != 0)
            {
                hkIstream is(buf.begin(), buf.getSize());
                return hkSerializeDeprecated::getInstance().loadOldPackfile(*is.getStreamReader(), details, errorOut);
            }
        }
        return hkSerializeDeprecated::getInstance().loadOldPackfile(streamIn, details, errorOut);
    }

    // 0x00F431D0 (:303): loaded with the objects left unfinished, then copied to heap objects through a native world
    hkObjectResource* loadNewPackfileOnHeap(hkStreamReader* streamIn, hkSerializeUtil::FormatDetails& details, hkSerializeUtil::ErrorDetails* errorOut, const hkClassNameRegistry* classReg, const hkTypeInfoRegistry* typeReg, hkSerializeUtil::LoadOptions options)
    {
        hkTypeInfoRegistry noFinishRegistry(0, 1);
        noFinishRegistry.merge(*typeReg);
        hkResource* res = loadPackfile(*streamIn, details, errorOut, noFinishRegistry, options);
        if (res == nullptr)
        {
            return nullptr;
        }
        hkObjectResource* result = nullptr;
        {
            hkDataWorldNative world(false);
            world.setClassRegistry(classReg);
            const hkClass* klass = classReg->getClassByName(res->getContentsTypeName());
            if (klass != nullptr)
            {
                hkDataObject top(world.wrapObject(res->getContentsPointer(nullptr, typeReg), *klass));
                result = hkDataObjectUtil::toObjectWithRegistry(top, classReg, typeReg, true);
            }
        }
        res->removeReference();
        return result;
    }

    // 0x00F42CD0 (:327): every class named in the class name section (signature, tab, name) has the registry's
    // signature
    hkBool classSignaturesUpToDate(const hkPackfileHeader* packfileHeader, const hkClassNameRegistry* classReg)
    {
        const hkPackfileSectionHeader* section = packfileHeader->getSectionHeader(static_cast<const void*>(packfileHeader), 0);
        for (int i = 0; i < packfileHeader->m_numSections; i++, section++)
        {
            if (hkString::strCmp(section->m_sectionTag, hkBinaryPackfileWriter::SECTION_TAG_CLASSNAMES) == 0)
            {
                const char* names = reinterpret_cast<const char*>(packfileHeader) + section->m_absoluteDataStart;
                for (int offset = 0; offset < section->m_localFixupsOffset - 6;)
                {
                    const char* entry = names + offset;
                    const char* name = entry + 5;
                    if ((unsigned char)name[0] == 0xFF)
                    {
                        break; // padding
                    }
                    const hkUint32 signature = *reinterpret_cast<const hkUint32*>(entry);
                    if (entry[4] != '\t')
                    {
                        return false;
                    }
                    const hkClass* k = classReg->getClassByName(name);
                    if (k == nullptr || signature != k->getSignature(0))
                    {
                        return false;
                    }
                    offset += hkString::strLen(name) + 6;
                }
                return true;
            }
        }
        return false;
    }
}

// 0x00F42D90 (:397)
const hkClassNameRegistry* hkSerializeUtil::LoadOptions::getClassNameRegistry() const
{
    return m_classNameReg != nullptr ? m_classNameReg : hkBuiltinTypeRegistry::getInstance().getClassNameRegistry();
}

// 0x00F42DB0 (:404)
const hkTypeInfoRegistry* hkSerializeUtil::LoadOptions::getTypeInfoRegistry() const
{
    return m_typeInfoReg != nullptr ? m_typeInfoReg : hkBuiltinTypeRegistry::getInstance().getTypeInfoRegistry();
}

// 0x00F44110 (:415): a resource (packfile data) of the stream's contents
hkResource* hkSerializeUtil::load(hkStreamReader* streamIn, ErrorDetails* errorOut, LoadOptions options)
{
    if (streamIn == nullptr)
    {
        if (errorOut != nullptr)
        {
            errorOut->raiseError(ErrorDetails::ERRORID_READ_FAILED, "Stream pointer is null");
        }
        return nullptr;
    }
    if (!streamIn->isOk())
    {
        if (errorOut != nullptr)
        {
            errorOut->raiseError(ErrorDetails::ERRORID_READ_FAILED, "Stream is not ok");
        }
        return nullptr;
    }
    const hkClassNameRegistry* classReg = options.getClassNameRegistry();
    const hkTypeInfoRegistry* typeReg = options.getTypeInfoRegistry();
    FormatDetails details;
    detectFormat(streamIn, details, errorOut);
    switch (details.m_formatType)
    {
        case FORMAT_PACKFILE_BINARY:
        case FORMAT_PACKFILE_XML:
            return loadPackfile(*streamIn, details, errorOut, *typeReg, options);
        case FORMAT_TAGFILE_BINARY:
        case FORMAT_TAGFILE_XML:
        {
            // a 16 KB LIFO block taken for the world and given back unused (Havok's temp buffer)
            void* scratch = hkMemoryRouter::getInstance().stack().fastBlockAlloc(0x4000);
            hkResource* resource = nullptr;
            {
                hkDataWorldDict world(&hkMemoryRouter::getInstance().heap());
                const hkResult loaded = details.m_formatType == FORMAT_TAGFILE_BINARY
                    ? loadBinaryTagfileIntoWorld(*streamIn, world, errorOut, *classReg)
                    : loadXmlTagfileIntoWorld(*streamIn, world, errorOut, *classReg);
                if (loaded == HK_SUCCESS || options.anyIsSet(LOAD_FORCED))
                {
                    resource = hkDataObjectUtil::toResourceWithRegistry(world.getContents(), classReg, true);
                }
            }
            hkMemoryRouter::getInstance().stack().fastBlockFree(scratch, 0x4000);
            return resource;
        }
        default:
            if (errorOut != nullptr)
            {
                errorOut->raiseError(ErrorDetails::ERRORID_UNSUPPORTED_FORMAT, "Unable to detect format from stream");
            }
            return nullptr;
    }
}

// 0x00F44460 (:493)
hkResource* hkSerializeUtil::load(const char* filename, ErrorDetails* resultOut, LoadOptions options)
{
    hkIstream is(filename);
    return load(is.getStreamReader(), resultOut, options);
}

// 0x00F444B0 (:500): a current packfile of this platform in memory loads in place
hkResource* hkSerializeUtil::load(const void* buf, int buflen, ErrorDetails* resultOut, LoadOptions options)
{
    const hkPackfileHeader magic;
    const hkPackfileHeader* header = static_cast<const hkPackfileHeader*>(buf);
    if (header->m_magic[0] == magic.m_magic[0] && header->m_magic[1] == magic.m_magic[1] &&
        hkString::memCmp(header->m_layoutRules, &hkStructureLayout::HostLayoutRules, 4) == 0 &&
        hkString::strCmp(header->m_contentsVersion, hkVersionUtil::getCurrentVersion()) == 0 &&
        classSignaturesUpToDate(header, options.getClassNameRegistry()))
    {
        return hkNativePackfileUtils::load(buf, buflen, nullptr);
    }
    hkIstream is(buf, buflen);
    return load(is.getStreamReader(), resultOut, options);
}

// 0x00F445A0 (:521): heap objects of the stream's contents (packfiles copied off their buffer)
hkObjectResource* hkSerializeUtil::loadOnHeap(hkStreamReader* streamIn, ErrorDetails* errorOut, LoadOptions options)
{
    if (streamIn == nullptr)
    {
        if (errorOut != nullptr)
        {
            errorOut->raiseError(ErrorDetails::ERRORID_READ_FAILED, "Stream pointer is null");
        }
        return nullptr;
    }
    if (!streamIn->isOk())
    {
        if (errorOut != nullptr)
        {
            errorOut->raiseError(ErrorDetails::ERRORID_READ_FAILED, "Stream is not ok");
        }
        return nullptr;
    }
    const hkClassNameRegistry* classReg = options.getClassNameRegistry();
    const hkTypeInfoRegistry* typeReg = options.getTypeInfoRegistry();
    FormatDetails details;
    detectFormat(streamIn, details, errorOut);
    switch (details.m_formatType)
    {
        case FORMAT_PACKFILE_BINARY:
        {
            if (details.m_layoutRules == hkStructureLayout::HostLayoutRules &&
                details.m_version == hkVersionUtil::getCurrentVersion() &&
                details.m_formatVersion == (int)hkBinaryPackfileWriter::CURRENT_FILE_VERSION)
            {
                return loadNewPackfileOnHeap(streamIn, details, errorOut, classReg, typeReg, options);
            }
            if (options.anyIsSet(LOAD_FAIL_IF_VERSIONING))
            {
                if (errorOut != nullptr)
                {
                    errorOut->raiseError(ErrorDetails::ERRORID_VERSIONING_FAILED, "Packfile required versioning but versioning not supported in this context.");
                }
                return nullptr;
            }
            return hkSerializeDeprecated::getInstance().loadOldPackfileOnHeap(*streamIn, details, errorOut);
        }
        case FORMAT_PACKFILE_XML:
            return hkSerializeDeprecated::getInstance().loadOldPackfileOnHeap(*streamIn, details, errorOut);
        case FORMAT_TAGFILE_BINARY:
        case FORMAT_TAGFILE_XML:
        {
            hkDataWorldDict world;
            const hkResult loaded = details.m_formatType == FORMAT_TAGFILE_BINARY
                ? loadBinaryTagfileIntoWorld(*streamIn, world, errorOut, *classReg)
                : loadXmlTagfileIntoWorld(*streamIn, world, errorOut, *classReg);
            if (loaded == HK_SUCCESS)
            {
                return hkDataObjectUtil::toObjectWithRegistry(world.getContents(), classReg, typeReg, true);
            }
            return nullptr;
        }
        default:
            if (errorOut != nullptr)
            {
                errorOut->raiseError(ErrorDetails::ERRORID_UNSUPPORTED_FORMAT, "Unable to detect format from stream");
            }
            return nullptr;
    }
}

// 0x00F44880 (:595)
hkObjectResource* hkSerializeUtil::loadOnHeap(const char* filename, ErrorDetails* errorOut, LoadOptions options)
{
    hkIstream is(filename);
    return loadOnHeap(is.getStreamReader(), errorOut, options);
}

// 0x00F448D0 (:600)
hkObjectResource* hkSerializeUtil::loadOnHeap(const void* buf, int buflen, ErrorDetails* errorOut, LoadOptions options)
{
    hkIstream is(buf, buflen);
    return loadOnHeap(is.getStreamReader(), errorOut, options);
}

// 0x00F44920 (:685)
hkBool32 hkSerializeUtil::isLoadable(hkStreamReader* sr)
{
    if (sr == nullptr || !sr->isOk())
    {
        return false;
    }
    if (hkTagfileReader::detectFormat(sr) > hkTagfileReader::FORMAT_UNKNOWN)
    {
        return true;
    }
    FormatDetails details;
    detectFormat(sr, details, nullptr);
    if (details.m_formatType == FORMAT_PACKFILE_BINARY &&
        hkString::memCmp(&details.m_layoutRules, &hkStructureLayout::HostLayoutRules, 4) == 0)
    {
        if (details.m_version == hkVersionUtil::getCurrentVersion() &&
            details.m_formatVersion == (int)hkBinaryPackfileWriter::CURRENT_FILE_VERSION)
        {
            return true;
        }
    }
    else if (details.m_formatType != FORMAT_PACKFILE_XML)
    {
        return false;
    }
    return hkSerializeDeprecated::getInstance().isLoadable(details);
}

// 0x00F44A10 (:717)
hkEnum<hkSerializeUtil::FormatType, int> hkSerializeUtil::detectFormat(hkStreamReader* reader, ErrorDetails* errorOut)
{
    FormatDetails details;
    detectFormat(reader, details, errorOut);
    return details.m_formatType;
}

// 0x00F42DD0 (:760): 1 when the predicate is true, 0 when false, -1 when the format does not know it
hkSerializeUtil::FormatDetails::PredicateResult hkSerializeUtil::FormatDetails::verifiesPredicate(hkEnum<hkContentPredicate::PredicateIdEnum, hkUint16> pred) const
{
    if (pred >= m_maxPredicateId)
    {
        return PredicateResult::invalid();
    }
    return PredicateResult(m_truePredicates.indexOf((hkUint16)pred) >= 0 ? 1 : 0);
}

// 0x00F44AA0 (:836)
void hkSerializeUtil::detectFormat(const char* fileName, FormatDetails& details, ErrorDetails* errorOut)
{
    hkIstream is(fileName);
    detectFormat(is.getStreamReader(), details, errorOut);
}

// 0x00F43C60 (:841): binary tagfile, binary packfile, then XML (tagfile, else packfile) when the first character
// after white space is '<'
void hkSerializeUtil::detectFormat(hkStreamReader* reader, FormatDetails& details, ErrorDetails* errorOut)
{
    details.m_formatType = FORMAT_ERROR;
    details.m_formatVersion = 0;
    details.m_version = nullptr;
    details.m_maxPredicateId = hkContentPredicate::NO_PREDICATE;
    hkMemUtil::memSet(&details.m_layoutRules, 0, 4);
    PeekStreamReader peekReader(reader);
    hkBinaryTagfileReader tagfileReader;
    hkBinaryTagfile::Header tagfileHeader;
    if (tagfileReader.readHeader(&peekReader, tagfileHeader) == HK_SUCCESS)
    {
        details.m_formatType = FORMAT_TAGFILE_BINARY;
        details.m_formatVersion = tagfileHeader.m_version;
        details.m_version = tagfileHeader.m_sdk;
        if (tagfileHeader.supportsPredicates())
        {
            details.m_maxPredicateId = (hkContentPredicate::PredicateIdEnum)tagfileHeader.m_maxpredicate;
            details.m_truePredicates.append(tagfileHeader.m_truePredicates.begin(), tagfileHeader.m_truePredicates.getSize());
        }
        return;
    }
    peekReader.resetPos();
    hkPackfileHeader packfileHeader;
    if (hkPackfileHeader::readHeader(&peekReader, packfileHeader) == HK_SUCCESS)
    {
        details.m_formatType = FORMAT_PACKFILE_BINARY;
        details.m_version = packfileHeader.m_contentsVersion;
        details.m_formatVersion = packfileHeader.m_fileVersion;
        hkMemUtil::memCpy(&details.m_layoutRules, packfileHeader.m_layoutRules, 4);
        if (details.m_layoutRules.m_littleEndian != hkStructureLayout::HostLayoutRules.m_littleEndian)
        {
            // the version read in the other byte order
            hkUint8* v = reinterpret_cast<hkUint8*>(&details.m_formatVersion);
            hkUint8 t = v[0];
            v[0] = v[3];
            v[3] = t;
            t = v[1];
            v[1] = v[2];
            v[2] = t;
        }
        else if (details.m_layoutRules.m_bytesInPointer == hkStructureLayout::HostLayoutRules.m_bytesInPointer)
        {
            if (packfileHeader.readPredicates(&peekReader, details.m_truePredicates) == HK_SUCCESS)
            {
                details.m_maxPredicateId = (hkContentPredicate::PredicateIdEnum)packfileHeader.m_maxpredicate;
            }
            return;
        }
        if (errorOut != nullptr)
        {
            errorOut->raiseError(ErrorDetails::ERRORID_PACKFILE_PLATFORM, "Binary Packfile has the wrong endiannes or pointer size. Predicates won't be read.");
        }
        return;
    }
    peekReader.resetPos();
    char c;
    while (peekReader.read(&c, 1) != 0)
    {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
            continue;
        }
        if (c != '<')
        {
            return;
        }
        break;
    }
    peekReader.resetPos();
    hkXmlTagfileReader xmlReader;
    hkXmlTagfile::Header xmlHeader;
    if (xmlReader.readHeader(&peekReader, xmlHeader) == HK_SUCCESS)
    {
        details.m_formatVersion = xmlHeader.m_version;
        details.m_formatType = FORMAT_TAGFILE_XML;
        details.m_version = xmlHeader.m_sdkVersion;
        if (xmlHeader.m_version >= 2)
        {
            details.m_maxPredicateId = (hkContentPredicate::PredicateIdEnum)xmlHeader.m_maxpredicate;
            details.m_truePredicates.append(xmlHeader.m_truePredicates.begin(), xmlHeader.m_truePredicates.getSize());
        }
        return;
    }
    peekReader.resetPos();
    hkSerializeDeprecated::XmlPackfileHeader xmlPackfileHeader;
    if (hkSerializeDeprecated::getInstance().readXmlPackfileHeader(&peekReader, xmlPackfileHeader, errorOut) == HK_SUCCESS)
    {
        details.m_formatType = FORMAT_PACKFILE_XML;
        details.m_version = xmlPackfileHeader.m_contentsVersion;
        details.m_formatVersion = xmlPackfileHeader.m_classVersion;
        if (xmlPackfileHeader.m_classVersion >= 10)
        {
            details.m_maxPredicateId = (hkContentPredicate::PredicateIdEnum)xmlPackfileHeader.m_maxpredicate;
            details.m_truePredicates.append(xmlPackfileHeader.m_truePredicates.begin(), xmlPackfileHeader.m_truePredicates.getSize());
        }
    }
}
