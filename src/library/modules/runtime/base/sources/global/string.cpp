// Original: library/modules/runtime/base/sources/global/string.cpp
//
// C string and keen::String helpers. Comparisons treat a null pointer as less than any string. Case folding is ASCII
// only ('A'..'Z'). copyString returns the length of the source (not what was copied), so callers detect truncation
// with "result >= capacity".
//
// The inline helpers of string.hpp and types.hpp (keen::String) are defined here too.

#include "keen/base/string.hpp"
#include "keen/base/memoryallocator.hpp"

#include <stdlib.h>
#include <string.h>

namespace keen
{
    static inline char toLowerAscii(char ch)
    {
        return (uint8)(ch - 'A') < 26 ? (char)(ch + ' ') : ch;
    }

    // Copies size bytes, with memmove when the ranges overlap (the original checks both directions).
    static inline void copyBytes(void* pDest, const void* pSource, size_t size)
    {
        const char* pD = (const char*)pDest;
        const char* pS = (const char*)pSource;
        const bool overlaps = (pD < pS && pS < pD + size) || (pS < pD && pD < pS + size);
        if (overlaps)
        {
            memmove(pDest, pSource, size);
        }
        else
        {
            memcpy(pDest, pSource, size);
        }
    }

    // Length of the UTF-8 sequence starting with leadByte; exits on an invalid lead byte.
    static inline uint32 getUTF8SequenceLength(uint8 leadByte)
    {
        if ((leadByte & 0x80) == 0)
        {
            return 1;
        }
        if ((leadByte & 0xE0) == 0xC0)
        {
            return 2;
        }
        if ((leadByte & 0xF0) == 0xE0)
        {
            return 3;
        }
        if ((leadByte & 0xF8) == 0xF0)
        {
            return 4;
        }
        exit(1);
    }

    // ---- keen::String (types.hpp)

    // 0x00BED2C0
    String::String()
    {
        pEnd = nullptr;
        pStart = nullptr;
    }

    // 0x00BED2D0
    String::String(const char* pInStart, const char* pInEnd)
    {
        pStart = pInStart;
        pEnd = pInEnd;
    }

    // 0x00BED2F0
    bool String::isEmpty() const
    {
        return pStart == pEnd;
    }

    uint32 String::getLength() const
    {
        return (uint32)(pEnd - pStart);
    }

    // ---- inline helpers (string.hpp)

    // 0x00BED400 (.h:18)
    String createString(const char* pStart, const char* pEnd)
    {
        return String(pStart, pEnd);
    }

    // 0x00BED420 (.h:19)
    String createEmptyString()
    {
        return String(nullptr, nullptr);
    }

    // 0x00BED440 (.h:22)
    uint32 getStringLength(String string)
    {
        return (uint32)(string.pEnd - string.pStart);
    }

    // 0x00BED450 (.h:23)
    bool isStringEmpty(String string)
    {
        return string.pStart == string.pEnd;
    }

    // 0x00741DB0 (.h:65)
    uint32 copyString(char* pDest, const char* pSource, uint32 capacity)
    {
        return copyString(pDest, capacity, pSource);
    }

    // 0x004B2200 (.h:70)
    uint32 copyString(char* pDest, const char* pSourceBegin, const char* pSourceEnd, uint32 capacity)
    {
        return copyString(pDest, capacity, pSourceBegin, pSourceEnd);
    }

    // 0x005D0930 (.h:75)
    uint32 copyUTF8String(char* pDest, const char* pSource, uint32 capacity)
    {
        return copyUTF8String(pDest, capacity, pSource);
    }

    // ---- string.cpp

    // 0x00BFA420 (:11)
    String createStringFromCString(const char* pString)
    {
        if (pString == nullptr)
        {
            return String(nullptr, nullptr);
        }
        uint32 length = 0;
        while (pString[length] != '\0')
        {
            ++length;
        }
        return String(pString, pString + length);
    }

