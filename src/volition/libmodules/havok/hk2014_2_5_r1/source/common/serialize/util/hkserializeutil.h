// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializeutil.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializeutil.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD): loading (and detecting the format of) Havok files: binary and XML
// tagfiles, current and older packfiles. A namespace (the PDB has no type for it); declarations as 2014.1
// hkSerializeUtil.h, the inline code from rfg.exe. The save functions come with the packfile pass (SE).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkhandle.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkcontentpredicate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkstructurelayout.h"

class hkClass;
class hkClassNameRegistry;
class hkObjectResource;
class hkResource;
class hkStreamReader;
class hkStreamWriter;
class hkTypeInfoRegistry;

namespace hkSerializeUtil
{
    // size 0x8
    struct ErrorDetails
    {
        enum ErrorID
        {
            ERRORID_NONE = 0,
            ERRORID_READ_FAILED = 1,
            ERRORID_UNSUPPORTED_FORMAT = 2,
            ERRORID_PACKFILE_PLATFORM = 3,
            ERRORID_VERSIONING_FAILED = 4,
            ERRORID_NONHEAP_OBJECT = 5,
            ERRORID_LOAD_FAILED = 6,
            ERRORID_DEPRECATED_NOT_INITIALIZED = 7,
            ERRORID_MAX_ID = 8,
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ErrorDetails);

        // 0x008199A0 (.h:37; the SDK's id(ERRORID_NONE), hkEnum has no converting constructor here)
        ErrorDetails()
        {
            id = ERRORID_NONE;
        }

        // 0x00F44B40 (.h:67): the first error is kept
        void raiseError(ErrorID i, const char* msg)
        {
            if (id == ERRORID_NONE)
            {
                id = i;
                defaultMessage = msg;
            }
        }
        // ~ErrorDetails 0x0080F3B0 is the implicit destructor (it destructs defaultMessage)

        hkEnum<ErrorID,int> id; // 0x00
        hkStringPtr defaultMessage; // 0x04
    };

    enum SaveOptionBits
    {
        SAVE_DEFAULT = 0x00,
        SAVE_TEXT_FORMAT = 0x01,
        SAVE_SERIALIZE_IGNORED_MEMBERS = 0x02,
        SAVE_WRITE_ATTRIBUTES = 0x04,
        SAVE_CONCISE = 0x08,
    };

    enum LoadOptionBits
    {
        LOAD_DEFAULT = 0x00,
        LOAD_FAIL_IF_VERSIONING = 0x01, // no versioning paths: the current version is required
        LOAD_FORCED = 0x02, // the objects even when patching failed
    };

    // size 0xC
    struct LoadOptions : public hkFlags<LoadOptionBits, int>
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(LoadOptions);
        // 0x008199C0 (.h:111)
        LoadOptions(LoadOptionBits b = LOAD_DEFAULT) : hkFlags<LoadOptionBits, int>(b), m_classNameReg(nullptr), m_typeInfoReg(nullptr) {}
        LoadOptions& useClassNameRegistry(const hkClassNameRegistry* creg) { m_classNameReg = creg; return *this; }
        LoadOptions& useTypeInfoRegistry(const hkTypeInfoRegistry* treg) { m_typeInfoReg = treg; return *this; }
        const hkClassNameRegistry* getClassNameRegistry() const; // 0x00F42D90
        const hkTypeInfoRegistry* getTypeInfoRegistry() const; // 0x00F42DB0

    protected:
        const hkClassNameRegistry* m_classNameReg; // 0x04
        const hkTypeInfoRegistry* m_typeInfoReg; // 0x08
    };

    hkResource* load(hkStreamReader* streamIn, ErrorDetails* errorOut = nullptr, LoadOptions options = LOAD_DEFAULT); // 0x00F44110
    hkResource* load(const char* filename, ErrorDetails* resultOut = nullptr, LoadOptions options = LOAD_DEFAULT); // 0x00F44460
    hkResource* load(const void* buf, int buflen, ErrorDetails* resultOut = nullptr, LoadOptions options = LOAD_DEFAULT); // 0x00F444B0
    hkObjectResource* loadOnHeap(hkStreamReader* streamIn, ErrorDetails* errorOut = nullptr, LoadOptions options = LOAD_DEFAULT); // 0x00F445A0
    hkObjectResource* loadOnHeap(const char* filename, ErrorDetails* errorOut = nullptr, LoadOptions options = LOAD_DEFAULT); // 0x00F44880
    hkObjectResource* loadOnHeap(const void* buf, int buflen, ErrorDetails* errorOut = nullptr, LoadOptions options = LOAD_DEFAULT); // 0x00F448D0

    enum FormatType
    {
        FORMAT_ERROR = 0,
        FORMAT_UNKNOWN = 1,
        FORMAT_PACKFILE_BINARY = 2,
        FORMAT_PACKFILE_XML = 3,
        FORMAT_TAGFILE_BINARY = 4,
        FORMAT_TAGFILE_XML = 5,
    };

    // size 0x20
    // What detectFormat found: the format and its version, the SDK version string, the packfile layout and the
    // content predicates the file declares true. The constructor (0x00F45590) and destructor (0x00F455B0) are
    // compiler-generated.
    struct FormatDetails
    {
        typedef hkHandle<int, -1, FormatDetails> PredicateResult;

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(FormatDetails);
        bool supportsPredicates() const { return m_maxPredicateId > hkContentPredicate::NO_PREDICATE; }
        bool supportsPredicate(hkEnum<hkContentPredicate::PredicateIdEnum, hkUint16> pred) const { return pred < m_maxPredicateId; } // 0x00F44B60 (.h:253)
        PredicateResult verifiesPredicate(hkEnum<hkContentPredicate::PredicateIdEnum, hkUint16> pred) const; // 0x00F42DD0

        hkEnum<FormatType, int> m_formatType; // 0x00
        int m_formatVersion; // 0x04
        hkStringPtr m_version; // 0x08
        hkStructureLayout::LayoutRules m_layoutRules; // 0x0C
        hkEnum<hkContentPredicate::PredicateIdEnum, hkUint16> m_maxPredicateId; // 0x10
        hkArray<hkUint16, hkContainerHeapAllocator> m_truePredicates; // 0x14
    };

    hkEnum<FormatType, int> detectFormat(hkStreamReader* reader, ErrorDetails* errorOut = nullptr); // 0x00F44A10
    void detectFormat(const char* fileName, FormatDetails& details, ErrorDetails* errorOut = nullptr); // 0x00F44AA0
    void detectFormat(hkStreamReader* reader, FormatDetails& details, ErrorDetails* errorOut = nullptr); // 0x00F43C60
    hkBool32 isLoadable(hkStreamReader* sr); // 0x00F44920
}
