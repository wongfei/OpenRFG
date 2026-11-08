// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializedeprecated.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD), from rfg.exe: the default hkSerializeDeprecated singleton, the one
// in place until hkFeature_serializeDeprecatedPre700 installs hkSerializeDeprecated2 (the versioning of old
// packfiles and the XML packfiles). It loads nothing and reports that the deprecated code is not linked.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializedeprecated.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlparser.h"

namespace
{
    // The message of loadOldPackfile, loadOldPackfileOnHeap and readXmlPackfileHeader ("AsseetCc2" and "the the" are
    // the original's).
    const char* const s_packfileVersioningNotLinked =
        "Packfile versioning support is not linked. Versioning packfiles at runtime was deprecated in Havok-7.0.0.\n"
        "To do so requires linking some deprecated code from Source/Common/Compat/Deprecated\n"
        "If you are using hkProductFeatures.cxx, ensure you do not define HK_EXCLUDE_FEATURE_SerializeDeprecatedPre700.\n"
        "Note that by default this pulls in a lot of code and data (mainly previous versions of hkClasses).\n"
        "Some extra effort is required to strip the unused code and data but it will still cost several hundred Kb.\n"
        "Alternatively, you can use Tools/PackfileConvert/AsseetCc2 to convert your packfiles the the latest version before loading.\n";
}

template<> hkSerializeDeprecated* hkSingleton<hkSerializeDeprecated>::s_instance = nullptr; // 0x03A38178

// 0x00F534E0 (.h:32)
hkSerializeDeprecated::hkSerializeDeprecated()
{
}

// 0x00F532F0
hkSerializeDeprecated::~hkSerializeDeprecated()
{
}

// 0x00F45880
hkSerializeDeprecated::XmlPackfileHeader::XmlPackfileHeader()
{
}

// 0x00F458B0
hkSerializeDeprecated::XmlPackfileHeader::~XmlPackfileHeader()
{
}

// 0x00F42AB0 (.h:26): the predicates came with class version 10
bool hkSerializeDeprecated::XmlPackfileHeader::supportsPredicates() const
{
    return m_classVersion > 9;
}

// 0x00F53330 (:22)
hkResult hkSerializeDeprecated::saveXmlPackfile(const void* object, const hkClass& klass, hkStreamWriter* writer, const hkPackfileWriter::Options& options, hkPackfileWriter::AddObjectListener* userListener, hkSerializeUtil::ErrorDetails* errorOut)
{
    if (errorOut != nullptr)
    {
        errorOut->raiseError(hkSerializeUtil::ErrorDetails::ERRORID_DEPRECATED_NOT_INITIALIZED, "XML packfile support is not linked. Perhaps you have HK_EXCLUDE_FEATURE_SerializeDeprecatedPre700 in hkProductFeatures");
    }
    return HK_FAILURE;
}

// 0x00F53310 (:31)
unsigned int hkSerializeDeprecated::isLoadable(const hkSerializeUtil::FormatDetails& details)
{
    return 0;
}

// 0x00F53360 (:36)
hkResource* hkSerializeDeprecated::loadOldPackfile(hkStreamReader& sr, const hkSerializeUtil::FormatDetails& details, hkSerializeUtil::ErrorDetails* errorOut)
{
    if (errorOut != nullptr)
    {
        errorOut->raiseError(hkSerializeUtil::ErrorDetails::ERRORID_DEPRECATED_NOT_INITIALIZED, s_packfileVersioningNotLinked);
    }
    return nullptr;
}

// 0x00F53390 (:45)
hkObjectResource* hkSerializeDeprecated::loadOldPackfileOnHeap(hkStreamReader& sr, const hkSerializeUtil::FormatDetails& details, hkSerializeUtil::ErrorDetails* errorOut)
{
    if (errorOut != nullptr)
    {
        errorOut->raiseError(hkSerializeUtil::ErrorDetails::ERRORID_DEPRECATED_NOT_INITIALIZED, s_packfileVersioningNotLinked);
    }
    return nullptr;
}

// 0x00F53430 (:54): raises the "not linked" error when the first XML node is an <hkpackfile> element; always fails.
hkResult hkSerializeDeprecated::readXmlPackfileHeader(hkStreamReader* stream, XmlPackfileHeader& out, hkSerializeUtil::ErrorDetails* errorOut)
{
    if (errorOut != nullptr)
    {
        hkXmlParser parser;
        hkXmlParser::Node* node = nullptr;
        if (parser.nextNode(&node, stream) == HK_SUCCESS)
        {
            hkXmlParser::StartElement* start = node->asStart();
            if (start != nullptr && start->name == "hkpackfile")
            {
                errorOut->raiseError(hkSerializeUtil::ErrorDetails::ERRORID_DEPRECATED_NOT_INITIALIZED, s_packfileVersioningNotLinked);
            }
            node->removeReference();
        }
    }
    return HK_FAILURE;
}

// 0x00F53320 (:77)
bool hkSerializeDeprecated::isEnabled() const
{
    return false;
}

// 0x00F533C0 (:82): a static instance; each creation adds a reference to it
static hkReferencedObject* createInstance()
{
    static hkSerializeDeprecated s_instance;
    s_instance.addReference();
    return &s_instance;
}

// 0x016FA894
static hkSingletonInitNode hkSingletonRegisterhkSerializeDeprecated("hkSerializeDeprecated", createInstance, (void**)&hkSingleton<hkSerializeDeprecated>::s_instance);