    // 0x00BF19E0 (:29)
    String duplicateString(MemoryAllocator* pAllocator, String string)
    {
        if (string.pStart == nullptr)
        {
            return String(nullptr, string.pEnd);
        }
        const uint32 length = (uint32)(string.pEnd - string.pStart);
        char* pCopy = (char*)pAllocator->allocate(length + 1, 1, 0, nullptr);
        if (pCopy != nullptr)
        {
            // Copies length + 1 bytes: the source is expected to be terminated.
            memcpy(pCopy, string.pStart, length + 1);
        }
        return String(pCopy, pCopy + length);
    }

    // 0x00BF1A40 (:49)
    String findNextStringToken(const char* pString, const char separatorChar)
    {
        String result(pString, pString);
        if (pString != nullptr)
        {
            while (*pString != '\0' && *result.pEnd != separatorChar)
            {
                ++result.pEnd;
                pString = result.pEnd;
            }
        }
        return result;
    }

    // 0x00C04240 (:67)
    String copyString(char* pBuffer, uint32 capacity, String source)
    {
        String result(pBuffer, pBuffer);
        uint32 length = 0;
        if (source.pStart != source.pEnd)
        {
            const char* pSource = source.pStart;
            do
            {
                if (length == capacity - 1)
                {
                    break;
                }
                pBuffer[length++] = *pSource++;
            }
            while (pSource != source.pEnd);
        }
        pBuffer[length] = '\0';
        // The original returns the empty string at pBuffer (pEnd is not advanced).
        return result;
    }

    // 0x00BFA460 (:89)
    int compareString(String a, String b)
    {
        return compareString(a.pStart, a.pEnd, b.pStart, b.pEnd);
    }

    // 0x00BFA480 (:94)
    bool isStringEqual(String a, String b)
    {
        return compareString(a.pStart, a.pEnd, b.pStart, b.pEnd) == 0;
    }

    // 0x00BFA4B0 (:99)
    bool isStringEqualNoCase(String a, String b)
    {
        return compareStringNoCase(a.pStart, a.pEnd, b.pStart, b.pEnd) == 0;
    }

    // 0x00BF1A80 (:104)
    bool splitString(String* pFirstPart, String* pLastPart, String input, char splitChar)
    {
        const char* p = input.pStart;
        while (p != input.pEnd)
        {
            if (*p == splitChar)
            {
                if (pFirstPart != nullptr)
                {
                    *pFirstPart = String(input.pStart, p);
                }
                if (pLastPart != nullptr)
                {
                    *pLastPart = String(p + 1, input.pEnd);
                }
                return true;
            }
            ++p;
        }
        return false;
    }

    // 0x00BFA4E0 (:126)
    bool splitString(String* pFirstPart, String* pLastPart, const char* pInput, char splitChar)
    {
        return splitString(pFirstPart, pLastPart, createStringFromCString(pInput), splitChar);
    }

    // 0x00C042A0 (:148)
    bool splitAndTerminateSubString(Slice<char>* pFirstPart, Slice<char>* pRest, Slice<char> input, char splitChar)
    {
        char* pStart = input.getStart();
        const uint32 inputSize = input.getSize();
        const uint32 inputCapacity = input.getCapacity();
        for (uint32 i = 0; i < inputSize; ++i)
        {
            if (pStart[i] == splitChar)
            {
                const uint32 firstSize = i + 1;
                pStart[i] = '\0';
                if (pFirstPart != nullptr)
                {
                    pFirstPart->create(pStart, firstSize, firstSize);
                }
                if (pRest != nullptr)
                {
                    const uint32 restCapacity = inputCapacity - firstSize;
                    // Original arithmetic (size - 2 * firstSize), kept as is.
                    pRest->create(pStart + firstSize, inputSize - firstSize * 2, restCapacity != 0 ? restCapacity : inputCapacity);
                }
                return true;
            }
        }
        return false;
    }

    // 0x00BFA530 (:174)
    int compareString(String a, const char* pB)
    {
        return compareString(a.pStart, a.pEnd, pB);
    }

    // 0x00BFA550 (:179)
    bool isStringEqual(String a, const char* pB)
    {
        return compareString(a.pStart, a.pEnd, pB) == 0;
    }

    // 0x00BFA570 (:184)
    bool doesStringStartWith(String a, const char* pB)
    {
        return doesStringStartWith(a.pStart, a.pEnd, pB);
    }

