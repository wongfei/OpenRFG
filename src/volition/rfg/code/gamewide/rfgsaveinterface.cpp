// Original: volition/rfg/code/gamewide/rfgsaveinterface.cpp
//
// The game's save data for the keen save system: see rfgsaveinterface.hpp. rfg.exe checks the block with crc32_16bytes
// (fastcrc32) and stores the checksum with keen::getCrc32Value in game_save_generic.cpp: both are the standard CRC-32, so
// the fast variant, which only saves time here, is not ported.

#include "volition/rfg/code/gamewide/rfgsaveinterface.hpp"

#include "keen/base/crc32.hpp"
#include "keen/base/string.hpp"

#include <stddef.h>
#include <string.h>

namespace keen
{
namespace rfg
{
    // 0x007D24B0 (:34)
    RfgSaveInterface::RfgSaveInterface()
    {
        memset(&m_expected_header, 0, sizeof(m_expected_header));
        copyString(m_expected_header.text, sizeof(m_expected_header.text),
                   "Red Faction Guerrilla: Guerrillfinitive Edition - If you are reading this, you're probably a hacker. Shame on you! >:(\n*staring intensifies*\nDid you feel my stare of indignation? Okay, good!");
        // Magic number: the save data's version.
        m_expected_header.version = 0x1000000;
    }

    // 0x007C6CD0 (:42)
    RfgSaveInterface::~RfgSaveInterface()
    {
    }

    // 0x007C6CE0 (:46)
    uint32 RfgSaveInterface::getSaveDataSize() const
    {
        return sizeof(RfgSaveData);
    }

    // 0x007C6CF0 (:51): the checksum over the block with the checksum field taken as 0. Only the checksum decides: the
    // header's version and text are not compared.
    bool RfgSaveInterface::isSaveDataValid(const void* pData)
    {
        const RfgSaveData* pSaveData = (const RfgSaveData*)pData;
        const uint32 zero = 0;
        const uint8* pAfterChecksum = (const uint8*)&pSaveData->header.checksum + sizeof(uint32);
        // addCrc32Value continues a final value (the seed and the final xor are inside it).
        uint32 checksum = getCrc32Value(pData, offsetof(RfgSaveHeader, checksum));
        checksum = addCrc32Value(checksum, &zero, sizeof(zero));
        checksum = addCrc32Value(checksum, pAfterChecksum, (uint32)sizeof(RfgSaveData) - (uint32)(pAfterChecksum - (const uint8*)pData));
        return pSaveData->header.checksum == checksum;
    }

    // 0x007D2510 (:88)
    void RfgSaveInterface::initializeSaveDataToDefault(void* pData)
    {
        RfgSaveData* pSaveData = (RfgSaveData*)pData;
        memset(pData, 0, sizeof(RfgSaveData));
        game_options_reset(&pSaveData->options.options);
        pSaveData->header = m_expected_header;
        pSaveData->header.checksum = 0;
        pSaveData->header.checksum = getCrc32Value(pData, sizeof(RfgSaveData));
    }
}
}
