// Original: library/modules/runtime/base/include/keen/base/resourceconfiguration.hpp
// Implementation: library/modules/runtime/base/sources/global/resourceconfiguration.cpp
//
// Resource configuration: the platform APIs, SKUs, endianness, pointer size and alignment rules resources are built
// for, and their compatibility check.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/platformtype.hpp"

namespace keen
{
    enum ResourceConfigurationApiId
    {
        ResourceConfigurationApiId_Generic = 0,
        ResourceConfigurationApiId_Gcm = 1,
        ResourceConfigurationApiId_Edge = 2,
        ResourceConfigurationApiId_Direct3D11 = 3,
        ResourceConfigurationApiId_Direct3D9 = 4,
        ResourceConfigurationApiId_XGraphics = 5,
        ResourceConfigurationApiId_Gnm = 6,
        ResourceConfigurationApiId_GX2 = 7,
        ResourceConfigurationApiId_XAudio2 = 8,
        ResourceConfigurationApiId_MultiStream = 9,
        ResourceConfigurationApiId_Bink = 10,
        ResourceConfigurationApiId_AX = 11,
        ResourceConfigurationApiId_OpenGL_ES = 12,
        ResourceConfigurationApiId_PhysX_PC = 13,
        ResourceConfigurationApiId_PhysX_PlayStation3 = 14,
        ResourceConfigurationApiId_PhysX_Xenon = 15,
        ResourceConfigurationApiId_AJM = 16,
        ResourceConfigurationApiId_PICA = 17,
        ResourceConfigurationApiId_OpenGL = 18,
        ResourceConfigurationApiId_OpenAL = 19,
        ResourceConfigurationApiId_XBone = 20,
        ResourceConfigurationApiId_Gxm = 21,
        ResourceConfigurationApiId_Nvn = 22,
        ResourceConfigurationApiId_Atk = 23,
        ResourceConfigurationApiId_Count = 24,
    };

    enum ResourceConfigurationSkuId
    {
        ResourceConfigurationSkuId_Count = 16,
    };

    enum ResourceConfigurationEndianness
    {
        ResourceConfigurationEndianness_Little = 0,
        ResourceConfigurationEndianness_Big = 1,
        ResourceConfigurationEndianness_Count = 2,
    };

    enum ResourceConfigurationPointerSize
    {
        ResourceConfigurationPointerSize_32 = 0,
        ResourceConfigurationPointerSize_64 = 1,
        ResourceConfigurationPointerSize_Count = 2,
    };

    enum ResourceConfigurationPadBaseType
    {
        ResourceConfigurationPadBaseType_true = 0,
        ResourceConfigurationPadBaseType_false = 1,
        ResourceConfigurationPadBaseType_Count = 2,
    };

    enum ResourceConfigurationMathTypeAlignment
    {
        ResourceConfigurationMathTypeAlignment_4Byte = 0,
        ResourceConfigurationMathTypeAlignment_16Byte = 1,
        ResourceConfigurationMathTypeAlignment_Count = 2,
    };

    // size 0x10
    struct ResourceConfigurationCompatibility
    {
        ResourceConfigurationCompatibility(); // 0x00BF15A0 (:149)
        bool setForPlatformType(PlatformType platform); // 0x00C00360 (:156)
        void setHardwareConfiguration(ResourceConfigurationPointerSize pointerSize, ResourceConfigurationEndianness endianness, ResourceConfigurationMathTypeAlignment mathTypeAlignment, ResourceConfigurationPadBaseType padBaseType); // 0x00BF1650 (:296)
        void setHostHardwareConfiguration(); // 0x00BF1670 (:301)
        void enablePlatformApis(PlatformType platform); // 0x00BFA1B0 (:204)
        void enableApiSupport(ResourceConfigurationApiId apiId); // 0x00BF15C0 (:274)
        bool isApiSupported(ResourceConfigurationApiId apiId) const; // 0x00BF15E0 (:280)
        void enableSkuSupport(uint32 skuId); // 0x00BF1610 (:285)
        bool isSkuSupported(uint32 skuId) const; // 0x00BF1630 (:291)
        bool isResourceConfigurationCompatible(uint16 configuration) const; // 0x00BF1680 (:324)
        void trace() const; // 0x00BFA0D0 (:124)

        uint64 supportedApiMask; // 0x00
        uint16 supportedSkuMask; // 0x08
        uint16 supportedHardwareConfiguration; // 0x0A
    };

    ResourceConfigurationPadBaseType getHostResourceConfigurationPadBaseType(); // 0x00BF1110 (.h:133)
    uint16 createResourceConfiguration(ResourceConfigurationApiId apiId, ResourceConfigurationEndianness endianness, ResourceConfigurationPointerSize pointerSize, ResourceConfigurationMathTypeAlignment mathTypeAlignment, ResourceConfigurationPadBaseType padBaseTypes, uint32 skuId); // 0x00BF1120 (.h:142)
    ResourceConfigurationApiId getResourceConfigurationApiId(uint16 configuration); // 0x00BF1150 (.h:167)
    ResourceConfigurationEndianness getResourceConfigurationEndianness(uint16 configuration); // 0x00CDC490 (.h:172)
    uint32 getResourceConfigurationSkuId(uint16 configuration); // 0x00BF1160 (.h:192)
    const char* getResourceConfigurationApiIdString(ResourceConfigurationApiId apiId); // 0x00BF1500 (:64)
    const char* getResourceConfigurationEndiannessString(ResourceConfigurationEndianness endianness); // 0x00BF1520 (:76)
    const char* getResourceConfigurationPointerSizeString(ResourceConfigurationPointerSize pointerSize); // 0x00BF1540 (:88)
    const char* getResourceConfigurationMathTypeAlignmentString(ResourceConfigurationMathTypeAlignment mathTypeAlignment); // 0x00BF1560 (:100)
    const char* getResourceConfigurationPadBaseTypeString(ResourceConfigurationPadBaseType padBasetype); // 0x00BF1580 (:112)
    bool fillPlatformCppTypeConfiguration(PlatformCppTypeConfiguration* pTarget, ResourceConfigurationPointerSize pointerSize, ResourceConfigurationMathTypeAlignment mathTypeAlignment, ResourceConfigurationPadBaseType padBaseType); // 0x00BF16E0 (:345)
}
