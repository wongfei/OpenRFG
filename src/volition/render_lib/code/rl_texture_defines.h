// Original: volition/render_lib/code/rl_texture_defines.h
//
// rl_texture_desc, the serialized texture description. Placement inferred.
#pragma once

// size 0xC
struct rl_texture_desc
{
    // size 0x4
    union Data
    {
        // size 0x4
        struct DiscData
        {
            unsigned short texture_stage_disk; // 0x00
            unsigned short texture_flags_disk; // 0x02
        };

        // size 0x4
        struct DataStruct
        {
            unsigned int texture_stage : 4; // 0x00:0
            unsigned int texture_flags : 4; // 0x00:4
            unsigned int sampler_state_idx : 24; // 0x00:8
        };

        union
        {
            DiscData disc; // 0x00
            DataStruct rt; // 0x00
        };
    };

    static const unsigned int kFlagsSize; // 0x012B8010
    static const unsigned int kStageSize; // 0x012B8014
    int texture_handle; // 0x00
    unsigned int name_checksum; // 0x04
    Data data; // 0x08
};
