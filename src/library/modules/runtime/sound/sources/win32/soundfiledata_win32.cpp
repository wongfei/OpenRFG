// Original: library/modules/runtime/sound/sources/win32/soundfiledata_win32.cpp
//
// initializeSoundFileData (SFX.md SX1): a win32 sound file's data starts with a 0x34 byte WAVEFORMATEX (plus extra
// bytes), which is copied into SoundFileData::xmaBuffer and handed to XAudio2 as the source format. The duration comes
// from the format: PCM (1), WAVE_FORMAT_EXTENSIBLE (0xFFFE) and MS-ADPCM (2); other tags keep the old duration.

#include "keen/sound/soundfiledata.hpp"
#include "library/modules/runtime/sound/soundformat/include/keen/soundformat/soundfileheader.hpp"

#include <string.h>

namespace keen
{
    // The WAVEFORMATEX fields read here, at their offsets in xmaBuffer.
    static inline uint16 readUint16(const uint8* pBuffer, uint32 offset)
    {
        uint16 value;
        memcpy(&value, pBuffer + offset, sizeof(value));
        return value;
    }

    static inline uint32 readUint32(const uint8* pBuffer, uint32 offset)
    {
        uint32 value;
        memcpy(&value, pBuffer + offset, sizeof(value));
        return value;
    }

    // 0x00C337F0 (:19)
    // x87 code at the CRT's 53 bit precision (double), truncated to the integer.
    bool initializeSoundFileData(SoundFileData* pData, const SoundSystem::SoundFileHeader* pHeader, const void* pInitData, uint32 dataSize)
    {
        // Magic numbers: platform '\n' (win32) or 'W'.
        if (pHeader->platform != '\n' && pHeader->platform != 'W')
        {
            return false;
        }
        pData->providerId = 0;
        memmove(pData->xmaBuffer, pInitData, SoundFileData::Sizeof_XMA2WAVEFORMATEX);

        const uint8* pFormat = pData->xmaBuffer;
        const uint16 formatTag = readUint16(pFormat, 0x00);
        const uint16 channels = readUint16(pFormat, 0x02);
        const uint32 samplesPerSecond = readUint32(pFormat, 0x04);
        uint32 headerSize = 0;
        if (formatTag == 1)
        {
            // PCM: the sample count is the dword at 0x18, past the WAVEFORMATEX (converter field); the data follows
            // the 0x34 byte format.
            const uint32 sampleCount = readUint32(pFormat, 0x18);
            headerSize = 0x34;
            pData->dataLength = dataSize - headerSize;
            pData->pData = (const uint8*)pInitData + headerSize;
            pData->durationInms = (uint32)(sint64)(((double)sampleCount * 1000.0 / (double)channels) / (double)samplesPerSecond);
            return true;
        }
        if (formatTag == 0xFFFE)
        {
            // Extensible: the data follows the format and its cbSize extra bytes.
            headerSize = readUint16(pFormat, 0x10) + 0x12u;
            const uint16 bitsPerSample = readUint16(pFormat, 0x0E);
            // The byte count times 8 is 32 bit (wraps for waves over 512 MB, original).
            const uint32 sampleCount = (((dataSize - headerSize) * 8) / bitsPerSample) / channels;
            pData->durationInms = (uint32)(sint64)((double)sampleCount * 1000.0 / (double)samplesPerSecond);
        }
        else if (formatTag == 2)
        {
            // MS-ADPCM: whole blocks times wSamplesPerBlock (0x12). The per-channel count is divided by the channels
            // again (original).
            headerSize = readUint16(pFormat, 0x10) + 0x12u;
            const uint16 blockAlign = readUint16(pFormat, 0x0C);
            const uint16 samplesPerBlock = readUint16(pFormat, 0x12);
            const uint32 sampleCount = ((dataSize - headerSize) / blockAlign) * samplesPerBlock;
            pData->durationInms = (uint32)(sint64)(((double)sampleCount * 1000.0 / (double)channels) / (double)samplesPerSecond);
        }
        pData->dataLength = dataSize - headerSize;
        pData->pData = (const uint8*)pInitData + headerSize;
        return true;
    }
}
