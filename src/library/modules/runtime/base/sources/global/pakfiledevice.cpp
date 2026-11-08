// Original: library/modules/runtime/base/sources/global/pakfiledevice.cpp
//
// Read-only device over a keen .pak file. Layout (all values in the pak's endianness):
//   0x00 char[4] type: "PAKl"/"PAKb" uncompressed, "KPKl"/"bKPK" compressed (little/big endian)
//   uncompressed: 0x04 uint32 alignment, 0x08 uint64 directory position. Older paks have no alignment field (the
//                 directory position follows the type), detected by an invalid alignment or position.
//   compressed:   0x04 uint32 (unused), 0x08 uint32 alignment, 0x0C uint32 chunk size, 0x10 uint64 directory position
//   directory:    uint32 count, then per file: uint32 name crc, uint32 position / alignment, uint32 size (uncompressed
//                 paks only). Sorted by crc on mount, looked up by the crc32 of the name relative to the mount point.
//   compressed file: uint32 size, uint32 chunk count, uint32 compressed chunk sizes[chunk count], then aPLib chunks of
//                 4 KB each; a chunk count of 0 means the data follows uncompressed.
//
// The device calls through FileSystem's device table are inlined in the original; here they go through keen::file.

#include "pakfiledevice.hpp"
#include "keen/base/apdepacker.hpp"
#include "keen/base/binaryreader.hpp"
#include "keen/base/crc32.hpp"
#include "keen/base/filereadstream.hpp"
#include "keen/base/filesystem.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/search.hpp"
#include "keen/base/sort.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <string.h>

namespace keen
{
    static const char* pUncompressedLittleEndian = "PAKl";
    static const char* pUncompressedBigEndian = "PAKb";
    static const char* pCompressedLittleEndian = "KPKl";
    // Notes (decompiled behaviour, kept as is): the big endian compressed tag is "bKPK", not "KPKb".
    static const char* pCompressedBigEndian = "bKPK";

    // Magic number: the uncompressed size of a chunk (getPosition/setPosition use it rather than the mount's
    // chunkSize; PakFileStream::uncompressedChunkData has this size).
    static const uint32 PakChunkSize = 0x1000;

    // 0x00BF1010 (:19)
    bool PakfileEntry::operator<(const PakfileEntry& other) const
    {
        return crc < other.crc;
    }

    // 0x00BF9D20
    PakFileDeviceMountData::PakFileDeviceMountData()
    {
    }

    // 0x00BF3DA0
    PakFileDeviceMountData::~PakFileDeviceMountData()
    {
    }

    // 0x00BFBF50
    PakFileDevice::PakFileStream::PakFileStream()
    {
    }

    // 0x00BFBF70
    PakFileDevice::PakFileStream::~PakFileStream()
    {
    }

    // 0x00C040B0 (:39)
    PakFileDevice::PakFileDevice()
    {
    }

    // 0x00BF9D10 (:43)
    PakFileDevice::~PakFileDevice()
    {
    }

    // 0x00C0D9A0 (:47)
    bool PakFileDevice::create(MemoryAllocator* pAllocator, uint32 maxStreamCount, uint32 maxFileSize)
    {
        if (!m_streams.create(pAllocator, maxStreamCount))
        {
            return false;
        }
        // Chunk size table memory: one uint32 per 4 KB chunk of the largest file.
        const uint32 chunkAllocatorSize = ((maxFileSize + 0x1000) >> 12) * 4;
        uint8* pChunkMemory = (uint8*)pAllocator->allocate(chunkAllocatorSize, 16, 0, nullptr);
        if (pChunkMemory == nullptr)
        {
            m_streams.destroy(pAllocator);
            return false;
        }
        if (!m_chunkAllocator.create(createMemoryBlock(pChunkMemory, chunkAllocatorSize), 0, "PakFileDevice"))
        {
            // The original releases everything but still reports success.
            pAllocator->free(pChunkMemory);
            m_streams.destroy(pAllocator);
        }
        return true;
    }

    // 0x00C0C4A0 (:70)
    void PakFileDevice::destroy(MemoryAllocator* pAllocator)
    {
        m_streams.destroy(pAllocator);
        pAllocator->free(m_chunkAllocator.destroy().pStart);
    }

