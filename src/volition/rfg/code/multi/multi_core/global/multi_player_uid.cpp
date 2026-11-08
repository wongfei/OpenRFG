// Original: volition/rfg/code/multi/multi_core/global/multi_player_uid.cpp
//
// Player unique ids (multiplayer.md MP2, ahead of MP3: the platform layer sets the local one). The comparisons look at
// the RemoteUserId's provider data only (rfg.exe tail-jumps into RemoteUserId::is_equal). On the wire a uid is 11
// bytes: the provider data, guestIndex, isValid and is_real_uid. The inline functions of multi_player_uid.h are
// defined here too.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"

#include <string.h>

// The local player's uid (multi_core_platform_set_my_player_uid). A global without a source file in the PDB; placed
// with its type.
multi_player_uid My_player_uid; // 0x02FE5B14

// 0x0094B720 (:12)
void multi_player_uid::set(const multi_player_uid& uid)
{
    u_id = uid.u_id;
    is_real_uid = uid.is_real_uid;
}

// 0x0094B740 (:18)
void multi_player_uid::set(const unit4::RemoteUserId& uid, bool real_uid)
{
    u_id = uid;
    is_real_uid = real_uid;
}

// 0x0094B760 (:24)
bool multi_player_uid::is_equal(const multi_player_uid& uid) const
{
    return u_id.is_equal(uid.u_id);
}

// 0x0094B770 (:29)
bool multi_player_uid::is_equal(const unit4::RemoteUserId& uid) const
{
    return u_id.is_equal(uid);
}

// 0x0094B780 (:34)
bool multi_player_uid::is_valid() const
{
    return u_id.isValid;
}

// 0x009740D0 (:39): is_real_uid is left as it is
void multi_player_uid::invalidate()
{
    memset(u_id.providerData, 0, sizeof(u_id.providerData));
    u_id.guestIndex = 0;
    u_id.isValid = false;
}

// 0x0094B790 (:45)
void multi_player_uid::write_to_packet(unsigned char* packet_data, unsigned short& packet_size) const
{
    for (unsigned int i = 0; i < sizeof(u_id.providerData); i++)
    {
        packet_data[packet_size++] = (unsigned char)u_id.providerData[i];
    }
    packet_data[packet_size++] = u_id.guestIndex;
    packet_data[packet_size++] = u_id.isValid;
    packet_data[packet_size++] = is_real_uid;
}

// 0x0094B830 (:52)
void multi_player_uid::read_from_packet(const unsigned char* packet_data, int& packet_offset)
{
    for (unsigned int i = 0; i < sizeof(u_id.providerData); i++)
    {
        u_id.providerData[i] = (char)packet_data[packet_offset++];
    }
    u_id.guestIndex = packet_data[packet_offset++];
    u_id.isValid = packet_data[packet_offset++] != 0;
    is_real_uid = packet_data[packet_offset++] != 0;
}

// 0x0094B8C0 (:61)
uint64_t multi_player_uid::get_offline_as_uint64() const
{
    uint64_t value;
    memcpy(&value, u_id.providerData, sizeof(value));
    return value;
}

// 0x0094B8D0 (:66)
uint64_t multi_player_uid::get_online_as_uint64() const
{
    uint64_t value;
    memcpy(&value, u_id.providerData, sizeof(value));
    return value;
}

// 0x0094B8E0 (:71)
bool multi_player_uid::is_official() const
{
    return is_real_uid;
}

// 0x0094B8F0 (:76)
bool multi_player_uid::is_online() const
{
    return is_real_uid;
}

// 0x008835D0 (.h:52)
multi_player_uid& multi_player_uid::operator=(const multi_player_uid& uid)
{
    set(uid);
    return *this;
}

// 0x009702C0 (.h:53)
bool multi_player_uid::operator==(const multi_player_uid& uid) const
{
    return u_id.is_equal(uid.u_id);
}

// 0x008835F0 (.h:56)
unit4::RemoteUserId& multi_player_uid::get_internal_id()
{
    return u_id;
}

// 0x009419D0 (.h:57)
const unit4::RemoteUserId& multi_player_uid::get_internal_id() const
{
    return u_id;
}
