// Original: library/modules/runtime/resources/sources/global/resource.cpp
//
// Partial (SFX.md SX1): the Resource constructor, destructor and tag the keen sound resources build on. The resource
// manager itself is not ported.

#include "keen/resources/resource.hpp"

namespace keen
{
    // 0x00CDCBB0 (:26)
    Resource::Resource(uint32 key)
    {
        m_key = key;
        m_flags = 0;
        m_tag = InvalidTag;
        m_unused = 0;
    }

    // 0x00CDCBD0 (:42)
    Resource::~Resource()
    {
    }

    // 0x00C32850 (resource.hpp:93)
    uint8 Resource::getTag() const
    {
        return m_tag;
    }
}
