// Original: library/modules/runtime/base/sources/global/filereadstream.cpp
//
// FileReadStream, a ReadStream refilled 2 KB at a time from a keen::file handle. A stream opened by name owns its
// handle; one opened from a handle leaves it open on close.
//
// The device calls through FileSystem's device table are inlined in the original; here they go through keen::file.

#include "keen/base/filereadstream.hpp"
#include "keen/base/filesystem.hpp"

namespace keen
{
    // 0x00BFD0A0 (:8)
    FileReadStream::FileReadStream()
    {
        m_pFileSystem = nullptr;
        m_fileHandle = 0xffffffffu;
    }

    // 0x00C04DF0 (:14)
    FileReadStream::FileReadStream(FileSystem* pFileSystem, uint32 file)
    {
        m_pFileSystem = nullptr;
        m_fileHandle = 0xffffffffu;
        open(pFileSystem, file);
    }

    // 0x00C02E00 (:21)
    FileReadStream::~FileReadStream()
    {
        close();
    }

    // 0x00C02E10 (:49)
    void FileReadStream::open(FileSystem* pFileSystem, uint32 file)
    {
        close();
        m_pFileSystem = pFileSystem;
        m_fileHandle = file;
        m_isInternalHandle = false;
        initialize(m_buffer, 0, refillBuffer, "FileReadStream");
    }

    // 0x00C04E60 (:26)
    bool FileReadStream::open(FileSystem* pFileSystem, const char* pFileName)
    {
        close();
        const IoResult<uint32> result = file::openFile(pFileSystem, pFileName, FileAccessMode_Read);
        if (result.error != IoError_Ok)
        {
            setError(result.error);
            return false;
        }
        m_fileHandle = result.result;
        m_pFileSystem = pFileSystem;
        m_isInternalHandle = true;
        initialize(m_buffer, 0, refillBuffer, "FileReadStream");
        return true;
    }

    // 0x00BFD0D0 (:63)
    void FileReadStream::close()
    {
        if (m_fileHandle != 0xffffffffu)
        {
            if (m_isInternalHandle)
            {
                file::closeFile(m_pFileSystem, m_fileHandle);
            }
            m_fileHandle = 0xffffffffu;
        }
        m_pFileSystem = nullptr;
    }

    // 0x00BEFCC0 (.h:23)
    bool FileReadStream::isOpen() const
    {
        return m_pFileSystem != nullptr;
    }

    // 0x00BFD130 (:78)
    void FileReadStream::setFilePosition(uint64 position)
    {
        const IoError error = file::setFilePosition(m_pFileSystem, m_fileHandle, position);
        setBuffer(m_buffer, 0);
        setError(error);
    }

    // 0x00BFD1B0 (:85)
    uint64 FileReadStream::getFileSize()
    {
        const IoResult<uint64> result = file::getFileSize(m_pFileSystem, m_fileHandle);
        if (result.error != IoError_Ok)
        {
            setError(result.error);
            return 0;
        }
        return result.result;
    }

    FileSystem* FileReadStream::getFileSystem()
    {
        return m_pFileSystem;
    }

    uint32 FileReadStream::getFileHandle()
    {
        return m_fileHandle;
    }

    // 0x00BEFCD0 (:98)
    const char* FileReadStream::getFileName() const
    {
        return nullptr;
    }

    // 0x00BFD2C0 (:124)
    void FileReadStream::refillBuffer(ReadStream* pBuffer)
    {
        ((FileReadStream*)pBuffer)->readBufferFromFile();
    }

    // 0x00BFD260 (:110)
    void FileReadStream::readBufferFromFile()
    {
        const IoResult<uint32> result = file::read(m_pFileSystem, m_fileHandle, m_buffer, BufferSize);
        if (result.result != 0)
        {
            setBuffer(m_buffer, result.result);
            return;
        }
        // Notes (decompiled behaviour, kept as is): a zero-byte read without an error (end of file) leaves the
        // drained buffer in place, so the reader spins until the device reports an error.
        setError(result.error);
    }
}
