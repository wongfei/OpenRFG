// Original: library/modules/runtime/base/include/keen/base/deltacompression.hpp
// Implementation: library/modules/runtime/base/sources/global/deltacompression.cpp
//
// Delta compression of state buffers against a reference (network state sync).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    bool compressDeltaState(uint8* pData, uint32* pSize, uint32 capacity, const uint8* pBaseState, const uint8* pTargetState, uint32 stateSize); // 0x00BEF880 (:62)
    bool uncompressDeltaState(uint8* pTargetState, const uint8* pBaseState, uint32 stateSize, const uint8* pStateDelta, uint32 remainingDataSize); // 0x00BF73C0 (:131)
}