    // 0x00BFA590 (:189)
    bool doesStringStartWith(const char* pString, const char* pPrefix)
    {
        const char* pEnd = pString;
        while (pEnd != nullptr && *pEnd != '\0')
        {
            ++pEnd;
        }
        return doesStringStartWith(pString, pEnd, pPrefix);
    }

    // 0x00BFA5C0 (:203)
    char* duplicateString(MemoryAllocator* pAllocator, const char* pSource)
    {
        if (pSource == nullptr)
        {
            return nullptr;
        }
        const uint32 size = getStringLength(pSource) + 1;
        char* pCopy = (char*)pAllocator->allocate(size, 16, 0, nullptr);
        if (pCopy != nullptr)
        {
            copyBytes(pCopy, pSource, size);
        }
        return pCopy;
    }

    // 0x00BF1AC0 (:241)
    uint32 getStringLength(const char* pSource)
    {
        uint32 length = 0;
        if (pSource != nullptr)
        {
            while (pSource[length] != '\0')
            {
                ++length;
            }
        }
        return length;
    }

    // 0x00BFA650 (:263)
    uint32 getStringLengthUTF8(const char* pSource)
    {
        if (pSource == nullptr || *pSource == '\0')
        {
            return 0;
        }
        uint32 length = 0;
        do
        {
            pSource += getUTF8SequenceLength((uint8)*pSource);
            ++length;
        }
        while (*pSource != '\0');
        return length;
    }

    // 0x00BF1AE0 (:318)
    uint32 copyString(char* pDest, uint32 capacity, const char* pSource)
    {
        uint32 length = 0;
        if (pSource != nullptr)
        {
            while (pSource[length] != '\0')
            {
                if (length < capacity - 1)
                {
                    *pDest++ = pSource[length];
                }
                ++length;
            }
        }
        *pDest = '\0';
        return length;
    }

    // 0x00BF1B30 (:345)
    uint32 copyString(char* pDest, uint32 capacity, const char* pSourceBegin, const char* pSourceEnd)
    {
        uint32 length = 0;
        while (pSourceBegin != pSourceEnd)
        {
            if (length < capacity - 1)
            {
                *pDest++ = *pSourceBegin;
            }
            ++pSourceBegin;
            ++length;
        }
        *pDest = '\0';
        return length;
    }

    // 0x00BFA6B0 (:381)
    uint32 copyUTF8String(char* pTarget, uint32 capacity, const char* pSource)
    {
        // Copies whole UTF-8 sequences only: stops at the last sequence that starts within capacity - 1 bytes.
        if (pSource == nullptr || *pSource == '\0')
        {
            return 0;
        }
        const char* pLast = pSource;
        const char* p = pSource;
        while (p <= pSource + (capacity - 1))
        {
            pLast = p;
            if (*p == '\0')
            {
                break;
            }
            p += getUTF8SequenceLength((uint8)*p);
        }
        const uint32 size = (uint32)(pLast - pSource);
        if (size != 0)
        {
            copyBytes(pTarget, pSource, size);
        }
        pTarget[size] = '\0';
        return size;
    }

    // 0x00BFA780 (:450)
    uint32 copyUTF8String(char* pDest, uint32 capacity, const char* pSource, const char* pSourceEnd)
    {
        if (pSource + (capacity - 1) <= pSourceEnd)
        {
            pSourceEnd = pSource + (capacity - 1);
        }
        const char* pLast = pSource;
        const char* p = pSource;
        while (*p != '\0' && p <= pSourceEnd)
        {
            pLast = p;
            p += getUTF8SequenceLength((uint8)*p);
        }
        const uint32 size = (uint32)(pLast - pSource);
        copyBytes(pDest, pSource, size);
        pDest[size] = '\0';
        return size;
    }

    // 0x00BF1B70 (:488)
    bool isUTF8String(const char* pString, uint32 length)
    {
        const uint8* p = (const uint8*)pString;
        while (*p != 0 && (const char*)p <= pString + (length - 1))
        {
            const uint8 leadByte = *p;
            if ((leadByte & 0x80) == 0)
            {
                ++p;
                continue;
            }
            uint32 sequenceLength;
            if ((leadByte & 0xE0) == 0xC0)
            {
                sequenceLength = 2;
            }
            else if ((leadByte & 0xF0) == 0xE0)
            {
                sequenceLength = 3;
            }
            else if ((leadByte & 0xF8) == 0xF0)
            {
                sequenceLength = 4;
            }
            else
            {
                return false;
            }
            // The original checks the continuation bytes starting at the lead byte itself, so any multi-byte sequence
            // fails here (kept as is).
            for (uint32 i = 1; i < sequenceLength; ++i)
            {
                if ((*p & 0xC0) != 0x80)
                {
                    return false;
                }
                ++p;
            }
        }
        return true;
    }

