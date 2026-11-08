// Original: volition/rfg/code/gamewide/rfgsaveinterface.hpp
// Implementation: volition/rfg/code/gamewide/rfgsaveinterface.cpp
//
// The description of the game's save data given to the keen save system (unit4::LocalGameSession) (decompiled).
// The save data is one RfgSaveData block of 0x1E0589C bytes that starts with an RfgSaveHeader. The constructor fills
// m_expected_header (version 0x1000000 and a fixed text). isSaveDataValid recomputes crc32_16bytes over the block
// with the checksum field taken as 0 and compares it with header.checksum. initializeSaveDataToDefault clears the
// block, resets the game options inside it, copies the expected header and stores the checksum. Written in keen
// style (.hpp, namespace keen::rfg) like the rest of the keen glue; the PDB records no inline code for it.
#pragma once

#include "keen/base/types.hpp"
#include "unit4/gamesession/localgamesession.hpp"
#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/player_profile.h"

namespace keen
{
namespace rfg
{
    // size 0x188
    struct RfgSaveHeader
    {
        uint32 version; // 0x00
        uint32 checksum; // 0x04
        char text[256]; // 0x08
        uint8 padding[128]; // 0x108
    };

    // size 0x2804D4, one save slot
    struct RfgSaveFile
    {
        bool exists; // 0x00
        game_save_info info; // 0x04
        int size_used; // 0xC0
        unsigned char data[2621456]; // 0xC4
        unsigned char padding[1024]; // 0x2803D4
    };

    // size 0x964
    struct RfgSaveOptions
    {
        rfg_game_options options; // 0x00
        unsigned char padding[1024]; // 0x564
    };

    // size 0x11C0
    struct RfgSaveProfileData
    {
        player_profile_data profile_data; // 0x00
        unsigned char padding[1024]; // 0xDC0
    };

    // size 0x1E0589C, the whole save data (RfgSaveInterface::getSaveDataSize)
    struct RfgSaveData
    {
        RfgSaveHeader header; // 0x00
        RfgSaveFile save_files[12]; // 0x188
        RfgSaveOptions options; // 0x1E03B78
        RfgSaveProfileData profile_data; // 0x1E044DC
        unsigned char padding[512]; // 0x1E0569C
    };

    // size 0x18C
    class RfgSaveInterface : public unit4::SaveDataDescriptionInterface
    {
    public:
        RfgSaveInterface(); // 0x007D24B0 (:34)
        virtual ~RfgSaveInterface(); // vtbl+0x00 0x007C6CD0 (:42)
        virtual uint32 getSaveDataSize() const; // vtbl+0x04 0x007C6CE0 (:46)
        virtual bool isSaveDataValid(const void* pData); // vtbl+0x08 0x007C6CF0 (:51)
        virtual void initializeSaveDataToDefault(void* pData); // vtbl+0x0C 0x007D2510 (:88)

    protected:
        RfgSaveHeader m_expected_header; // 0x04
    };
}
}
