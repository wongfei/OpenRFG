// Original: library/modules/runtime/base/include/keen/base/string.hpp
// Implementation: library/modules/runtime/base/sources/global/string.cpp
//
// String helpers: copying, comparing and formatting (formatString with printf-style or StringParameter arguments),
// ASCII character classes, StringParameter/StringParameterBuilder. formatstring.cpp and formatparameterizedstring.cpp
// have no header of their own; placement of their declarations inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/slice.hpp"

namespace keen
{
    class MemoryAllocator;
    struct String;
    class WriteStream;

    enum StringParameterType
    {
        StringParameterType_Invalid = 0,
        StringParameterType_IntegerUnsigned = 1,
        StringParameterType_IntegerSigned = 2,
        StringParameterType_Float = 3,
        StringParameterType_String = 4,
    };

    // size 0x8
    class StringParameter
    {
        StringParameter(const StringParameter&);
    public:
        StringParameter(const char*);
        StringParameter(uint32);
        StringParameter(int);
        StringParameter(float);
        StringParameter(); // 0x00CBA710 (.h:197)
    private:
        void operator=(const StringParameter&);
    public:
        void operator=(const char* pParam); // 0x00BF41C0 (.h:206)
        void operator=(uint32 param); // 0x00BF41B0 (.h:205)
        void operator=(int param); // 0x00BF41A0 (.h:204)
        void operator=(float param); // 0x00BF4180 (.h:203)
    private:
        void assertInvalid() const; // 0x00BED460 (.h:212)

        friend uint32 formatString(char* pTargetBuffer, uint32 targetCapacity, const char* pFormat, const StringParameter* pParameters, uint32 parameterCount);

        StringParameterType type; // 0x00
        union
        {
            float floatValue; // 0x04
            uint32 integerValue; // 0x04
            const char* pStringValue; // 0x04
        };
    };

    // The inline members (stub_impl.md S2, kaiko::fillVariableGameSessionInteractionParams).
    // 0x00CBA710 (.h:197)
    inline StringParameter::StringParameter()
    {
        type = StringParameterType_Invalid;
    }

    // 0x00BED460 (.h:212): a check of debug builds only
    inline void StringParameter::assertInvalid() const
    {
    }

    // 0x00BF41C0 (.h:206)
    inline void StringParameter::operator=(const char* pParam)
    {
        assertInvalid();
        type = StringParameterType_String;
        pStringValue = pParam;
    }

    // 0x00BF41B0 (.h:205)
    inline void StringParameter::operator=(uint32 param)
    {
        assertInvalid();
        type = StringParameterType_IntegerUnsigned;
        integerValue = param;
    }

    // 0x00BF41A0 (.h:204)
    inline void StringParameter::operator=(int param)
    {
        assertInvalid();
        type = StringParameterType_IntegerSigned;
        integerValue = (uint32)param;
    }

    // 0x00BF4180 (.h:203)
    inline void StringParameter::operator=(float param)
    {
        assertInvalid();
        type = StringParameterType_Float;
        floatValue = param;
    }

    // size 0x84
    class StringParameterBuilder
    {
    public:
        StringParameterBuilder();
        StringParameterBuilder& operator<<(const char* pString); // 0x00BF7F00 (formatparameterizedstring.cpp:53)
        StringParameterBuilder& operator<<(uint32 param); // 0x00BF7ED0 (formatparameterizedstring.cpp:43)
        StringParameterBuilder& operator<<(int param); // 0x00BF7EA0 (formatparameterizedstring.cpp:33)
        StringParameterBuilder& operator<<(float param); // 0x00BF7E70 (formatparameterizedstring.cpp:23)
        uint32 getParameterCount() const; // 0x00BED470 (.h:245)
        const StringParameter* getParameter() const; // 0x00BED480 (.h:246)
    private:
        bool hasSpace() const; // 0x00BF00B0 (formatparameterizedstring.cpp:18)

        StringParameter m_parameters[16]; // 0x00
        uint32 m_parameterCount; // 0x80
    };

