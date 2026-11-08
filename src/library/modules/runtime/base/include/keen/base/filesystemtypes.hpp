// Original: library/modules/runtime/base/include/keen/base/filesystemtypes.hpp
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // Capacities for file::createFileSystem (filled by GameFramework::createFileSystem from GameFrameworkParameters).
    // size 0x28
    struct FileSystemCreationParameters
    {
        FileSystemCreationParameters(); // 0x00C0E010

        uint32 maxDeviceCount; // 0x00
        uint32 maxMountPoints; // 0x04
        uint32 maxNativeStreamCount; // 0x08
        uint32 maxMemoryStreamCount; // 0x0C
        uint32 maxMemoryFileCount; // 0x10
        uint32 maxPakStreamCount; // 0x14
        uint32 maxAliasStreamCount; // 0x18
        uint32 maxPakFileSize; // 0x1C
        uint32 nativeThreadBufferSize; // 0x20
        bool enableDefaultMount; // 0x24
        bool useReadThreads; // 0x25
    };

    // size 0x118
    struct FileStatus
    {
        enum
        {
            ValueMask_LastModificationTime = 1,
            ValueMask_Size = 2,
            ValueMask_NativeFileName = 4,
            ValueMask_Writeable = 8,
            ValueMask_Exists = 16,
            ValueMask_Directory = 32,
            ValueMask_All = 63,
        };

        int lastModificationTime; // 0x00
        uint64 size; // 0x08
        char nativeFileName[260]; // 0x10
        bool isWriteable; // 0x114
        bool isDirectory; // 0x115
    };
}