    namespace AsciiCharacter
    {
        // 0x00BF1BE0 (:537)
        bool isDecimalDigit(char ch)
        {
            return (uint8)(ch - '0') <= 9;
        }

        // 0x00BF1BF0 (:542)
        bool isHexadecimalDigit(char ch)
        {
            return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (uint8)(ch - 'A') <= 5;
        }

        // 0x00BF1C20 (:547)
        bool isAlphabetic(char ch)
        {
            return (ch >= 'a' && ch <= 'z') || (uint8)(ch - 'A') <= 25;
        }

        // 0x00BF1C40 (:552)
        bool isWhiteSpace(char ch)
        {
            return (ch >= '\t' && ch <= '\r') || ch == ' ';
        }

        // 0x00BF1C60 (:557)
        char toLower(char ch)
        {
            return toLowerAscii(ch);
        }
    }

    // 0x00BF1C80 (:571)
    char* changeStringToLowercase(char* pSource)
    {
        for (char* p = pSource; *p != '\0'; ++p)
        {
            if (*p > '@' && *p < '[')
            {
                *p += ' ';
            }
        }
        return pSource;
    }

    // 0x00BF1CB0 (:587)
    char* changeStringToUppercase(char* pSource)
    {
        for (char* p = pSource; *p != '\0'; ++p)
        {
            if (*p > '`' && *p < '{')
            {
                *p -= ' ';
            }
        }
        return pSource;
    }

    // 0x00BFA850 (:603)
    uint32 appendString(char* pDest, uint32 capacity, const char* pSource)
    {
        const uint32 destLength = getStringLength(pDest);
        const uint32 sourceLength = getStringLength(pSource);
        uint32 size = 0;
        if (destLength + 1 < capacity)
        {
            uint32 newLength = sourceLength + destLength;
            if (newLength >= capacity - 1)
            {
                newLength = capacity - 1;
            }
            size = newLength - destLength;
            copyBytes(pDest + destLength, pSource, size);
            pDest[destLength + size] = '\0';
        }
        return size;
    }

    // 0x00BFA900 (:624)
    uint32 appendString(char* pDest, uint32 capacity, const char* pBegin, const char* pEnd)
    {
        const uint32 destLength = getStringLength(pDest);
        uint32 size = 0;
        if (destLength + 1 < capacity)
        {
            uint32 newLength = (uint32)(pEnd - pBegin) + destLength;
            if (newLength >= capacity - 1)
            {
                newLength = capacity - 1;
            }
            size = newLength - destLength;
            copyBytes(pDest + destLength, pBegin, size);
            pDest[destLength + size] = '\0';
        }
        return size;
    }

    // 0x00BF1CE0 (:652)
    const char* findString(const char* pString, const char letter)
    {
        for (; *pString != '\0'; ++pString)
        {
            if (*pString == letter)
            {
                return pString;
            }
        }
        return nullptr;
    }

    // 0x00BFA9B0 (:645)
    char* findString(char* pString, const char letter)
    {
        return (char*)findString((const char*)pString, letter);
    }

    // 0x00BF1D00 (:675)
    const char* findString(const char* pString, const char* pToken)
    {
        for (; *pString != '\0'; ++pString)
        {
            const char* pT = pToken;
            const char* pS = pString;
            while (*pT != '\0' && *pS == *pT)
            {
                ++pS;
                ++pT;
            }
            if (*pT == '\0')
            {
                return pString;
            }
        }
        return nullptr;
    }

    // 0x00BFA9D0 (:668)
    char* findString(char* pString, const char* pToken)
    {
        return (char*)findString((const char*)pString, pToken);
    }

