// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkpackfileheader.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer; the inline constructor added in SD).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"

class hkClass;
struct hkContainerHeapAllocator;
struct hkInternalClassMember;
class hkPackfileSectionHeader;
struct hkResult;
class hkStreamReader;

// size 0x40
class hkPackfileHeader
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkPackfileHeader);
    static const hkClass& staticClass(); // 0x00F72F40
    // 0x00F42A80 (inline in 2014.1 hkPackfileHeader.h): all ones but the magic, an empty version and no flags
    hkPackfileHeader()
    {
        hkString::memSet(this, -1, sizeof(*this));
        m_magic[0] = 0x57e0e057; // both byte orders read the same (byte symmetric)
        m_magic[1] = 0x10c0c010;
        m_contentsVersion[0] = 0;
        m_flags = 0; // 1 once loaded in place
    }
    hkPackfileSectionHeader* getSectionHeader(void* packfileData, int i) const; // 0x00F5C330
    const hkPackfileSectionHeader* getSectionHeader(const void* packfileData, int i) const; // 0x00F5C2D0
    hkResult readPredicates(hkStreamReader* stream, hkArray<unsigned short,hkContainerHeapAllocator>& predicates) const; // 0x00F5C340
    static hkResult readHeader(hkStreamReader* stream, hkPackfileHeader& out); // 0x00F5C220

    static const hkInternalClassMember Members[];
    int m_magic[2]; // 0x00
    int m_userTag; // 0x08
    int m_fileVersion; // 0x0C
    unsigned char m_layoutRules[4]; // 0x10
    int m_numSections; // 0x14
    int m_contentsSectionIndex; // 0x18
    int m_contentsSectionOffset; // 0x1C
    int m_contentsClassNameSectionIndex; // 0x20
    int m_contentsClassNameSectionOffset; // 0x24
    char m_contentsVersion[16]; // 0x28
    int m_flags; // 0x38
    unsigned short m_maxpredicate; // 0x3C
    unsigned short m_predicateArraySizePlusPadding; // 0x3E
};
