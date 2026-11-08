// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/hkpackfilewriter.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE), from rfg.exe: the object graph a packfile writer saves (the
// pending writes in order, with their sections, imports and exports); the formats' save() is in the subclasses.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/hkpackfilewriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkvtableclassregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/offsetonly/hkoffsetonlystreamwriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkrelocationinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/platform/hkplatformobjectwriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.h"

extern const hkClass hkClassClass;
extern const hkClass hkClassMemberClass;
extern const hkClass hkClassEnumClass;

const char* hkPackfileWriter::SECTION_TAG_DATA = "__data__"; // 0x016FA9E8
const char* hkPackfileWriter::SECTION_TAG_TYPES = "__types__"; // 0x016FA9EC

// 0x00F6A0F0 (:34): the reflection classes' own metadata is never written (known, no pending write)
hkPackfileWriter::hkPackfileWriter(const Options& options)
    : m_contentsPtrPWIndex(-1), m_contentsClassPWIndex(-1), m_numDataInstances(0), m_numClassInstances(0), m_startOptions(options)
{
    m_knownObjects.insert(&hkClassEnumClass, -1);
    m_knownObjects.insert(&hkClassMemberClass, -1);
    m_knownObjects.insert(&hkClassClass, -1);
}

// 0x00F69B90 (:43): the section tags are the writer's copies
hkPackfileWriter::~hkPackfileWriter()
{
    for (int i = 0; i < m_knownSections.getSize(); i++)
    {
        hkMemoryRouter::easyFree(hkMemoryRouter::getInstance().heap(), m_knownSections[i]);
    }
}

// 0x00F6A280 (:52): the contents' exact class from the registry (the builtin one when none is given; addObject
// still gets the given one)
void hkPackfileWriter::setContentsWithRegistry(const void* pointer, const hkClass& klass, const hkVtableClassRegistry* classRegistry, AddObjectListener* addListener)
{
    const hkVtableClassRegistry* reg = classRegistry != nullptr ? classRegistry : hkBuiltinTypeRegistry::getInstance().getVtableClassRegistry();
    const hkClass* klassIn = klass.hasVtable() ? reg->getClassFromVirtualInstance(pointer) : &klass;
    // Original bug kept: addObject gets the caller's registry, not reg, so with a null registry getExactClass returns
    // null for a virtual object and the contents are dropped with a warning.
    addObject(pointer, *klassIn, classRegistry, addListener, SECTION_TAG_DATA);
    m_contentsPtrPWIndex = m_knownObjects.getWithDefault(pointer, -1);
    m_contentsClassPWIndex = m_knownObjects.getWithDefault(klassIn, -1);
}

// 0x00F69720 (:69)
void hkPackfileWriter::setContents(const void* object, const hkClass& klass, AddObjectListener* addListen)
{
    setContentsWithRegistry(object, klass, hkBuiltinTypeRegistry::getInstance().getVtableClassRegistry(), addListen);
}

// 0x00F697C0 (:74): an object written elsewhere; a pending write of it is redirected to the import
void hkPackfileWriter::addImport(const void* object, const char* id)
{
    const int index = m_knownObjects.getWithDefault(object, -1);
    if (index >= 0)
    {
        m_pendingWrites[index].m_sectionIndex = INDEX_IMPORT;
    }
    m_knownObjects.insert(object, INDEX_IMPORT);
    m_imports.insert(object, id);
}

// 0x00F69810 (:85)
void hkPackfileWriter::addExport(const void* object, const char* id)
{
    m_exports.insert(object, id);
}

// 0x00F69830 (:91): a new tag gets the next index
unsigned int hkPackfileWriter::sectionTagToIndex(const char* sectionTag)
{
    int index;
    if (m_sectionTagToIndex.get(sectionTag, &index) == HK_SUCCESS)
    {
        return index;
    }
    char* tag = hkString::strDup(sectionTag);
    index = m_knownSections.getSize();
    m_sectionTagToIndex.insert(tag, index);
    m_knownSections.pushBack(tag);
    return index;
}

