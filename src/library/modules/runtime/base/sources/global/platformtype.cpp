// Original: library/modules/runtime/base/sources/global/platformtype.cpp
//
// Partial: getHostPlatform. Not ported: the platform names, build tags and pointer sizes.

#include "keen/base/platformtype.hpp"

namespace keen
{
    // 0x00BF1170 (:9)
    PlatformType getHostPlatform()
    {
        return PlatformType_Win32;
    }
}
