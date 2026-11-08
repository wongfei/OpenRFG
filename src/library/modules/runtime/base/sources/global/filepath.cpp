// Original: library/modules/runtime/base/sources/global/filepath.cpp
//
// FilePath keeps a path split into prefix ("c:", "//server"), directory (normalized: '/' separators, "./" and "../"
// resolved, always ending in '/' unless empty), file name and extension (with its dot). The combined strings are built
// on demand into m_completePath; m_completePathState says which one it currently holds.

#include "keen/base/filepath.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringbuilder.hpp"

#include <stdarg.h>

namespace keen
{
    namespace
    {
        const char* FilePath_SourcePathSeparatorChars = "/\\"; // 0x016EBEFC
        const char* FilePath_CurrentDirectoryName = "./"; // 0x016EBF00
        const char* FilePath_PreviousDirectoryName = "../"; // 0x016EBF04

        // 0x00BEFBD0, LTCG (pStringABegin ECX, pStringB EAX, pStringAEnd EDI): true when [begin, end) and B agree up to
        // the end of the shorter one.
        bool isStringPrefixEqual(const char* pStringABegin, const char* pStringAEnd, const char* pStringB)
        {
            if (pStringABegin == nullptr)
            {
                return pStringB == nullptr;
            }
            if (pStringB == nullptr)
            {
                return false;
            }
            for (;;)
            {
                if (pStringABegin == pStringAEnd)
                {
                    return true;
                }
                const char b = *pStringB;
                if (*pStringABegin != b)
                {
                    return false;
                }
                ++pStringB;
                if (*pStringABegin == '\0' || b == '\0')
                {
                    return true;
                }
                ++pStringABegin;
            }
        }

        // Inlined in several functions: drive prefix ("c:"), network prefix ("//"), or a rooted directory.
        bool isAbsolutePath(const char* pPrefix, const char* pDirectory)
        {
            return (pPrefix[0] != '\0' && pPrefix[1] == ':' && pPrefix[2] == '\0') || (pPrefix[0] == '/' && pPrefix[1] == '/') || pDirectory[0] == '/' || pDirectory[0] == ':';
        }

        // Inlined: appends pSource while capacity lasts (capacity becomes 0 on truncation).
        void appendPart(char** ppDest, uint32* pCapacity, const char* pSource)
        {
            if (pSource != nullptr && *pSource != '\0' && *pCapacity != 0)
            {
                const uint32 length = copyString(*ppDest, *pCapacity, pSource);
                if (length < *pCapacity)
                {
                    *pCapacity -= length;
                    *ppDest += length;
                }
                else
                {
                    *pCapacity = 0;
                }
            }
        }
    }

    // 0x00BF7590
    FilePath::FilePath()
    {
        clear();
    }

    // 0x00C07E90
    FilePath::FilePath(const char* pPath, bool pathIsOnlyDirectory)
    {
        setCompletePath(pPath, pathIsOnlyDirectory);
    }

    // 0x00BEFC10
    void FilePath::clear()
    {
        m_completePath[0] = '\0';
        m_completePathState = CompletePathState_Invalid;
        m_prefix[0] = '\0';
        m_directory[0] = '\0';
        m_fileName[0] = '\0';
        m_extension[0] = '\0';
    }

    // 0x00BEFC40
    bool FilePath::isAbsolute() const
    {
        return isAbsolutePath(m_prefix, m_directory);
    }

    // 0x00BF75C0
    bool FilePath::hasDirectoryPrefix(const char* pDirectory) const
    {
        return isStringPrefixEqual(pDirectory, pDirectory + getStringLength(pDirectory), m_directory);
    }

    // 0x00C07EB0
    FilePath& FilePath::createCombinedPath(const char* pPath0, const char* pPath1)
    {
        // pPath1 relative to the directory pPath0, unless pPath1 is absolute.
        setCompletePath(pPath0, true);
        FilePath path1;
        path1.setCompletePath(pPath1, false);
        if (path1.isAbsolute())
        {
            setCompletePath(pPath1, false);
        }
        else
        {
            pushDirectory(path1.m_directory);
            copyString(m_fileName, FileNameCapacity, path1.m_fileName);
            copyString(m_extension, ExtensionCapacity, path1.m_extension);
            m_completePathState = CompletePathState_Invalid;
        }
        return *this;
    }

