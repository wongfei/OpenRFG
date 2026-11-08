// Original: library/modules/runtime/base/sources/global/language.cpp
//
// The language ids, their "xx-YY" strings and their families. Tables recovered from rfg.exe (s_languageEntries
// 0x0133D2A8 in .rdata, s_familyForLanguage 0x016EF670 in .data).

#include "keen/base/language.hpp"
#include "keen/base/string.hpp"

namespace keen
{
    // size 0x8
    struct LanguageEntry
    {
        LanguageId id; // 0x00
        const char* pString; // 0x04
    };

    static const LanguageEntry s_languageEntries[LanguageId_Count] = // 0x0133D2A8
    {
        { LanguageId_En_En, "en-EN" },
        { LanguageId_En_Us, "en-US" },
        { LanguageId_De_De, "de-DE" },
        { LanguageId_Fr_Fr, "fr-FR" },
        { LanguageId_It_It, "it-IT" },
        { LanguageId_Es_Es, "es-ES" },
        { LanguageId_Pl_Pl, "pl-PL" },
        { LanguageId_Ru_Ru, "ru-RU" },
        { LanguageId_Ja_Jp, "ja-JP" },
        { LanguageId_Pt_Br, "pt-BR" },
        { LanguageId_Pt_Pt, "pt-PT" },
        { LanguageId_Ko_Kr, "ko-KR" },
        { LanguageId_Cs_Cz, "cs-CZ" },
        { LanguageId_Es_Ar, "es-AR" },
        { LanguageId_Es_Bo, "es-BO" },
        { LanguageId_Es_Cl, "es-CL" },
        { LanguageId_Es_Co, "es-CO" },
        { LanguageId_Es_Cr, "es-CR" },
        { LanguageId_Es_Do, "es-DO" },
        { LanguageId_Es_Ec, "es-EC" },
        { LanguageId_Es_Gt, "es-GT" },
        { LanguageId_Es_Hn, "es-HN" },
        { LanguageId_Es_Mx, "es-MX" },
        { LanguageId_Es_Ni, "es-NI" },
        { LanguageId_Es_Pa, "es-PA" },
        { LanguageId_Es_Pe, "es-PE" },
        { LanguageId_Es_Pr, "es-PR" },
        { LanguageId_Es_Py, "es-PY" },
        { LanguageId_Es_Sv, "es-SV" },
        { LanguageId_Es_Uy, "es-UY" },
        { LanguageId_Es_Ve, "es-VE" },
        { LanguageId_Zh_Cn, "zh-CN" },
        { LanguageId_Zh_Tw, "zh-TW" },
        { LanguageId_Ar_Eg, "ar-EG" },
        { LanguageId_Ar_Dz, "ar-DZ" },
        { LanguageId_Ar_Bh, "ar-BH" },
        { LanguageId_Ar_Iq, "ar-IQ" },
        { LanguageId_Ar_Jo, "ar-JO" },
        { LanguageId_Ar_Kw, "ar-KW" },
        { LanguageId_Ar_Lb, "ar-LB" },
        { LanguageId_Ar_Ly, "ar-LY" },
        { LanguageId_Ar_Ma, "ar-MA" },
        { LanguageId_Ar_Om, "ar-OM" },
        { LanguageId_Ar_Qa, "ar-QA" },
        { LanguageId_Ar_Sa, "ar-SA" },
        { LanguageId_Ar_Sy, "ar-SY" },
        { LanguageId_Ar_Tn, "ar-TN" },
        { LanguageId_Ar_Ae, "ar-AE" },
        { LanguageId_Ar_Ye, "ar-YE" },
    };

    static LanguageFamily s_familyForLanguage[LanguageId_Count] = // 0x016EF670
    {
        LanguageFamily_En, LanguageFamily_En, LanguageFamily_De, LanguageFamily_Fr, LanguageFamily_It, LanguageFamily_Es,
        LanguageFamily_Pl, LanguageFamily_Ru, LanguageFamily_Ja, LanguageFamily_Pt, LanguageFamily_Pt, LanguageFamily_Ko,
        LanguageFamily_Cs,
        // es-AR .. es-VE
        LanguageFamily_Es_LA, LanguageFamily_Es_LA, LanguageFamily_Es_LA, LanguageFamily_Es_LA, LanguageFamily_Es_LA,
        LanguageFamily_Es_LA, LanguageFamily_Es_LA, LanguageFamily_Es_LA, LanguageFamily_Es_LA, LanguageFamily_Es_LA,
        LanguageFamily_Es_LA, LanguageFamily_Es_LA, LanguageFamily_Es_LA, LanguageFamily_Es_LA, LanguageFamily_Es_LA,
        LanguageFamily_Es_LA, LanguageFamily_Es_LA, LanguageFamily_Es_LA,
        LanguageFamily_Zh_Simplified, LanguageFamily_Zh_Traditional,
        // ar-EG .. ar-YE
        LanguageFamily_Arabic, LanguageFamily_Arabic, LanguageFamily_Arabic, LanguageFamily_Arabic, LanguageFamily_Arabic,
        LanguageFamily_Arabic, LanguageFamily_Arabic, LanguageFamily_Arabic, LanguageFamily_Arabic, LanguageFamily_Arabic,
        LanguageFamily_Arabic, LanguageFamily_Arabic, LanguageFamily_Arabic, LanguageFamily_Arabic, LanguageFamily_Arabic,
        LanguageFamily_Arabic,
    };

