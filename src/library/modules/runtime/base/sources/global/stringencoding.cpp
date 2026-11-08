// Original: library/modules/runtime/base/sources/global/stringencoding.cpp
//
// UTF-8 / UTF-16 / UCS-2 conversion. Malformed UTF-8 decodes to code point 1 (the lead byte and the valid
// continuation bytes are consumed).
//
// Partial: the HTML entity, capitalization and replace functions (convertAsciiToHtmlEntities, convertHtmlEntitiesToAscii,
// getMaximumNumberOfLetters, isUnicodeWhiteSpaceCharacter, compareCapitalizationEntry, capitalizeUTF8String,
// replaceUTF8String) are not implemented yet.

#include "keen/base/stringencoding.hpp"
#include "keen/base/trace.hpp"

#include <stdlib.h>

namespace keen
{
    // Decodes one UTF-8 sequence (inlined in the converters). Returns the number of bytes consumed.
    static uint32 decodeUTF8Sequence(uint32* pCharacter, const char* pUtf8String)
    {
        const uint8 leadByte = (uint8)*pUtf8String;
        uint32 character = (uint32)(sint32)(char)leadByte;
        uint32 length;
        if ((leadByte & 0xE0) == 0xC0)
        {
            length = 2;
            character &= 0x1F;
        }
        else if ((leadByte & 0xF0) == 0xE0)
        {
            length = 3;
            character &= 0x0F;
        }
        else if ((leadByte & 0xF8) == 0xF0)
        {
            length = 4;
            character &= 0x07;
        }
        else if (character < 0x80)
        {
            length = 1;
        }
        else
        {
            *pCharacter = 1;
            return 1;
        }

        for (uint32 i = 1; i < length; ++i)
        {
            const uint8 continuationByte = (uint8)pUtf8String[i];
            if ((continuationByte & 0xC0) != 0x80)
            {
                *pCharacter = 1;
                return i;
            }
            character = (character << 6) | (continuationByte & 0x3F);
        }
        *pCharacter = character;
        return length;
    }

    // 0x00BF24F0 (:13)
    uint32 getUCS2StringLength(const uint16* pString)
    {
        uint32 length = 0;
        if (pString != nullptr)
        {
            while (pString[length] != 0)
            {
                ++length;
            }
        }
        return length;
    }

    // 0x00BFADA0 (:51)
    uint32 convertUTF8ToUCS2(uint16* pTarget, uint32 targetCapacity, const char* pUtf8String)
    {
        // Counts every character but writes only what fits; returns the byte size the whole string needs.
        uint32 characterCount = 0;
        while (*pUtf8String != '\0')
        {
            uint32 character;
            pUtf8String += decodeUTF8Sequence(&character, pUtf8String);
            ++characterCount;
            if (characterCount < targetCapacity / 2)
            {
                *pTarget++ = character < 0xFFFF ? (uint16)character : 0xFFFF;
            }
        }
        *pTarget = 0;
        return characterCount * 2;
    }

    // 0x00C0C510 (:90)
    uint32 convertUCS2ToUTF8(char* pTargetBuffer, uint32 targetCapacity, const uint16* pUcs2String)
    {
        if (targetCapacity == 0)
        {
            return 0;
        }
        uint32 remaining = targetCapacity - 1;
        uint32 length = 0;
        while (*pUcs2String != 0)
        {
            const uint32 written = writeUTF8Character(pTargetBuffer, remaining, *pUcs2String);
            ++pUcs2String;
            pTargetBuffer += written;
            length += written;
            remaining -= written;
        }
        *pTargetBuffer = '\0';
        return length + 1;
    }

    // 0x00BF2510 (:133)
    uint32 readUTF8Character(uint32* pResult, const char* pUtf8String)
    {
        return decodeUTF8Sequence(pResult, pUtf8String);
    }