    // 0x00C07FA0
    FilePath& FilePath::createRelativePath(const char* pSource, const char* pTarget)
    {
        // The path that leads from the directory pSource to pTarget. Both must be absolute; with different prefixes the
        // result is pTarget itself. Otherwise this stays empty.
        clear();
        FilePath source;
        source.setCompletePath(pSource, true);
        FilePath target;
        target.setCompletePath(pTarget, false);
        if (source.isAbsolute() && target.isAbsolute())
        {
            if (compareString(source.m_prefix, target.m_prefix) == 0)
            {
                while (!target.hasDirectoryPrefix(source.m_directory))
                {
                    source.popDirectory(1);
                    pushDirectory(FilePath_PreviousDirectoryName);
                }
                pushDirectory(target.m_directory + getStringLength(source.m_directory));
                setFileName(target.m_fileName);
                setExtension(target.m_extension);
            }
            else
            {
                *this = target;
            }
        }
        return *this;
    }

    // 0x00C028F0
    FilePath& FilePath::popDirectory(int levelCount)
    {
        char* pDirectory = m_directory;
        char* pEnd = pDirectory;
        for (uint32 i = 0; *pEnd != '\0' && i < DirectoryCapacity; ++i)
        {
            ++pEnd;
        }
        for (; levelCount > 0; --levelCount)
        {
            // Drop the trailing '/', then everything back to the previous '/'.
            if (pEnd != pDirectory && pEnd[-1] == '/' && pEnd > pDirectory)
            {
                *--pEnd = '\0';
            }
            while (pEnd != pDirectory && pEnd[-1] != '/' && pDirectory < pEnd)
            {
                *--pEnd = '\0';
            }
        }
        m_completePathState = CompletePathState_Invalid;
        return *this;
    }

    // 0x00C04A20
    FilePath& FilePath::pushDirectory(const char* pDirectory)
    {
        if (pDirectory == nullptr)
        {
            return *this;
        }

        char buffer[512];
        copyString(buffer, sizeof(buffer), m_directory);
        char* pDest = buffer;
        uint32 capacity = sizeof(buffer);
        while (*pDest != '\0' && capacity != 0)
        {
            ++pDest;
            --capacity;
        }

        // Exactly one separator between the current directory and the pushed one.
        const bool startsWithSeparator = findString(FilePath_SourcePathSeparatorChars, *pDirectory) != nullptr && *pDirectory != '\0';
        const char lastChar = pDest != buffer ? pDest[-1] : '\0';
        bool append = true;
        if (!startsWithSeparator)
        {
            if (lastChar != '/' && pDest != buffer)
            {
                if (*pDirectory == '\0')
                {
                    append = false;
                }
                else if (capacity > 1)
                {
                    *pDest++ = '/';
                    --capacity;
                    *pDest = '\0';
                }
            }
        }
        else if (lastChar == '/' && buffer < pDest)
        {
            --pDest;
            ++capacity;
            *pDest = '\0';
        }

        if (append && *pDirectory != '\0')
        {
            if (capacity != 0)
            {
                const uint32 length = copyString(pDest, capacity, pDirectory);
                if (length < capacity)
                {
                    capacity -= length;
                    pDest += length;
                }
                else
                {
                    capacity = 0;
                }
            }
            // Terminate with a separator.
            const char newLastChar = pDest != buffer ? pDest[-1] : '\0';
            if (findString(FilePath_SourcePathSeparatorChars, newLastChar) == nullptr && capacity > 1)
            {
                pDest[0] = '/';
                pDest[1] = '\0';
            }
        }

        replaceStringCharacters(buffer, FilePath_SourcePathSeparatorChars, '/');
        normalizeDirectory(m_directory, DirectoryCapacity, buffer);
        m_completePathState = CompletePathState_Invalid;
        return *this;
    }