    // 0x00C08AB0 (:76)
    static bool getPakfileTypeFromString(const char* pTypeString, Endianness* pEndianness, bool* pIsCompressed)
    {
        if (compareString(pTypeString, pUncompressedLittleEndian) == 0)
        {
            *pEndianness = Endianness_Little;
            *pIsCompressed = false;
            return true;
        }
        if (compareString(pTypeString, pUncompressedBigEndian) == 0)
        {
            *pEndianness = Endianness_Big;
            *pIsCompressed = false;
            return true;
        }
        if (compareString(pTypeString, pCompressedLittleEndian) == 0)
        {
            *pEndianness = Endianness_Little;
            *pIsCompressed = true;
            return true;
        }
        if (compareString(pTypeString, pCompressedBigEndian) == 0)
        {
            *pEndianness = Endianness_Big;
            *pIsCompressed = true;
            return true;
        }
        debug::nativePrintArgs("[pakfiledevice] Encountered invalid pakfile type %s\n", pTypeString);
        return false;
    }

    // 0x00C08B60 (:112)
    IoResult<FileDeviceMountData*> PakFileDevice::allocateMountData(MemoryAllocator* pAllocator, FileSystem* pFileSystem, const char* pPakFileName)
    {
        IoResult<FileDeviceMountData*> result;
        // Raw allocation: the constructor is not run (the entry array is cleared by hand).
        PakFileDeviceMountData* pMountData = (PakFileDeviceMountData*)pAllocator->allocate(sizeof(PakFileDeviceMountData), 4, 0, nullptr);
        if (pMountData == nullptr)
        {
            result.result = nullptr;
            result.error = IoError_OutOfMemory;
            return result;
        }
        new (&pMountData->fileEntries) Array<PakfileEntry>();

        const IoResult<uint32> pakFile = file::openFile(pFileSystem, pPakFileName, FileAccessMode_Read);
        if (pakFile.error != IoError_Ok)
        {
            pAllocator->free(pMountData);
            result.result = nullptr;
            result.error = pakFile.error;
            return result;
        }

        FileReadStream stream(pFileSystem, pakFile.result);
        char typeString[5] = "    ";
        stream.readDataBlock(typeString, 4);
        if (getPakfileTypeFromString(typeString, &pMountData->endianness, &pMountData->isPakfileCompressed))
        {
            BinaryReader reader(&stream, pMountData->endianness);
            if (!pMountData->isPakfileCompressed)
            {
                pMountData->alignment = reader.readUint32();
                const uint64 directoryPosition = reader.readUint64();
                // Magic number: alignments above 32 are not valid; an invalid alignment or directory position means a
                // pak without the alignment field.
                const uint32 alignment = pMountData->alignment;
                if (alignment != 0 && (alignment & (alignment - 1)) == 0 && alignment <= 0x20 && directoryPosition <= stream.getFileSize())
                {
                    stream.setFilePosition(8);
                }
                else
                {
                    stream.setFilePosition(4);
                    pMountData->alignment = 1;
                }
            }
            else
            {
                reader.readUint32();
                pMountData->alignment = reader.readUint32();
                pMountData->chunkSize = reader.readUint32();
            }
            if (!stream.hasError())
            {
                stream.setFilePosition(reader.readUint64());
                const uint32 fileCount = reader.readUint32();
                pMountData->fileEntries.create(pAllocator, fileCount, 16);
                for (uint32 i = 0; i < fileCount; ++i)
                {
                    PakfileEntry& entry = pMountData->fileEntries[i];
                    entry.crc = reader.readUint32();
                    entry.position = (uint64)reader.readUint32() * pMountData->alignment;
                    if (!pMountData->isPakfileCompressed)
                    {
                        entry.size = reader.readUint32();
                    }
                    else
                    {
                        entry.size = 0;
                    }
                }
                insertionSort(pMountData->fileEntries.getStart(), pMountData->fileEntries.getEnd(), lessThanPredicate<PakfileEntry>());
                pMountData->pFileSystem = pFileSystem;
                pMountData->pakFileHandle = pakFile.result;
                result.result = pMountData;
                result.error = stream.hasError() ? IoError_DecompressionError : IoError_Ok;
                return result;
            }
        }
        // Notes (decompiled behaviour, kept as is): on an invalid type or header the mount data and the pak file
        // handle are leaked; the mount data is returned along with the error.
        result.result = pMountData;
        result.error = IoError_DecompressionError;
        return result;
    }

