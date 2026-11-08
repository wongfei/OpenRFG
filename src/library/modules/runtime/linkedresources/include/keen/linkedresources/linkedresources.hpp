// Original: library/modules/runtime/linkedresources/include/keen/linkedresources/linkedresources.hpp
// Implementation: library/modules/runtime/linkedresources/source/linkedresources.cpp
//
// Mounting of resources linked into the executable as a file system.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/ioresult.hpp"

namespace keen
{
    struct FileSystem;

    IoError mountLinkedResources(FileSystem* pFileSystem, const char* pPath); // 0x00402000 (:11)
    IoError unmountLinkedResources(FileSystem* pFileSystem); // 0x00401F70 (:41)
}