    String createString(const char* pStart, const char* pEnd); // 0x00BED400 (.h:18)
    String createEmptyString(); // 0x00BED420 (.h:19)
    uint32 getStringLength(String string); // 0x00BED440 (.h:22)
    bool isStringEmpty(String string); // 0x00BED450 (.h:23)
    uint32 copyString(char* pDest, const char* pSource, uint32 capacity); // 0x00741DB0 (.h:65)
    uint32 copyString(char* pDest, const char* pSourceBegin, const char* pSourceEnd, uint32 capacity); // 0x004B2200 (.h:70)
    uint32 copyUTF8String(char* pDest, const char* pSource, uint32 capacity); // 0x005D0930 (.h:75)
    String createStringFromCString(const char* pString); // 0x00BFA420 (:11)
    String duplicateString(MemoryAllocator* pAllocator, String string); // 0x00BF19E0 (:29)
    String findNextStringToken(const char* pString, const char separatorChar); // 0x00BF1A40 (:49)
    String copyString(char* pBuffer, uint32 capacity, String source); // 0x00C04240 (:67)
    int compareString(String a, String b); // 0x00BFA460 (:89)
    bool isStringEqual(String a, String b); // 0x00BFA480 (:94)
    bool isStringEqualNoCase(String a, String b); // 0x00BFA4B0 (:99)
    bool splitString(String* pFirstPart, String* pLastPart, String input, char splitChar); // 0x00BF1A80 (:104)
    bool splitString(String* pFirstPart, String* pLastPart, const char* pInput, char splitChar); // 0x00BFA4E0 (:126)
    bool splitAndTerminateSubString(Slice<char>* pFirstPart, Slice<char>* pRest, Slice<char> input, char splitChar); // 0x00C042A0 (:148)
    int compareString(String a, const char* pB); // 0x00BFA530 (:174)
    bool isStringEqual(String a, const char* pB); // 0x00BFA550 (:179)
    bool doesStringStartWith(String a, const char* pB); // 0x00BFA570 (:184)
    bool doesStringStartWith(const char* pString, const char* pPrefix); // 0x00BFA590 (:189)
    char* duplicateString(MemoryAllocator* pAllocator, const char* pSource); // 0x00BFA5C0 (:203)
    uint32 getStringLength(const char* pSource); // 0x00BF1AC0 (:241)
    uint32 getStringLengthUTF8(const char* pSource); // 0x00BFA650 (:263)
    uint32 copyString(char* pDest, uint32 capacity, const char* pSource); // 0x00BF1AE0 (:318)
    uint32 copyString(char* pDest, uint32 capacity, const char* pSourceBegin, const char* pSourceEnd); // 0x00BF1B30 (:345)
    uint32 copyUTF8String(char* pTarget, uint32 capacity, const char* pSource); // 0x00BFA6B0 (:381)
    uint32 copyUTF8String(char* pDest, uint32 capacity, const char* pSource, const char* pSourceEnd); // 0x00BFA780 (:450)
    bool isUTF8String(const char* pString, uint32 length); // 0x00BF1B70 (:488)

    namespace AsciiCharacter
    {
        bool isDecimalDigit(char ch); // 0x00BF1BE0 (:537)
        bool isHexadecimalDigit(char ch); // 0x00BF1BF0 (:542)
        bool isAlphabetic(char ch); // 0x00BF1C20 (:547)
        bool isWhiteSpace(char ch); // 0x00BF1C40 (:552)
        char toLower(char ch); // 0x00BF1C60 (:557)
    }

