// Original: library/modules/runtime/protocol/sources/global/protocolhelpers.hpp
// Implementation: library/modules/runtime/protocol/sources/global/protocolhelpers.cpp
//
// Helpers of the protocol serializers (field access and sizes).
#pragma once

#include "keen/base/types.hpp"
#include "keen/protocol/protocoldescription.hpp"

namespace keen
{
    namespace protocol
    {
        bool isRequiredField(const ProtocolMessageFieldDescription& fieldDescription); // 0x00CD8DC0 (.h:18)
        bool isOptionalField(const ProtocolMessageFieldDescription& fieldDescription); // 0x00CD8DD0 (.h:23)
        bool isRepeatedField(const ProtocolMessageFieldDescription& fieldDescription); // 0x00CD8DF0 (.h:28)
        uint32 getMessageFieldCppTypeByteSize(ProtocolMessageFieldCppType cppType, const ProtocolMessageTypeDescription* pTypeDescription); // 0x00CD92F0 (:7)
        uint32 getElementCount(const void* pMessage, const ProtocolMessageFieldDescription& fieldDescription); // 0x00CD9360 (:47)
    }
}
