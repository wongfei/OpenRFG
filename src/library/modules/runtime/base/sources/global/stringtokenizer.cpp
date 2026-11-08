// Original: library/modules/runtime/base/sources/global/stringtokenizer.cpp
//
// StringTokenizer: splits a writable string in place at any of the separator characters (stub_impl.md S2, for the
// Steam save data provider's file names).

#include "keen/base/stringtokenizer.hpp"
#include "keen/base/string.hpp"

namespace keen
{
    // 0x00BF2DC0 (:11)
    void StringTokenizer::setString(char* pString)
    {
        m_pNextToken = pString;
    }

    // 0x00BFB0E0 (:16): the next token (the separator is overwritten with a terminator), nullptr past the last one
    const char* StringTokenizer::getToken(const char* pSeparatorString)
    {
        char* pToken = m_pNextToken;
        if (pToken == nullptr)
        {
            return nullptr;
        }
        m_pNextToken = findFirstCharacterInString(pToken, pSeparatorString);
        if (m_pNextToken != nullptr)
        {
            *m_pNextToken = '\0';
            m_pNextToken++;
        }
        return pToken;
    }
}
