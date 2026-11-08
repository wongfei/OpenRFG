// Original: volition/vlib/code/math/crc.h
// Implementation: volition/vlib/code/math/crc.cpp
//
// CRC-32 helpers (table built by crc_init) and checksum wrappers: checksum_stri is the case-insensitive CRC of a
// string (crc_stri), used as a cheap name key (weapon_info::name_crc, prop_name::crc); checksum_str is case
// sensitive, checksum_mem hashes a buffer, checksum_stri_pair keeps the string next to its checksum.
#pragma once

// size 0x4
class checksum_mem
{
public:
    checksum_mem(const unsigned char* const buffer, int size, unsigned int crc); // 0x0056B9F0 (:226)
    checksum_mem(const checksum_mem&);
    checksum_mem();
    checksum_mem& operator=(const checksum_mem&);
    bool operator==(const checksum_mem&) const;
    bool operator!=(const checksum_mem&) const;
    unsigned int get_crc() const;

private:
    unsigned int checksum; // 0x00
public:
    static const checksum_mem INVALID; // 0x01D0CB68
};

// size 0x4
class checksum_str
{
public:
    checksum_str(unsigned int);
    checksum_str(const char* const string, unsigned int crc); // 0x0056BA40 (:252)
    checksum_str(const checksum_str&);
    checksum_str();
    checksum_str& operator=(const checksum_str&);
    bool operator==(const checksum_str& lhs) const; // 0x00563B70 (.h:35)
    bool operator!=(const checksum_str&) const;
    unsigned int get_crc() const;

private:
    unsigned int checksum; // 0x00
public:
    static const checksum_str INVALID; // 0x01CE9B9C
};

// size 0x4
class checksum_stri
{
public:
    checksum_stri(unsigned int crc); // 0x0045AFA0 (.h:71)
    checksum_stri(const char* const string, unsigned int crc); // 0x0056BA90 (:316)
    checksum_stri(const checksum_stri& to_copy); // 0x0083F460 (.h:67)
    checksum_stri(); // 0x004B6CF0 (.h:64)
    checksum_stri& operator=(const checksum_stri& lhs); // 0x004B6D00 (.h:74)
    bool operator==(const checksum_stri& lhs) const; // 0x0045AFB0 (.h:77)
    bool operator!=(const checksum_stri& lhs) const; // 0x0083F470 (.h:80)
    unsigned int get_crc() const; // 0x0045AFC0 (.h:83)

private:
    unsigned int checksum; // 0x00
public:
    static const checksum_stri INVALID; // 0x01CE9988
};

// size 0x8
class checksum_stri_pair
{
public:
    checksum_stri_pair(const char* _str); // 0x009419B0 (.h:91)

    const char* str; // 0x00
    checksum_stri str_checksum; // 0x04
};

void crc_init(); // 0x0056B8F0 (:89)
void crc_incremental(unsigned char c, unsigned int& crc); // 0x0056B900 (:123)
unsigned int crc_mem(const unsigned char* const buffer, int size, unsigned int crc); // 0x0056B920 (:144)
unsigned int crc_str(const char* const string, unsigned int crc); // 0x0056B960 (:168)
unsigned int crc_stri(const char* const string, unsigned int crc); // 0x0056B9A0 (:196)
int checksum_lookup(const checksum_str* checksums, unsigned int num_checksums, const char* string); // 0x0056BB00 (:383)
