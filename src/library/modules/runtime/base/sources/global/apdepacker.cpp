// Original: library/modules/runtime/base/sources/global/apdepacker.cpp
//
// aPLib depacker (the reference "depack.c", unchecked: the caller guarantees the destination is large enough).
// Used by PakFileDevice for 4 KB chunks.

#include "keen/base/apdepacker.hpp"

namespace keen
{
    namespace compression
    {
        // size 0x10
        struct APDEPACKDATA
        {
            const uint8* source; // 0x00
            uint8* destination; // 0x04
            uint32 tag; // 0x08
            uint32 bitcount; // 0x0C
        };

        // 0x00BEDF30 (:28)
        static uint32 aP_getbit(APDEPACKDATA* ud)
        {
            // Check if tag is empty.
            if (!ud->bitcount--)
            {
                // Load next tag.
                ud->tag = *ud->source++;
                ud->bitcount = 7;
            }
            // Shift bit out of tag.
            const uint32 bit = (ud->tag >> 7) & 0x01;
            ud->tag <<= 1;
            return bit;
        }

        // 0x00BEDF60 (:47)
        static uint32 aP_getgamma(APDEPACKDATA* ud)
        {
            uint32 result = 1;
            // Input gamma2-encoded bits.
            do
            {
                result = (result << 1) + aP_getbit(ud);
            }
            while (aP_getbit(ud));
            return result;
        }

        // 0x00BEDFD0 (:59)
        uint32 apdepack(const void* source, void* destination)
        {
            APDEPACKDATA ud;
            uint32 offs = 0;
            uint32 len = 0;
            uint32 R0 = 0;
            int done = 0;
            int LWM = 0;

            ud.source = (const uint8*)source;
            ud.destination = (uint8*)destination;
            ud.bitcount = 0;
            ud.tag = 0;

            // The first byte is a literal.
            *ud.destination++ = *ud.source++;

            // Main decompression loop.
            while (!done)
            {
                if (aP_getbit(&ud))
                {
                    if (aP_getbit(&ud))
                    {
                        if (aP_getbit(&ud))
                        {
                            // Short offset (4 bits), 0 writes a zero byte.
                            offs = 0;
                            for (int i = 4; i; i--)
                            {
                                offs = (offs << 1) + aP_getbit(&ud);
                            }
                            if (offs)
                            {
                                *ud.destination = *(ud.destination - offs);
                                ud.destination++;
                            }
                            else
                            {
                                *ud.destination++ = 0x00;
                            }
                            LWM = 0;
                        }
                        else
                        {
                            // 7-bit offset, 2-3 bytes; offset 0 ends the stream.
                            offs = *ud.source++;
                            len = 2 + (offs & 0x0001);
                            offs >>= 1;
                            if (offs)
                            {
                                for (; len; len--)
                                {
                                    *ud.destination = *(ud.destination - offs);
                                    ud.destination++;
                                }
                            }
                            else
                            {
                                done = 1;
                            }
                            R0 = offs;
                            LWM = 1;
                        }
                    }
                    else
                    {
                        offs = aP_getgamma(&ud);
                        if ((LWM == 0) && (offs == 2))
                        {
                            // Repeat the last offset.
                            offs = R0;
                            len = aP_getgamma(&ud);
                            for (; len; len--)
                            {
                                *ud.destination = *(ud.destination - offs);
                                ud.destination++;
                            }
                        }
                        else
                        {
                            if (LWM == 0)
                            {
                                offs -= 3;
                            }
                            else
                            {
                                offs -= 2;
                            }
                            offs <<= 8;
                            offs += *ud.source++;
                            len = aP_getgamma(&ud);
                            // Magic numbers: aPLib length adjustments for long (>= 32000, >= 1280) and short (< 128)
                            // offsets.
                            if (offs >= 32000)
                            {
                                len++;
                            }
                            if (offs >= 1280)
                            {
                                len++;
                            }
                            if (offs < 128)
                            {
                                len += 2;
                            }
                            for (; len; len--)
                            {
                                *ud.destination = *(ud.destination - offs);
                                ud.destination++;
                            }
                            R0 = offs;
                        }
                        LWM = 1;
                    }
                }
                else
                {
                    // Literal byte.
                    *ud.destination++ = *ud.source++;
                    LWM = 0;
                }
            }

            // Return the decompressed size.
            return (uint32)(ud.destination - (uint8*)destination);
        }
    }
}
