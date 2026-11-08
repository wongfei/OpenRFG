// Original: library/modules/runtime/linkedresources/source/linkedresources.cpp
//
// The resources linked into the executable (stock shaders, ...) form a compressed keen pak, mounted as a read-only
// memory file and then as a pak file system.
//
// The pak bytes (s_linkedResourcesPakFile) are generated at build time from the user's rfg.exe by
// tools/linkedresources/extract.py (game data, not committed).

#include "keen/linkedresources/linkedresources.hpp"
#include "keen/base/filesystem.hpp"
#include "keen/base/trace.hpp"

namespace keen
{
    // Magic number: the pak size in rfg.exe.
    extern const uint8 s_linkedResourcesPakFile[0x3d05b];

    static uint32 s_memoryFileMountHandle = 0xffffffffu;
    static uint32 s_pakFileMountHandle = 0xffffffffu;

    // 0x00402000 (:11)
    IoError mountLinkedResources(FileSystem* pFileSystem, const char* pPath)
    {
        const IoResult<uint32> memoryFile = file::mountReadOnlyMemoryFile(pFileSystem, "/linkedresourcespakfile", s_linkedResourcesPakFile, sizeof(s_linkedResourcesPakFile));
        if (memoryFile.error != IoError_Ok)
        {
            debug::nativePrintArgs("Could not mount LinkedResourcesMemoryFile\n");
            return memoryFile.error;
        }
        const IoResult<uint32> pakFile = file::mountPakFile(pFileSystem, pPath, "/linkedresourcespakfile");
        if (pakFile.error != IoError_Ok)
        {
            debug::nativePrintArgs("Could not mount LinkedResourcesPakFile\n");
            file::unmount(pFileSystem, memoryFile.result);
            return pakFile.error;
        }
        s_memoryFileMountHandle = memoryFile.result;
        s_pakFileMountHandle = pakFile.result;
        return IoError_Ok;
    }

    // 0x00401F70 (:41)
    IoError unmountLinkedResources(FileSystem* pFileSystem)
    {
        IoError error = file::unmount(pFileSystem, s_pakFileMountHandle);
        if (error != IoError_Ok)
        {
            debug::nativePrintArgs("Could not unmount LinkedResourcesPakFile\n");
            return error;
        }
        error = file::unmount(pFileSystem, s_memoryFileMountHandle);
        if (error != IoError_Ok)
        {
            debug::nativePrintArgs("Could not unmount LinkedResourcesMemoryFile\n");
            return error;
        }
        s_memoryFileMountHandle = 0xffffffffu;
        s_pakFileMountHandle = 0xffffffffu;
        return IoError_Ok;
    }
}
