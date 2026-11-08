// Original: library/modules/runtime/base/include/keen/base/ioresult.hpp
// Implementation: library/modules/runtime/base/sources/global/ioresult.cpp
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum IoError
    {
        IoError_Ok = 0,
        IoError_Generic = 1,
        IoError_CompressionError = 2,
        IoError_DecompressionError = 3,
        IoError_ReadError = 4,
        IoError_WriteError = 5,
        IoError_NotImplemented = 6,
        IoError_EofReached = 7,
        IoError_InvalidState = 8,
        IoError_FileNotFound = 9,
        IoError_InvalidPosition = 10,
        IoError_NoPermission = 11,
        IoError_NoConnection = 12,
        IoError_TimeOut = 13,
        IoError_InvalidArgument = 14,
        IoError_OutOfMemory = 15,
        IoError_AlreadyExists = 16,
        IoError_NotSupported = 17,
        IoError_Count = 18,
    };

    // Value plus error code; returned by value from every file API.
    // size 0x8 for 4-byte T, 0x10 for uint64
    template<typename T>
    struct IoResult
    {
        bool isOk() const; // <uint32> 0x00401FE0

        T result; // 0x00
        IoError error;
    };

    template<typename T>
    IoResult<T> createIoResult(T result, IoError error); // <uint32> 0x00BF35D0

    const char* getIoErrorString(IoError error); // 0x00BF0BF0
}
