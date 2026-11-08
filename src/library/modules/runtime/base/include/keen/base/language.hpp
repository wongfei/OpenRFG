// Original: library/modules/runtime/base/include/keen/base/language.hpp
// Implementation: library/modules/runtime/base/sources/global/language.cpp
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum LanguageId
    {
        LanguageId_En_En = 0,
        LanguageId_En_Us = 1,
        LanguageId_De_De = 2,
        LanguageId_Fr_Fr = 3,
        LanguageId_It_It = 4,
        LanguageId_Es_Es = 5,
        LanguageId_Pl_Pl = 6,
        LanguageId_Ru_Ru = 7,
        LanguageId_Ja_Jp = 8,
        LanguageId_Pt_Br = 9,
        LanguageId_Pt_Pt = 10,
        LanguageId_Ko_Kr = 11,
        LanguageId_Cs_Cz = 12,
        LanguageId_Es_Ar = 13,
        LanguageId_Es_Bo = 14,
        LanguageId_Es_Cl = 15,
        LanguageId_Es_Co = 16,
        LanguageId_Es_Cr = 17,
        LanguageId_Es_Do = 18,
        LanguageId_Es_Ec = 19,
        LanguageId_Es_Gt = 20,
        LanguageId_Es_Hn = 21,
        LanguageId_Es_Mx = 22,
        LanguageId_Es_Ni = 23,
        LanguageId_Es_Pa = 24,
        LanguageId_Es_Pe = 25,
        LanguageId_Es_Pr = 26,
        LanguageId_Es_Py = 27,
        LanguageId_Es_Sv = 28,
        LanguageId_Es_Uy = 29,
        LanguageId_Es_Ve = 30,
        LanguageId_Zh_Cn = 31,
        LanguageId_Zh_Tw = 32,
        LanguageId_Ar_Eg = 33,
        LanguageId_Ar_Dz = 34,
        LanguageId_Ar_Bh = 35,
        LanguageId_Ar_Iq = 36,
        LanguageId_Ar_Jo = 37,
        LanguageId_Ar_Kw = 38,
        LanguageId_Ar_Lb = 39,
        LanguageId_Ar_Ly = 40,
        LanguageId_Ar_Ma = 41,
        LanguageId_Ar_Om = 42,
        LanguageId_Ar_Qa = 43,
        LanguageId_Ar_Sa = 44,
        LanguageId_Ar_Sy = 45,
        LanguageId_Ar_Tn = 46,
        LanguageId_Ar_Ae = 47,
        LanguageId_Ar_Ye = 48,
        LanguageId_Count = 49,
    };

    enum LanguageFamily
    {
        LanguageFamily_En = 0,
        LanguageFamily_De = 1,
        LanguageFamily_Fr = 2,
        LanguageFamily_It = 3,
        LanguageFamily_Es = 4,
        LanguageFamily_Pl = 5,
        LanguageFamily_Ru = 6,
        LanguageFamily_Ja = 7,
        LanguageFamily_Pt = 8,
        LanguageFamily_Ko = 9,
        LanguageFamily_Cs = 10,
        LanguageFamily_Es_LA = 11,
        LanguageFamily_Zh_Simplified = 12,
        LanguageFamily_Zh_Traditional = 13,
        LanguageFamily_Arabic = 14,
        LanguageFamily_Count = 15,
    };

    const char* getLanguageIdString(LanguageId languageId); // 0x00BF0CB0 (language.cpp:75)
    LanguageFamily getLanguageFamily(LanguageId language); // 0x00BF0CC0
    LanguageId parseLanguageIdFromString(const char* pLanguageString); // 0x00BFEE50
    LanguageId findSupportedLanguage(LanguageId language, const LanguageId* pSupportedLanguages, uint32 numLanguages); // 0x00BF0D00
}
