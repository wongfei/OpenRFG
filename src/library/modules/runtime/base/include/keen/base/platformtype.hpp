// Original: library/modules/runtime/base/include/keen/base/platformtype.hpp
// Implementation: library/modules/runtime/base/sources/global/platformtype.cpp
//
// PlatformType, the target platforms and their build tags and pointer sizes.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum PlatformType
    {
        PlatformType_Invalid = 255,
        PlatformType_Ngc = 1,
        PlatformType_Nds = 2,
        PlatformType_Wii = 3,
        PlatformType_Ctr = 4,
        PlatformType_WiiU = 5,
        PlatformType_Ps2 = 6,
        PlatformType_Ps3 = 7,
        PlatformType_Psp = 8,
        PlatformType_XB360 = 9,
        PlatformType_Win32 = 10,
        PlatformType_Linux = 11,
        PlatformType_Osx = 12,
        PlatformType_IOS = 13,
        PlatformType_Ps4 = 14,
        PlatformType_Android = 15,
        PlatformType_XBone = 16,
        PlatformType_PsV = 17,
        PlatformType_Switch = 18,
    };

    // size 0x10
    struct PlatformCppTypeConfiguration
    {
        PlatformCppTypeConfiguration();

        uint32 intSize; // 0x00
        uint32 pointerSize; // 0x04
        uint32 mathTypeAlignment; // 0x08
        bool padBaseTypes; // 0x0C
    };

    PlatformType getHostPlatform(); // 0x00BF1170 (:9)
    const char* getPlatformTypeName(PlatformType platformType); // 0x00BF1180 (:44)
    PlatformType getPlatformTypeByBuildTag(const char* pBuildTag); // 0x00BF9E70 (:104)
    const char* getBuildTagByPlatformType(PlatformType platformType); // 0x00BF1250 (:180)
    uint32 getTargetPointerSize(PlatformType platformType); // 0x00BF1320 (:240)
}
