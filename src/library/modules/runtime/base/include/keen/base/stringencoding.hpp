// Original: library/modules/runtime/base/include/keen/base/stringencoding.hpp
// Implementation: library/modules/runtime/base/sources/global/stringencoding.cpp
//
// UTF-8/UTF-16/UCS-2 conversions, capitalization tables and HTML entities.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class StringBuilder;

    enum CharacterEncodingId
    {
        CharacterEncodingId_ASCII = 0,
        CharacterEncodingId_UTF8 = 1,
        CharacterEncodingId_UTF16 = 2,
        CharacterEncodingId_USC2 = 3,
    };

    // size 0xA
    struct CapitalizationEntry
    {
        uint8 utf32highByte; // 0x00
        uint16 utf32LowercaseCharacterLowWord; // 0x02
        uint16 utf32UppercaseCharactersLowWords[3]; // 0x04
    };

    uint32 getUCS2StringLength(const uint16* pString); // 0x00BF24F0 (:13)
    uint32 convertUTF8ToUCS2(uint16* pTarget, uint32 targetCapacity, const char* pUtf8String); // 0x00BFADA0 (:51)
    uint32 convertUCS2ToUTF8(char* pTargetBuffer, uint32 targetCapacity, const uint16* pUcs2String); // 0x00C0C510 (:90)
    uint32 readUTF8Character(uint32* pResult, const char* pUtf8String); // 0x00BF2510 (:133)
    uint32 writeUTF8Character(char* pTargetBuffer, uint32 targetCapacity, uint32 character); // 0x00C09780 (:198)
    uint32 convertUTF16ToUTF8(char* pTargetBuffer, uint32 targetCapacity, const uint16* pUtf16String); // 0x00C09870 (:264)
    uint32 convertUTF8ToUTF16(uint16* pTargetBuffer, uint32 targetByteCapacity, const char* pUtf8String); // 0x00BF25C0 (:302)
    char* getUTF8StringPosition(char* pString, uint32 position); // 0x00BFAEB0 (:346)
    const char* getUTF8StringPosition(const char* pString, uint32 position); // 0x00BF26E0 (:363)
    uint32 convertAsciiToHtmlEntities(StringBuilder& target, const char* pAsciiString); // 0x00BFAEC0 (:406)
    uint32 convertHtmlEntitiesToAscii(StringBuilder& target, const char* pHtmlEntityString); // 0x00C00600 (:453)
    uint32 getMaximumNumberOfLetters(uint32 capacity, CharacterEncodingId encodingId); // 0x00C099C0 (:507)
    bool isUnicodeWhiteSpaceCharacter(uint32 character); // 0x00BF2740 (:543)
    int compareCapitalizationEntry(const CapitalizationEntry* pFirst, const CapitalizationEntry* pSecond); // 0x00BF27A0 (:588)
    uint32 capitalizeUTF8String(char* pTargetBuffer, uint32 targetCapacity, const char* pUTF8String); // 0x00C09A10 (:622)
    uint32 replaceUTF8String(char* pTargetBuffer, uint32 targetCapacity, const char* pUTF8String, const uint32* pCharactersToReplaceZeroTerminated, uint32 replacementCharacter); // 0x00C09C20 (:727)
}
