// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkpackfileheader.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkpackfileheader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkpackfilesectionheader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/hkstreamreader.h"

// 0x00F5C220 (:17): the header is read whole, then the magic checked against a default header's
hkResult hkPackfileHeader::readHeader(hkStreamReader* stream, hkPackfileHeader& out)
{
    if (stream->read(&out, sizeof(hkPackfileHeader)) < static_cast<int>(sizeof(hkPackfileHeader)))
    {
        return HK_FAILURE;
    }
    hkPackfileHeader magic;
    if (out.m_magic[0] != magic.m_magic[0] || out.m_magic[1] != magic.m_magic[1])
    {
        return HK_FAILURE;
    }
    // 2013.2 version 11 files left the predicate fields unset (all ones): read them as no predicates
    if (hkString::strNcmp(out.m_contentsVersion, "hk_2013.2", 9) == 0 && out.m_fileVersion == 11 &&
        out.m_predicateArraySizePlusPadding == 0xFFFF && out.m_maxpredicate == 0xFFFF)
    {
        out.m_maxpredicate = 0;
        out.m_predicateArraySizePlusPadding = 0;
    }
    return HK_SUCCESS;
}

// 0x00F5C2D0 (:40): the section headers follow the file header and the predicate array (file versions 10 and
// 11); 0x30 bytes each before version 11, 0x40 in it. The header read is packfileData's, not this one.
const hkPackfileSectionHeader* hkPackfileHeader::getSectionHeader(const void* packfileData, int i) const
{
    const hkPackfileHeader* header = static_cast<const hkPackfileHeader*>(packfileData);
    if (header->m_numSections == 0)
    {
        return nullptr;
    }
    const int predicateSize = (header->m_fileVersion >= 10 && header->m_fileVersion <= 11) ? header->m_predicateArraySizePlusPadding : 0;
    int sectionHeaderSize;
    if (header->m_fileVersion < 11)
    {
        sectionHeaderSize = 0x30; // the pre-11 section header has no m_pad
    }
    else
    {
        sectionHeaderSize = header->m_fileVersion == 11 ? static_cast<int>(sizeof(hkPackfileSectionHeader)) : 0; // unknown versions: all at the first
    }
    return reinterpret_cast<const hkPackfileSectionHeader*>(static_cast<const char*>(packfileData) + sizeof(hkPackfileHeader) + predicateSize + sectionHeaderSize * i);
}

// 0x00F5C330 (:90)
hkPackfileSectionHeader* hkPackfileHeader::getSectionHeader(void* packfileData, int i) const
{
    return const_cast<hkPackfileSectionHeader*>(getSectionHeader(static_cast<const void*>(packfileData), i));
}

// 0x00F5C340 (:95): the predicate array (after the header) up to its first zero; none before file version 10.
// The read's result is not checked.
hkResult hkPackfileHeader::readPredicates(hkStreamReader* stream, hkArray<unsigned short,hkContainerHeapAllocator>& predicates) const
{
    if (m_fileVersion < 10)
    {
        return HK_FAILURE;
    }
    if (m_predicateArraySizePlusPadding != 0)
    {
        const int maxPredicates = m_predicateArraySizePlusPadding / 2;
        predicates.reserve(maxPredicates);
        unsigned short* p = predicates.begin();
        stream->read(p, m_predicateArraySizePlusPadding);
        int n = 0;
        while (n < maxPredicates && p[n] != 0)
        {
            n++;
        }
        predicates.setSize(n);
    }
    return HK_SUCCESS;
}