// 0x00F698B0 (:104): the pointer's override, then the class's or its nearest parent's, then the given tag
unsigned int hkPackfileWriter::findSectionFor(const void* pointer, const hkClass& klass, const char* sectionTag)
{
    unsigned int index = m_sectionOverrideByPointer.getWithDefault(pointer, 0xFFFFFFFF);
    if (index != 0xFFFFFFFF)
    {
        return index;
    }
    index = m_sectionOverrideByType.getWithDefault(klass.getName(), 0xFFFFFFFF);
    if (index != 0xFFFFFFFF)
    {
        return index;
    }
    for (const hkClass* c = klass.getParent(); c != nullptr; c = c->getParent())
    {
        index = m_sectionOverrideByType.getWithDefault(c->getName(), 0xFFFFFFFF);
        if (index != 0xFFFFFFFF)
        {
            return index;
        }
    }
    return sectionTagToIndex(sectionTag);
}

// 0x00F696D0 (:133): null for a virtual object without a registry
static const hkClass* getExactClass(const void* instance, const hkClass& base, const hkVtableClassRegistry* reg)
{
    if (!base.hasVtable())
    {
        return &base;
    }
    if (reg != nullptr)
    {
        return reg->getClassFromVirtualInstance(instance);
    }
    return nullptr;
}

// 0x00F69AD0 (:142)
void hkPackfileWriter::addPendingWrite(const void* pointer, const hkClass& klass, const void* pointerIn, const hkClass& klassIn, const char* sectionTag)
{
    const int sectionIndex = findSectionFor(pointerIn, klassIn, sectionTag);
    PendingWrite& pw = m_pendingWrites.expandOne();
    pw.m_pointer = pointer;
    pw.m_klass = &klass;
    pw.m_origPointer = pointerIn;
    pw.m_origClass = &klassIn;
    pw.m_sectionIndex = sectionIndex;
    pw.m_dataSize = -1;
    m_numClassInstances += &klass == &hkClassClass;
    m_numDataInstances += &klass != &hkClassClass;
}

// 0x00F69750 (:157): the object's pointers, through a host layout write into an offset-only stream
static void chasePointers(const void* pointer, const hkClass& klass, hkRelocationInfo& reloc, hkBool writeSerializeIgnore)
{
    hkStructureLayout layout;
    hkPlatformObjectWriter writer(layout, nullptr, writeSerializeIgnore ? hkObjectCopier::FLAG_NONE : hkObjectCopier::FLAG_RESPECT_SERIALIZE_IGNORED);
    hkOffsetOnlyStreamWriter nullWriter;
    writer.writeObject(&nullWriter, pointer, klass, reloc);
}

// 0x00F69930 (:173): a class whose name is known already is replaced by the known one (0); an hkClass with a
// different signature is an error, after which it is replaced all the same
int hkPackfileWriter::notDuplicateMetaData(const void* pointer, const hkClass* klass)
{
    if (hkString::strCmp(klass->getName(), "hkClass") != 0)
    {
        return 1;
    }
    const hkClass* c = static_cast<const hkClass*>(pointer);
    const hkClass* known = m_knownClasses.getWithDefault(c->getName(), nullptr);
    if (known == nullptr)
    {
        m_knownClasses.insert(c->getName(), c);
        return 1;
    }
    if (c->getSignature(0) != known->getSignature(0))
    {
        char buf[512];
        hkErrStream os(buf, sizeof(buf));
        os << "Conflicting metadata found for class " << c->getName()
           << ". Perhaps you have called setContents on data which has not been updated to the current version.";
        if (hkErrorFwd::messageError(0x2518721C, buf, "Packfile\\hkPackfileWriter.cpp", 184) != 0)
        {
            __debugbreak(); // HK_BREAKPOINT
        }
    }
    m_knownObjects.insert(pointer, m_knownObjects.getWithDefault(known, -1));
    m_replacements.insert(pointer, known);
    return 0;
}

