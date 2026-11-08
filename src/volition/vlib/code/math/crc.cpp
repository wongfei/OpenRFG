// Original: volition/vlib/code/math/crc.cpp
//
// Standard CRC-32 (polynomial 0xEDB88320) without the usual inversions: callers pass the start value (usually 0) and
// get the raw register back. crc_stri folds the characters with tolower() first. A null string or buffer gives 0.
// The inline checksum_* members of crc.h are defined here too.

#include "volition/vlib/code/math/crc.h"

#include <ctype.h>

namespace
{
    struct crc_table_builder
    {
        unsigned int values[256];

        constexpr crc_table_builder() : values()
        {
            for (unsigned int i = 0; i < 256; ++i)
            {
                unsigned int value = i;
                for (int bit = 0; bit < 8; ++bit)
                {
                    value = (value & 1) ? (value >> 1) ^ 0xEDB88320u : value >> 1;
                }
                values[i] = value;
            }
        }
    };

    constexpr crc_table_builder Crc_table_builder;
}

// 0x01518B80, initialized data in the binary (the standard table); also read by cfile.cpp (same unity object in the
// original, where it is file static).
extern const unsigned int* const CRC_table = Crc_table_builder.values;

const checksum_mem checksum_mem::INVALID; // 0x01D0CB68
const checksum_str checksum_str::INVALID; // 0x01CE9B9C
const checksum_stri checksum_stri::INVALID; // 0x01CE9988

// 0x0056B8F0 (:89)
void crc_init()
{
}

// 0x0056B900 (:123)
void crc_incremental(unsigned char c, unsigned int& crc)
{
    crc = (crc >> 8) ^ CRC_table[(c ^ crc) & 0xFF];
}

// 0x0056B920 (:144)
unsigned int crc_mem(const unsigned char* const buffer, int size, unsigned int crc)
{
    if (buffer == nullptr)
    {
        return 0;
    }
    const unsigned char* p = buffer;
    for (; size != 0; --size)
    {
        crc = (crc >> 8) ^ CRC_table[(*p++ ^ crc) & 0xFF];
    }
    return crc;
}

// 0x0056B960 (:168)
unsigned int crc_str(const char* const string, unsigned int crc)
{
    if (string == nullptr)
    {
        return 0;
    }
    for (const unsigned char* p = (const unsigned char*)string; *p != 0; ++p)
    {
        crc = (crc >> 8) ^ CRC_table[(*p ^ crc) & 0xFF];
    }
    return crc;
}

// 0x0056B9A0 (:196)
unsigned int crc_stri(const char* const string, unsigned int crc)
{
    if (string == nullptr)
    {
        return 0;
    }
    for (const unsigned char* p = (const unsigned char*)string; *p != 0; ++p)
    {
        crc = (crc >> 8) ^ CRC_table[((tolower(*p) & 0xFF) ^ crc) & 0xFF];
    }
    return crc;
}

// 0x0056B9F0 (:226)
checksum_mem::checksum_mem(const unsigned char* const buffer, int size, unsigned int crc)
{
    checksum = crc_mem(buffer, size, crc);
}

checksum_mem::checksum_mem(const checksum_mem& other)
{
    checksum = other.checksum;
}

checksum_mem::checksum_mem()
{
    checksum = INVALID.checksum;
}

checksum_mem& checksum_mem::operator=(const checksum_mem& other)
{
    checksum = other.checksum;
    return *this;
}

bool checksum_mem::operator==(const checksum_mem& other) const
{
    return checksum == other.checksum;
}

bool checksum_mem::operator!=(const checksum_mem& other) const
{
    return checksum != other.checksum;
}

unsigned int checksum_mem::get_crc() const
{
    return checksum;
}

checksum_str::checksum_str(unsigned int crc)
{
    checksum = crc;
}

// 0x0056BA40 (:252)
checksum_str::checksum_str(const char* const string, unsigned int crc)
{
    checksum = crc_str(string, crc);
}

checksum_str::checksum_str(const checksum_str& other)
{
    checksum = other.checksum;
}

checksum_str::checksum_str()
{
    checksum = INVALID.checksum;
}

checksum_str& checksum_str::operator=(const checksum_str& other)
{
    checksum = other.checksum;
    return *this;
}

// 0x00563B70 (.h:35)
bool checksum_str::operator==(const checksum_str& lhs) const
{
    return checksum == lhs.checksum;
}

bool checksum_str::operator!=(const checksum_str& lhs) const
{
    return checksum != lhs.checksum;
}

unsigned int checksum_str::get_crc() const
{
    return checksum;
}

// 0x0045AFA0 (.h:71)
checksum_stri::checksum_stri(unsigned int crc)
{
    checksum = crc;
}

// 0x0056BA90 (:316)
checksum_stri::checksum_stri(const char* const string, unsigned int crc)
{
    checksum = crc_stri(string, crc);
}

// 0x0083F460 (.h:67)
checksum_stri::checksum_stri(const checksum_stri& to_copy)
{
    checksum = to_copy.checksum;
}

// 0x004B6CF0 (.h:64)
checksum_stri::checksum_stri()
{
    checksum = INVALID.checksum;
}

// 0x004B6D00 (.h:74)
checksum_stri& checksum_stri::operator=(const checksum_stri& lhs)
{
    checksum = lhs.checksum;
    return *this;
}

// 0x0045AFB0 (.h:77)
bool checksum_stri::operator==(const checksum_stri& lhs) const
{
    return checksum == lhs.checksum;
}

// 0x0083F470 (.h:80)
bool checksum_stri::operator!=(const checksum_stri& lhs) const
{
    return checksum != lhs.checksum;
}

// 0x0045AFC0 (.h:83)
unsigned int checksum_stri::get_crc() const
{
    return checksum;
}

// 0x009419B0 (.h:91)
checksum_stri_pair::checksum_stri_pair(const char* _str)
    : str(_str), str_checksum(_str, 0)
{
}

// 0x0056BB00 (:383)
int checksum_lookup(const checksum_str* checksums, unsigned int num_checksums, const char* string)
{
    const unsigned int crc = crc_str(string, 0);
    for (unsigned int i = 0; i < num_checksums; ++i)
    {
        if (checksums[i].get_crc() == crc)
        {
            return (int)i;
        }
    }
    return -1;
}
