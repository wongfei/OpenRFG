// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkpackfilesectionheader.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkpackfilesectionheader.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE): a packfile section (data, then its local, global, virtual
// (finish) fixups, exports and imports, each block ending where the next starts). Layout from rfg.pdb; the inline
// members as the 2014.1 SDK's hkPackfileSectionHeader.h, checked against rfg.exe.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkresource.h"

class hkClass;
struct hkContainerHeapAllocator;
struct hkInternalClassMember;

// size 0x40
class hkPackfileSectionHeader
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkPackfileSectionHeader);
    static const hkClass& staticClass(); // 0x00F72F20
    // 0x00F59770 (.h:34): all zero (2014.1 clears only sizeof(this) bytes; 2014.2.5 the whole header)
    hkPackfileSectionHeader()
    {
        hkString::memSet(this, 0, sizeof(*this));
    }
    void* getStart(void* absoluteStart) { return static_cast<char*>(absoluteStart) + m_absoluteDataStart; }
    int getDataSize() const { return m_localFixupsOffset; } // 0x00F42A50 (.h:46)
    int getLocalSize() const { return m_globalFixupsOffset - m_localFixupsOffset; } // 0x00F63CF0 (.h:51)
    int getGlobalSize() const { return m_virtualFixupsOffset - m_globalFixupsOffset; } // 0x00F63D00 (.h:56)
    int getFinishSize() const { return m_exportsOffset - m_virtualFixupsOffset; } // 0x00F63D10 (.h:61)
    int getExportsSize() const { return m_importsOffset - m_exportsOffset; } // 0x00F63D20 (.h:66)
    int getImportsSize() const { return m_endOffset - m_importsOffset; } // 0x00F63D30 (.h:71)
    void getExports(const void* sectionBegin, hkArray<hkResource::Export,hkContainerHeapAllocator>& exportsOut) const; // 0x00F73340
    void getImports(const void* sectionBegin, hkArray<hkResource::Import,hkContainerHeapAllocator>& importsOut) const; // 0x00F733F0

    static const hkInternalClassMember Members[];
    char m_sectionTag[19]; // 0x00
    char m_nullByte; // 0x13
    int m_absoluteDataStart; // 0x14
    int m_localFixupsOffset; // 0x18
    int m_globalFixupsOffset; // 0x1C
    int m_virtualFixupsOffset; // 0x20
    int m_exportsOffset; // 0x24
    int m_importsOffset; // 0x28
    int m_endOffset; // 0x2C
    int m_pad[4]; // 0x30
};
