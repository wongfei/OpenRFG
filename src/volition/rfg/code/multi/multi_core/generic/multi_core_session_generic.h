// Original: volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h
// Implementation: volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.cpp
//
// The generic (GameLink based) session: multi_session over a GameLink lobby.
#pragma once

#include "kaiko/gamelink/gamelink.hpp"
#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"

struct connection_address;
class multi_connection;

// size 0x30
class multi_session_info : public multi_session_info_base
{
public:
    multi_session_info(const multi_session_info_base& src); // 0x009A8000 (:88)
    multi_session_info(const multi_session_info& src); // 0x009A7FB0 (:82)
    multi_session_info(); // 0x009B4A90 (:76)
    virtual ~multi_session_info(); // vtbl+0x00 0x005D0E10 (.h:35)
    virtual void init(); // vtbl+0x04 0x009A8030 (:93)
    virtual void copy(const multi_session_info_base& src); // vtbl+0x08 0x0098E380 (:99)
    virtual int compare_by_id(const multi_session_info_base& comparand) const; // vtbl+0x0C 0x009425A0 (:107)
    virtual void append_migration_info_to_packet(unsigned char* packet_data, unsigned short& packet_size) const; // vtbl+0x10 0x00970F20 (:117)
    virtual void read_migration_info_from_packet(unsigned char* packet_data, int& packet_offset); // vtbl+0x14 0x00970F30 (:123)
    virtual void append_modification_info_to_packet(unsigned char* packet_data, unsigned short& packet_size) const; // vtbl+0x18 0x009425C0 (:129)
    virtual void read_modification_info_from_packet(unsigned char* packet_data, int& packet_offset); // vtbl+0x1C 0x00942690 (:152)
    virtual void append_join_info_to_packet(unsigned char* packet_data, unsigned short& packet_size) const; // vtbl+0x20 0x00970F40 (:175)
    virtual void read_join_info_from_packet(const unsigned char* packet_data, int& packet_offset); // vtbl+0x24 0x009710B0 (:207)
    virtual void append_id_to_packet(unsigned char* packet_data, unsigned short& packet_size) const; // vtbl+0x28 0x00942760 (:239)
    virtual void read_id_from_packet(unsigned char* packet_data, int& packet_offset); // vtbl+0x2C 0x009427E0 (:244)
    virtual void append_arbitration_info_to_packet(unsigned char*, unsigned short&) const; // vtbl+0x30 0x00942850 (:249)
    virtual void read_arbitration_info_from_packet(unsigned char*, int&); // vtbl+0x34 0x00942860 (:255)
    multi_session_info& operator=(const multi_session_info& __that); // 0x008AC600

    kaiko::GameLinkLobby m_lobbyId; // 0x28
};

// size 0x5F0
class multi_session : public multi_session_base
{
public:
    multi_session(); // 0x009B4AE0 (:266)
    virtual ~multi_session(); // vtbl+0x00 0x00971270 (:270)
    virtual session_op_status platform_session_create(); // vtbl+0x04 0x0098E3A0 (:274)
    virtual session_op_status platform_session_join(); // vtbl+0x08 0x00942870 (:380)
    virtual session_op_status platform_session_start_arbitration(); // vtbl+0x0C 0x00942910 (:409)
    virtual session_op_status platform_session_start_gameplay(); // vtbl+0x10 0x00971290 (:415)
    virtual session_op_status platform_session_end_gameplay(); // vtbl+0x14 0x009FD560 (:442)
    virtual session_op_status platform_session_delete(); // vtbl+0x18 0x009B4BF0 (:487)
    virtual session_op_status platform_session_modify(); // vtbl+0x1C 0x00942920 (:515)
    virtual session_op_status platform_session_migrate(); // vtbl+0x20 0x00942930 (:557)
    virtual bool platform_session_lock(); // vtbl+0x24 0x009712F0 (:564)
    virtual bool platform_session_unlock(bool bSetlobbyTypePublic); // vtbl+0x28 0x00971360 (:584)
    virtual session_op_status platform_process_operation(); // vtbl+0x2C 0x009E3230 (:608)
    virtual session_op_status platform_get_operation_status(); // vtbl+0x30 0x00942940 (:818)
    virtual void platform_cancel_current_operation(); // vtbl+0x34 0x00942950 (:825)
    virtual session_op_status platform_get_my_connection_address(connection_address& address, bool systemlink_or_lan); // vtbl+0x38 0x00942960 (:832)
    virtual session_op_status platform_get_host_connection_address(connection_address& address); // vtbl+0x3C 0x00971400 (:863)
    virtual session_op_status platform_add_connection(multi_connection* cp); // vtbl+0x40 0x00942A20 (:880)
    virtual session_op_status platform_remove_connection(multi_connection* cp); // vtbl+0x44 0x009B4D00 (:887)
    virtual int platform_compare_by_id(const multi_session_info_base* info); // vtbl+0x48 0x00942A30 (:906)
    virtual void platform_session_update(); // vtbl+0x4C 0x00942A40 (:915)

private:
    multi_session_info m_current_info; // 0x590
    multi_session_info m_desired_info; // 0x5C0
};

// Whether the game lobby was entered from an invitation; cleared when a session ends (placement inferred: next to
// this file's statics).
extern bool g_joinedGameLobbyViaInvitation; // 0x02FEB701

// 0x005D0E10 (.h:35)
inline multi_session_info::~multi_session_info()
{
}
