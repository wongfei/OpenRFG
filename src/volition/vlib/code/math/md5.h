// Original: volition/vlib/code/math/md5.h
// Implementation: volition/vlib/code/math/md5.cpp
//
// MD5 digests (md5_value).
#pragma once

// size 0x10
struct md5_value
{
    md5_value(); // 0x005782B0 (.h:14)
    bool operator!=(const md5_value& other) const; // 0x007C9340 (.h:15)
    bool operator==(const md5_value&) const;

    unsigned int val0; // 0x00
    unsigned int val1; // 0x04
    unsigned int val2; // 0x08
    unsigned int val3; // 0x0C
};

md5_value console_md5_mem(const unsigned char* const buffer, unsigned int size); // 0x00578300 (:89)