    // 0x00C04120 (:209)
    IoError PakFileDevice::freeMountData(MemoryAllocator* pAllocator, FileDeviceMountData* pMountData)
    {
        PakFileDeviceMountData* pPakMountData = (PakFileDeviceMountData*)pMountData;
        const IoError error = file::closeFile(pPakMountData->pFileSystem, pPakMountData->pakFileHandle);
        pPakMountData->fileEntries.destroy(pAllocator);
        pAllocator->free(pPakMountData);
        return error;
    }

    // 0x00C08F00 (:226)
    IoResult<uint16> PakFileDevice::openFile(const FileDeviceMountData* pMountData, const char* pFileName, FileAccessMode accessMode)
    {
        IoResult<uint16> result;
        PakFileDeviceMountData* pPakMountData = (PakFileDeviceMountData*)pMountData;
        PakFileStream* pStream = m_streams.allocateStream();
        if (pStream == nullptr)
        {
            result.result = 0xffff;
            result.error = IoError_OutOfMemory;
            return result;
        }
        const uint32 fileNameCrc = getCrc32Value(pFileName);
        const PakfileEntry* pEntry = (const PakfileEntry*)searchBinary(&pPakMountData->fileEntries.getStart()->crc, pPakMountData->fileEntries.getSize(), fileNameCrc, sizeof(PakfileEntry));
        if (pEntry == nullptr)
        {
            m_streams.freeStream(pStream);
            result.result = 0xffff;
            result.error = IoError_FileNotFound;
            return result;
        }

        FileReadStream stream(pPakMountData->pFileSystem, pPakMountData->pakFileHandle);
        stream.setFilePosition(pEntry->position);
        BinaryReader reader(&stream, pPakMountData->endianness);
        if (!pPakMountData->isPakfileCompressed)
        {
            pStream->isFileCompressed = false;
            pStream->chunkCount = 0;
            pStream->fileSize = pEntry->size;
            pStream->uncompressedDataBegin = pEntry->position;
        }
        else
        {
            pStream->fileSize = reader.readUint32();
            const uint32 chunkCount = reader.readUint32();
            pStream->isFileCompressed = chunkCount != 0;
            pStream->chunkCount = chunkCount;
            // The chunks follow the size, the chunk count and the chunk size table.
            const uint64 firstChunkPosition = pEntry->position + chunkCount * 4 + 8;
            pStream->firstChunkPosition = firstChunkPosition;
            pStream->nextChunkPosition = firstChunkPosition;
            if (!pStream->isFileCompressed)
            {
                pStream->uncompressedDataBegin = firstChunkPosition;
            }
            IoError error = IoError_Ok;
            if (stream.hasError())
            {
                error = IoError_DecompressionError;
            }
            else if (!pStream->chunkSizes.create(&m_chunkAllocator, chunkCount, 16))
            {
                error = IoError_OutOfMemory;
            }
            else
            {
                for (uint32 i = 0; i < pStream->chunkCount; ++i)
                {
                    pStream->chunkSizes[i] = reader.readUint32();
                }
                if (stream.hasError())
                {
                    pStream->chunkSizes.destroy(&m_chunkAllocator);
                    error = IoError_DecompressionError;
                }
            }
            if (error != IoError_Ok)
            {
                m_streams.freeStream(pStream);
                result.result = 0xffff;
                result.error = error;
                return result;
            }
        }
        pStream->pMountData = pPakMountData;
        pStream->fileNameCrc = fileNameCrc;
        pStream->uncompressedDataPosition = 0;
        pStream->uncompressedChunkDataSize = 0;
        pStream->uncompressedChunkDataPosition = 0;
        pStream->chunkIndex = (uint32)InvalidChunkIndex;
        result.result = m_streams.getHandleFromStream(pStream);
        result.error = IoError_Ok;
        return result;
    }

    // 0x00C093F0 (:319)
    IoError PakFileDevice::closeFile(uint16 file)
    {
        PakFileStream* pStream = m_streams.getStreamFromHandle(file);
        if (pStream->pMountData->isPakfileCompressed)
        {
            pStream->chunkSizes.destroy(&m_chunkAllocator);
        }
        m_streams.freeStream(pStream);
        return IoError_Ok;
    }

