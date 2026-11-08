// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/hkstreamreader.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SA): the default skip and peek of a stream reader, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/hkstreamreader.h"

// 0x00D17580: reads until n bytes are gone or a read returns nothing (the bytes go to a stack buffer each time)
int hkStreamReader::skip(int nbytes)
{
    char buf[512];
    int left = nbytes;
    while (left != 0)
    {
        const int n = read(buf, left < (int)sizeof(buf) ? left : (int)sizeof(buf));
        if (n == 0)
        {
            break;
        }
        left -= n;
    }
    return nbytes - left;
}

// 0x00D175E0: no peeking
int hkStreamReader::peek(void* buf, int nbytes)
{
    return 0;
}