    // 0x00BF1D50 (:715)
    const char* findFirstCharacterInString(const char* pString, char letter)
    {
        return findString(pString, letter);
    }

    // 0x00BFA9E0 (:705)
    char* findFirstCharacterInString(char* pString, char letter)
    {
        return (char*)findString((const char*)pString, letter);
    }

    // 0x00BF1D70 (:720)
    const char* findFirstCharacterInString(const char* pString, const char* pLetterArray)
    {
        for (; *pString != '\0'; ++pString)
        {
            for (const char* pLetter = pLetterArray; *pLetter != '\0'; ++pLetter)
            {
                if (*pString == *pLetter)
                {
                    return pString;
                }
            }
        }
        return nullptr;
    }

    // 0x00BFAA00 (:710)
    char* findFirstCharacterInString(char* pString, const char* pLetterArray)
    {
        return (char*)findFirstCharacterInString((const char*)pString, pLetterArray);
    }

    // 0x00BF1DB0 (:755)
    const char* findLastCharacterInString(const char* pString, char letter)
    {
        const char* pResult = nullptr;
        for (; *pString != '\0'; ++pString)
        {
            if (*pString == letter)
            {
                pResult = pString;
            }
        }
        return pResult;
    }

    // 0x00BFAA10 (:745)
    char* findLastCharacterInString(char* pString, char letter)
    {
        return (char*)findLastCharacterInString((const char*)pString, letter);
    }

    // 0x00BF1DE0 (:775)
    const char* findLastCharacterInString(const char* pString, const char* pLetterArray)
    {
        const char* pResult = nullptr;
        for (; *pString != '\0'; ++pString)
        {
            for (const char* pLetter = pLetterArray; *pLetter != '\0'; ++pLetter)
            {
                if (*pString == *pLetter)
                {
                    pResult = pString;
                }
            }
        }
        return pResult;
    }

    // 0x00BFAA40 (:750)
    char* findLastCharacterInString(char* pString, const char* pLetterArray)
    {
        return (char*)findLastCharacterInString((const char*)pString, pLetterArray);
    }

    // 0x00BF1E30 (:801)
    uint32 countCharacterInString(const char* pString, char letter)
    {
        uint32 count = 0;
        for (; *pString != '\0'; ++pString)
        {
            if (*pString == letter)
            {
                ++count;
            }
        }
        return count;
    }

    // 0x00BF1E60 (:820)
    void replaceStringCharacters(char* pString, char sourceLetter, char replaceLetter)
    {
        for (; *pString != '\0'; ++pString)
        {
            if (*pString == sourceLetter)
            {
                *pString = replaceLetter;
            }
        }
    }

    // 0x00BF1E80 (:835)
    void replaceStringCharacters(char* pString, const char* pSourceLetters, char replaceLetter)
    {
        for (; *pString != '\0'; ++pString)
        {
            for (const char* pLetter = pSourceLetters; *pLetter != '\0'; ++pLetter)
            {
                if (*pLetter == *pString)
                {
                    *pString = replaceLetter;
                    break;
                }
            }
        }
    }

    // 0x00BF1EC0 (:850)
    int compareString(const char* pStringA, const char* pStringB)
    {
        if (pStringA == nullptr)
        {
            return pStringB != nullptr ? -1 : 0;
        }
        if (pStringB == nullptr)
        {
            return 1;
        }
        char a = *pStringA;
        char b = *pStringB;
        while (a == b)
        {
            if (a == '\0' || b == '\0')
            {
                return 0;
            }
            a = *++pStringA;
            b = *++pStringB;
        }
        return b < a ? 1 : -1;
    }

    // 0x00BF1F20 (:897)
    bool isStringEqual(const char* pStringA, const char* pStringB)
    {
        return compareString(pStringA, pStringB) == 0;
    }

    // 0x00BF1F40 (:902)
    bool doesStringStartWith(const char* pBegin, const char* pEnd, const char* pPrefx)
    {
        if (pBegin == nullptr)
        {
            return pPrefx == nullptr;
        }
        if (pPrefx == nullptr)
        {
            return false;
        }
        for (;;)
        {
            if (pBegin == pEnd)
            {
                return *pPrefx == '\0';
            }
            if (*pPrefx == '\0')
            {
                return true;
            }
            if (*pBegin != *pPrefx)
            {
                return false;
            }
            ++pBegin;
            ++pPrefx;
        }
    }

