// Original: library/modules/runtime/base/include/keen/base/xmlwriter.hpp
// Implementation: library/modules/runtime/base/sources/global/xmlwriter.cpp
//
// XmlWriter, writes XML elements and attributes to a WriteStream.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/textwriter.hpp"

namespace keen
{
    class WriteStream;

    // size 0x10C
    class XmlWriter
    {
    public:
        enum ElementType
        {
            ElementType_StartEnd = 0,
            ElementType_Empty = 1,
        };

        // size 0x8
        struct Tag
        {
            const char* pTagName; // 0x00
            bool closed; // 0x04
            bool emptyTag; // 0x05
        };

        XmlWriter(WriteStream* pBuffer, const char* pEncoding); // 0x00C06EE0 (:9)
        ~XmlWriter(); // 0x00BF3050 (:17)
        void openElement(const char* pElementName, ElementType type); // 0x00C09E50 (:21)
        void addAttribute(const char* pAttributename, uint32 value, const char* pFormatString); // 0x00C09EB0 (:55)
        void addAttribute(const char* pAttributename, int value, const char* pFormatString); // 0x00C06F60 (:60)
        void addAttribute(const char* pAttributename, const char* pValue); // 0x00C06F20 (:45)
        void addAttributeFloat32(const char* pAttributename, float value, const char* pFormatString); // 0x00C06FE0 (:72)
        void addBodyData(const char* pData); // 0x00C09FB0 (:104)
        void addBody(const char*, ...); // 0x00C09F30 (:84)
        void closeElement(); // 0x00C09FE0 (:114)
    private:
        void closeTag(bool addLinefeed); // 0x00C07060 (:132)
        void writeIndentation(); // 0x00BFB3D0 (:151)
        void writeEscapedString(const char* pString); // 0x00BFB420 (:169)

        Tag m_tagStack[32]; // 0x00
        uint32 m_tagStackSize; // 0x100
        TextWriter m_writer; // 0x104
        bool m_newLine; // 0x108
    };
}