    // 0x00C04BB0
    FilePath& FilePath::setCompletePath(const char* pPath, bool pathIsOnlyDirectory)
    {
        clear();
        if (pPath == nullptr)
        {
            return *this;
        }

        // Prefix: everything up to and including the first ':', or a "//server" / "\\server" part.
        const char* pColon = findString(pPath, ':');
        if (pColon != nullptr)
        {
            copyString(m_prefix, pPath, pColon + 1, PrefixCapacity);
            pPath = pColon + 1;
        }
        else if ((pPath[0] == '/' && pPath[1] == '/') || (pPath[0] == '\\' && pPath[1] == '\\'))
        {
            const char* pSeparator = findFirstCharacterInString(pPath + 2, FilePath_SourcePathSeparatorChars);
            if (pSeparator == nullptr)
            {
                copyString(m_prefix, PrefixCapacity, pPath);
                pSeparator = pPath + getStringLength(pPath);
            }
            else
            {
                copyString(m_prefix, pPath, pSeparator, PrefixCapacity);
            }
            replaceStringCharacters(m_prefix, FilePath_SourcePathSeparatorChars, '/');
            pPath = pSeparator;
        }

        if (pathIsOnlyDirectory)
        {
            pushDirectory(pPath);
        }
        else
        {
            const char* pLastSeparator = findLastCharacterInString(pPath, FilePath_SourcePathSeparatorChars);
            if (pLastSeparator != nullptr)
            {
                char directory[256];
                copyString(directory, pPath, pLastSeparator + 1, sizeof(directory));
                pushDirectory(directory);
                pPath = pLastSeparator + 1;
            }
            const char* pDot = findLastCharacterInString(pPath, '.');
            if (*pPath != '\0' && pDot != nullptr)
            {
                copyString(m_fileName, pPath, pDot, FileNameCapacity);
                copyString(m_extension, ExtensionCapacity, pDot);
            }
            else
            {
                copyString(m_fileName, FileNameCapacity, pPath);
            }
        }
        m_completePathState = CompletePathState_Invalid;
        return *this;
    }

    // 0x00C08150
    FilePath& FilePath::formatDirectoryPath(const char* pFormat, ...)
    {
        // The original formats into m_completePath and parses that (setCompletePath clears the buffer first, so the
        // formatted text is copied out of it before it is overwritten).
        char path[CompletePathCapacity];
        va_list arguments;
        va_start(arguments, pFormat);
        formatStringVarArgs(path, sizeof(path), pFormat, arguments);
        va_end(arguments);
        return setCompletePath(path, false);
    }

    // 0x00BF7630
    FilePath& FilePath::setPrefix(const char* pPrefix)
    {
        if (pPrefix == nullptr)
        {
            m_prefix[0] = '\0';
            return *this;
        }
        copyString(m_prefix, PrefixCapacity, pPrefix);
        replaceStringCharacters(m_prefix, FilePath_SourcePathSeparatorChars, '/');
        m_completePathState = CompletePathState_Invalid;
        return *this;
    }

    // 0x00C04D70
    FilePath& FilePath::setDirectory(const char* pDirectory)
    {
        m_directory[0] = '\0';
        if (pDirectory != nullptr)
        {
            pushDirectory(pDirectory);
        }
        return *this;
    }

    // 0x00BF7680
    FilePath& FilePath::setFileName(const char* pFileName)
    {
        if (pFileName == nullptr)
        {
            m_fileName[0] = '\0';
            return *this;
        }
        copyString(m_fileName, FileNameCapacity, pFileName);
        m_completePathState = CompletePathState_Invalid;
        return *this;
    }

    // 0x00BF76C0
    FilePath& FilePath::setExtension(const char* pExtension)
    {
        if (pExtension == nullptr)
        {
            m_extension[0] = '\0';
            return *this;
        }
        m_extension[0] = '.';
        if (*pExtension == '.')
        {
            ++pExtension;
        }
        // Capacity as in the original (one byte more than the remaining space).
        copyString(m_extension + 1, ExtensionCapacity, pExtension);
        m_completePathState = CompletePathState_Invalid;
        return *this;
    }

