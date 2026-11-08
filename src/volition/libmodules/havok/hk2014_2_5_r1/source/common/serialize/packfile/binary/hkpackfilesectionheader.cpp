// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkpackfilesectionheader.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/binary/hkpackfilesectionheader.h"

namespace
{
    // 0x00F734A0 (:15)
    template<typename T>
    T extractAndAdvance(const void* p, int& off)
    {
        const T* t = reinterpret_cast<const T*>(static_cast<const char*>(p) + off);
        off += sizeof(T);
        return *t;
    }

    // 0x00F73310 (:22): the string and its terminator, padded to 4 bytes
    const char* extractAndAdvanceString(const void* p, int& off)
    {
        const char* s = static_cast<const char*>(p) + off;
        int len = hkString::strLen(s);
        do
        {
            len++;
        } while ((len & 3) != 0);
        off += len;
        return s;
    }
}

// 0x00F73340 (:36): {offset in the section, name} records; -1 offsets are not skipped (the SDK's check is an
// assert, compiled out)
void hkPackfileSectionHeader::getExports(const void* sectionBegin, hkArray<hkResource::Export,hkContainerHeapAllocator>& exportsOut) const
{
    const void* exports = static_cast<const char*>(sectionBegin) + m_exportsOffset;
    int off = 0;
    while (off < getExportsSize())
    {
        int objectOffset = extractAndAdvance<int>(exports, off);
        const char* name = extractAndAdvanceString(exports, off);
        hkResource::Export& e = exportsOut.expandOne();
        e.name = name;
        e.data = const_cast<char*>(static_cast<const char*>(sectionBegin)) + objectOffset;
    }
}

// 0x00F733F0 (:54): {location offset in the section, name} records
void hkPackfileSectionHeader::getImports(const void* sectionBegin, hkArray<hkResource::Import,hkContainerHeapAllocator>& importsOut) const
{
    const void* imports = static_cast<const char*>(sectionBegin) + m_importsOffset;
    int off = 0;
    while (off < getImportsSize())
    {
        int locationOffset = extractAndAdvance<int>(imports, off);
        const char* name = extractAndAdvanceString(imports, off);
        hkResource::Import& imp = importsOut.expandOne();
        imp.name = name;
        imp.location = reinterpret_cast<void**>(const_cast<char*>(static_cast<const char*>(sectionBegin)) + locationOffset);
    }
}
