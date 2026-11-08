// Original: library/modules/runtime/resources/sources/synchronousresourcereader.hpp
// Implementation: library/modules/runtime/resources/sources/synchronousresourcereader.cpp
//
// SynchronousResourceReader, loads resources synchronously through the KrofManager.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class File;
    class KrofManager;
    struct LoadFailReport;
    class Resource;

    // size 0x4
    class SynchronousResourceReader
    {
    public:
        void create(KrofManager* pKrofManager); // 0x00CDCF80 (:16)
        void destroy(); // 0x00CDCF90 (:21)
        Resource* getResource(uint32 type, const char* pResourcename, uint32 resourceContextIndex, uint8 tag, LoadFailReport* pLoadFailReport, const char* pKrofFilename, bool resolveReferences); // 0x00CE2140 (:65)
        bool releaseResource(Resource* pResource, uint32 type); // 0x00CE0710 (:185)
        bool reloadResource(File& file, uint32 type, const char* pResourcename); // 0x00CE1AB0 (:26)
    private:
        bool loadUnresolvedResources(uint32 resourceContextIndex, LoadFailReport* pLoadFailReport, uint8 tag); // 0x00CE18B0 (:195)

        KrofManager* m_pKrofManager; // 0x00
    };
}
