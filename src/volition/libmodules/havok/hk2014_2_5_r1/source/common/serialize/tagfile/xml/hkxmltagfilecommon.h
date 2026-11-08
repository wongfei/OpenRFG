// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/xml/hkxmltagfilecommon.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); inline members added by hand
// (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"

namespace hkXmlTagfile
{
    // size 0x18
    // The constructor (0x00F45810) and destructor (0x00F45830) are compiler-generated.
    struct Header
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Header);
        // 0x00F42A70 (hkxmltagfilecommon.h:22): predicates from version 2 on
        bool supportsPredicates() const { return m_version > 1; }

        int m_version; // 0x00
        hkStringPtr m_sdkVersion; // 0x04
        int m_maxpredicate; // 0x08
        hkArray<unsigned short,hkContainerHeapAllocator> m_truePredicates; // 0x0C
    };
}
