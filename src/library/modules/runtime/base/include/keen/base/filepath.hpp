// Original: library/modules/runtime/base/include/keen/base/filepath.hpp
// Implementation: library/modules/runtime/base/sources/global/filepath.cpp
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class StringBuilder;

    // Path split into prefix / directory / file name / extension; the combined forms are built lazily
    // into m_completePath (m_completePathState records which form it currently holds).
    // size 0x448
    class FilePath
    {
    public:
        enum
        {
            CompletePathCapacity = 260,
            PrefixCapacity = 64,
            DirectoryCapacity = 256,
            FileNameCapacity = 256,
            ExtensionCapacity = 256,
        };

        enum CompletePathState
        {
            CompletePathState_Invalid = 0,
            CompletePathState_CompletePath = 1,
            CompletePathState_DirectoryWithPrefix = 2,
            CompletePathState_FileNameWithExtension = 3,
            CompletePathState_LastDirectoryPart = 4,
            CompletePathState_Count = 5,
        };

        FilePath(const char* pPath, bool pathIsOnlyDirectory); // 0x00C07E90
        FilePath(); // 0x00BF7590
        void clear(); // 0x00BEFC10
        bool isAbsolute() const; // 0x00BEFC40
        bool hasDirectoryPrefix(const char* pDirectory) const; // 0x00BF75C0
        FilePath& createCombinedPath(const char* pPath0, const char* pPath1); // 0x00C07EB0
        FilePath& createRelativePath(const char* pSource, const char* pTarget); // 0x00C07FA0
        FilePath& popDirectory(int levelCount); // 0x00C028F0
        FilePath& pushDirectory(const char* pDirectory); // 0x00C04A20
        FilePath& setCompletePath(const char* pPath, bool pathIsOnlyDirectory); // 0x00C04BB0
        FilePath& formatDirectoryPath(const char* pFormat, ...); // 0x00C08150
        FilePath& setPrefix(const char* pPrefix); // 0x00BF7630
        FilePath& setDirectory(const char* pDirectory); // 0x00C04D70
        FilePath& setFileName(const char* pFileName); // 0x00BF7680
        FilePath& setExtension(const char* pExtension); // 0x00BF76C0
        FilePath& setFileNameWithExtension(const char* pFileName); // 0x00BF7710
        FilePath& convertToLowerCase(); // 0x00BF77B0
        const char* getCompletePath() const; // 0x00C04D90
        const char* getPrefix() const; // 0x00BEFC80
        const char* getDirectory() const; // 0x00BEFC90
        const char* getDirectoryWithPrefix() const; // 0x00C04DD0
        const char* getFileName() const; // 0x00BEFCA0
        const char* getExtension() const; // 0x00BEFCB0
        const char* getFileNameWithExtension() const; // 0x00C04DB0
        const char* getLastDirectoryPart() const; // 0x00BFD030
        bool getNextDirectoryPart(String* pPart) const; // 0x00BF78C0

    private:
        void buildCompletePath() const; // 0x00C02970
        void buildFileNameWithExtension() const; // 0x00C02AB0
        void buildDirectoryWithPrefix() const; // 0x00C02B30
        void buildLastDirectoryPart() const; // 0x00BF7860
        void normalizeDirectory(char* pTarget, uint32 targetCapacity, const char* pSource); // 0x00C02BB0
        void removeLastDirectoryPart(StringBuilder& directoryString); // 0x00BFD050

        mutable char m_completePath[260]; // 0x00, built on demand by the const getters
        mutable CompletePathState m_completePathState; // 0x104
        char m_prefix[64]; // 0x108
        char m_directory[256]; // 0x148
        char m_fileName[256]; // 0x248
        char m_extension[256]; // 0x348
    };
}
