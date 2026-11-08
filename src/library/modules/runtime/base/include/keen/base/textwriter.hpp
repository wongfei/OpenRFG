// Original: library/modules/runtime/base/include/keen/base/textwriter.hpp
// Implementation: library/modules/runtime/base/sources/global/textwriter.cpp
//
// TextWriter, formatted text output to a WriteStream.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/ioresult.hpp"

namespace keen
{
    class WriteStream;

    // size 0x4
    class TextWriter
    {
        TextWriter(const TextWriter&);
    public:
        TextWriter(WriteStream* pStream); // 0x00BF4350 (.h:19)
        void open(WriteStream* pStream); // 0x00BED760 (.h:21)
        void close();
        bool isOpen() const;
        bool hasError();
        IoError getError() const;
        void writeString(const char* pText, uint32 size); // 0x00BFB2D0 (:13)
        void writeString(const char* pText); // 0x00BFB270 (:8)
        void writeFormattedString(const char*, ...); // 0x00C06EA0 (:19)
        void writeFormattedStringVarArgs(const char* pFormat, char* vargs); // 0x00C06EC0 (:28)
    private:
        TextWriter& operator=(const TextWriter&);

        WriteStream* m_pStream; // 0x00
    };
}
