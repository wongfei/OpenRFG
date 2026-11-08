// Original: volition/rfg/code/multi/multi_core/include/multi_core_bandwidth.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_core_bandwidth.cpp
//
// Bandwidth estimation and limits of the connections.
#pragma once


class multi_connection;

void multi_core_bandwidth_do_frame(); // 0x00943E80 (:816)
void multi_core_bandwidth_increase_used(multi_connection* cp, unsigned int bytes, unsigned int packet_type, bool sent, int subtype); // 0x00943E90 (:844)
void multi_core_bandwidth_add_packet(multi_connection* cp, unsigned int bytes, bool sent, unsigned int reliable, unsigned int resent); // 0x00943EA0 (:868)
void multi_core_bandwidth_render(); // 0x00943EB0 (:885)
unsigned int multi_core_bandwidth_get_packet_bandwidth_not_sent(); // 0x00943EC0 (:1025)
void multi_core_bandwidth_add_packet_bandwidth_not_sent(multi_connection* cp, unsigned int bytes); // 0x00943ED0 (:1039)
void multi_core_bandwidth_init(); // 0x00943EE0 (:1081)
bool multi_core_bandwidth_is_enabled(); // 0x00943F00 (:1110)
void multi_core_bandwidth_enable(bool enable); // 0x00971DE0 (:1120)
bool multi_core_bandwidth_write_log(); // 0x00943F20 (:1136)