    // 0x00BF0CB0 (:75)
    const char* getLanguageIdString(LanguageId languageId)
    {
        return s_languageEntries[languageId].pString;
    }

    // 0x00BF0CC0
    LanguageFamily getLanguageFamily(LanguageId language)
    {
        return s_familyForLanguage[language];
    }

    // 0x00BFEE50
    // A full match ("de-DE", case-insensitive) first; otherwise the language part before the '-' is compared with each
    // entry's language part, so "de" or "de-AT" give de-DE and "es-XX" the first Spanish entry (es-ES). A string of more
    // than two characters without '-' matches nothing. LanguageId_Count when nothing matches.
    LanguageId parseLanguageIdFromString(const char* pLanguageString)
    {
        for (uint32 i = 0u; i < LanguageId_Count; ++i)
        {
            if (compareStringNoCase(pLanguageString, s_languageEntries[i].pString) == 0)
            {
                return s_languageEntries[i].id;
            }
        }

        const char* pLanguageEnd;
        const uint32 length = getStringLength(pLanguageString);
        if (length > 2u)
        {
            pLanguageEnd = pLanguageString;
            while (*pLanguageEnd != '-')
            {
                if (pLanguageEnd == pLanguageString + length)
                {
                    return LanguageId_Count;
                }
                ++pLanguageEnd;
            }
        }
        else
        {
            pLanguageEnd = pLanguageString + length;
        }

        for (uint32 i = 0u; i < LanguageId_Count; ++i)
        {
            const char* pEntry = s_languageEntries[i].pString;
            const char* pEntryEnd = pEntry + getStringLength(pEntry);
            // The entry's language part ends at its '-'; an entry without one would compare as empty (all have one).
            const char* pPartBegin = nullptr;
            const char* pPartEnd = nullptr;
            for (const char* p = pEntry; p != pEntryEnd; ++p)
            {
                if (*p == '-')
                {
                    pPartBegin = pEntry;
                    pPartEnd = p;
                    break;
                }
            }
            if (compareStringNoCase(pLanguageString, pLanguageEnd, pPartBegin, pPartEnd) == 0)
            {
                return s_languageEntries[i].id;
            }
        }
        return LanguageId_Count;
    }

    // The base language of each family (en-EN, de-DE, ..., es-MX for Latin American Spanish, ar-EG).
    static const LanguageId s_baseLanguageForFamily[LanguageFamily_Count] = // 0x016EF734
    {
        LanguageId_En_En, LanguageId_De_De, LanguageId_Fr_Fr, LanguageId_It_It, LanguageId_Es_Es, LanguageId_Pl_Pl,
        LanguageId_Ru_Ru, LanguageId_Ja_Jp, LanguageId_Pt_Pt, LanguageId_Ko_Kr, LanguageId_Cs_Cz, LanguageId_Es_Mx,
        LanguageId_Zh_Cn, LanguageId_Zh_Tw, LanguageId_Ar_Eg,
    };

    // 0x00BF0D00 (orig_main.md OM5, the localization's language choice)
    // The language itself when supported, else its family's base language, else a supported language of the same
    // family, else the first supported language.
    LanguageId findSupportedLanguage(LanguageId language, const LanguageId* pSupportedLanguages, uint32 numLanguages)
    {
        for (uint32 i = 0u; i < numLanguages; ++i)
        {
            if (pSupportedLanguages[i] == language)
            {
                return pSupportedLanguages[i];
            }
        }
        const LanguageId baseLanguage = s_baseLanguageForFamily[s_familyForLanguage[language]];
        for (uint32 i = 0u; i < numLanguages; ++i)
        {
            if (pSupportedLanguages[i] == baseLanguage)
            {
                return pSupportedLanguages[i];
            }
        }
        for (uint32 i = 0u; i < numLanguages; ++i)
        {
            if (s_baseLanguageForFamily[s_familyForLanguage[pSupportedLanguages[i]]] == baseLanguage)
            {
                return pSupportedLanguages[i];
            }
        }
        return *pSupportedLanguages;
    }
}
