// Original: library/modules/runtime/base/include/keen/base/patchdata.hpp
// Implementation: library/modules/runtime/base/sources/global/patchdata.cpp
//
// Pointer patching of loaded binary data (relative offsets to pointers, with endian conversion).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    void patchData(void* pSourceData, uint32 size, uint32* pPatchTable); // 0x00BF1040 (:12)
    void patchDataDifferentEndianness(void* pSourceData, uint32 size, uint32* pPatchTable); // 0x00BF1070 (:41)
}