    // 0x00BFFDD0 (:333)
    IoResult<uint32> PakFileDevice::read(uint16 file, void* pDest, uint32 size)
    {
        IoResult<uint32> result;
        PakFileStream* pStream = m_streams.getStreamFromHandle(file);
        if (size == 0)
        {
            result.result = 0;
            result.error = IoError_Ok;
            return result;
        }
        if (pStream->isFileCompressed)
        {
            uint8* pTarget = (uint8*)pDest;
            uint32 readSize = 0;
            while (size != 0)
            {
                if (pStream->uncompressedChunkDataPosition == pStream->uncompressedChunkDataSize || pStream->chunkIndex == (uint32)InvalidChunkIndex)
                {
                    const uint32 nextChunkIndex = pStream->chunkIndex == (uint32)InvalidChunkIndex ? 0 : pStream->chunkIndex + 1;
                    const IoResult<uint32> chunk = readAndDecompressChunk(pStream, nextChunkIndex);
                    if (chunk.error != IoError_Ok)
                    {
                        if (readSize != 0 && chunk.error == IoError_EofReached)
                        {
                            break;
                        }
                        return chunk;
                    }
                }
                uint32 copySize = pStream->uncompressedChunkDataSize - pStream->uncompressedChunkDataPosition;
                if (size < copySize)
                {
                    copySize = size;
                }
                memmove(pTarget, pStream->uncompressedChunkData + pStream->uncompressedChunkDataPosition, copySize);
                pStream->uncompressedChunkDataPosition += copySize;
                readSize += copySize;
                pTarget += copySize;
                size -= copySize;
            }
            result.result = readSize;
            result.error = IoError_Ok;
            return result;
        }

        if (pStream->uncompressedDataPosition == pStream->fileSize)
        {
            result.result = 0;
            result.error = IoError_EofReached;
            return result;
        }
        // Notes (decompiled behaviour, kept as is): the read is not clamped to the file size, so reading past the
        // end of an uncompressed file returns the following pak data.
        const IoError error = file::setFilePosition(pStream->pMountData->pFileSystem, pStream->pMountData->pakFileHandle, pStream->uncompressedDataBegin + pStream->uncompressedDataPosition);
        if (error != IoError_Ok)
        {
            result.result = 0;
            result.error = error;
            return result;
        }
        result = file::read(pStream->pMountData->pFileSystem, pStream->pMountData->pakFileHandle, pDest, size);
        if (result.error == IoError_Ok)
        {
            pStream->uncompressedDataPosition += result.result;
        }
        return result;
    }

    // 0x00BF9D30 (:406)
    IoResult<uint32> PakFileDevice::write(uint16 file, const void* pSrc, uint32 size)
    {
        IoResult<uint32> result;
        result.result = 0;
        result.error = IoError_NoPermission;
        return result;
    }

    // 0x00BF1020 (:415)
    IoError PakFileDevice::flushWriteBuffer(uint16 file)
    {
        return IoError_NoPermission;
    }

