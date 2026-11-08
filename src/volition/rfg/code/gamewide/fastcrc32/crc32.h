// Original: volition/rfg/code/gamewide/fastcrc32/crc32.h
// Implementation: volition/rfg/code/gamewide/fastcrc32/crc32.inl
//
// Fast CRC32 implementations (Stephan Brumme's crc32 code: bitwise, half-byte, 1/4/8/16-byte slicing, tableless);
// crc32_fast picks the best one.
#pragma once

unsigned int crc32_bitwise(const void* data, unsigned int length, unsigned int previousCrc32); // 0x007C5DA0 (crc32.inl:79)
unsigned int crc32_halfbyte(const void* data, unsigned int length, unsigned int previousCrc32); // 0x007C5E50 (crc32.inl:106)
unsigned int crc32_1byte(const void* data, unsigned int length, unsigned int previousCrc32); // 0x007C5EA0 (crc32.inl:131)
unsigned int crc32_1byte_tableless(const void* data, unsigned int length, unsigned int previousCrc32); // 0x007C5EE0 (crc32.inl:145)
unsigned int crc32_1byte_tableless2(const void* data, unsigned int length, unsigned int previousCrc32); // 0x007C5F50 (crc32.inl:199)
unsigned int crc32_4bytes(const void* data, unsigned int length, unsigned int previousCrc32); // 0x007C6000 (crc32.inl:226)
unsigned int crc32_8bytes(const void* data, unsigned int length, unsigned int previousCrc32); // 0x007C6090 (crc32.inl:263)
unsigned int crc32_4x8bytes(const void* data, unsigned int length, unsigned int previousCrc32); // 0x007C6170 (crc32.inl:308)
unsigned int crc32_16bytes(const void* data, unsigned int length, unsigned int previousCrc32); // 0x007C63D0 (crc32.inl:363)
unsigned int crc32_16bytes_prefetch(const void* data, unsigned int length, unsigned int previousCrc32, unsigned int prefetchAhead); // 0x007C6830 (crc32.inl:434)
unsigned int crc32_fast(const void* data, unsigned int length, unsigned int previousCrc32); // 0x007C6CB0 (crc32.inl:511)