    // 0x00BF7710
    FilePath& FilePath::setFileNameWithExtension(const char* pFileName)
    {
        const char* pDot = findLastCharacterInString(pFileName, '.');
        if (*pFileName != '\0' && pDot != nullptr)
        {
            copyString(m_fileName, pFileName, pDot, FileNameCapacity);
            copyString(m_extension, ExtensionCapacity, pDot);
        }
        else
        {
            copyString(m_fileName, FileNameCapacity, pFileName);
            m_extension[0] = '\0';
        }
        m_completePathState = CompletePathState_Invalid;
        return *this;
    }

    // 0x00BF77B0
    FilePath& FilePath::convertToLowerCase()
    {
        m_completePath[0] = '\0';
        m_completePathState = CompletePathState_Invalid;
        changeStringToLowercase(m_prefix);
        changeStringToLowercase(m_directory);
        changeStringToLowercase(m_fileName);
        changeStringToLowercase(m_extension);
        return *this;
    }

    // 0x00C04D90
    const char* FilePath::getCompletePath() const
    {
        if (m_completePathState != CompletePathState_CompletePath)
        {
            buildCompletePath();
        }
        return m_completePath;
    }

    // 0x00BEFC80
    const char* FilePath::getPrefix() const
    {
        return m_prefix;
    }

    // 0x00BEFC90
    const char* FilePath::getDirectory() const
    {
        return m_directory;
    }

    // 0x00C04DD0
    const char* FilePath::getDirectoryWithPrefix() const
    {
        if (m_completePathState != CompletePathState_DirectoryWithPrefix)
        {
            buildDirectoryWithPrefix();
        }
        return m_completePath;
    }

    // 0x00BEFCA0
    const char* FilePath::getFileName() const
    {
        return m_fileName;
    }

    // 0x00BEFCB0
    const char* FilePath::getExtension() const
    {
        return m_extension;
    }

    // 0x00C04DB0
    const char* FilePath::getFileNameWithExtension() const
    {
        if (m_completePathState != CompletePathState_FileNameWithExtension)
        {
            buildFileNameWithExtension();
        }
        return m_completePath;
    }

    // 0x00BFD030
    const char* FilePath::getLastDirectoryPart() const
    {
        if (m_completePathState != CompletePathState_LastDirectoryPart)
        {
            buildLastDirectoryPart();
        }
        return m_completePath;
    }

    // 0x00BF78C0
    bool FilePath::getNextDirectoryPart(String* pPart) const
    {
        if (pPart->pStart == nullptr)
        {
            const char* pEnd = m_directory;
            while (*pEnd != '\0' && *pEnd != '/')
            {
                ++pEnd;
            }
            pPart->pEnd = pEnd;
            pPart->pStart = m_directory;
            return m_directory[0] != '\0';
        }
        if (*pPart->pEnd == '\0')
        {
            return false;
        }
        *pPart = findNextStringToken(pPart->pEnd + 1, '/');
        return *pPart->pStart != '\0';
    }

    // 0x00C02970
    void FilePath::buildCompletePath() const
    {
        char* pDest = m_completePath;
        uint32 capacity = CompletePathCapacity;
        m_completePath[0] = '\0';
        appendPart(&pDest, &capacity, m_prefix);
        // An absolute path gets a separator between prefix and a directory that does not start with one.
        const bool needsSeparator = m_directory[0] != '\0' && m_directory[0] != '/';
        if (isAbsolutePath(m_prefix, m_directory) && needsSeparator && capacity > 1)
        {
            *pDest++ = '/';
            *pDest = '\0';
            --capacity;
        }
        appendPart(&pDest, &capacity, m_directory);
        appendPart(&pDest, &capacity, m_fileName);
        if (m_extension[0] != '\0' && capacity != 0)
        {
            copyString(pDest, capacity, m_extension);
        }
        m_completePathState = CompletePathState_CompletePath;
    }