    // 0x00C00030 (:422)
    IoResult<uint64> PakFileDevice::getSize(uint16 file) const
    {
        IoResult<uint64> result;
        result.result = m_streams.getStreamFromHandle(file)->fileSize;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00C00090 (:430)
    IoError PakFileDevice::setPosition(uint16 file, uint64 position)
    {
        PakFileStream* pStream = m_streams.getStreamFromHandle(file);
        // Notes (decompiled behaviour, kept as is): seeking to the end of the file (position == size) fails.
        if (position >= pStream->fileSize)
        {
            return IoError_EofReached;
        }
        if (!pStream->isFileCompressed)
        {
            pStream->uncompressedDataPosition = position;
            return IoError_Ok;
        }
        const uint32 chunkIndex = (uint32)(position / PakChunkSize);
        if (chunkIndex != pStream->chunkIndex)
        {
            pStream->nextChunkPosition = pStream->firstChunkPosition;
            for (uint32 i = 0; i < chunkIndex; ++i)
            {
                pStream->nextChunkPosition += pStream->chunkSizes[i];
            }
            const IoResult<uint32> chunk = readAndDecompressChunk(pStream, chunkIndex);
            if (chunk.error != IoError_Ok)
            {
                return chunk.error;
            }
        }
        pStream->uncompressedChunkDataPosition = (uint32)position & (PakChunkSize - 1);
        return IoError_Ok;
    }

    // 0x00C00170 (:475)
    IoResult<uint64> PakFileDevice::getPosition(uint16 file) const
    {
        IoResult<uint64> result;
        const PakFileStream* pStream = m_streams.getStreamFromHandle(file);
        uint64 position;
        if (!pStream->isFileCompressed)
        {
            position = pStream->uncompressedDataPosition;
        }
        else if (pStream->chunkIndex == (uint32)InvalidChunkIndex)
        {
            result.result = 0;
            result.error = IoError_Ok;
            return result;
        }
        else
        {
            position = (uint64)pStream->chunkIndex * PakChunkSize + pStream->uncompressedChunkDataPosition;
        }
        // Notes (decompiled behaviour, kept as is): at the end of the file the position is reported as an error.
        if (position >= pStream->fileSize)
        {
            result.result = 0;
            result.error = IoError_EofReached;
            return result;
        }
        result.result = position;
        result.error = IoError_Ok;
        return result;
    }

    // 0x00BF1030 (:512)
    IoError PakFileDevice::setFilePermissionByName(const FileDeviceMountData* pMountData, const char* pFileName, bool isWriteable)
    {
        return IoError_NoPermission;
    }

    // 0x00C06D30 (:521)
    IoError PakFileDevice::getFileStatusByName(FileStatus* pStatus, const FileDeviceMountData* pMountData, const char* pFileName, uint32 valueMask) const
    {
        const PakFileDeviceMountData* pPakMountData = (const PakFileDeviceMountData*)pMountData;
        const PakfileEntry* pEntry = (const PakfileEntry*)searchBinary(&pPakMountData->fileEntries.getStart()->crc, pPakMountData->fileEntries.getSize(), getCrc32Value(pFileName), sizeof(PakfileEntry));
        if (pEntry == nullptr)
        {
            return IoError_FileNotFound;
        }
        if ((valueMask & FileStatus::ValueMask_NativeFileName) != 0)
        {
            return IoError_NotImplemented;
        }
        if ((valueMask & FileStatus::ValueMask_Writeable) != 0)
        {
            pStatus->isWriteable = false;
        }
        if ((valueMask & FileStatus::ValueMask_Size) != 0)
        {
            if (!pPakMountData->isPakfileCompressed)
            {
                pStatus->size = pEntry->size;
            }
            else
            {
                FileReadStream stream(pPakMountData->pFileSystem, pPakMountData->pakFileHandle);
                stream.setFilePosition(pEntry->position);
                BinaryReader reader(&stream, pPakMountData->endianness);
                pStatus->size = reader.readUint32();
            }
        }
        // The modification time is not available.
        return (valueMask & FileStatus::ValueMask_LastModificationTime) != 0 ? IoError_NotImplemented : IoError_Ok;
    }

    // 0x00BF9D50 (:570)
    IoResult<uint32> PakFileDevice::readAndDecompressChunk(PakFileStream* pStream, uint32 chunkIndex)
    {
        IoResult<uint32> result;
        if (chunkIndex >= pStream->chunkSizes.getSize())
        {
            result.result = 0;
            result.error = IoError_EofReached;
            return result;
        }
        const uint64 chunkPosition = pStream->nextChunkPosition;
        const uint32 compressedSize = pStream->chunkSizes[chunkIndex];
        pStream->chunkIndex = chunkIndex;
        pStream->nextChunkPosition = chunkPosition + compressedSize;
        const IoError error = file::setFilePosition(pStream->pMountData->pFileSystem, pStream->pMountData->pakFileHandle, chunkPosition);
        if (error != IoError_Ok)
        {
            result.result = 0;
            result.error = error;
            return result;
        }
        const IoResult<uint32> readResult = file::read(pStream->pMountData->pFileSystem, pStream->pMountData->pakFileHandle, pStream->compressedChunkData, compressedSize);
        if (readResult.error != IoError_Ok)
        {
            result.result = 0;
            result.error = readResult.error;
            return result;
        }
        const uint32 uncompressedSize = compression::apdepack(pStream->compressedChunkData, pStream->uncompressedChunkData);
        pStream->uncompressedChunkDataSize = uncompressedSize;
        pStream->uncompressedChunkDataPosition = 0;
        result.result = uncompressedSize;
        result.error = IoError_Ok;
        return result;
    }
}