    // 0x00BF1F90 (:951)
    int compareString(const char* pStringABegin, const char* pStringAEnd, const char* pStringB)
    {
        if (pStringABegin == nullptr)
        {
            return pStringB != nullptr ? -1 : 0;
        }
        if (pStringB == nullptr)
        {
            return 1;
        }
        for (;;)
        {
            if (pStringABegin == pStringAEnd)
            {
                return *pStringB != '\0' ? -1 : 0;
            }
            const char a = *pStringABegin++;
            const char b = *pStringB++;
            if (a != b)
            {
                return b <= a ? 1 : -1;
            }
            if (a == '\0' || b == '\0')
            {
                return 0;
            }
        }
    }

    // 0x00BF2000 (:1011)
    bool isStringEqual(const char* pStringABegin, const char* pStringAEnd, const char* pStringB)
    {
        return compareString(pStringABegin, pStringAEnd, pStringB) == 0;
    }

    // 0x00BF2020 (:1016)
    int compareString(const char* pStringABegin, const char* pStringAEnd, const char* pStringBBegin, const char* pStringBEnd)
    {
        if (pStringABegin == nullptr)
        {
            return pStringBBegin != nullptr ? -1 : 0;
        }
        if (pStringBBegin == nullptr)
        {
            return 1;
        }
        while (pStringABegin != pStringAEnd && pStringBBegin != pStringBEnd)
        {
            const char a = *pStringABegin;
            const char b = *pStringBBegin;
            if (b < a)
            {
                return 1;
            }
            if (a < b)
            {
                return -1;
            }
            if (a == '\0' || b == '\0')
            {
                return 0;
            }
            ++pStringABegin;
            ++pStringBBegin;
        }
        if (pStringABegin == pStringAEnd)
        {
            return pStringBBegin == pStringBEnd ? 0 : -1;
        }
        return 1;
    }

    // 0x00BF20A0 (:1089)
    bool isStringEqual(const char* pStringABegin, const char* pStringAEnd, const char* pStringBBegin, const char* pStringBEnd)
    {
        return compareString(pStringABegin, pStringAEnd, pStringBBegin, pStringBEnd) == 0;
    }

    // 0x00BF20D0 (:1094)
    int compareStringNoCase(const char* pStringABegin, const char* pStringAEnd, const char* pStringBBegin, const char* pStringBEnd)
    {
        if (pStringABegin == nullptr)
        {
            return pStringBBegin != nullptr ? -1 : 0;
        }
        if (pStringBBegin == nullptr)
        {
            return 1;
        }
        while (pStringABegin != pStringAEnd && pStringBBegin != pStringBEnd)
        {
            const char a = toLowerAscii(*pStringABegin);
            const char b = toLowerAscii(*pStringBBegin);
            if (b < a)
            {
                return 1;
            }
            if (a < b)
            {
                return -1;
            }
            if (a == '\0' || b == '\0')
            {
                return 0;
            }
            ++pStringABegin;
            ++pStringBBegin;
        }
        if (pStringABegin == pStringAEnd)
        {
            return pStringBBegin == pStringBEnd ? 0 : -1;
        }
        return 1;
    }

    // 0x00BF2170 (:1177)
    bool isStringEqualNoCase(const char* pStringABegin, const char* pStringAEnd, const char* pStringBBegin, const char* pStringBEnd)
    {
        return compareStringNoCase(pStringABegin, pStringAEnd, pStringBBegin, pStringBEnd) == 0;
    }

    // 0x00BF21A0 (:1182)
    int compareStringNoCase(const char* pStringA, const char* pStringB)
    {
        if (pStringA == nullptr)
        {
            return pStringB == nullptr ? 0 : -1;
        }
        if (pStringB == nullptr)
        {
            return 1;
        }
        for (;;)
        {
            const char a = toLowerAscii(*pStringA++);
            const char b = toLowerAscii(*pStringB++);
            if (a != b)
            {
                return b < a ? 1 : -1;
            }
            if (a == '\0' || b == '\0')
            {
                return 0;
            }
        }
    }

