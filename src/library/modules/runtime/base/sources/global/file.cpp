// Original: library/modules/runtime/base/sources/global/file.cpp
//
// File, a file system handle with a cached size, the current position and an error state; reads of multi-byte values
// swap them when the data endianness differs from the platform's. Once in the error state, reads return nothing.
//
// Partial: opening, closing, raw reads, size/position and the uint32/uint64 readers (what the recovered code uses).
// The device calls through FileSystem's device table are inlined in the original; here they go through keen::file.

#include "keen/base/file.hpp"
#include "keen/base/filesystem.hpp"
#include "keen/base/trace.hpp"

namespace keen
{
    static inline uint32 swapUint32(uint32 value)
    {
        return (value >> 24) | ((value >> 8) & 0xff00u) | ((value << 8) & 0xff0000u) | (value << 24);
    }

    static inline uint64 swapUint64(uint64 value)
    {
        return ((uint64)swapUint32((uint32)value) << 32) | swapUint32((uint32)(value >> 32));
    }

    // 0x00BEFB40 (:15)
    File::File()
    {
        m_pFileSystem = nullptr;
        m_fileHandle = 0xffffffffu;
        m_needEndianSwapping = false;
        m_ownsFileHandle = false;
    }

    // 0x00C01240 (:28)
    File::~File()
    {
        close();
    }

    // 0x00C01290 (:33)
    bool File::openFromHandle(FileSystem* pFileSystem, uint32 fileHandle, Endianness sourceEndianness)
    {
        close();
        if (fileHandle == 0xffffffffu)
        {
            return false;
        }
        const IoResult<uint64> position = file::getFilePosition(pFileSystem, fileHandle);
        if (position.error != IoError_Ok)
        {
            return false;
        }
        m_filePosition = position.result;
        m_needEndianSwapping = sourceEndianness != Endianness_Little;
        m_fileHandle = fileHandle;
        m_pFileSystem = pFileSystem;
        m_ownsFileHandle = false;
        m_fileSizeDirty = true;
        m_state = State_Ready;
        return true;
    }

    // 0x00C078F0 (:65)
    bool File::open(FileSystem* pFileSystem, const char* pFileName, FileAccessMode accessMode, Endianness sourceEndianness)
    {
        if (pFileSystem == nullptr)
        {
            debug::nativePrintArgs("Could not open File '%s'! - no valid file system specified!\n", pFileName);
            return false;
        }
        close();
        const IoResult<uint32> result = file::openFile(pFileSystem, pFileName, accessMode);
        if (result.error != IoError_Ok)
        {
            return false;
        }
        m_pFileSystem = pFileSystem;
        m_fileHandle = result.result;
        m_ownsFileHandle = true;
        m_fileSizeDirty = true;
        m_filePosition = 0;
        m_needEndianSwapping = sourceEndianness != Endianness_Little;
        m_state = State_Ready;
        return true;
    }

    // 0x00BFCAA0 (:95)
    uint32 File::close()
    {
        if (m_fileHandle == 0xffffffffu)
        {
            return 0xffffffffu;
        }
        if (m_ownsFileHandle)
        {
            file::closeFile(m_pFileSystem, m_fileHandle);
            m_fileHandle = 0xffffffffu;
        }
        m_state = State_Ready;
        // Notes (decompiled behaviour, kept as is): a borrowed handle is kept and returned.
        return m_fileHandle;
    }

    // 0x00BEFB60 (:120)
    bool File::isOpen() const
    {
        return m_fileHandle != 0xffffffffu;
    }

    // 0x00BFCAF0 (:141)
    uint32 File::read(void* pTarget, uint32 size)
    {
        if (m_state == State_Error)
        {
            return 0;
        }
        const IoResult<uint32> result = file::read(m_pFileSystem, m_fileHandle, pTarget, size);
        m_filePosition += result.result;
        if (result.error != IoError_Ok)
        {
            setErrorState();
        }
        return result.result;
    }

    // 0x00CDC410 (.h:51)
    uint32 File::getFileHandle()
    {
        return m_fileHandle;
    }

    // 0x00BFCBF0 (:211)
    uint64 File::getSize() const
    {
        if (!m_fileSizeDirty)
        {
            return m_fileSize;
        }
        const IoResult<uint64> result = file::getFileSize(m_pFileSystem, m_fileHandle);
        if (result.error != IoError_Ok)
        {
            return 0;
        }
        // The size is cached on first use.
        File* pThis = const_cast<File*>(this);
        pThis->m_fileSize = result.result;
        pThis->m_fileSizeDirty = false;
        return result.result;
    }

    // 0x00BFCCA0 (:245)
    void File::setPosition(uint64 pointer)
    {
        if (file::setFilePosition(m_pFileSystem, m_fileHandle, pointer) == IoError_Ok)
        {
            m_filePosition = pointer;
        }
        else
        {
            setErrorState();
        }
    }

    // 0x00BEFB20 (.h:55)
    uint64 File::getPosition() const
    {
        return m_filePosition;
    }

    // 0x00BEFB70 (:409)
    void File::setDataEndianness(Endianness endianness)
    {
        m_needEndianSwapping = endianness != Endianness_Little;
    }

    // 0x00BEFB80 (:414)
    Endianness File::getDataEndianness() const
    {
        return m_needEndianSwapping ? Endianness_Big : Endianness_Little;
    }

    // 0x00BEFB30 (.h:69)
    bool File::isInErrorState() const
    {
        return m_state == State_Error;
    }

    // 0x00C01210 (.h:70)
    bool File::isEndOfFile() const
    {
        return getSize() == m_filePosition;
    }

    // 0x00BEFB90 (:426)
    void File::setErrorState()
    {
        if (m_state == State_Ready)
        {
            m_state = State_Error;
        }
    }

    // 0x00BEFBA0 (:434)
    void File::resetErrorState()
    {
        if (m_state == State_Error)
        {
            m_state = State_Ready;
        }
    }

    // 0x00C01B70 (:721)
    uint32 File::readUint32()
    {
        uint32 value = 0;
        if (readData(&value, sizeof(value)) != sizeof(value))
        {
            setErrorState();
        }
        return m_needEndianSwapping ? swapUint32(value) : value;
    }

    // 0x00C015F0 (:569)
    void File::readUint32(uint32* pTarget, uint32 count)
    {
        readData(pTarget, count * 4);
        if (m_needEndianSwapping)
        {
            for (uint32 i = 0; i < count; ++i)
            {
                pTarget[i] = swapUint32(pTarget[i]);
            }
        }
    }

    // 0x00C01D00 (:743)
    uint64 File::readUint64()
    {
        uint64 value = 0;
        if (readData(&value, sizeof(value)) != sizeof(value))
        {
            setErrorState();
        }
        return m_needEndianSwapping ? swapUint64(value) : value;
    }

    // 0x00C017D0 (:613)
    void File::readUint64(uint64* pTarget, uint32 count)
    {
        readData(pTarget, count * 8);
        if (m_needEndianSwapping)
        {
            for (uint32 i = 0; i < count; ++i)
            {
                pTarget[i] = swapUint64(pTarget[i]);
            }
        }
    }

    // 0x00BFCE00 (:1185)
    uint32 File::readData(void* pTarget, uint32 size)
    {
        uint32 readSize = 0;
        if (m_state != State_Error)
        {
            const IoResult<uint32> result = file::read(m_pFileSystem, m_fileHandle, pTarget, size);
            readSize = result.result;
            m_filePosition += readSize;
            if (result.error != IoError_Ok)
            {
                setErrorState();
            }
        }
        if (readSize != size)
        {
            setErrorState();
        }
        return readSize;
    }
}
