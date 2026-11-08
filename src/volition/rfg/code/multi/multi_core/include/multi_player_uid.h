// Original: volition/rfg/code/multi/multi_core/include/multi_player_uid.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_player_uid.cpp
//
// Player unique ids: the platform user id of a player (a keen RemoteUserId).
#pragma once

#include <stdint.h>
#include "unit4/systemservices/systemservices.hpp"

// size 0xB
class multi_player_uid
{
public:
    void set(const unit4::RemoteUserId& uid, bool real_uid); // 0x0094B740 (:18)
    void set(const multi_player_uid& uid); // 0x0094B720 (:12)
    bool is_equal(const unit4::RemoteUserId& uid) const; // 0x0094B770 (:29)
    bool is_equal(const multi_player_uid& uid) const; // 0x0094B760 (:24)
    bool is_valid() const; // 0x0094B780 (:34)
    void invalidate(); // 0x009740D0 (:39)
    void write_to_packet(unsigned char* packet_data, unsigned short& packet_size) const; // 0x0094B790 (:45)
    void read_from_packet(const unsigned char* packet_data, int& packet_offset); // 0x0094B830 (:52)
    uint64_t get_offline_as_uint64() const; // 0x0094B8C0 (:61)
    uint64_t get_online_as_uint64() const; // 0x0094B8D0 (:66)
    bool is_official() const; // 0x0094B8E0 (:71)
    bool is_online() const; // 0x0094B8F0 (:76)
    multi_player_uid& operator=(const multi_player_uid& uid); // 0x008835D0 (.h:52)
    bool operator==(const multi_player_uid& uid) const; // 0x009702C0 (.h:53)
    const unit4::RemoteUserId& get_internal_id() const; // 0x009419D0 (.h:57)
    unit4::RemoteUserId& get_internal_id(); // 0x008835F0 (.h:56)

private:
    unit4::RemoteUserId u_id; // 0x00
    bool is_real_uid; // 0x0A
};

// The local player's uid (set by multi_core_platform_set_my_player_uid; placement inferred).
extern multi_player_uid My_player_uid; // 0x02FE5B14