    // 0x00C09780 (:198)
    uint32 writeUTF8Character(char* pTargetBuffer, uint32 targetCapacity, uint32 character)
    {
        if (character < 0x80)
        {
            if (targetCapacity != 0)
            {
                pTargetBuffer[0] = (char)character;
                return 1;
            }
        }
        else if (character < 0x800)
        {
            if (targetCapacity > 1)
            {
                pTargetBuffer[1] = (char)((character & 0x3F) | 0x80);
                pTargetBuffer[0] = (char)(((character >> 6) & 0x1F) | 0xC0);
                return 2;
            }
        }
        else if (character < 0x10000)
        {
            if (targetCapacity > 2)
            {
                pTargetBuffer[0] = (char)(((character >> 12) & 0x0F) | 0xE0);
                pTargetBuffer[2] = (char)((character & 0x3F) | 0x80);
                pTargetBuffer[1] = (char)(((character >> 6) & 0x3F) | 0x80);
                return 3;
            }
        }
        else if (character < 0x200000)
        {
            if (targetCapacity > 3)
            {
                pTargetBuffer[0] = (char)(((character >> 18) & 0x07) | 0xF0);
                pTargetBuffer[1] = (char)(((character >> 12) & 0x3F) | 0x80);
                pTargetBuffer[3] = (char)((character & 0x3F) | 0x80);
                pTargetBuffer[2] = (char)(((character >> 6) & 0x3F) | 0x80);
                return 4;
            }
        }
        else
        {
            debug::nativePrintArgs("Illegal unicode code point %08x!", character);
        }
        return 0;
    }

    // 0x00C09870 (:264)
    uint32 convertUTF16ToUTF8(char* pTargetBuffer, uint32 targetCapacity, const uint16* pUtf16String)
    {
        char* pTarget = pTargetBuffer;
        while (*pUtf16String != 0 && targetCapacity > 1)
        {
            uint32 character = *pUtf16String++;
            if ((character & 0xF800) == 0xD800)
            {
                // Surrogate pair (the low surrogate is not validated).
                character = (((character & 0x3FF) + 0x40) << 10) + (*pUtf16String++ & 0x3FF);
            }
            const uint32 written = writeUTF8Character(pTarget, targetCapacity, character);
            pTarget += written;
            targetCapacity -= written;
            if (written == 0)
            {
                break;
            }
        }
        *pTarget = '\0';
        return (uint32)(pTarget - pTargetBuffer);
    }

    // 0x00BF25C0 (:302)
    uint32 convertUTF8ToUTF16(uint16* pTargetBuffer, uint32 targetByteCapacity, const char* pUtf8String)
    {
        // Keeps room for the terminator. Returns the bytes written without the terminator.
        uint16* pTarget = pTargetBuffer;
        uint16* pTargetEnd = (uint16*)((uint8*)pTargetBuffer + (targetByteCapacity & ~1u) - 2);
        while (*pUtf8String != '\0' && pTarget < pTargetEnd)
        {
            uint32 character;
            pUtf8String += decodeUTF8Sequence(&character, pUtf8String);
            if (character < 0x10000)
            {
                *pTarget++ = (uint16)character;
            }
            else
            {
                if (targetByteCapacity < 6)
                {
                    break;
                }
                pTarget[0] = (uint16)((((character - 0x10000) >> 10) & 0x3FF) | 0xD800);
                pTarget[1] = (uint16)(((character - 0x10000) & 0x3FF) | 0xDC00);
                pTarget += 2;
            }
        }
        *pTarget = 0;
        return (uint32)(pTarget - pTargetBuffer) * 2;
    }

    // 0x00BF26E0 (:363)
    const char* getUTF8StringPosition(const char* pString, uint32 position)
    {
        while (pString != nullptr && position != 0)
        {
            const uint8 leadByte = (uint8)*pString;
            if ((leadByte & 0x80) == 0)
            {
                pString += 1;
            }
            else if ((leadByte & 0xE0) == 0xC0)
            {
                pString += 2;
            }
            else if ((leadByte & 0xF0) == 0xE0)
            {
                pString += 3;
            }
            else if ((leadByte & 0xF8) == 0xF0)
            {
                pString += 4;
            }
            else
            {
                exit(1);
            }
            --position;
        }
        return pString;
    }

    // 0x00BFAEB0 (:346)
    char* getUTF8StringPosition(char* pString, uint32 position)
    {
        return (char*)getUTF8StringPosition((const char*)pString, position);
    }
}
