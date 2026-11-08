// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializedeprecated.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializeutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/hkpackfilewriter.h"

class hkClass;
class hkDataWorldDict;
class hkObjectResource;
class hkPackfileReader;
class hkResource;
struct hkResult;
class hkStreamReader;
class hkStreamWriter;

// size 0x8
class hkSerializeDeprecated : public hkReferencedObject, public hkSingleton<hkSerializeDeprecated>
{
public:
    // size 0x1C
    struct XmlPackfileHeader
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(XmlPackfileHeader);
        bool supportsPredicates() const; // 0x00F42AB0
        XmlPackfileHeader(); // 0x00F45880
        ~XmlPackfileHeader(); // 0x00F458B0

        int m_classVersion; // 0x00
        hkStringPtr m_contentsVersion; // 0x04
        hkStringPtr m_topLevelObject; // 0x08
        unsigned short m_maxpredicate; // 0x0C
        hkArray<unsigned short,hkContainerHeapAllocator> m_truePredicates; // 0x10
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F534F0
    hkSerializeDeprecated(); // 0x00F534E0
    static void initDeprecated();
    virtual ~hkSerializeDeprecated(); // vtbl+0x00 0x00F532F0
    virtual bool isEnabled() const; // vtbl+0x10 0x00F53320
    virtual hkResult saveXmlPackfile(const void* object, const hkClass& klass, hkStreamWriter* writer, const hkPackfileWriter::Options& options, hkPackfileWriter::AddObjectListener* userListener, hkSerializeUtil::ErrorDetails* errorOut); // vtbl+0x14 0x00F53330
    virtual unsigned int isLoadable(const hkSerializeUtil::FormatDetails& details); // vtbl+0x18 0x00F53310
    virtual hkResource* loadOldPackfile(hkStreamReader& sr, const hkSerializeUtil::FormatDetails& details, hkSerializeUtil::ErrorDetails* errorOut); // vtbl+0x1C 0x00F53360
    virtual hkObjectResource* loadOldPackfileOnHeap(hkStreamReader& sr, const hkSerializeUtil::FormatDetails& details, hkSerializeUtil::ErrorDetails* errorOut); // vtbl+0x20 0x00F53390
    virtual hkResult readXmlPackfileHeader(hkStreamReader* stream, XmlPackfileHeader& out, hkSerializeUtil::ErrorDetails* errorOut); // vtbl+0x24 0x00F53430

};

// size 0x8
struct hkSerializeDeprecated2 : public hkSerializeDeprecated
{
    virtual ~hkSerializeDeprecated2(); // vtbl+0x00 0x00FA14B0
    virtual bool isEnabled() const; // vtbl+0x10 0x00FA1550
    hkResult packfileToDictWorld(hkDataWorldDict& dictWorld, hkStreamReader& sr, const hkSerializeUtil::FormatDetails& details, hkSerializeUtil::ErrorDetails* errorOut); // 0x00FA1E40
    static hkResult packfileToDictWorld(hkDataWorldDict& dictWorld, hkPackfileReader& reader, hkStreamReader& sr, const hkSerializeUtil::FormatDetails& details, hkSerializeUtil::ErrorDetails* errorOut); // 0x00FA19A0
    virtual hkResult saveXmlPackfile(const void* object, const hkClass& klass, hkStreamWriter* stream, const hkPackfileWriter::Options& options, hkPackfileWriter::AddObjectListener* userListener, hkSerializeUtil::ErrorDetails* errorOut); // vtbl+0x14 0x00FA20A0
    virtual unsigned int isLoadable(const hkSerializeUtil::FormatDetails& details); // vtbl+0x18 0x00FA1650
    virtual hkResource* loadOldPackfile(hkStreamReader& sr, const hkSerializeUtil::FormatDetails& details, hkSerializeUtil::ErrorDetails* errorOut); // vtbl+0x1C 0x00FA1F10
    virtual hkObjectResource* loadOldPackfileOnHeap(hkStreamReader& sr, const hkSerializeUtil::FormatDetails& details, hkSerializeUtil::ErrorDetails* errorOut); // vtbl+0x20 0x00FA1FC0
    hkSerializeDeprecated2(); // 0x00FA14C0
    virtual hkResult readXmlPackfileHeader(hkStreamReader* stream, hkSerializeDeprecated::XmlPackfileHeader& out, hkSerializeUtil::ErrorDetails* errorOut); // vtbl+0x24 0x00FA1510

};

namespace hkSerializeDeprecatedInit
{
    void initDeprecated(); // 0x00FA11E0
}
