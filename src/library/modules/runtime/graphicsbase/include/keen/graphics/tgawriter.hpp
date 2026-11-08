// Original: library/modules/runtime/graphicsbase/include/keen/graphics/tgawriter.hpp
// Implementation: library/modules/runtime/graphicsbase/sources/global/tgawriter.cpp
//
// TGAWriter, writes uncompressed TGA images to a WriteStream (screenshots).
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/binarywriter.hpp"

namespace keen
{
    class WriteStream;

    // size 0x18
    class TGAWriter
    {
    public:
        enum Format
        {
            Format_BGR = 0,
            Format_BGRA = 1,
            Format_Count = 2,
        };

        // size 0x4
        struct BGRAPixel
        {
            uint8 b; // 0x00
            uint8 g; // 0x01
            uint8 r; // 0x02
            uint8 a; // 0x03
        };

        // size 0x3
        struct BGRPixel
        {
            uint8 b; // 0x00
            uint8 g; // 0x01
            uint8 r; // 0x02
        };

        TGAWriter(); // 0x00CE3A80 (:8)
        ~TGAWriter(); // 0x00CE55A0 (:14)
        void start(WriteStream* pStream, uint32 width, uint32 height, Format format); // 0x00CE55E0 (:19)
        void writeLine(const void* pLine); // 0x00CE5730 (:64)
        void writeImage(const void* pImage); // 0x00CE56D0 (:52)
        bool stop(); // 0x00CE3AA0 (:78)
    private:
        uint32 getPixelSizeInBytes() const; // 0x00CE3AE0 (:94)
        uint32 getAlphaBits() const; // 0x00CE3B00 (:102)

        BinaryWriter m_writer; // 0x00
        uint32 m_width; // 0x08
        uint32 m_height; // 0x0C
        Format m_format; // 0x10
        uint32 m_lineCount; // 0x14
    };
}
