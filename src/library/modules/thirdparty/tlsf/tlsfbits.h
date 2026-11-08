// Original: library/modules/thirdparty/tlsf/tlsfbits.h (third party, recovered from rfg.exe)
//
// Bit scans. The build uses the generic loops (no intrinsics): fls/ffs return -1 for 0.
#pragma once

// 0x004010D0 (tlsfbits.h:63)
static int tlsf_fls(unsigned int word)
{
    int bit = 31;
    if (word == 0)
    {
        return -1;
    }
    while ((word >> bit) == 0)
    {
        --bit;
    }
    return bit;
}

// 0x004010E0 (tlsfbits.h:69)
static int tlsf_ffs(unsigned int word)
{
    int bit = 0;
    if (word == 0)
    {
        return -1;
    }
    while (((word >> bit) & 1) == 0)
    {
        ++bit;
    }
    return bit;
}
