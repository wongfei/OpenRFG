// Original: library/modules/runtime/base/include/keen/base/stringtokenizer.hpp
// Implementation: library/modules/runtime/base/sources/global/stringtokenizer.cpp
//
// StringTokenizer, splits a string at delimiters.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x4
    class StringTokenizer
    {
    public:
        StringTokenizer(char* pString) // 0x00CCD220 (.h:20)
        {
            setString(pString);
        }
        StringTokenizer()
        {
        }
        void setString(char* pString); // 0x00BF2DC0 (:11)
        const char* getToken(const char* pSeparatorString); // 0x00BFB0E0 (:16)

    private:
        char* m_pNextToken; // 0x00
    };
}
