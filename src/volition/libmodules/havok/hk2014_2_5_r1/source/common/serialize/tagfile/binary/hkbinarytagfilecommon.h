// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/binary/hkbinarytagfilecommon.h
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD): the binary tagfile's tags, magic numbers and header, with the
// inline code of 2014.1 hkBinaryTagfileCommon.h (checked against rfg.exe).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

namespace hkBinaryTagfile
{
    enum TagType
    {
        TAG_EOF = -1,
        TAG_NONE = 0,
        TAG_FILE_INFO = 1, // followed by the version; the rest of the header depends on it
        TAG_METADATA = 2, // a class
        TAG_OBJECT = 3, // an object not referenced again
        TAG_OBJECT_REMEMBER = 4, // an object TAG_OBJECT_BACKREF may refer to (ids count the remembered objects)
        TAG_OBJECT_BACKREF = 5, // a remembered object, by id
        TAG_OBJECT_NULL = 6, // the null object (versions 0 and 1)
        TAG_FILE_END = 7,
    };

    enum
    {
        BINARY_MAGIC_0 = 0xCAB00D1E,
        BINARY_MAGIC_1 = 0xD011FACE,
    };

    // size 0x24
    // The constructor (0x00F457A0) and destructor (0x00F457C0) are compiler-generated.
    struct Header
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Header);
        bool supportsPredicates() const { return m_version >= 5; } // 0x00F42A60 (.h:63)

        unsigned int m_magic1; // 0x00
        unsigned int m_magic2; // 0x04
        int m_fileInfo; // 0x08
        int m_version; // 0x0C
        hkStringPtr m_sdk; // 0x10
        unsigned short m_numPredicates; // 0x14
        unsigned short m_maxpredicate; // 0x16
        hkArray<unsigned short,hkContainerHeapAllocator> m_truePredicates; // 0x18
    };

    // 0x00F6AB40 (.h:70): the byte order reversed
    inline unsigned int convertEndianU32(unsigned int n)
    {
        union fmtU32
        {
            hkUint8 b[4];
            hkUint32 v;
        };
        fmtU32 in, out;
        in.v = n;
        out.b[0] = in.b[3];
        out.b[1] = in.b[2];
        out.b[2] = in.b[1];
        out.b[3] = in.b[0];
        return out.v;
    }

    // 0x00F6B010 (.h:88): the magic numbers in either byte order
    inline hkBool isBinaryMagic(unsigned int number0, unsigned int number1)
    {
        return ((number0 == BINARY_MAGIC_0) && (number1 == BINARY_MAGIC_1)) ||
               ((convertEndianU32(number0) == hkUint32(BINARY_MAGIC_0)) &&
                (convertEndianU32(number1) == hkUint32(BINARY_MAGIC_1)));
    }
}
