// Original: library/modules/runtime/base/include/keen/base/bitarray.hpp
// Implementation: library/modules/runtime/base/sources/global/bitarray.cpp
//
// BitArray, a dynamically allocated array of bits with bit references.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"

namespace keen
{
    class MemoryAllocator;

    // size 0x8
    class BitArray
    {
    public:
        enum
        {
            BaseTypeBitCount = 8,
        };

        typedef uint8 BaseType;

        // size 0x8
        class ConstBitReference
        {
            ConstBitReference(const uint8& data, uint32 bitIndex); // 0x00BEEA10 (:87)
        class BitReference;
        public:
            ConstBitReference(BitReference& ref); // 0x00BEE9A0 (:64)
            ConstBitReference(ConstBitReference& ref); // 0x00BEE980 (:59)
            ~ConstBitReference(); // 0x00BEE9C0 (:69)
            operator bool() const; // 0x00BEE9D0 (:74)
            ConstBitReference& operator=(const BitReference& ref); // 0x00BEE9F0 (:79)

        private:
            const uint8& m_data; // 0x00
            uint32 m_bitIndex; // 0x04
        };

        // size 0x8
        class BitReference
        {
        public:
            ~BitReference(); // 0x00BEE8C0 (:12)
            operator bool() const; // 0x00BEE8D0 (:16)
            BitReference& flip(); // 0x00BF6950 (:21)
            BitReference& operator=(const ConstBitReference&);
            BitReference& operator=(const BitReference& value); // 0x00BEE920 (:41)
            BitReference& operator=(bool value); // 0x00BEE8F0 (:28)
        private:
            BitReference(uint8& data, uint32 bitIndex); // 0x00BEE960 (:54)

            uint8& m_data; // 0x00
            uint32 m_bitIndex; // 0x04
        };

    private:
        BitArray(const BitArray&);
    public:
        BitArray(); // 0x00BF6980 (:91)
        ~BitArray(); // 0x00BF6990 (:95)
        bool create(MemoryAllocator* pAllocator, uint32 bitCount, uint32 alignment); // 0x00BFC8F0 (:99)
        void destroy(MemoryAllocator* pAllocator); // 0x00BFC930 (:104)
        void fill(bool value); // 0x00BFC960 (:109)
        bool isEmpty() const; // 0x00BF69A0 (:125)
        uint32 getSize() const; // 0x00BF69B0 (:130)
        ConstBitReference operator[](uint32 index) const; // 0x00BF69F0 (:142)
        BitReference operator[](uint32 index); // 0x00BF69C0 (:135)
        void operator<<(uint32 amount); // 0x00BF6A20 (:150)
        void swap(BitArray& other); // 0x00BFC9A0 (:183)
    private:
        BitArray& operator=(const BitArray&);

        Array<uint8> m_data; // 0x00
    };
}
