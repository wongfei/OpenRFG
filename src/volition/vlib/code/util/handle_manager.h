// Original: volition/vlib/code/util/handle_manager.h
// Implementation: volition/vlib/code/util/handle_manager.inl
//
// Allocator of 32-bit object handles: a bit per slot (status, N/8 bytes), searched round-robin from the last allocated
// slot (allocate_handle, decompiled). A handle is upper_bits | slot index; world_zone sets the zone's grid id as upper
// bits (world_zone::set_gid), so a handle names its origin zone. allocate_handle returns 0xFFFFFFFF when all N slots are
// taken. Methods are in handle_manager.inl (save/load were never emitted).
#pragma once

struct cfile;

// size 0x39C for N = 7280
template<int N>
class handle_manager
{
public:
    handle_manager(); // <7280> 0x00925F00 (handle_manager.inl:39)
    void clear(); // <7280> 0x0091F280 (handle_manager.inl:53)
    void set_upper_bits(unsigned short bits); // <7280> 0x0091F2C0 (handle_manager.inl:69)
    unsigned int allocate_handle(); // <7280> 0x0091F2E0 (handle_manager.inl:78)
    void reserve_handle(unsigned int handle); // <7280> 0x0091F3B0 (handle_manager.inl:127)
    void release_handle(unsigned int handle); // <7280> 0x0091F3D0 (handle_manager.inl:142)
    bool is_handle_allocated(unsigned int handle); // <7280> 0x0091F3F0 (handle_manager.inl:157)
    void save(cfile*);
    void load(cfile*);

private:
    unsigned char status[(N + 7) / 8]; // 0x00
    unsigned int last_bin; // 0x390 for N = 7280
    unsigned int last_slot; // 0x394
    unsigned int upper_bits; // 0x398

    // world sets the zone grid id directly and marks preexisting handles in status (inlined in world_serialize.cpp,
    // world_stream.cpp and world.cpp); the friendship is inferred.
    friend class world;
    friend class world_zone;
};

#include "volition/vlib/code/util/handle_manager.inl"
