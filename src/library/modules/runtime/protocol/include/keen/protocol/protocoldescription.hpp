// Original: library/modules/runtime/protocol/include/keen/protocol/protocoldescription.hpp
// Implementation: library/modules/runtime/protocol/sources/global/protocoldescription.cpp
//
// Protocol descriptions: the message types, their fields (C++ type, offset, bit size) and enums, generated for each
// protocol. Placement of the message description types inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct ProtocolMessageEnumMemberDescription;

    enum ProtocolMessageFieldType
    {
        ProtocolMessageFieldType_Bool = 0,
        ProtocolMessageFieldType_Float32 = 1,
        ProtocolMessageFieldType_Float64 = 2,
        ProtocolMessageFieldType_SInt8 = 3,
        ProtocolMessageFieldType_UInt8 = 4,
        ProtocolMessageFieldType_SInt16 = 5,
        ProtocolMessageFieldType_UInt16 = 6,
        ProtocolMessageFieldType_SInt32 = 7,
        ProtocolMessageFieldType_UInt32 = 8,
        ProtocolMessageFieldType_SInt64 = 9,
        ProtocolMessageFieldType_UInt64 = 10,
        ProtocolMessageFieldType_String = 11,
        ProtocolMessageFieldType_Message = 12,
        ProtocolMessageFieldType_Enum = 13,
        ProtocolMessageFieldType_QuantizedSInt = 14,
        ProtocolMessageFieldType_QuantizedUInt = 15,
        ProtocolMessageFieldType_VarUInt32 = 16,
        ProtocolMessageFieldType_VarSInt32 = 17,
        ProtocolMessageFieldType_Count = 18,
    };

    enum ProtocolMessageFieldCppType
    {
        ProtocolMessageFieldCppType_Bool = 0,
        ProtocolMessageFieldCppType_Float32 = 1,
        ProtocolMessageFieldCppType_Float64 = 2,
        ProtocolMessageFieldCppType_SInt8 = 3,
        ProtocolMessageFieldCppType_UInt8 = 4,
        ProtocolMessageFieldCppType_SInt16 = 5,
        ProtocolMessageFieldCppType_UInt16 = 6,
        ProtocolMessageFieldCppType_SInt32 = 7,
        ProtocolMessageFieldCppType_UInt32 = 8,
        ProtocolMessageFieldCppType_SInt64 = 9,
        ProtocolMessageFieldCppType_UInt64 = 10,
        ProtocolMessageFieldCppType_Message = 11,
        ProtocolMessageFieldCppType_Enum = 12,
        ProtocolMessageFieldCppType_String = 13,
        ProtocolMessageFieldCppType_Count = 14,
    };

    // size 0x8
    struct ProtocolMessageTypeDescription
    {
        const char* pName; // 0x00
        uint32 cppTypeSize; // 0x04
    };

    // size 0x14
    struct ProtocolMessageEnumDescription : public ProtocolMessageTypeDescription
    {
        ProtocolMessageEnumDescription(const char*, const ProtocolMessageEnumMemberDescription*, uint32, uint32, uint32);

        const ProtocolMessageEnumMemberDescription* pMember; // 0x08
        uint32 memberCount; // 0x0C
        uint32 minBitCount; // 0x10
    };

    // size 0x24
    struct ProtocolMessageFieldDescription
    {
        const char* pName; // 0x00
        ProtocolMessageFieldType wireType; // 0x04
        ProtocolMessageFieldCppType cppType; // 0x08
        uint32 dataOffsetInBytes; // 0x0C
        uint32 sizeOffsetInBytes; // 0x10
        uint32 minRepeatCount; // 0x14
        uint32 maxRepeatCount; // 0x18
        uint32 quantizationBitCount; // 0x1C
        const ProtocolMessageTypeDescription* pTypeDescription; // 0x20
    };

    // size 0x10
    struct ProtocolMessageDescription : public ProtocolMessageTypeDescription
    {
        ProtocolMessageDescription(const char*, const ProtocolMessageFieldDescription*, uint32, uint32);

        const ProtocolMessageFieldDescription* pFieldDescriptions; // 0x08
        uint32 fieldDescriptionCount; // 0x0C
    };

    // size 0x10
    struct ProtocolDescription
    {
        const ProtocolMessageDescription* pProtocolMessageDescriptions; // 0x00
        uint32 messageDescriptionCount; // 0x04
        const ProtocolMessageEnumDescription* pEnumDescriptions; // 0x08
        uint32 enumDescriptionCount; // 0x0C
    };

    namespace protocol
    {
        bool isMessageFieldDescriptionValid(const ProtocolMessageFieldDescription& fieldDescription); // 0x00CD8F20 (:18)
        bool isMessageContentValid(const void* pMessage, const ProtocolMessageDescription& messageDescription); // 0x00CDBD90 (:159)
        uint32 getMessageFormatCrc32(const ProtocolMessageDescription& messageDescription); // 0x00CDC2A0 (:370)
        uint32 getProtocolFormatCrc32(const ProtocolDescription& protocolDescription); // 0x00CDC2B0 (:375)
    }
}