    char* changeStringToLowercase(char* pSource); // 0x00BF1C80 (:571)
    char* changeStringToUppercase(char* pSource); // 0x00BF1CB0 (:587)
    uint32 appendString(char* pDest, uint32 capacity, const char* pSource); // 0x00BFA850 (:603)
    uint32 appendString(char* pDest, uint32 capacity, const char* pBegin, const char* pEnd); // 0x00BFA900 (:624)
    char* findString(char* pString, const char letter); // 0x00BFA9B0 (:645)
    const char* findString(const char* pString, const char letter); // 0x00BF1CE0 (:652)
    char* findString(char* pString, const char* pToken); // 0x00BFA9D0 (:668)
    const char* findString(const char* pString, const char* pToken); // 0x00BF1D00 (:675)
    char* findFirstCharacterInString(char* pString, char letter); // 0x00BFA9E0 (:705)
    char* findFirstCharacterInString(char* pString, const char* pLetterArray); // 0x00BFAA00 (:710)
    const char* findFirstCharacterInString(const char* pString, char letter); // 0x00BF1D50 (:715)
    const char* findFirstCharacterInString(const char* pString, const char* pLetterArray); // 0x00BF1D70 (:720)
    char* findLastCharacterInString(char* pString, char letter); // 0x00BFAA10 (:745)
    char* findLastCharacterInString(char* pString, const char* pLetterArray); // 0x00BFAA40 (:750)
    const char* findLastCharacterInString(const char* pString, char letter); // 0x00BF1DB0 (:755)
    const char* findLastCharacterInString(const char* pString, const char* pLetterArray); // 0x00BF1DE0 (:775)
    uint32 countCharacterInString(const char* pString, char letter); // 0x00BF1E30 (:801)
    void replaceStringCharacters(char* pString, char sourceLetter, char replaceLetter); // 0x00BF1E60 (:820)
    void replaceStringCharacters(char* pString, const char* pSourceLetters, char replaceLetter); // 0x00BF1E80 (:835)
    int compareString(const char* pStringA, const char* pStringB); // 0x00BF1EC0 (:850)
    bool isStringEqual(const char* pStringA, const char* pStringB); // 0x00BF1F20 (:897)
    bool doesStringStartWith(const char* pBegin, const char* pEnd, const char* pPrefx); // 0x00BF1F40 (:902)
    int compareString(const char* pStringABegin, const char* pStringAEnd, const char* pStringB); // 0x00BF1F90 (:951)
    bool isStringEqual(const char* pStringABegin, const char* pStringAEnd, const char* pStringB); // 0x00BF2000 (:1011)
    int compareString(const char* pStringABegin, const char* pStringAEnd, const char* pStringBBegin, const char* pStringBEnd); // 0x00BF2020 (:1016)
    bool isStringEqual(const char* pStringABegin, const char* pStringAEnd, const char* pStringBBegin, const char* pStringBEnd); // 0x00BF20A0 (:1089)
    int compareStringNoCase(const char* pStringABegin, const char* pStringAEnd, const char* pStringBBegin, const char* pStringBEnd); // 0x00BF20D0 (:1094)
    bool isStringEqualNoCase(const char* pStringABegin, const char* pStringAEnd, const char* pStringBBegin, const char* pStringBEnd); // 0x00BF2170 (:1177)
    int compareStringNoCase(const char* pStringA, const char* pStringB); // 0x00BF21A0 (:1182)
    bool isStringEqualNoCase(const char* pStringA, const char* pStringB); // 0x00BF2210 (:1242)
    int compareStringNoCase(const char* pStringA, const char* pStringB, uint32 size); // 0x00BF2230 (:1247)
    bool isStringEqualNoCase(const char* pStringA, const char* pStringB, uint32 size); // 0x00BF22D0 (:1297)
    bool isStringEmpty(const char* pString); // 0x00BF22F0 (:1313)
    void trimString(char* pString, char trimChar); // 0x00BFAA50 (:1323)
    void trimStringLeft(char* pString, char trimChar); // 0x00BF2310 (:1329)
    void trimStringRight(char* pString, char trimChar); // 0x00BF2350 (:1351)
    void trimString(String* pString, char trimChar); // 0x00BFAAC0 (:1366)
    void trimStringLeft(String* pString, char trimChar); // 0x00BF2380 (:1372)
    void trimStringRight(String* pString, char trimChar); // 0x00BF23A0 (:1380)
    uint32 replaceString(char* pDest, uint32 capacity, const char* pSource, const char* pSearch, const char* pReplace, uint32 maxReplaceCount); // 0x00BFAB00 (:1388)
    void setStringLength(char* pString, uint32 capacity, uint32 newLength, char fillCharater); // 0x00BF23D0 (:1435)
    uint32 formatStringVarArgs(WriteStream* pStream, const char* pFormat, char* arguments); // 0x00C05880 (formatstring.cpp:31)
    uint32 formatStringVarArgs(char* pTargetBuffer, uint32 targetCapacity, const char* pFormat, char* arguments); // 0x00C05EC0 (formatstring.cpp:363)
    uint32 formatString(char*, uint32, const char*, ...); // 0x00C05F50 (formatstring.cpp:392)
    uint32 formatString(WriteStream*, const char*, ...); // 0x00C05F70 (formatstring.cpp:402)
    uint32 formatString(char* pTargetBuffer, uint32 targetCapacity, const char* pFormat, const StringParameterBuilder& stringParameterBuilder); // 0x00C083A0 (formatparameterizedstring.cpp:63)
    uint32 formatString(char* pTargetBuffer, uint32 targetCapacity, const char* pFormat, const StringParameter* pParameters, uint32 parameterCount); // 0x00C054D0 (formatparameterizedstring.cpp:68)
}