// 0x00F69DA0 (:200): the object (once; the listener may substitute or drop it), then everything it points to,
// then its class in the types section. Related pointers (homogeneous array classes) follow the object's section.
void hkPackfileWriter::addObject(const void* pointerIn, const hkClass& klassIn, const hkVtableClassRegistry* classRegistry, AddObjectListener* addListener, const char* sectionTag)
{
    if (m_knownObjects.hasKey(pointerIn))
    {
        return;
    }
    const hkClass* klass = getExactClass(pointerIn, klassIn, classRegistry);
    if (klass == nullptr)
    {
        char buf[512];
        hkErrStream os(buf, sizeof(buf));
        os << "Found an un-registered class derived from " << klassIn.getName()
           << ". Derived class will not be serialized unless added to class registry.\n"
           << "All saved pointers to this object will be set to NULL.\n"
           << "Saved file will not generate warnings (or asserts) on load but NULL pointers may cause runtime crashes.";
        hkErrorFwd::messageWarning(0xABBAABBA, buf, "Packfile\\hkPackfileWriter.cpp", 275);
        return;
    }
    const void* pointer = pointerIn;
    if (addListener != nullptr)
    {
        addListener->addObjectCallback(pointer, klass);
    }
    if (pointer == nullptr)
    {
        m_knownObjects.insert(pointerIn, INDEX_IGNORE);
        return;
    }
    if (!notDuplicateMetaData(pointer, klass))
    {
        return;
    }
    if (pointer != pointerIn && m_knownObjects.hasKey(pointer))
    {
        return;
    }
    m_knownObjects.insert(pointerIn, m_pendingWrites.getSize());
    if (pointer != pointerIn)
    {
        m_knownObjects.insert(pointer, m_pendingWrites.getSize());
        m_replacements.insert(pointer, pointerIn);
    }
    addPendingWrite(pointer, *klass, pointerIn, klassIn, sectionTag);

    hkRelocationInfo reloc;
    chasePointers(pointer, *klass, reloc, m_startOptions.m_writeSerializedFalse);
    for (int i = 0; i < reloc.m_global.getSize(); i++)
    {
        const hkRelocationInfo::Global& global = reloc.m_global[i];
        const hkClass* targetClass = global.m_toClass;
        void* target = global.m_toAddress;
        if (targetClass == nullptr || target == nullptr)
        {
            continue;
        }
        if (global.m_related && !m_sectionOverrideByPointer.hasKey(target))
        {
            const unsigned int defaultIndex = sectionTagToIndex(sectionTag);
            m_sectionOverrideByPointer.insert(target, m_sectionOverrideByPointer.getWithDefault(pointer, defaultIndex));
        }
        addObject(target, *targetClass, classRegistry, addListener, sectionTag);
        if (pointer != target)
        {
            int index = m_knownObjects.getWithDefault(target, -1);
            if (index != -1)
            {
                m_pwIndexesFromReferencedPointer.insert(pointer, index);
            }
        }
    }
    addObject(klass, hkClassClass, classRegistry, addListener, SECTION_TAG_TYPES);
}

// 0x00F69A60 (:285)
void hkPackfileWriter::addSection(const char* sectionTag)
{
    sectionTagToIndex(sectionTag);
}

// 0x00F69A70 (:290)
void hkPackfileWriter::setSectionForPointer(const void* ptr, const char* sectionTag)
{
    m_sectionOverrideByPointer.insert(ptr, sectionTagToIndex(sectionTag));
}

// 0x00F69AA0 (:295)
void hkPackfileWriter::setSectionForClass(const hkClass& k, const char* sectionTag)
{
    m_sectionOverrideByType.insert(k.getName(), sectionTagToIndex(sectionTag));
}

// 0x00F69700 (:300)
void hkPackfileWriter::getCurrentVersion(char* buf, int bufLen)
{
    hkString::snprintf(buf, bufLen, "%s", "hk_2014.2.5-r1");
}