    // 0x00C02AB0
    void FilePath::buildFileNameWithExtension() const
    {
        char* pDest = m_completePath;
        uint32 capacity = CompletePathCapacity;
        m_completePath[0] = '\0';
        appendPart(&pDest, &capacity, m_fileName);
        if (m_extension[0] != '\0' && capacity != 0)
        {
            copyString(pDest, capacity, m_extension);
        }
        m_completePathState = CompletePathState_FileNameWithExtension;
    }

    // 0x00C02B30
    void FilePath::buildDirectoryWithPrefix() const
    {
        char* pDest = m_completePath;
        uint32 capacity = CompletePathCapacity;
        m_completePath[0] = '\0';
        appendPart(&pDest, &capacity, m_prefix);
        if (m_directory[0] != '\0' && capacity != 0)
        {
            copyString(pDest, capacity, m_directory);
        }
        m_completePathState = CompletePathState_DirectoryWithPrefix;
    }

    // 0x00BF7860
    void FilePath::buildLastDirectoryPart() const
    {
        // Leaves the state unchanged when the directory is empty.
        m_completePath[0] = '\0';
        const uint32 length = getStringLength(m_directory);
        if (length == 0)
        {
            return;
        }
        const char* p = m_directory + length - 1;
        if (*p == '/')
        {
            --p;
        }
        while (m_directory <= p && *p != '/')
        {
            --p;
        }
        copyString(m_completePath, CompletePathCapacity, p + 1);
        m_completePathState = CompletePathState_LastDirectoryPart;
    }

    // 0x00C02BB0
    void FilePath::normalizeDirectory(char* pTarget, uint32 targetCapacity, const char* pSource)
    {
        // Copies pSource part by part ("name/"). "../" removes the previous part unless the result is empty, the root
        // "/" or already ends with "../"; "./" is dropped unless the result is empty or "/". An empty or root result
        // is replaced by "./" or "../" (the builder restarts at the beginning of the buffer). Empty parts ("//") are
        // dropped except at the start.
        *pTarget = '\0';
        StringBuilder builder(pTarget, targetCapacity);
        while (*pSource != '\0')
        {
            const char* pSeparator = findString(pSource, '/');
            if (pSeparator == nullptr)
            {
                builder.appendString(pSource);
                return;
            }
            const char* pPartEnd = pSeparator + 1;
            const bool isEmpty = *pTarget == '\0';
            const bool isRoot = pTarget[0] == '/' && pTarget[1] == '\0';
            const uint32 size = builder.getSize();
            const bool endsWithPreviousDirectory = size >= 3 && isStringPrefixEqual(pTarget + size - 3, pTarget + size, FilePath_PreviousDirectoryName);

            if (isStringPrefixEqual(pSource, pPartEnd, FilePath_PreviousDirectoryName))
            {
                if (isRoot || isEmpty)
                {
                    builder.clear();
                    builder.appendString(pSource, pPartEnd);
                }
                else if (endsWithPreviousDirectory)
                {
                    builder.appendString(pSource, pPartEnd);
                }
                else
                {
                    removeLastDirectoryPart(builder);
                }
            }
            else if (isStringPrefixEqual(pSource, pPartEnd, FilePath_CurrentDirectoryName))
            {
                if (isRoot || isEmpty)
                {
                    builder.clear();
                    builder.appendString(pSource, pPartEnd);
                }
            }
            else if (isEmpty || pSource < pSeparator)
            {
                builder.appendString(pSource, pPartEnd);
            }
            pSource = pPartEnd;
        }
    }

    // 0x00BFD050
    void FilePath::removeLastDirectoryPart(StringBuilder& directoryString)
    {
        if (directoryString.getLastCharacter() == '/')
        {
            directoryString.removeLastCharacter();
        }
        while (directoryString.getLastCharacter() != '/' && !directoryString.isEmpty())
        {
            directoryString.removeLastCharacter();
        }
    }
}
