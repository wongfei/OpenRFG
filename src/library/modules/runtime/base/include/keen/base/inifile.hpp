// Original: library/modules/runtime/base/include/keen/base/inifile.hpp
// Implementation: library/modules/runtime/base/sources/global/inifile.cpp
//
// Reading and writing typed values in ini files.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class FilePath;

    bool readIniFileString(char* pResult, uint32 capacity, const char* pFilename, const char* pSection, const char* pKey); // 0x00C0B460 (:143)
    void writeIniFileString(const char* pFilename, const char* pSection, const char* pKey, const char* pValue); // 0x00BF83D0 (:319)
    void readIniFileString(const FilePath& path, const char* pSection, const char* pKey, char* pResult, uint32 capacity, const char* pDefaultValue); // 0x00C0B560 (:353)
    int readIniFileInt(const FilePath& path, const char* pSection, const char* pKey, int defaultValue); // 0x00C0B5B0 (:361)
    float readIniFileFloat(const FilePath& path, const char* pSection, const char* pKey, float defaultValue); // 0x00C0B650 (:379)
    bool readIniFileBool(const FilePath& path, const char* pSection, const char* pKey, bool defaultValue); // 0x00C0B700 (:397)
    void writeIniFileString(const FilePath& path, const char* pSection, const char* pKey, const char* pValue); // 0x00C05F90 (:415)
    void writeIniFileInt(const FilePath& path, const char* pSection, const char* pKey, int value); // 0x00C05FC0 (:420)
    void writeIniFileFloat(const FilePath& path, const char* pSection, const char* pKey, float value); // 0x00C06030 (:428)
    void writeIniFileBool(const FilePath& path, const char* pSection, const char* pKey, bool value); // 0x00C060B0 (:436)
}
