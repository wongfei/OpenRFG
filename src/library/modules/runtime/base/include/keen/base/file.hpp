// Original: library/modules/runtime/base/include/keen/base/file.hpp
// Implementation: library/modules/runtime/base/sources/global/file.cpp
//
// File, a file of the keen file system opened for typed, buffered reads and writes.
#pragma once

#include "keen/base/types.hpp"
#include <stdint.h>
#include "keen/base/endianness.hpp"
#include "keen/base/fileaccessmode.hpp"

namespace keen
{
    struct FileSystem;

    // size 0x20
    class File
    {
    public:
        enum State
        {
            State_Ready = 0,
            State_Error = 1,
            State_Count = 2,
        };

    private:
        File(const File&);
    public:
        File(); // 0x00BEFB40 (:15)
        ~File(); // 0x00C01240 (:28)
        bool openFromHandle(FileSystem* pFileSystem, uint32 fileHandle, Endianness sourceEndianness); // 0x00C01290 (:33)
        bool open(FileSystem* pFileSystem, const char* pFileName, FileAccessMode accessMode, Endianness sourceEndianness); // 0x00C078F0 (:65)
        uint32 close(); // 0x00BFCAA0 (:95)
        void swap(File*);
        bool isOpen() const; // 0x00BEFB60 (:120)
        bool needsEndianSwapping() const;
        uint32 read(void* pTarget, uint32 size); // 0x00BFCAF0 (:141)
        uint32 write(const void* pSrc, uint32 size); // 0x00BFCB40 (:177)
        void flushWriteBuffer(); // 0x00BFCBB0 (:197)
        FileSystem* getFileSystem();
        uint32 getFileHandle(); // 0x00CDC410 (.h:51)
        uint64 getSize() const; // 0x00BFCBF0 (:211)
        void setPosition(uint64 pointer); // 0x00BFCCA0 (:245)
        uint64 getPosition() const; // 0x00BEFB20 (.h:55)
        void movePosition(sint64 offset); // 0x00C07960 (:273)
        void alignPosition(uint32 alignment); // 0x00C047B0 (:317)
        bool readLine(char* pTarget, uint32 len); // 0x00BFCD00 (:348)
        uint32 writeFormattedString(const char*, ...); // 0x00C07BF0 (:1151)
        uint32 writeString(const char* pString); // 0x00BFCE90 (:1210)
        void setDataEndianness(Endianness endianness); // 0x00BEFB70 (:409)
        Endianness getDataEndianness() const; // 0x00BEFB80 (:414)
        State getState() const;
        bool isInErrorState() const; // 0x00BEFB30 (.h:69)
        bool isEndOfFile() const; // 0x00C01210 (.h:70)
        void setErrorState(); // 0x00BEFB90 (:426)
        void resetErrorState(); // 0x00BEFBA0 (:434)
        bool readBool(); // 0x00C01910 (:667)
        void readBool(bool* pTarget, uint32 count); // 0x00C047F0 (:454)
        sint8 readSint8(); // 0x00C01970 (:674)
        void readSint8(sint8* pTarget, uint32 count); // 0x00C01390 (:476)
        uint8 readUint8(); // 0x00C019D0 (:681)
        void readUint8(uint8* pTarget, uint32 count); // 0x00C013F0 (:489)
        sint16 readSint16(); // 0x00C01A30 (:688)
        void readSint16(sint16* pTarget, uint32 count); // 0x00C01450 (:505)
        uint16 readUint16(); // 0x00C01A90 (:699)
        void readUint16(uint16* pTarget, uint32 count); // 0x00C014D0 (:526)
        int readSint32(); // 0x00C01AF0 (:710)
        void readSint32(int* pTarget, uint32 count); // 0x00C01550 (:547)
        uint32 readUint32(); // 0x00C01B70 (:721)
        void readUint32(uint32* pTarget, uint32 count); // 0x00C015F0 (:569)
        sint64 readSint64(); // 0x00C01BF0 (:732)
        void readSint64(sint64* pTarget, uint32 count); // 0x00C01690 (:591)
        uint64 readUint64(); // 0x00C01D00 (:743)
        void readUint64(uint64* pTarget, uint32 count); // 0x00C017D0 (:613)
        float readFloat32(); // 0x00C01E10 (:767)
        void readFloat32(float* pTarget, uint32 count); // 0x00C04870 (:659)
        uint32 readFixedString(char* pTarget, uint32 capacity); // 0x00C01E90 (:793)
        uint32 readCompactString(char* pTarget, uint32 capacity); // 0x00C07AC0 (:833)
        void writeBool(const bool* pValues, uint32 count); // 0x00C04930 (:948)
        void writeBool(bool value); // 0x00C04910 (:858)
        void writeSint8(const sint8* pValues, uint32 count); // 0x00C024B0 (:957)
        void writeSint8(sint8 value); // 0x00C01EC0 (:864)
        void writeUint8(const uint8* pValues, uint32 count); // 0x00C024E0 (:962)
        void writeUint8(uint8 value); // 0x00C01F30 (:869)
        void writeSint16(const sint16* pValues, uint32 count); // 0x00C02510 (:967)
        void writeSint16(sint16 value); // 0x00C01FA0 (:874)
        void writeUint16(const uint16* pValues, uint32 count); // 0x00C02570 (:983)
        void writeUint16(uint16 value); // 0x00C02020 (:883)
        void writeSint32(const int* pValues, uint32 count); // 0x00C025D0 (:999)
        void writeSint32(int value); // 0x00C020A0 (:892)
        void writeUint32(const uint32* pValues, uint32 count); // 0x00C02630 (:1015)
        void writeUint32(uint32 value); // 0x00C02140 (:901)
        void writeSint64(const sint64* pValues, uint32 count); // 0x00C02690 (:1031)
        void writeSint64(sint64 value); // 0x00C021E0 (:910)
        void writeUint64(const uint64* pValues, uint32 count); // 0x00C026F0 (:1047)
        void writeUint64(uint64 value); // 0x00C02300 (:919)
        void writeFloat32(const float* pValues, uint32 count); // 0x00C02750 (:1081)
        void writeFloat32(float value); // 0x00C02420 (:939)
        void writeFixedString(const char* pSource, uint32 capacity); // 0x00C049C0 (:1090)
        void writeCompactString(const char* pSource); // 0x00C02810 (:1105)
        uint32 writeFormattedCompactString(const char*, ...); // 0x00C07B40 (:1127)
        void fill(uint8 value, uint32 size); // 0x00C02860 (:1177)
    private:
        File& operator=(const File&);
        uint32 readData(void* pTarget, uint32 size); // 0x00BFCE00 (:1185)
        uint32 writeData(const void* pSource, uint32 size); // 0x00BFCE60 (:1197)

        FileSystem* m_pFileSystem; // 0x00
        uint32 m_fileHandle; // 0x04
        uint64 m_fileSize; // 0x08
        uint64 m_filePosition; // 0x10
        State m_state; // 0x18
        bool m_needEndianSwapping; // 0x1C
        bool m_ownsFileHandle; // 0x1D
        bool m_fileSizeDirty; // 0x1E
    };

    uint64_t copyFileData(File& destination, File& source, uint64_t length); // 0x00BFCEC0 (:1228)
    uint32 computeCrc32FromFile(FileSystem* pFilesystem, const char* pFilename); // 0x00C07C90 (:1248)
}
