// Original: library/modules/runtime/base/include/keen/base/stringbuilder.hpp
// Implementation: library/modules/runtime/base/sources/global/stringbuilder.cpp
//
// StringBuilder, appends formatted text into a fixed buffer.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x14
    class StringBuilder
    {
    public:
        StringBuilder(char* pDestination, uint32 capacity); // 0x00C005C0 (:21)
        StringBuilder(); // 0x00BF2410 (:11)
        void reset(char* pDestination, uint32 capacity); // 0x00BFAC10 (:26)
        void clear(); // 0x00BF2430 (:40)
        void setBuffer(char* pDestination, uint32 capacity); // 0x00BF2440 (:46)
        uint32 getSize(); // 0x00BF2460 (:54)
        bool appendString(const char* pBegin, const char* pEnd); // 0x00BFACB0 (:86)
        bool appendString(const char* pSource); // 0x00BFAC50 (:60)
        bool appendFormattedString(const char*, ...); // 0x00C09720 (:98)
        bool copyString(const char* pBegin, const char* pEnd); // 0x00BFAD60 (:135)
        bool copyString(const char* pSource); // 0x00BFAD00 (:129)
        bool appendCharacter(char source); // 0x00BF2470 (:141)
        bool isOverflown() const; // 0x00BEFBB0 (.h:39)
        bool isEmpty() const; // 0x00BEFBC0 (.h:40)
        void removeLastCharacter(); // 0x00BF24B0 (:161)
        void removeStringUntilCharacter(char letter); // 0x00BFAD70 (:172)
        char getLastCharacter() const; // 0x00BF24D0 (:180)

    private:
        char* m_pOriginalDestination; // 0x00
        uint32 m_originalCapacity; // 0x04
        char* m_pCurrentDestination; // 0x08
        uint32 m_remainingCapacity; // 0x0C
        bool m_overflow; // 0x10
    };
}
