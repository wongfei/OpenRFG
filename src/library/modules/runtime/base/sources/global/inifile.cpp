// Original: library/modules/runtime/base/sources/global/inifile.cpp
//
// Typed ini file values through the Win32 profile API (GetPrivateProfileStringW), converted from and to UTF-8.
//
// Writers since stub_impl.md S5 (game_options_write_ini_file_from_cache).

#include "keen/base/inifile.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/stringscanner.hpp"

#include <windows.h>

namespace keen
{
    // 0x00C0B460 (:143)
    bool readIniFileString(char* pResult, uint32 capacity, const char* pFilename, const char* pSection, const char* pKey)
    {
        wchar_t section[128];
        wchar_t key[128];
        wchar_t fileName[520];
        wchar_t value[520];
        convertUTF8ToUTF16((uint16*)section, sizeof(section), pSection);
        convertUTF8ToUTF16((uint16*)key, sizeof(key), pKey);
        // Magic number: 0x410 bytes are the capacity of the file name buffer (the original compares the converted size
        // with it).
        if (convertUTF8ToUTF16((uint16*)fileName, 0x410, pFilename) > 0x410)
        {
            return false;
        }
        if (GetPrivateProfileStringW(section, key, nullptr, value, 0x410, fileName) == 0)
        {
            return false;
        }
        return convertUTF16ToUTF8(pResult, capacity, (const uint16*)value) <= capacity;
    }

    // 0x00BF83D0 (:319)
    // A null value deletes the key (WritePrivateProfileStringW). The conversions are not checked.
    void writeIniFileString(const char* pFilename, const char* pSection, const char* pKey, const char* pValue)
    {
        wchar_t section[128];
        wchar_t key[128];
        wchar_t fileName[520];
        wchar_t value[520];
        convertUTF8ToUTF16((uint16*)section, sizeof(section), pSection);
        convertUTF8ToUTF16((uint16*)key, sizeof(key), pKey);
        convertUTF8ToUTF16((uint16*)fileName, sizeof(fileName), pFilename);
        const wchar_t* pString = nullptr;
        if (pValue != nullptr)
        {
            convertUTF8ToUTF16((uint16*)value, sizeof(value), pValue);
            pString = value;
        }
        WritePrivateProfileStringW(section, key, pString, fileName);
    }

    // 0x00C0B560 (:353)
    void readIniFileString(const FilePath& path, const char* pSection, const char* pKey, char* pResult, uint32 capacity, const char* pDefaultValue)
    {
        if (!readIniFileString(pResult, capacity, path.getCompletePath(), pSection, pKey))
        {
            copyString(pResult, capacity, pDefaultValue);
        }
    }

    // 0x00C0B5B0 (:361)
    int readIniFileInt(const FilePath& path, const char* pSection, const char* pKey, int defaultValue)
    {
        char buffer[256];
        if (readIniFileString(buffer, sizeof(buffer), path.getCompletePath(), pSection, pKey))
        {
            const char* pString = buffer;
            int value;
            if (readSint32FromDecimalString(&value, &pString) == StringReadResult_Ok)
            {
                return value;
            }
        }
        return defaultValue;
    }

    // 0x00C0B650 (:379)
    float readIniFileFloat(const FilePath& path, const char* pSection, const char* pKey, float defaultValue)
    {
        char buffer[256];
        if (readIniFileString(buffer, sizeof(buffer), path.getCompletePath(), pSection, pKey))
        {
            const char* pString = buffer;
            float value;
            if (readApproximateFloatFromDecimalString(&value, &pString) == StringReadResult_Ok)
            {
                return value;
            }
        }
        return defaultValue;
    }

    // 0x00C0B700 (:397)
    // Anything other than "true" (case-insensitive) reads as false; the default only applies to a missing key.
    bool readIniFileBool(const FilePath& path, const char* pSection, const char* pKey, bool defaultValue)
    {
        char buffer[256];
        if (readIniFileString(buffer, sizeof(buffer), path.getCompletePath(), pSection, pKey))
        {
            return compareStringNoCase(buffer, "true") == 0;
        }
        return defaultValue;
    }

    // 0x00C05F90 (:415)
    void writeIniFileString(const FilePath& path, const char* pSection, const char* pKey, const char* pValue)
    {
        writeIniFileString(path.getCompletePath(), pSection, pKey, pValue);
    }

    // 0x00C05FC0 (:420)
    void writeIniFileInt(const FilePath& path, const char* pSection, const char* pKey, int value)
    {
        char buffer[32];
        formatString(buffer, sizeof(buffer), "%d", value);
        writeIniFileString(path.getCompletePath(), pSection, pKey, buffer);
    }

    // 0x00C06030 (:428)
    void writeIniFileFloat(const FilePath& path, const char* pSection, const char* pKey, float value)
    {
        char buffer[32];
        formatString(buffer, sizeof(buffer), "%f", (double)value);
        writeIniFileString(path.getCompletePath(), pSection, pKey, buffer);
    }

    // 0x00C060B0 (:436)
    void writeIniFileBool(const FilePath& path, const char* pSection, const char* pKey, bool value)
    {
        writeIniFileString(path.getCompletePath(), pSection, pKey, value ? "true" : "false");
    }
}
