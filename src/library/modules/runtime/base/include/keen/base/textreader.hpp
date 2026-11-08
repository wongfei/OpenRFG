// Original: library/modules/runtime/base/include/keen/base/textreader.hpp
// Implementation: library/modules/runtime/base/sources/global/textreader.cpp
//
// TextReader, line-based reading of text from a ReadStream.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/ioresult.hpp"
#include "keen/base/slice.hpp"

namespace keen
{
    class ReadStream;

    // size 0x8
    class TextReader
    {
        TextReader(const TextReader&);
    public:
        TextReader(ReadStream*);
        void open(ReadStream*);
        void close();
        bool isOpen() const;
        uint32 getCurrentLineNumber() const;
        bool hasError();
        IoError getError() const;
        bool readLine(char* pBuffer, uint32 capacity); // 0x00C04340 (:8)
        bool readLineToSlice(Slice<char>* pTarget); // 0x00BFB170 (:23)
    private:
        TextReader& operator=(const TextReader&);

        ReadStream* m_pBuffer; // 0x00
        uint32 m_currentLineNumber; // 0x04
    };
}