    // 0x00BF2210 (:1242)
    bool isStringEqualNoCase(const char* pStringA, const char* pStringB)
    {
        return compareStringNoCase(pStringA, pStringB) == 0;
    }

    // 0x00BF2230 (:1247)
    int compareStringNoCase(const char* pStringA, const char* pStringB, uint32 size)
    {
        if (pStringA == nullptr)
        {
            return pStringB == nullptr ? 0 : -1;
        }
        if (pStringB == nullptr)
        {
            return 1;
        }
        for (uint32 i = 0; i < size; ++i)
        {
            const char a = toLowerAscii(*pStringA++);
            const char b = toLowerAscii(*pStringB++);
            if (a != b)
            {
                return b < a ? 1 : -1;
            }
            if (a == '\0' || b == '\0')
            {
                return 0;
            }
        }
        return 0;
    }

    // 0x00BF22D0 (:1297)
    bool isStringEqualNoCase(const char* pStringA, const char* pStringB, uint32 size)
    {
        return compareStringNoCase(pStringA, pStringB, size) == 0;
    }

    // 0x00BF22F0 (:1313)
    bool isStringEmpty(const char* pString)
    {
        return pString == nullptr || *pString == '\0';
    }

    // 0x00BFAA50 (:1323)
    void trimString(char* pString, char trimChar)
    {
        trimStringRight(pString, trimChar);
        trimStringLeft(pString, trimChar);
    }

    // 0x00BF2310 (:1329)
    void trimStringLeft(char* pString, char trimChar)
    {
        const char* pFirst = pString;
        while (*pFirst != '\0' && *pFirst == trimChar)
        {
            ++pFirst;
        }
        char* pDest = pString;
        while (*pFirst != '\0')
        {
            *pDest++ = *pFirst++;
        }
        *pDest = '\0';
    }

    // 0x00BF2350 (:1351)
    void trimStringRight(char* pString, char trimChar)
    {
        char* pEnd = pString;
        while (*pString != '\0')
        {
            const char ch = *pString++;
            if (ch != trimChar)
            {
                pEnd = pString;
            }
        }
        *pEnd = '\0';
    }

    // 0x00BFAAC0 (:1366)
    void trimString(String* pString, char trimChar)
    {
        trimStringLeft(pString, trimChar);
        trimStringRight(pString, trimChar);
    }

    // 0x00BF2380 (:1372)
    void trimStringLeft(String* pString, char trimChar)
    {
        while (pString->pStart != pString->pEnd && *pString->pStart == trimChar)
        {
            ++pString->pStart;
        }
    }

    // 0x00BF23A0 (:1380)
    void trimStringRight(String* pString, char trimChar)
    {
        while (pString->pStart != pString->pEnd && pString->pEnd[-1] == trimChar)
        {
            --pString->pEnd;
        }
    }

    // 0x00BFAB00 (:1388)
    uint32 replaceString(char* pDest, uint32 capacity, const char* pSource, const char* pSearch, const char* pReplace, uint32 maxReplaceCount)
    {
        const uint32 searchLength = getStringLength(pSearch);
        if (searchLength == 0)
        {
            copyString(pDest, capacity, pSource);
            return 0;
        }
        *pDest = '\0';
        uint32 replaceCount = 0;
        while (*pSource != '\0')
        {
            const char* pFound = findString(pSource, pSearch);
            if (pFound == nullptr)
            {
                appendString(pDest, capacity, pSource);
                return replaceCount;
            }
            appendString(pDest, capacity, pSource, pFound);
            appendString(pDest, capacity, pReplace);
            ++replaceCount;
            pSource = pFound + searchLength;
            if (maxReplaceCount != 0 && replaceCount >= maxReplaceCount)
            {
                appendString(pDest, capacity, pSource);
                return replaceCount;
            }
        }
        return replaceCount;
    }

    // 0x00BF23D0 (:1435)
    void setStringLength(char* pString, uint32 capacity, uint32 newLength, char fillCharater)
    {
        pString[newLength] = '\0';
        uint32 length = 0;
        while (length < newLength && pString[length] != '\0')
        {
            ++length;
        }
        if (length < newLength)
        {
            memset(pString + length, fillCharater, newLength - length);
        }
    }
}
